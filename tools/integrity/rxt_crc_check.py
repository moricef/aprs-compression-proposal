#!/usr/bin/env python3
"""The CRC-16 over the RXT block of spec 11: block = tuples | CRC-16/X-25 of (tuples + mask), low byte first | mask.
Exhaustive 1, 2 and 3 bit flips anywhere in a block (tuples, mask or CRC) for 1, 2 and 3 tuples, and random bytes in place of a whole block.   python3 tools/integrity/rxt_crc_check.py"""
import importlib.util, itertools, os, random
spec = importlib.util.spec_from_file_location("lacf", os.path.join(os.path.dirname(os.path.abspath(__file__)), "lacf_py", "lacf.py"))
L = importlib.util.module_from_spec(spec); spec.loader.exec_module(L)
random.seed(64000); crc = L.crc16_x25
print("CRC-16/X-25 over the RXT block (tuples + mask), stored in 2 bytes: every 1, 2 and 3 bit flip anywhere in tuples, mask or CRC, exhaustively")
for k in (1, 2, 3):
    tot = miss = 0
    for trial in range(3):                                   # 3 random blocks per size
        tup = bytes(random.randrange(94) for _ in range(4 * k)); mask = bytes([0x80 | ((1 << k) - 1)])
        c = crc(tup + mask); blk = bytearray(tup + bytes([c & 0xFF, c >> 8]) + mask)
        for nflip in (1, 2, 3):
            for pos in itertools.combinations(range(len(blk) * 8), nflip):
                b = bytearray(blk)
                for p in pos: b[p // 8] ^= 1 << (p % 8)
                tot += 1
                if crc(bytes(b[:4 * k]) + bytes([b[-1]])) == (b[-3] | (b[-2] << 8)): miss += 1
    print("  k=%d (%d byte block): %d flips tried, %d undetected" % (k, 4 * k + 3, tot, miss))
n = ok = 0
for _ in range(2000000):
    k = random.choice((1, 2, 3)); b = bytes(random.randrange(256) for _ in range(4 * k + 3)); n += 1
    if crc(b[:4 * k] + b[-1:]) == (b[-3] | (b[-2] << 8)): ok += 1
print("random bytes in place of a whole block: %d of %d pass the CRC (a 16 bit check expects about %.0f)" % (ok, n, n / 65536))
