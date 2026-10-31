#!/usr/bin/env python3
"""How much smaller LACF frames are than plain frames, from two vector files (format "<frame hex or -> <text hex>").
  build/ppm_tool gen < texts.txt > v1.txt            (current format, spec 01)
  build/lacf_ending gen < texts.txt > lacf.txt       (LACF, spec 10)
  python3 tools/integrity/size_stats.py lacf.txt v1.txt
A packet that does not compress to a smaller frame is sent plain (3 + L bytes). Airtime: LoRa formula, BW 125 kHz, CR 4/5, 8 preamble symbols, explicit header, CRC on."""
import math, sys

def load(path):
    d = {}
    for l in open(path):
        if l.startswith('#') or not l.strip(): continue
        a, b = l.split(); d.setdefault(b, a)
    return d

def airtime(payload, sf, bw=125000, cr=1, crc=1, preamble=8):
    tsym = (2 ** sf) / bw; de = 1 if (sf >= 11 and bw == 125000) else 0
    n = 8 + max(math.ceil((8 * payload - 4 * sf + 28 + 16 * crc) / (4 * (sf - 2 * de))) * (cr + 4), 0)
    return (preamble + 4.25 + n) * tsym

lacf, v1 = load(sys.argv[1]), load(sys.argv[2])
rows = []
for h in lacf:
    plain = 3 + len(h) // 2
    f, g = lacf[h], v1.get(h)
    fl = len(f) // 2 if f != '-' else None
    gl = len(g) // 2 if g and g != '-' else None
    rows.append((plain, gl or plain, fl or plain, gl is not None, fl is not None))
n = len(rows); P = sum(r[0] for r in rows); V = sum(r[1] for r in rows); A = sum(r[2] for r in rows)
print("packets %d: mean plain frame %.1f B, current format (spec 01) %.1f B, LACF %.1f B" % (n, P / n, V / n, A / n))
print("saving vs plain: current %.1f%%, LACF %.1f%%   (LACF costs %.2f B more than the current format per packet)" % (100 * (P - V) / P, 100 * (P - A) / P, (A - V) / n))
print("sent compressed: current %d, LACF %d; sent plain instead: %d / %d" % (sum(r[3] for r in rows), sum(r[4] for r in rows), n - sum(r[3] for r in rows), n - sum(r[4] for r in rows)))
sav = sorted(100 * (r[0] - r[2]) / r[0] for r in rows)
print("per-packet saving percentiles (all packets): p10 %.0f%%, median %.0f%%, p90 %.0f%%" % (sav[n // 10], sav[n // 2], sav[9 * n // 10]))
for lo, hi in ((0, 40), (40, 60), (60, 80), (80, 100), (100, 300)):
    s = [r for r in rows if lo <= r[0] < hi]
    if s: print("  plain frame %3d-%3d B: %5d packets, mean %.1f -> %.1f B (%.0f%% smaller)" % (lo, hi, len(s), sum(r[0] for r in s) / len(s), sum(r[2] for r in s) / len(s), 100 * (sum(r[0] for r in s) - sum(r[2] for r in s)) / sum(r[0] for r in s)))
for sf in (7, 9, 12):
    ap = sum(airtime(r[0], sf) for r in rows) / n; al = sum(airtime(r[2], sf) for r in rows) / n; av = sum(airtime(r[1], sf) for r in rows) / n
    print("airtime SF%d BW125 CR4/5: plain %.0f ms, current format %.0f ms, LACF %.0f ms per packet (LACF %.0f%% less than plain)" % (sf, ap * 1e3, av * 1e3, al * 1e3, 100 * (ap - al) / ap))
