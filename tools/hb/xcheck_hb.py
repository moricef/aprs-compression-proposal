#!/usr/bin/env python3
"""Cross-check the C reference (build/hb_test) against the independent Python implementation (tools/hb/hb_py) on the same frames:
  python3 tools/hb/xcheck_hb.py PACKETS.txt [N_DAMAGED] [RELAY_VECTORS]
1. encode: both implementations must produce the same frame for every packet (gen), 2. receive: the same accept or drop and the same packet on intact frames,
every cut, appended bytes, every single-bit flip of a sample, and relayed frames (which carry RXT blocks). Prints disagreements."""
import os, random, subprocess, sys, importlib.util
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("hb", os.path.join(here, "hb_py", "hb.py")); H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)
random.seed(110031)
pk = [l.rstrip("\n").encode("latin-1") for l in open(sys.argv[1], encoding="latin-1") if l.strip() and not l.startswith("#")]
nd = int(sys.argv[2]) if len(sys.argv) > 2 else 150
c_gen = subprocess.run(["build/hb_test", "gen"], input=b"\n".join(pk) + b"\n", capture_output=True).stdout.decode().split("\n")
c_frames = [l.split()[0] for l in c_gen if l and not l.startswith("#")]
dis = 0
for p, cf in zip(pk, c_frames):
    pf = H.encode(p); ph = pf.hex() if pf else "-"
    if ph != cf: dis += 1; print("ENCODE DISAGREE", p[:60], "C", cf[:40], "Py", ph[:40])
print("%d packets: %d encode disagreements between C and Python" % (len(pk), dis))
frames = [bytes.fromhex(f) for f in c_frames if f != "-"]
cases = []                                                  # (kind, frame)
for f in frames: cases.append(("intact", f))
sample = random.sample(frames, min(nd, len(frames)))
for f in sample:
    for k in range(1, len(f)): cases.append(("cut", f[:-k]))
    for k in (1, 2, 3, 8): cases.append(("append", f + bytes(random.randrange(256) for _ in range(k))))
    for bit in range(len(f) * 8):
        b = bytearray(f); b[bit // 8] ^= 1 << (bit % 8); cases.append(("flip", bytes(b)))
if len(sys.argv) > 3:                                       # relayed frames carry RXT blocks: damage those too
    rel = [bytes.fromhex(l.split()[1]) for l in open(sys.argv[3]) if l.strip() and not l.startswith("#")]
    for f in rel: cases.append(("relayed", f))
    for f in random.sample(rel, min(nd, len(rel))):
        for k in range(1, len(f)): cases.append(("rxtcut", f[:-k]))
        for bit in range(len(f) * 8):
            b = bytearray(f); b[bit // 8] ^= 1 << (bit % 8); cases.append(("rxtflip", bytes(b)))
inp = "".join(c[1].hex() + "\n" for c in cases)
cout = subprocess.run(["build/hb_test", "recv"], input=inp.encode(), capture_output=True).stdout.decode().split("\n")
stats = {}; dis = 0
for (kind, f), co in zip(cases, cout):
    r = H.decode(f); pyo = r.hex() if isinstance(r, (bytes, bytearray)) else "-"
    s = stats.setdefault(kind, [0, 0, 0]); s[0] += 1
    if pyo != "-": s[1] += 1
    if pyo != co:
        dis += 1; s[2] += 1
        if dis <= 0: print("RECEIVE DISAGREE", kind, f.hex()[:60], "C", co[:30], "Py", pyo[:30], "(%s)" % (r if not isinstance(r, (bytes, bytearray)) else "ok"))
print("%d damaged or intact frames: %d receive disagreements" % (len(cases), dis))
print("%-8s %8s %9s %13s" % ("kind", "frames", "accepted", "disagreements"))
for k, (n, a, d) in stats.items(): print("%-8s %8d %9d %13d" % (k, n, a, d))
if os.environ.get("XC_WHY"):
    import collections
    c = collections.Counter(); ex = {}
    for (kind, f), co in zip(cases, cout):
        r = H.decode(f); pyo = r.hex() if isinstance(r, (bytes, bytearray)) else "-"
        if pyo != co:
            key = (kind, f[3] & 3, "C ok" if co != "-" else "C drop", "Py ok" if pyo != "-" else "Py " + str(r))
            c[key] += 1; ex.setdefault(key, f.hex()[:70])
    for k, n in c.most_common(14): print(n, k, ex[k])
