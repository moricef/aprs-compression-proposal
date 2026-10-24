#!/usr/bin/env python3
"""Can a receiver decode a packet that is primed with the sender's earlier packet?

A receiver can only use a reference packet it actually heard. This takes raw APRS-IS captures of
VHF packets heard over the air, rebuilds each moving station's sequence of transmissions (one element
per transmission, with the set of iGates that heard it, from the q-construct), and for every
(receiving iGate, moving station) pair asks: of the packets this iGate heard, in how many had it also
heard the reference?

References tried: the previous packet, the one two back, three back, and the latest full frame when a
full frame is sent every N packets. A packet that is a full frame, or the first of its sequence, is
always decodable.

Limits: only transmissions at least one iGate heard are visible, so the true gaps are larger than
these (the results are an upper bound). Receiver "elevation" is not known; iGates are grouped by how
many distinct stations they heard, a proxy. Raw captures hold real callsigns and are not published.

  receiver_sim.py --capture a.txt b.txt
"""
import argparse, collections, re
from delta_sim_pos import position   # same position parser as delta_sim.py

LINE = re.compile(rb"^([^>]+)>([^,:]+)((?:,[^,:]+)*):(.*)$", re.S)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", nargs="+", required=True)
    ap.add_argument("--min-seq", type=int, default=4, help="moving stations need at least this many transmissions")
    a = ap.parse_args()

    seq = collections.defaultdict(list)          # src -> [ [info, set(igates), position] ]
    heard_by = collections.defaultdict(set)      # igate -> stations
    for fn in a.capture:
        for line in open(fn, "rb"):
            line = line.rstrip(b"\r\n")
            m = LINE.match(line)
            if not m or line.startswith(b"#"): continue
            src, dst, path_, info = m.groups()
            if dst.startswith(b"APL") or not info: continue
            parts = path_.split(b",")[1:]
            ig = None
            for i, p in enumerate(parts):
                if p in (b"qAR", b"qAO") and i + 1 < len(parts): ig = parts[i + 1]
            if ig is None: continue
            heard_by[ig].add(src)
            els = seq[src]
            for e in els[-3:]:
                if e[0] == info: e[1].add(ig); break
            else:
                els.append([info, {ig}, position(info)])

    def spread(els):
        p = [e[2] for e in els if e[2]]
        if len(p) < 3: return None
        return max(max(x for x, y in p) - min(x for x, y in p), max(y for x, y in p) - min(y for x, y in p))
    movers = {s: e for s, e in seq.items() if len(e) >= a.min_seq and (spread(e) or 0) >= 0.005}
    print(f"stations {len(seq)}; moving stations with >= {a.min_seq} transmissions: {len(movers)}")

    ig_rank = sorted(heard_by, key=lambda g: len(heard_by[g]))
    q = max(1, len(ig_rank) // 4)
    low, high = set(ig_rank[:q]), set(ig_rank[-q:])

    schemes = ["prev", "2 back", "3 back", "full/4", "full/8"]
    def ok(scheme, j, H):
        if j == 0: return True
        if scheme == "prev": return (j - 1) in H
        if scheme == "2 back": return j < 2 or (j - 2) in H
        if scheme == "3 back": return j < 3 or (j - 3) in H
        n = 4 if scheme == "full/4" else 8
        return j % n == 0 or (j - j % n) in H
    tot = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0]))   # group -> scheme -> [ok, heard]
    pairs = collections.Counter()
    for src, els in movers.items():
        m = len(els)
        per = collections.defaultdict(set)
        for j, e in enumerate(els):
            for g in e[1]: per[g].add(j)
        for g, H in per.items():
            h = len(H) / m
            groups = ["all pairs", "coverage >= 90%" if h >= 0.9 else "coverage 50-90%" if h >= 0.5 else "coverage < 50%",
                      "busiest quarter of iGates" if g in high else "quietest quarter of iGates" if g in low else None]
            for grp in groups:
                if not grp: continue
                pairs[grp] += 1
                for sch in schemes:
                    for j in H:
                        tot[grp][sch][1] += 1; tot[grp][sch][0] += ok(sch, j, H)
    print(f"{'group':28}{'pairs':>6}{'heard':>7}" + "".join(f"{s:>9}" for s in schemes))
    for grp in ["all pairs", "coverage >= 90%", "coverage 50-90%", "coverage < 50%", "busiest quarter of iGates", "quietest quarter of iGates"]:
        t = tot[grp]
        if t["prev"][1]:
            print(f"{grp:28}{pairs[grp]:6}{t['prev'][1]:7}" + "".join(f"{100*t[s][0]/t[s][1]:8.0f}%" for s in schemes))

main()
