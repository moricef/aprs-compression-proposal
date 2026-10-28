#!/usr/bin/env python3
"""Print the bytes-saved tables from hybrid_layouts output.
  summarize.py whole.out nopath.out info.out      (each file from a run with its own layout and model, e.g. LAYOUTS=1 / 2 / 4)
Direct = heard over the air; relayed = packets whose path starts WIDE1-1 after one relay hop. Percent = bytes saved vs the plain frame."""
import re, sys
names = ("whole packet", "hybrid A", "hybrid B")
def parse(f):
    d = {}; cur = None; hop = False
    for ln in open(f):
        h = re.match(r"(WHOLE|HYBRID [AB]).*N=(\d), (heard direct|after one relay hop)", ln)
        if h: cur = int(h.group(2)); hop = h.group(3).startswith("after"); continue
        g = re.match(r"\s+(all LoRa packets|heard over RF|stationary stations|moving stations|RF, path starts WIDE1-1).*?avg\s+([\d.]+) B.*?saved\s+([\d.]+)%\s+SF12", ln)
        if g and cur: d[(cur, hop, g.group(1).split()[0])] = (float(g.group(3)), float(g.group(2)))
    return d
runs = [parse(f) for f in sys.argv[1:4]]
def row(label, key):
    cells = [f"{r[key][0]:5.1f}%" if key in r else "   n/a" for r in runs]
    print(f"  {label:34}" + "  ".join(f"{c:>12}" for c in cells))
print("  " + " " * 34 + "  ".join(f"{n:>12}" for n in names))
print("direct (heard over the air), bytes saved:")
for N in (1, 2, 4):
    row(f"all packets, N={N}", (N, False, "all"))
row("moving stations, N=2", (2, False, "moving")); row("stationary stations, N=2", (2, False, "stationary"))
print("relayed (path starts WIDE1-1, one relay), bytes saved:")
for N in (2, 4): row(f"N={N}", (N, True, "RF,"))
