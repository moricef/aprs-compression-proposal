import os
import unittest

import hb

TEST = os.path.join(hb.ROOT, 'test')


def read_vectors(name):
    rows = []
    with open(os.path.join(TEST, name), 'rb') as fh:
        data = fh.read()
    for ln in data.split(b'\n'):
        if ln and not ln.startswith(b'#'):
            rows.append([bytes.fromhex(x.decode()) if x != b'-' else None for x in ln.split()])
    return rows


VEC_ALL = read_vectors('vectors_hb_v1.txt')           # includes the packets that are not encodable (frame '-')
VEC = [r for r in VEC_ALL if r[0] is not None]
RELAY = read_vectors('vectors_hb_relay_v1.txt')
CHAIN = read_vectors('vectors_hb_relay_chain_v1.txt')
EDGE = read_vectors('vectors_hb_relay_edge_v1.txt')    # relay edge cases; a relayed frame of '-' means the relay must not relay it
REJECT = []
with open(os.path.join(TEST, 'vectors_hb_reject_v1.txt'), 'rb') as _fh:
    for _ln in _fh.read().split(b'\n'):
        if _ln and not _ln.startswith(b'#'):
            REJECT.append(bytes.fromhex(_ln.split()[0].decode()))


def ppm_frames():
    out = []
    for fr, pkt in VEC:
        if hb._parse(fr)['kind'] >= 2 and fr[hb._parse(fr)['cstart']] >> 6 == 0:
            out.append((fr, pkt))
    return out


def accepted(r):
    return isinstance(r, bytes)


class Vectors(unittest.TestCase):
    def test_counts(self):
        self.assertEqual(len(VEC_ALL), 554)
        self.assertEqual(len(VEC), 537)
        self.assertEqual(len(RELAY), 298)

    def test_encode(self):
        bad = [i for i, (fr, pkt) in enumerate(VEC_ALL) if hb.encode(pkt) != fr]
        self.assertEqual(bad, [])

    def test_decode(self):
        bad = [i for i, (fr, pkt) in enumerate(VEC) if hb.decode(fr) != pkt]
        self.assertEqual(bad, [])

    def test_relay(self):
        for i, (rx, tx, pkt) in enumerate(RELAY):
            self.assertEqual(hb.relay_auto(rx), tx, i)
            self.assertEqual(hb.decode(tx), pkt, i)

    def test_relay_refuses_invalid_frames(self):
        for fr in REJECT:
            self.assertIsNone(hb.relay_auto(fr))

    def test_reject_vectors(self):
        self.assertGreaterEqual(len(REJECT), 10)
        for i, fr in enumerate(REJECT):
            self.assertFalse(accepted(hb.decode(fr)), i)

    def test_relay_edge_cases(self):
        self.assertGreaterEqual(len(EDGE), 6)
        for i, (rx, tx, pkt) in enumerate(EDGE):
            self.assertEqual(hb.relay_auto(rx), tx, i)
            if tx is not None:
                self.assertEqual(hb.decode(tx), pkt, i)

    def test_relay_copies_source_and_destination_bytes(self):
        rx, tx, _ = EDGE[0]                                       # ff bits set on the destination byte
        d = hb._parse(rx)
        self.assertEqual(tx[4:d['path_start']], rx[4:d['path_start']])

    def test_colon_in_source_is_not_encodable(self):
        self.assertIsNone(hb.encode(b'A:B>APLRG1:hello world this is a test'))

    def test_relay_chain(self):
        self.assertGreater(len(CHAIN), 100)
        for i, (rx, tx, pkt) in enumerate(CHAIN):
            self.assertEqual(hb.relay_auto(rx), tx, i)
            self.assertEqual(hb.decode(tx), pkt, i)

    def test_packets_file_matches(self):
        with open(os.path.join(TEST, 'hb_packets.txt'), 'rb') as fh:
            pk = fh.read().split(b'\n')
        pk = [p for p in pk if p]
        self.assertEqual(pk, [p for _, p in VEC_ALL])

    def test_crc(self):
        self.assertEqual(hb.crc16_x25(b'123456789'), 0x906E)

    def test_determinism(self):
        for fr, pkt in VEC[:100]:
            self.assertEqual(hb.encode(pkt), hb.encode(pkt))


