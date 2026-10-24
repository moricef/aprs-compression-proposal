#!/usr/bin/env python3
"""Simulate priming the APRS-PPM model with a station's earlier packet (ideal coding cost).

For each VHF packet heard over the air, the cost is estimated three ways, all with the same pruned
order-3 model trained on other stations:
  static   the model alone (what spec 01 does)
  primed   the model plus the counts from the same station's earlier packet, weighted by w
It reports the average frame size and saving against a plain AX.25 frame. The frame is the LoRa-style
whole-packet frame of spec 07: coded bytes + 2 (version, length) + 2 (FCS), never larger than plain.

This is a Python approximation of the PPM blend (method C, no exclusions, uniform floor), not the C
codec; the static column should agree with `ppm_tool` to within a point or two (check it). Cost is
ideal arithmetic-coding length, not real bytes. Consecutive identical packets from one station are
dropped, so a beacon heard twice by different iGates is not counted as a free repeat.

  delta_sim.py --capture lora_and_vhf_raw.txt [more files] --max-packets 3000 --min-count 24 --ref-back 1

The input is raw APRS-IS text from tools/capture_aprsis.py, in capture order (the order is the
station's packet order). Raw captures hold real callsigns and are not published.
"""
import argparse, collections, hashlib, math, random, re

from delta_sim_pos import position

LINE = re.compile(rb"^([^>]+)>([^,:]+)((?:,[^,:]+)*):(.*)$", re.S)

def parse(line):
    line = line.rstrip(b"\r\n")
    if not line or line.startswith(b"#"): return None
    m = LINE.match(line)
    if not m: return None
    src, dst, path_, info = m.groups()
    keep, rf = [], False
    for p in path_.split(b",")[1:]:
        if p in (b"qAR", b"qAO"): rf = True
        if p.startswith(b"q"): break
        keep.append(p)
    if not info or len(info) > 250: return None
    text = src + b">" + dst + (b"," + b",".join(keep) if keep else b"") + b":" + info
    return (src, text, rf, dst.startswith(b"APL")) if len(text) <= 255 else None

