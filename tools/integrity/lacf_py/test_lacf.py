import os
import random
import unittest

import lacf

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_DIR = os.path.join(HERE, "..", "..", "..", "test")


def vectors():
    out = []
    with open(os.path.join(TEST_DIR, "vectors_lacf_v1.txt")) as fh:
        lines = fh.readlines()
    for line in lines:
        line = line.strip()
        if line and not line.startswith("#"):
            f, t = line.split()
            out.append((None if f == "-" else bytes.fromhex(f), bytes.fromhex(t)))
    return out


def packets():
    with open(os.path.join(TEST_DIR, "synthetic_packets.txt")) as fh:
        lines = fh.readlines()
    return [l.rstrip("\n").encode() for l in lines if l.strip() and not l.startswith("#")]


def frames():
    return [f for f, _ in vectors() if f is not None]


class T(unittest.TestCase):
    def test_crc_check_value(self):
        self.assertEqual(lacf.crc16_x25(b"123456789"), 0x906E)

    def test_vectors(self):
        for f, t in vectors():
            self.assertEqual(lacf.encode(t), f)
            if f is not None:
                r = lacf.receive(f)
                self.assertIsNotNone(r)
                self.assertEqual(r[0], t)
                self.assertEqual(r[1], len(f) - 6)

    def test_determinism(self):
        for p in packets():
            self.assertEqual(lacf.encode(p), lacf.encode(p))

    def test_random_roundtrip(self):
        rnd = random.Random(5)
        for _ in range(100):
            p = bytes(rnd.choice(b"ABCDEFGH>,:!/0123456789 .") for _ in range(rnd.randint(1, 120)))
            f = lacf.encode(p)
            if f is not None:
                self.assertEqual(lacf.receive(f), (p, len(f) - 6))

    def test_tail_ignored(self):
        rnd = random.Random(1)
        for f in frames():
            t, n = lacf.receive(f)
            for k in range(0, 9):
                g = f + bytes(rnd.randrange(256) for _ in range(k))
                self.assertEqual(lacf.receive(g, allow_tail=True), (t, n))
                if k:
                    self.assertIsNone(lacf.receive(g, allow_tail=False))

    def test_cut_dropped(self):
        rnd = random.Random(2)
        for f in frames():
            for c in range(1, 17):
                self.assertIsNone(lacf.receive(f[:-c], allow_tail=True), (f.hex(), c))
                self.assertIsNone(lacf.receive(f[:-c]), (f.hex(), c))
                n = len(f) - 6
                if c <= n:   # cut into the payload, then a 6-byte tail
                    g = f[:-c] + bytes(rnd.randrange(256) for _ in range(6))
                    self.assertIsNone(lacf.receive(g, allow_tail=True), (f.hex(), c))
                # tail appended first, then cut c bytes off the end
                g = (f + bytes(rnd.randrange(256) for _ in range(6)))[:-c]
                if c > 6:
                    self.assertIsNone(lacf.receive(g, allow_tail=True), (f.hex(), c))

    def test_bitflips(self):
        rnd = random.Random(3)
        fr = frames()
        acc = trials = 0
        for _ in range(3000):
            f = bytearray(rnd.choice(fr))
            if len(f) == 6:
                continue
            i = rnd.randrange(6, len(f))
            f[i] ^= 1 << rnd.randrange(8)
            trials += 1
            if lacf.receive(bytes(f)) is not None:
                acc += 1
        print("\nbit flips in payload: accepted %d of %d" % (acc, trials))


if __name__ == "__main__":
    unittest.main()
