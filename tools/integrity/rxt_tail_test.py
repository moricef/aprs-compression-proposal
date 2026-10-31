#!/usr/bin/env python3
"""Does the proposed native-frame tail (spec 11) catch truncation and damage?  python3 tools/integrity/rxt_tail_test.py texts.txt [N [plain|marked|marked-crc|marked-range]]
Models the part of a native PPM frame that matters for integrity, using the independent LACF codec (tools/integrity/lacf_py):
    section = <model version 0x81> <L> <crc16 lo> <crc16 hi> <payload> [<tuple 4 B> * k <mask>]      (the RXT block ends the frame, mask byte last)
The bytes before the section (header, addresses, path, position) are not modelled: the cut and the damage fall on the end of the frame, which is where truncation happens.
Receiver rule of spec 11: if the header says an RXT block is present, the last byte is the mask, k = popcount(mask) must be 1..8 (1..7 when marked) with every set bit below the path count (marked: bit 7 must be set),
and the block is the last 1 + 4k bytes; the payload is what lies between the CRC and the block; then the payload rules of spec 11 (decode, CRC-16, re-encode must equal the payload exactly)."""
import importlib.util, os, random, sys
spec = importlib.util.spec_from_file_location("lacf", os.path.join(os.path.dirname(os.path.abspath(__file__)), "lacf_py", "lacf.py"))
L = importlib.util.module_from_spec(spec); spec.loader.exec_module(L)
random.seed(1131)
NPATH = 4                                      # path elements in the modelled header; mask bits must be below this
MODE = sys.argv[3] if len(sys.argv) > 3 else "plain"
MARKED = MODE in ("marked", "marked-crc", "marked-range")
RANGE = MODE == "marked-range"               # also reject a tuple whose bytes exceed the field maxima (RSSI 89, SNR 88, frequency offset and time to hop 93): no extra bytes
CRC = MODE == "marked-crc"                     # tuples | CRC-16 of (tuples + mask), low byte first | mask: the relay recomputes it when it appends   # "marked": the mask byte has bit 7 set (path elements 0 to 6 only) and tuple bytes are 0..93, so a tuple byte is never a mask

def build(text):
    f = L.encode(text)
    if f is None: return None
    return f[2:3] + f[3:4] + f[4:6] + f[6:]    # 0x81 L crc_lo crc_hi payload

def with_rxt(section, k):
    if k == 0: return section, False, None
    bits = sorted(random.sample(range(NPATH), min(k, NPATH))); mask = sum(1 << b for b in bits) | (0x80 if MARKED else 0)
    tuples = b"".join(bytes([random.randrange(90), random.randrange(89), random.randrange(94), random.randrange(94)]) if MARKED else bytes(random.randrange(256) for _ in range(4)) for _ in bits)
    if CRC:
        c = L.crc16_x25(tuples + bytes([mask]))
        return section + tuples + bytes([c & 0xFF, c >> 8, mask]), True, (tuples, mask)
    return section + tuples + bytes([mask]), True, (tuples, mask)

def receive(sec, rxt_flag):
    """Returns (text, rxt) or None (drop)."""
    if len(sec) < 4 or sec[0] != 0x81: return None
    rxt = None
    if rxt_flag:
        mask = sec[-1]
        if MARKED and not mask & 0x80: return None
        k = bin(mask & 0x7F).count("1")
        extra = 2 if CRC else 0
        if k < 1 or (mask & 0x7F) >> NPATH or len(sec) < 4 + 1 + extra + 4 * k: return None
        blk = len(sec) - 1 - extra - 4 * k
        tup = sec[blk:blk + 4 * k]
        if RANGE and any(tup[i] > (89, 88, 93, 93)[i % 4] for i in range(len(tup))): return None
        if CRC and L.crc16_x25(tup + bytes([mask])) != sec[-3] | (sec[-2] << 8): return None
        rxt = (tup, mask); body = sec[:blk]
    else:
        body = sec
    r = L.receive(bytes([0x3C, 0xFF]) + body[0:1] + body[1:2] + body[2:4] + body[4:], allow_tail=False)   # rebuild the 6 byte test frame of the payload tools
    if r is None: return None
    return r[0], rxt

texts = [l.rstrip("\r\n").encode("latin-1", "replace") for l in open(sys.argv[1]) if l.strip()]
N = int(sys.argv[2]) if len(sys.argv) > 2 else 800
random.shuffle(texts)
stats = {}
def note(kind, ok_orig, got, orig_text, orig_rxt):
    s = stats.setdefault(kind, [0, 0, 0])      # frames, accepted, accepted but not what was sent
    s[0] += 1
    if got is not None:
        s[1] += 1
        if got[0] != orig_text or got[1] != orig_rxt: s[2] += 1
done = 0
for t in texts:
    sec = build(t)
    if sec is None: continue
    done += 1
    if done > N: break
    for k in (0, 1, 2, 3):
        full, flag, rx = with_rxt(sec, k)
        want_rxt = (rx[0], rx[1]) if rx else None
        note("intact k=%d" % k, 1, receive(full, flag), t, want_rxt)
        for cut in (1, 2, 3, 4, 5, 6, 8, 12, 16):
            if len(full) > cut + 4: note("cut%-2d k=%d" % (cut, k), 0, receive(full[:-cut], flag), t, want_rxt)
        if flag:
            blklen = (1 + (2 if CRC else 0) + 4 * k)
            for nb in (1, 3):
                for _ in range(10):
                    b = bytearray(full)
                    for _ in range(nb):
                        p = len(b) - 1 - random.randrange(blklen); b[p] ^= 1 << random.randrange(8)
                    if bytes(b) != full: note("block%d k=%d" % (nb, k), 0, receive(bytes(b), flag), t, want_rxt)
        for nb in (1, 3):
            b = bytearray(full)
            for _ in range(nb):
                p = random.randrange(1, len(b)); b[p] ^= 1 << random.randrange(8)
            if bytes(b) != full: note("flip%d k=%d" % (nb, k), 0, receive(bytes(b), flag), t, want_rxt)
print("%d real texts, each with 0 to 3 RXT tuples at the end of the frame (mask byte last)" % (done - 1 if done > N else done))
print("%-14s %7s %9s %22s" % ("case", "frames", "accepted", "accepted, not as sent"))
for k in sorted(stats, key=lambda x: (x.split()[1], x.split()[0] if not x.startswith("intact") else "")):
    n, a, w = stats[k]; print("%-14s %7d %9d %22d" % (k, n, a, w))