class Integrity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ppm = ppm_frames()

    def test_have_ppm_frames(self):
        self.assertGreater(len(self.ppm), 100)

    def test_cuts_dropped(self):
        for fr, _ in self.ppm:
            for n in range(1, len(fr)):
                self.assertFalse(accepted(hb.decode(fr[:len(fr) - n])), (fr.hex(), n))

    def test_appended_dropped(self):
        tails = [b'\x00', b'\xff', b'\x00\x00', b'abc', b'\x80', b'\x81', b'\x7f\x7f\x7f\x7f\x7f\x7f']
        for fr, _ in self.ppm:
            for t in tails:
                self.assertFalse(accepted(hb.decode(fr + t)), (fr.hex(), t))

    def test_content_coding_flip(self):
        for fr, pkt in self.ppm:
            i = hb._parse(fr)['cstart']
            for bit in range(8):
                g = bytearray(fr)
                g[i] ^= 1 << bit
                self.assertFalse(accepted(hb.decode(bytes(g))), (fr.hex(), bit))

    def test_raw_vectors_exist_and_decode(self):
        raw = [(f, p) for f, p in VEC if hb._parse(f)['kind'] >= 2 and f[hb._parse(f)['cstart']] >> 6 == 3]
        self.assertGreater(len(raw), 10)


class Rxt(unittest.TestCase):
    PKT = b'N0CALL>APLRG1,N0RLY-3*,N0RLY-5*,WIDE1-1:>Fixed station, solar powered, 433.775 MHz LoRa APRS iGate on a hill'

    @classmethod
    def setUpClass(cls):
        cls.base = hb.encode(cls.PKT)
        assert cls.base is not None and cls.base[hb._parse(cls.base)['cstart']] >> 6 == 0, 'need a PPM frame'

    def with_block(self, tuples, mask):
        f = bytearray(self.base)
        f[3] |= 8
        return bytes(f) + b''.join(bytes(t) for t in tuples) + bytes([mask])

    def test_one_tuple(self):
        f = self.with_block([(1, 2, 3, 4)], 0x82)
        self.assertEqual(hb.decode(f), self.PKT)

    def test_three_tuples(self):
        f = self.with_block([(1, 2, 3, 4), (89, 88, 89, 89), (0, 0, 0, 0)], 0x87)
        self.assertEqual(hb.decode(f), self.PKT)

    def test_cut_inside_block(self):
        f = self.with_block([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12)], 0x87)
        for n in range(1, 17):
            self.assertFalse(accepted(hb.decode(f[:-n])), n)
        for n in range(1, 13):
            g = self.with_block([(1, 2, 3, 4)], 0x81)[:-n]
            self.assertFalse(accepted(hb.decode(g)), n)

    def test_tuple_byte_above_max(self):
        for fld, mx in enumerate((89, 88, 89, 89)):
            t = [1, 2, 3, 4]
            t[fld] = mx
            self.assertEqual(hb.decode(self.with_block([t], 0x81)), self.PKT)
            t[fld] = mx + 1
            self.assertFalse(accepted(hb.decode(self.with_block([t], 0x81))), fld)

    def test_mask_without_marker(self):
        self.assertFalse(accepted(hb.decode(self.with_block([(1, 2, 3, 4)], 0x01))))
        self.assertFalse(accepted(hb.decode(self.with_block([(1, 2, 3, 4)], 0x02))))

    def test_mask_bad(self):
        self.assertFalse(accepted(hb.decode(self.with_block([], 0x80))))                    # k = 0
        self.assertFalse(accepted(hb.decode(self.with_block([(1, 2, 3, 4)], 0x88))))        # bit 3 >= 3 path elements
        self.assertFalse(accepted(hb.decode(self.with_block([(1, 2, 3, 4)], 0x83))))        # k=2 but one tuple present

    def test_relay_appends(self):
        pkt = b'N0CALL>APLRG1,WIDE1-1,WIDE2-1:>Fixed station, solar powered, 433.775 MHz LoRa APRS iGate on a hill'
        fr = hb.encode(pkt)
        out = hb.relay(fr, 'N0RLY-1', bytes([50, 20, 45, 10]))
        self.assertEqual(out[-5:], bytes([50, 20, 45, 10, 0x81]))
        self.assertEqual(hb.decode(out), b'N0CALL>APLRG1,N0RLY-1*,WIDE2-1:>Fixed station, solar powered, 433.775 MHz LoRa APRS iGate on a hill')
        out2 = hb.relay(out, 'N0RLY-2', bytes([1, 2, 3, 4]))      # the next unused element is WIDE2-1: relayed, second tuple
        self.assertEqual(out2[-9:], bytes([50, 20, 45, 10, 1, 2, 3, 4, 0x83]))
        self.assertIsNone(hb.relay(out2, 'N0RLY-3', bytes([5, 6, 7, 8])))   # nothing unused left