bucket = lambda s: int(hashlib.md5(s).hexdigest(), 16) % 10

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", nargs="+", required=True)
    ap.add_argument("--min-count", type=int, default=24)
    ap.add_argument("--max-packets", type=int, default=3000)
    ap.add_argument("--ref-back", type=int, default=1, help="1 = the previous packet; 4 = four packets back")
    ap.add_argument("--keyframe-every", type=int, default=0, help="if N>0: every Nth packet from a station is coded static (a full frame); the others are primed with the latest full frame")
    ap.add_argument("--subset", choices=["all", "stationary", "moving"], default="all",
                    help="stationary: at least 3 position reports, all within 0.002 degrees (about 200 m); moving: spread over 0.005 degrees; stations with other behaviour (including Mic-E only) are left out")
    ap.add_argument("--mode", choices=["vhf", "lora"], default="vhf", help="vhf: AX.25 frame as plain, whole-packet LoRa-style frame coded; lora: LoRa APRS frame (3 + text plain, 4 + coded)")
    ap.add_argument("--weights", type=int, nargs="+", default=[32, 128, 512])
    ap.add_argument("--seed", type=int, default=5)
    a = ap.parse_args()

    seqs, uniq, pos = collections.defaultdict(list), {}, collections.defaultdict(list)
    for fn in a.capture:
        for line in open(fn, "rb"):
            r = parse(line)
            if not r: continue
            src, text, rf, lora = r
            if lora or rf:
                uniq[text] = (src, lora)
                if (lora == (a.mode == "lora")) and not (seqs[src] and seqs[src][-1] == text):
                    seqs[src].append(text)
                    pp = position(text.split(b":", 1)[1])
                    if pp: pos[src].append(pp)

    tabs = [collections.defaultdict(collections.Counter) for _ in range(4)]
    for t, (s, l) in uniq.items():                      # train on stations with bucket >= 3
        if bucket(s) >= 3:
            for i, ch in enumerate(t):
                for k in range(min(3, i) + 1): tabs[k][t[i-k:i]][ch] += 1
    stat = [{} for _ in range(4)]
    for k in range(4):
        for ctx, c in tabs[k].items():
            kept = {s: v for s, v in c.items() if v >= a.min_count}
            if kept:
                top = max(kept.values())
                if top > 255: kept = {s: max(1, v * 255 // top) for s, v in kept.items()}
                stat[k][ctx] = kept

    def bits(t, local=None, w=0):
        total = 0.0
        for i, ch in enumerate(t):
            P, esc = 0.0, 1.0
            for k in range(min(3, i), -1, -1):
                ctx = t[i-k:i]; c = stat[k].get(ctx)
                if local is not None and local[k].get(ctx):
                    c = dict(c) if c else {}
                    for s, v in local[k][ctx].items(): c[s] = c.get(s, 0) + w * v
                if not c: continue
                n, u = sum(c.values()), len(c)
                if ch in c: P += esc * c[ch] / (n + u)
                esc *= u / (n + u)
            total += -math.log2(P + esc / 256)
        return total

    def local_tabs(t):
        L = [collections.defaultdict(collections.Counter) for _ in range(4)]
        for i, ch in enumerate(t):
            for k in range(min(3, i) + 1): L[k][t[i-k:i]][ch] += 1
        return L

    random.seed(a.seed)
    def kind(src):
        pl = pos.get(src, [])
        if len(pl) < 3: return None
        d = max(max(x for x, y in pl) - min(x for x, y in pl), max(y for x, y in pl) - min(y for x, y in pl))
        return "stationary" if d <= 0.002 else "moving" if d >= 0.005 else None
    ks = collections.Counter(kind(s) for s in seqs)
    print(f"stations: {len(seqs)}; stationary {ks['stationary']}, moving {ks['moving']}, other {ks[None]}")
    test = [(s, l) for s, l in seqs.items() if bucket(s) < 3 and (a.subset == "all" or kind(s) == a.subset)]
    random.shuffle(test)
    res = collections.defaultdict(lambda: [0, 0.0, 0]); count = npair = nfirst = 0
    for s, l in test:
        for j, t in enumerate(l):
            if count >= a.max_packets: break
            ax = (3 + len(t)) if a.mode == "lora" else 7 * (2 + t.split(b":", 1)[0].count(b",")) + 4 + len(t.split(b":", 1)[1])
            frame = lambda b: min(ax, math.ceil(b / 8) + 4)   # 2 bytes version+length and 2 FCS (vhf); 3C FF ver len (lora)
            base = bits(t)
            kf = a.keyframe_every
            if kf and j % kf == 0:
                nfirst += 1; r = res["_first"]; r[0] += 1; r[1] += frame(base); r[2] += ax
            elif j > 0:
                npair += 1
                lt = local_tabs(l[j - (j % kf)] if kf else l[max(0, j - a.ref_back)])
                r = res["static (same packets)"]; r[0] += 1; r[1] += frame(base); r[2] += ax
                best = None
                for w in a.weights:
                    b = bits(t, lt, w); r = res[f"primed, w={w}"]; r[0] += 1; r[1] += frame(b); r[2] += ax
                    best = b if best is None else min(best, b)
                r = res["primed, best w per packet"]; r[0] += 1; r[1] += frame(best); r[2] += ax
            else:
                nfirst += 1; r = res["_first"]; r[0] += 1; r[1] += frame(base); r[2] += ax
            count += 1
        if count >= a.max_packets: break
    print(f"packets {count}; with an earlier packet from the station {npair}; first heard or full frame {nfirst}; "
          f"min-count {a.min_count}; " + (f"full frame every {a.keyframe_every}, others primed with the latest full frame" if a.keyframe_every else f"reference {a.ref_back} back"))
    for k, (n, b, p) in res.items():
        if k != "_first" and n: print(f"{k:28} n={n:5}  avg frame {b/n:5.1f} B vs plain {p/n:5.1f} B  saved {100*(1-b/p):3.0f}%")
    if res["_first"][0]:
        nf, bf, pf = res["_first"]
        for k in res:
            if k.startswith("primed, best"):
                n, b, p = res[k]
                print(f"overall incl. first-heard packets at the static cost: saved {100*(1-(b+bf)/(p+pf)):.0f}%")

main()
