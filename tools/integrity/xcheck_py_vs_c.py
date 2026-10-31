"""Cross-check the C receiver (build/lacf_ending recv, or the tracker library test tool) against the independent Python receiver on damaged frames:
   python3 tools/integrity/xcheck_py_vs_c.py VECTORS.txt N BINARY      e.g. VECTORS = lacf_ending gen < texts.txt, N = number of vectors to damage, BINARY = build/lacf_ending
Damage: intact, intact with a 6 byte tail, cuts, cuts behind a tail, 1/2/3/8 bit flips, random payload. Reports disagreements and wrong accepts."""
import os
import sys, random, subprocess, importlib.util
spec = importlib.util.spec_from_file_location("lacf", os.path.join(os.path.dirname(os.path.abspath(__file__)), "lacf_py", "lacf.py")); L = importlib.util.module_from_spec(spec); spec.loader.exec_module(L)
random.seed(77)
vecs = [l.split() for l in open(sys.argv[1]) if l.strip() and not l.startswith('#')]
vecs = [(bytes.fromhex(a), bytes.fromhex(b)) for a, b in vecs if a != '-'][:int(sys.argv[2])]
cases = []                                    # (kind, mode, frame, original text, frame_len_intact)
for fr, text in vecs:
    cases.append(("intact", 'N', fr, text))
    tail = bytes(random.randrange(256) for _ in range(6))
    cases.append(("intact+tail", 'T', fr + tail, text))
    for k in (1, 2, 3, 6, 8, 16):
        if len(fr) > k + 6: cases.append(("cut%d" % k, 'N', fr[:-k], text))
        if len(fr) + 6 > k + 6 and k >= 8: cases.append(("tail+cut%d" % k, 'T', (fr + tail)[:-k], text))
    for nb in (1, 2, 3, 8):
        b = bytearray(fr)
        for _ in range(nb):
            p = random.randrange(6, len(b)); b[p] ^= 1 << random.randrange(8)
        if bytes(b) != fr: cases.append(("flip%d" % nb, 'N', bytes(b), text))
    b = bytearray(fr); 
    for p in range(6, len(b)): b[p] = random.randrange(256)
    cases.append(("random", 'N', bytes(b), text))
inp = "".join("%s %s\n" % (m, f.hex()) for _, m, f, _ in cases)
c = subprocess.run([sys.argv[3], "recv"], input=inp, capture_output=True, text=True).stdout.split("\n")
dis = 0; stats = {}
for i, (kind, m, f, text) in enumerate(cases):
    r = L.receive(f, allow_tail=(m == 'T'))
    py = "-" if r is None else r[0].hex()
    if py != c[i]: dis += 1; print("DISAGREE", kind, m, f.hex()[:60], "py", py[:30], "c", c[i][:30])
    s = stats.setdefault(kind, [0, 0, 0]); s[0] += 1
    if py != "-": s[1] += 1; s[2] += (py != text.hex())
print("%d frames, %d disagreements between the C receiver and the independent Python one" % (len(cases), dis))
print("%-14s %7s %9s %12s" % ("kind", "frames", "accepted", "wrong text"))
for k, (n, a, w) in stats.items(): print("%-14s %7d %9d %12d" % (k, n, a, w))