class SpecExample(unittest.TestCase):
    def test_worked_example_in_spec(self):
        import re
        with open(os.path.join(TEST, '..', 'spec', '11-native-ppm-frame-byte-plan.md'), encoding='utf-8') as fh:
            t = fh.read()
        t = t[t.index('## Worked example'):t.index('## Size')]
        pkt = re.search(r'`(N0CALL-9>[^`]+)`', t).group(1).encode()
        hops = [bytes.fromhex(h) for h in re.findall(r'`(3CFF02[0-9A-F]{60,})`', t)]
        self.assertEqual(len(hops), 2)
        f0 = hb.encode(pkt)
        self.assertEqual(len(f0), 35)
        f1 = hb.relay_auto(f0)
        f2 = hb.relay_auto(f1)
        self.assertEqual([f1, f2], hops)
        self.assertEqual(hb.decode(f2), b'N0CALL-9>APLRG1,N0RLY-1*,N0RLY-2*,WIDE2-1:' + pkt.split(b':', 1)[1])


class Edge(unittest.TestCase):
    def test_non_compliant_wide_paths_are_dropped(self):
        # n must be 1 to 7 and N 1 to n (spec 11, "Non-compliant WIDE paths"): such a packet is not encoded, so it cannot be repeated on RF
        for t in (b'WIDE1-2', b'WIDE2-3', b'WIDE1-2*', b'WIDE8-1', b'WIDE1-0', b'WIDE01-1', b'WIDE10-1', b'WIDE0-1'):
            pkt = b'N0CALL>APLRG1,' + t + b':>x'
            self.assertIsNone(hb.encode(pkt), pkt)
            self.assertFalse(hb.path_ok(pkt), pkt)
        for t in (b'WIDE1-1', b'WIDE2', b'WIDE3-3*', b'WIDE7-7', b'WIDER', b'WIDE'):
            pkt = b'N0CALL>APLRG1,' + t + b':>x'
            self.assertIsNotNone(hb.encode(pkt), pkt)
            self.assertTrue(hb.path_ok(pkt), pkt)

    def test_not_encodable(self):
        self.assertIsNone(hb.encode(b'nocolon'))
        self.assertIsNone(hb.encode(b'A>B:' + b'x' * 256))

    def test_roundtrips(self):
        for p in (b'N0CALL>APRS:', b'N0CALL>APRS::ALL      :hi', b'N0CALL-0>APRS,WIDE2-2:x',
                  b'LONGNAME-255>APRS,TCPIP*:!4903.50N/07201.75W-'):
            fr = hb.encode(p)
            self.assertIsNotNone(fr, p)
            self.assertEqual(hb.decode(fr), p)


if __name__ == '__main__':
    unittest.main()
