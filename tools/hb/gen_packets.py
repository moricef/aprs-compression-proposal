#!/usr/bin/env python3
"""Fabricated TNC2 packets for the spec 11 tests (made-up callsigns only; deterministic).   python3 tools/hb/gen_packets.py [N] > packets.txt
Covers: uncompressed and compressed positions (course/speed, altitude, timestamps), status, message, telemetry, object, Mic-E-like binary, odd sources and SSIDs,
destinations inside and outside destination list set 1, paths with WIDE1-1, WIDE2-2, used relays, none; comments of every length."""
import random, sys
random.seed(110031)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
LIST = [l.split()[1] for l in open('tools/hybrid/tables/tocall_set1.txt') if l[0].isdigit()]
OUT = ['APXYZ1', 'APZZZ9', 'TEST', 'APOTHR', 'N0CALL', 'BEACON']
SRC = ['N0CALL', 'TEST-1', 'K0TEST-7', 'N0DEMO-15', 'XX0YY-44', 'AB1CD-30', 'AB1CD-31', 'W0XYZ-9', 'Q1', 'Z9ZZ9Z-12', 'nocall-1', 'N0CALL-0', 'LONGNAME-1']
PATHS = ['', ',WIDE1-1', ',WIDE1-1,WIDE2-1', ',WIDE2-2', ',WIDE1-1,WIDE2-2', ',N0RLY-3*,WIDE2-1', ',N0RLY-3*', ',WIDE3-3', ',N0RLY-3*,N0RLY-5*,WIDE1-1', ',TCPIP*']
COM = ['', 'LoRa Tracker 5.2V', 'iGate hill', 'balloon', 'T-Deck', 'Bat 3.9V 21C', 'QRV 433.775 SF12', '73 de N0CALL', 'Fixed station, solar powered, 433.775 MHz LoRa APRS iGate on a hill',
       'wx: 12C 45% 1013hPa', 'PHG2360/test', 'Hello from the field, this is a longer comment that goes on for a while to test the longer texts of the model']
def lat_s():
    d = random.randrange(0, 89); m = random.randrange(0, 59); h = random.randrange(0, 99); return '%02d%02d.%02d%s' % (d, m, h, random.choice('NS'))
def lon_s():
    d = random.randrange(0, 179); m = random.randrange(0, 59); h = random.randrange(0, 99); return '%03d%02d.%02d%s' % (d, m, h, random.choice('EW'))
def b91(v):
    s = ''
    for _ in range(4): s = chr(33 + v % 91) + s; v //= 91
    return s
def com():
    c = random.choice(COM)
    if random.random() < 0.25:
        a = random.randrange(-999, 99999)
        alt = '/A=%06d' % a if a >= 0 else '/A=-%05d' % -a
        k = random.randrange(0, len(c) + 1); c = c[:k] + alt + c[k:]
    return c
def info():
    r = random.random()
    if r < 0.30:                                    # uncompressed position, optional timestamp and course/speed
        dti = random.choice('!=/@'); ts = ''
        if dti in '/@': ts = '%06d%s' % (random.randrange(0, 999999), random.choice('zh/'))
        cs = '%03d/%03d' % (random.randrange(0, 360), random.randrange(0, 120)) if random.random() < 0.25 else ''
        return dti + ts + lat_s() + random.choice('/\\') + lon_s() + random.choice('>-_k[') + cs + com()
    if r < 0.60:                                    # compressed position
        dti = random.choice('!=/@'); ts = ''
        if dti in '/@': ts = '%06d%s' % (random.randrange(0, 999999), random.choice('zh/'))
        la, lo = random.randrange(0, 68000000), random.randrange(0, 68000000)
        cs = (chr(33 + random.randrange(0, 90)) + chr(33 + random.randrange(0, 90)) + chr(random.choice([0x20 + 0x10, 0x20 + 0x30, 0x20 + 0x38])) if random.random() < 0.3 else '  ' + chr(0x20 + random.choice([0x10, 0x20, 0x30])))
        return dti + ts + random.choice('/\\') + b91(la) + b91(lo) + random.choice('>-_k[') + cs + com()
    if r < 0.70: return '>' + random.choice(COM + ['In service', 'Off duty'])
    if r < 0.78: return ':%-9s:%s{%d' % (random.choice(['N0CALL-9', 'TEST-1', 'ALL']), random.choice(['hello', 'QSL?', 'meet at 3']), random.randrange(1, 99))
    if r < 0.84: return 'T#%03d,%03d,%03d,%03d,%03d,%03d,00000000' % tuple(random.randrange(0, 999) for _ in range(6))
    if r < 0.90: return ';%-9s*111111z%s%s' % ('OBJ%d' % random.randrange(1, 99), lat_s(), '/' + lon_s() + '>')
    if r < 0.95: return "`" + ''.join(chr(random.randrange(28, 127)) for _ in range(9)) + random.choice(['', '>', ']']) + random.choice(COM)
    return random.choice(['=', '@', '!', '/']) + random.choice(COM)       # position-looking but malformed
for i in range(N):
    dest = random.choice(LIST) if random.random() < 0.85 else random.choice(OUT)
    src = random.choice(SRC)
    print('%s>%s%s:%s' % (src, dest, random.choice(PATHS), info()))
