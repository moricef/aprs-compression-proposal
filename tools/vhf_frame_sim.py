#!/usr/bin/env python3
"""Make audio (1200 baud Bell 202 AFSK, WAV) of plain and compressed VHF APRS frames, to see what an
unmodified decoder (for example Direwolf's atest) does with them. No radio needed.

  vhf_frame_sim.py --ppm build/ppm_tool --packets test/synthetic_packets.txt --outdir /tmp/vhf

Writes one WAV per frame variant:
  plain      ordinary AX.25 UI frame (control: legacy must decode these)
  infopid    addresses as normal, PID 0xF1, info = [version][length][range-coded info]
  hybrid     source/destination as normal, PID 0xF1, payload = compressed path + info
  whole      no AX.25 addresses: [version][length][range-coded SRC>DEST,PATH:info], standard FCS
  wholefcs   same as whole but with a different CRC, so a legacy TNC sees a bad checksum
The packets are fabricated (test/synthetic_packets.txt); the compressed bytes come from the
reference codec's `gen` mode. This tool builds frames only for the legacy-reaction test; it does not
decode compressed audio.
"""
import argparse, math, os, struct, subprocess, sys, wave

FS = 44100

def crc16(data, init=0xFFFF, poly=0x8408, xorout=0xFFFF):
    c = init
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ poly if c & 1 else c >> 1
    return c ^ xorout

def addr(call, last):
    name, _, ssid = call.partition("-")
    name = name.upper().ljust(6)[:6]
    b = bytearray((ord(ch) << 1) for ch in name)
    b.append(0x60 | ((int(ssid) if ssid else 0) << 1) | (1 if last else 0))
    return bytes(b)

def ax25(src, dst, digis, pid, payload):
    calls = [dst, src] + digis
    a = b"".join(addr(c.rstrip("*"), i == len(calls) - 1) for i, c in enumerate(calls))
    return a + bytes([0x03, pid]) + payload

def hdlc_bits(frame_with_fcs, nflags_pre=40, nflags_post=4):
    bits = []
    def flag(): bits.extend([0, 1, 1, 1, 1, 1, 1, 0])
    for _ in range(nflags_pre): flag()
    ones = 0
    for byte in frame_with_fcs:
        for i in range(8):
            bit = (byte >> i) & 1
            bits.append(bit)
            ones = ones + 1 if bit else 0
            if ones == 5: bits.append(0); ones = 0
    for _ in range(nflags_post): flag()
    return bits

def afsk(bits):
    out, phase, tone = [], 0.0, 1200.0   # NRZI: 0 = change tone, 1 = keep
    spb = FS / 1200.0
    n = 0
    for bit in bits:
        if bit == 0: tone = 2200.0 if tone == 1200.0 else 1200.0
        for _ in range(int(round((n + 1) * spb)) - int(round(n * spb))):
            phase += 2 * math.pi * tone / FS
            out.append(int(0.5 * 32767 * math.sin(phase)))
        n += 1
    return out

def write_wav(path, frames_bits):
    samples = []
    for bits in frames_bits:
        samples += [0] * int(0.3 * FS) + afsk(bits)
    samples += [0] * int(0.3 * FS)
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(FS)
        w.writeframes(struct.pack("<%dh" % len(samples), *samples))

def ppm_payloads(ppm, texts):
    r = subprocess.run([ppm, "gen"], input="\n".join(texts) + "\n", capture_output=True, text=True, check=True).stdout
    lines = [l for l in r.splitlines() if l and not l.startswith("#")][:len(texts)]
    out = []
    for l in lines:
        frame_hex = l.split()[0]
        out.append(bytes.fromhex(frame_hex)[2:] if frame_hex != "-" else None)   # drop 3C FF: keep [ver][len][coded]
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ppm", required=True); ap.add_argument("--packets", required=True)
    ap.add_argument("--outdir", required=True); ap.add_argument("--max", type=int, default=8)
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    pk = []
    for line in open(a.packets):
        line = line.rstrip("\n")
        if not line or line.startswith("#"): continue
        head, _, info = line.partition(":")
        src, _, rest = head.partition(">")
        parts = rest.split(",")
        pk.append((src, parts[0], parts[1:], info, line))
        if len(pk) == a.max: break
    whole = ppm_payloads(a.ppm, [p[4] for p in pk])
    hyb = ppm_payloads(a.ppm, ["A>A" + ("," + ",".join(p[2]) if p[2] else "") + ":" + p[3] for p in pk])
    infoc = ppm_payloads(a.ppm, [p[3] for p in pk])
    sets = {k: [] for k in ("plain", "infopid", "hybrid", "whole", "wholefcs")}
    used = 0
    for p, w, h, i in zip(pk, whole, hyb, infoc):
        src, dst, digis, info, text = p
        if not (w and h and i): continue
        used += 1
        def std(body): return hdlc_bits(body + struct.pack("<H", crc16(body)))
        sets["plain"].append(std(ax25(src, dst, digis, 0xF0, info.encode("latin1"))))
        sets["infopid"].append(std(ax25(src, dst, digis, 0xF1, i)))
        sets["hybrid"].append(std(ax25(src, dst, [], 0xF1, h)))
        sets["whole"].append(std(w))
        alt = w + struct.pack("<H", crc16(w, init=0x0000, xorout=0x0000))
        sets["wholefcs"].append(hdlc_bits(alt))
    for k, v in sets.items():
        write_wav(os.path.join(a.outdir, k + ".wav"), v)
    print(f"{used} packets per file; wrote {len(sets)} WAV files to {a.outdir}")

main()
