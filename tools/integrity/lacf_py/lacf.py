"""Independent second implementation of LACF (spec/10) on the model-v1 codec (spec/01).
Python 3, standard library only.  Written from the specs; spec sections cited in comments.

    python3 -I lacf.py gen < packets.txt     (one text per line -> vector lines)
    python3 -I lacf.py check vectors.txt
"""
import os
import re
import sys

MASK32 = 0xFFFFFFFF
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "..", "..", "..", "model", "aprs_ppm_model_v1.c")


# ---------------------------------------------------------------- model (spec 01, "The model")
def _load_model(path=MODEL_PATH):
    """Parse the C file purely as data: arrays KEYS0..3, OFFS0..3, symbols, counts."""
    with open(path, encoding="latin-1") as fh:
        src = fh.read()

    def arr(name):
        m = re.search(r"\b" + name + r"\[\]\s*=\s*\{(.*?)\};", src, re.S)
        return [int(x) for x in re.findall(r"\d+", m.group(1))]

    symbols, counts = arr("aprs_ppm_symbols"), arr("aprs_ppm_counts")
    orders = []
    for k in range(4):
        keys, offs = arr("KEYS%d" % k), arr("OFFS%d" % k)
        table = {}
        for i in range(len(keys) // 3):
            # key = last k bytes packed big-endian in 3 bytes
            key = (keys[3 * i] << 16) | (keys[3 * i + 1] << 8) | keys[3 * i + 2]
            lo, hi = offs[i], offs[i + 1]
            table[key] = list(zip(symbols[lo:hi], counts[lo:hi]))
        orders.append(table)
    return orders


_ORDERS = None


def _model():
    global _ORDERS
    if _ORDERS is None:
        _ORDERS = _load_model()
    return _ORDERS


def _freqs(text, i):
    """Spec 01 'Prediction (normative)': frequency table for position i."""
    orders = _model()
    rem = MASK32
    pr = [0] * 256
    for k in range(min(3, i), -1, -1):
        if rem <= 0:
            break
        key = 0
        for b in text[i - k:i]:
            key = (key << 8) | b
        ctx = orders[k].get(key)
        if ctx is None:
            continue
        n = sum(c for _, c in ctx)
        u = len(ctx)
        denom = n + u
        for b, c in ctx:
            pr[b] += rem * c // denom
        rem = rem * u // denom
    floor_share = rem >> 8
    freq = [1 + ((pr[s] + floor_share) * (65536 - 256) >> 32) for s in range(256)]
    return freq


# ---------------------------------------------------------------- CRC
def crc16_x25(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8408 if crc & 1 else crc >> 1
    return crc ^ 0xFFFF


# ---------------------------------------------------------------- encoder
def _encode_payload(text):
    """Range-code text, then apply the spec 10 'Ending (normative)'. Returns payload bytes."""
    out = []
    low, rng = 0, MASK32

    def carry():
        j = len(out) - 1
        while j >= 0:
            if out[j] == 255:
                out[j] = 0
                j -= 1
            else:
                out[j] += 1
                return
        raise AssertionError("carry out of empty output")

    for i, byte in enumerate(text):
        f = _freqs(text, i)
        cum = sum(f[:byte])
        total = sum(f)
        r = rng // total
        low += r * cum
        rng = r * f[byte]
        while rng < (1 << 24):  # shift (spec 01)
            if low >= (1 << 32):
                carry()
                low -= 1 << 32
            out.append(low >> 24)
            low = (low << 8) & MASK32
            rng <<= 8
    # spec 10 Ending
    if low >= (1 << 32):                                   # 1.
        carry()
        low -= 1 << 32
    hi = low + rng - 1                                     # 2.
    for nb in range(5):                                    # 3.
        blk = 1 << (8 * (4 - nb))
        v = -(-low // blk) * blk
        if v + blk - 1 <= hi:
            break
    if v >= (1 << 32):                                     # 4.
        carry()
        v -= 1 << 32
    for j in range(nb):                                    # 5. top nb bytes; 6. no stripping
        out.append((v >> (24 - 8 * j)) & 0xFF)
    return bytes(out)


def _frame(text, payload):
    crc = crc16_x25(text)
    return bytes([0x3C, 0xFF, 0x81, len(text), crc & 0xFF, crc >> 8]) + payload


def encode(text):
    """Spec 10 'Frame'. None if 6+n < 3+L does not hold (or L outside 1..255)."""
    if not 1 <= len(text) <= 255:
        return None
    payload = _encode_payload(text)
    if 6 + len(payload) < 3 + len(text):
        return _frame(text, payload)
    return None


# ---------------------------------------------------------------- receiver
def _decode_text(payload_and_more, L):
    """Spec 01 'Decoding'. Input = bytes from frame offset 6 on. Missing bytes are zero."""
    data = payload_and_more
    pos = 0

    def nxt():
        nonlocal pos
        b = data[pos] if pos < len(data) else 0
        pos += 1
        return b

    code = 0
    for _ in range(4):
        code = (code << 8) | nxt()
    rng = MASK32
    text = bytearray()
    for i in range(L):
        f = _freqs(text, i)
        total = sum(f)
        r = rng // total
        v = min(code // r, total - 1)
        cum = 0
        for s in range(256):
            if v < cum + f[s]:
                break
            cum += f[s]
        text.append(s)
        code -= r * cum
        rng = r * f[s]
        while rng < (1 << 24):
            code = ((code << 8) | nxt()) & MASK32
            rng <<= 8
    return bytes(text)


def receive(frame, allow_tail=False):
    """Spec 10 'Receiver (normative)'. Returns (text, payload_len) or None (drop)."""
    if len(frame) < 6 or frame[0] != 0x3C or frame[1] != 0xFF or frame[2] != 0x81:   # 1.
        return None
    L = frame[3]
    if L < 1:
        return None
    body = frame[6:]
    text = _decode_text(body, L)                                                    # 2.
    if crc16_x25(text) != frame[4] | (frame[5] << 8):                               # 3.
        return None
    mine = _encode_payload(text)                                                    # 4.
    m = len(mine)
    if 6 + m >= 3 + L:                                                              # 4. (revised spec: the sender would not have sent this compressed)
        return None
    if body[:m] != mine:
        return None
    if len(body) > m and not allow_tail:                                            # 5.
        return None
    return text, m


# ---------------------------------------------------------------- command line
def _line(text):
    f = encode(text)
    return "%s %s" % (f.hex() if f else "-", text.hex())


def main(argv):
    if len(argv) >= 2 and argv[1] == "gen":
        for line in sys.stdin.buffer.read().split(b"\n"):
            line = line.rstrip(b"\r")
            if not line or line.startswith(b"#"):
                continue
            print(_line(line))
        return 0
    if len(argv) >= 3 and argv[1] == "check":
        ok = total = 0
        for ln, line in enumerate(open(argv[2]), 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            fh, th = line.split()
            text = bytes.fromhex(th)
            want = None if fh == "-" else bytes.fromhex(fh)
            total += 1
            good = encode(text) == want
            if want is not None:
                r = receive(want)
                good = good and r is not None and r[0] == text and r[1] == len(want) - 6
            if good:
                ok += 1
            else:
                print("MISMATCH line %d: %s" % (ln, line[:80]))
        print("vectors matched %d of %d" % (ok, total))
        return 0 if ok == total else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
