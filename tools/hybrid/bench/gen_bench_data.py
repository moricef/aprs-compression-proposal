#!/usr/bin/env python3
"""Write the three packet sets the ESP32 benchmark compiles in (data_whole.h, data_nopath.h, data_info.h) from a prep.py TSV.
  gen_bench_data.py IN.tsv OUTDIR [--full 400] [--pairs 300] [--seed 1]
400 random heard-over-RF packets (full frames) and 300 consecutive same-station pairs (primed frames), in each layout's text form."""
import argparse, collections, random
ap = argparse.ArgumentParser(); ap.add_argument("tsv"); ap.add_argument("outdir")
ap.add_argument("--full", type=int, default=400); ap.add_argument("--pairs", type=int, default=300); ap.add_argument("--seed", type=int, default=1)
a = ap.parse_args()
rows = [l.rstrip("\n").split("\t", 4) for l in open(a.tsv, encoding="latin1")]
rows = [(r[0], r[4]) for r in rows if len(r) == 5 and r[1] == "1"]
rnd = random.Random(a.seed)
full = [t for _, t in rnd.sample(rows, a.full)]
bysrc = collections.defaultdict(list)
for s, t in rows: bysrc[s].append(t)
pairs = [(l[i], l[i + 1]) for l in bysrc.values() for i in range(len(l) - 1)]
pairs = rnd.sample(pairs, a.pairs)
def form(t, kind):
    gt = t.index(">"); c = t.index(":"); dst = t[gt + 1:c].split(",")[0]
    return t if kind == "whole" else t[:gt] + ">" + dst + ":" + t[c + 1:] if kind == "nopath" else ":" + t[c + 1:]
for kind in ("whole", "nopath", "info"):
    out = ["#include <Arduino.h>", "struct Pkt{const unsigned char*p;unsigned short n;};"]
    def block(prefix, items):
        names = []
        for i, t in enumerate(items):
            b = form(t, kind).encode("latin1"); out.append(f"static const unsigned char {prefix}{i}[] PROGMEM = {{{','.join(map(str, b))}}};"); names.append(f"{{{prefix}{i},{len(b)}}}")
        return names
    f = block("F", full); ra = block("RA", [p[0] for p in pairs]); rb = block("RB", [p[1] for p in pairs])
    out += ["static const Pkt FULL[]={" + ",".join(f) + "};", "static const Pkt REFS[]={" + ",".join(ra) + "};", "static const Pkt NEXTS[]={" + ",".join(rb) + "};",
            f"static const int NFULL={a.full}, NPAIR={a.pairs};"]
    open(f"{a.outdir}/data_{kind}.h", "w").write("\n".join(out) + "\n")
    print(kind, "avg bytes", round(sum(len(form(t, kind)) for t in full) / len(full), 1))
