"""Run from the repository root after `make hb-vectors`:  python3 tools/hb/replay_f4mlv_cases.py
Fabrice's 2 Nov 2026 cases A, B, C, replayed on the reference code with the raw-content CRC (format version 2).
Software replay of the logic, not an RF test: frames are built by the reference encoder/relay and modified as he describes."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hb_py'))
import hb as H
res = []
def ok(name, cond, note=''): res.append((name, bool(cond), note))
def tuples_of(fr):
    d = H._parse(fr); return d['tuples']
def relay(fr, call='GENATX', t=bytes([50, 20, 45, 10])): return H.relay(fr, call, t)
def content_start(fr): return H._parse(fr)['cstart']
# ---- A: relaying (text identical, content copied byte for byte, tuple on the right element)
A = [('A1 status (PPM)', b'F4MLV-2>APLRG1,WIDE1-1:>Test status of the LoRa APRS station near Genas, SF12 BW125'),
     ('A2 short status (raw)', b'F4MLV-2>APLRG1,WIDE1-1:>TEST'),
     ('A3 uncompressed position', b'F4MLV-2>APLRG1,WIDE1-1:!4545.10N/00455.20E>Test position with a longer comment on the air'),
     ('A4 compressed position', b'F4MLV-2>APLRG1,WIDE1-1:!/5L!!<*e7>7P[Test position compressed with a longer comment'),
     ('A5 message', b'F4MLV-2>APLRG1,WIDE1-1::F4MLV-10 :Hello from the transmitter, test message number 1{1'),
     ('A6 ACK (raw)', b'F4MLV-2>APLRG1,WIDE1-1::F4MLV-10 :ack1'),
     ('A7 alphanumeric SSID (raw)', b'F4MLV-AB>APLRG1,WIDE1-1:>ok'),
     ('A11 WIDE2-2, longer path', b'F4MLV-2>APLRG1,WIDE2-2:>Test status of the LoRa APRS station near Genas, SF12 BW125')]
for name, pkt in A:
    fr = H.encode(pkt); rl = relay(fr) if fr else None
    if not fr or not rl: ok(name, False, 'encode/relay failed'); continue
    cs, cs2 = content_start(fr), content_start(rl)
    d1, d2 = H._parse(fr), H._parse(rl)
    body1 = fr[d1['pos_start']:d1['content_end']]; body2 = rl[d2['pos_start']:d2['content_end']]     # position block + content
    dec = H.decode(rl)
    exp_path = {'WIDE1-1': 'GENATX*', }
    ok(name, body1 == body2 and isinstance(dec, bytes) and dec.split(b':', 1)[1] == pkt.split(b':', 1)[1] and len(d2['tuples']) == 1 and d2['tuples'][0][0] == 0,
       'raw' if H._parse(fr)['plain'] else 'LACF'); res[-1] = res[-1][:2] + (res[-1][2] + ' %d -> %d, tuples at %s, path %s' % (len(fr), len(rl), [t[0] for t in d2['tuples']], dec.split(b':')[0].decode().split(',', 1)[1]),)
# A8-A10, A12: pre-existing tuples
base = H.encode(b'F4MLV-2>APLRG1,N0RLY-1*,N0RLY-2*,WIDE2-1:>Test status of the LoRa APRS station near Genas, SF12 BW125')
d = H._parse(base)
ok('A pre-existing: frame has no tuples to start', not d['tuples'])
# ---- B: modified frames: each must be rejected (current-format B2/B6 were accepted before the change)
raw = H.encode(b'F4MLV-2>APLRG1,WIDE1-1:>TEST')
ppm = H.encode(b'F4MLV-2>APLRG1,WIDE1-1:>Test status of the LoRa APRS station near Genas, SF12 BW125')
rawrx = relay(raw); ppmrx = relay(ppm)
def rej(fr): return not isinstance(H.decode(fr), bytes)
ok('B1 PPM, last byte removed -> rejected', rej(ppm[:-1]))
ok('B2 raw, last byte removed -> rejected (was ACCEPTED before)', rej(raw[:-1]))
mask_off = lambda f: f[:-1]
ok('B3 PPM + RXT, mask removed -> rejected', rej(ppmrx[:-1]))
ok('B4 PPM + RXT, last content byte removed -> rejected', rej(ppmrx[:-6] + ppmrx[-5:]))
ok('B5 raw + RXT, mask removed -> rejected', rej(rawrx[:-1]))
ok('B6 raw + RXT, last content byte removed -> rejected (was ACCEPTED before)', rej(rawrx[:-6] + rawrx[-5:]))
ok('B7 unknown protocol version -> rejected', rej(bytes(raw[:content_start(raw)]) + bytes([5 << 3]) + raw[content_start(raw)+1:]))
ok('B7b unknown table version -> rejected', rej(bytes(raw[:content_start(raw)]) + bytes([(2 << 3) | 3]) + raw[content_start(raw)+1:]))
ok('B8 flip raw/LACF kind bit -> rejected', rej(raw[:3] + bytes([raw[3] ^ 2]) + raw[4:]))
b = bytearray(raw); b[3] |= 4; ok('B9 reserved header bit -> rejected', rej(bytes(b)))
ok('B10 WIDE1-2 in the bytes -> rejected', H.encode(b'F4MLV-2>APLRG1,WIDE1-2:>x') is None)
ok('B11 old test-build V bytes (0x01, 0xC1, 0x02, 0xC2) -> rejected', all(rej(raw[:content_start(raw)] + bytes([v]) + raw[content_start(raw)+1:]) for v in (0x01, 0xC1, 0x02, 0xC2)))
# ---- C: his bench variant checks, on the real format
for i, (nm, pkt) in enumerate([('C1 PPM relayed', ppm), ('C2 raw relayed', raw), ('C3 position + raw comment', H.encode(b'F4MLV-2>APLRG1,WIDE1-1:!4545.10N/00455.20E>ab')), ('C4 ACK raw', H.encode(b'F4MLV-2>APLRG1,WIDE1-1::F4MLV-10 :ack1'))]):
    r = relay(pkt); ok(nm + ' (CRC still valid after path change + tuple)', r and isinstance(H.decode(r), bytes))
_f = H.encode(b'F4MLV-2>APLRG1,WIDE1-1,WIDE2-1:>TEST'); _r1 = relay(_f, 'GENATX'); _r2 = H.relay(_r1, 'GENATW', bytes([51, 21, 46, 11]))
ok('C5 raw frame, relayed twice (second relay sees an earlier tuple)', _r2 and isinstance(H.decode(_r2), bytes) and [t[0] for t in H._parse(_r2)['tuples']] == [0, 1])
cs = content_start(raw)
ok('C7 raw, last byte removed -> rejected', rej(raw[:-1]))
ok('C8 raw + RXT, last content byte removed -> rejected', rej(rawrx[:-6] + rawrx[-5:]))
ok('C8b raw + RXT, mask removed -> rejected', rej(rawrx[:-1]))
ok('C9 PPM, last byte removed -> rejected', rej(ppm[:-1]))
b = bytearray(raw); b[-1] ^= 1; ok('C10 raw, one text bit flipped -> rejected', rej(bytes(b)))
b = bytearray(raw); b[cs + 1] ^= 0xFF; ok('C11 raw, wrong CRC -> rejected', rej(bytes(b)))
ok('C12 old V1 raw -> rejected', rej(raw[:cs] + bytes([0xC1]) + raw[cs + 1:]))
ok('C13 old V1 PPM -> rejected', rej(ppm[:content_start(ppm)] + bytes([0x01]) + ppm[content_start(ppm)+1:]))
# every truncation of raw frames, with and without RXT
n = acc = 0
for fr in (raw, rawrx, H.encode(b'F4MLV-2>APLRG1,WIDE1-1::F4MLV-10 :ack1'), H.encode(b'F4MLV-AB>APLRG1,WIDE1-1:>ok')):
    for k in range(1, len(fr)):
        n += 1; acc += isinstance(H.decode(fr[:-k]), bytes)
ok('every cut of four raw frames (%d cuts) -> 0 accepted' % n, acc == 0, 'accepted: %d' % acc)
bad = 0
for ln in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build', 'hb_packets.txt'), 'rb'):
    ln = ln.rstrip(b'\n')
    if not ln: continue
    fr = H.encode(ln)
    if fr and (fr[content_start(fr)] >> 6) == 3:
        for k in range(1, len(fr)):
            if isinstance(H.decode(fr[:-k]), bytes): bad += 1
ok('every cut of every raw frame in 3000 generated packets -> 0 accepted', bad == 0, 'accepted: %d' % bad)
for n_, c, note in res: print('PASS' if c else 'FAIL', n_, note)
print('%d/%d pass' % (sum(c for _, c, _ in res), len(res)))
