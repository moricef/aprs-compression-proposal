#!/usr/bin/env python3
"""Spec 11 RXT rejection vectors: frames with a damaged or impossible RXT block that every receiver must DROP.   python3 tools/hb/gen_reject.py > test/vectors_hb_reject_v1.txt
Each line: <frame hex> <reason>. Built from the first second-hop frame of test/vectors_hb_relay_chain_v1.txt (two tuples, mask 0x83) and its received frame."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__)); test = os.path.join(here, '..', '..', 'test')
rows = [l.split() for l in open(os.path.join(test, 'vectors_hb_relay_chain_v1.txt')) if l.strip() and l[0] != '#']
f = bytes.fromhex(rows[0][1])                    # relayed twice: ends with t1 t2 mask
assert f[-1] == 0x83 and f[3] & 8
body, t1, t2 = f[:-9], f[-9:-5], f[-5:-1]
hdr = f[3]
out = []
def add(frame, why): out.append('%s %s' % (frame.hex(), why))
add(body + t1 + t2 + bytes([0x03]), 'mask has no marker bit (bit 7 clear)')
add(body + t1 + t2 + bytes([0x83 | 0x40]), 'mask bit 6 set but the path has fewer elements')
add(body + t1 + t2 + bytes([0x80]), 'mask with no tuple bit (k = 0)')
add(body + t2 + bytes([0x83]), 'mask names 2 tuples but only one is present')
add(body + t1 + t2, 'tuples without a mask byte')
for i, (name, v) in enumerate((('RSSI', 90), ('SNR', 89), ('frequency offset', 90), ('time to hop', 90))):
    b = bytearray(t2); b[i] = v; add(body + t1 + bytes(b) + bytes([0x83]), '%s byte %d is above its maximum' % (name, v))
add(f[:3] + bytes([hdr & ~8]) + f[4:], 'RXT bit cleared but the block is still there')
o = bytes.fromhex(rows[0][0])                    # the frame before the second relay: one tuple; take its origin by dropping the block
o = o[:3] + bytes([o[3] & ~8]) + o[4:-5]
add(o[:3] + bytes([o[3] | 8]) + o[4:], 'RXT bit set but no block')
sys.stdout.write('# Spec 11 RXT rejection vectors: <frame hex> <why it must be dropped>. Every receiver must drop each line.\n' + '\n'.join(out) + '\n')
