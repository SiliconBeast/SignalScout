"""Sled Python checks: protocol and correlation agree with the shared vectors
(the C++ side is tested against the same files)."""
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "sled", "signalscout_sled", "python"))
import correlate, scanners, ssproto


def rows(name):
    with open(os.path.join(ROOT, "tests", "vectors", name)) as f:
        return [l.strip().split(",") for l in f if l.strip() and not l.startswith("#")]


class Proto(unittest.TestCase):
    def test_crc_check_value(self):
        self.assertEqual(ssproto.crc16(b"123456789"), 0x29B1)

    def test_vectors(self):
        for t, s, payload, frame in rows("proto.csv"):
            self.assertEqual(ssproto.encode(int(t), int(s), bytes.fromhex(payload)).hex(), frame)

    def test_parser_resync_and_split(self):
        good = ssproto.encode(ssproto.T_RF_SAMPLE, 3, ssproto.RF_SAMPLE.pack(1000, -5000, -5100, -4900, -6400))
        bad = bytearray(good); bad[-1] ^= 0xFF
        p = ssproto.Parser()
        stream = b"\x00\xA5\x11" + bytes(bad) + good
        got = []
        for i in range(0, len(stream), 5):  # arbitrary chunking
            got += list(p.feed(stream[i:i + 5]))
        self.assertEqual(len(got), 1)
        self.assertEqual(got[0][0], ssproto.T_RF_SAMPLE)
        self.assertEqual(ssproto.RF_SAMPLE.unpack(got[0][2])[1], -5000)
        self.assertEqual(p.crc_errors, 1)


class Correlate(unittest.TestCase):
    def test_vectors(self):
        n = 0
        for m, fl, known, st, ex, exp, conf in rows("correlate.csv"):
            r = correlate.classify(float(m), float(fl), float(known))
            self.assertEqual(r.state, int(st)); self.assertAlmostEqual(r.excess_db, float(ex), places=3)
            self.assertAlmostEqual(r.expected_dbm, float(exp), places=3); self.assertEqual(r.confidence, int(conf)); n += 1
        self.assertEqual(n, 90)

    def test_floor_tracker(self):
        t = correlate.FloorTracker(0.5)
        self.assertEqual(t.update(-60, 0.05), -60); self.assertEqual(t.update(-66, 0.05), -66)
        self.assertAlmostEqual(t.update(-30, 1.0), -65.5)


class Scanners(unittest.TestCase):
    def test_nmcli(self):
        got = scanners.parse_nmcli("HomeNet:80\nCafe\\:Guest:44\n:30\ngarbage\n")
        self.assertEqual(got, [("HomeNet", -60.0), ("Cafe:Guest", -78.0), ("", -85.0)])

    def test_iw(self):
        text = "BSS aa:bb(on wlan0)\n\tsignal: -52.00 dBm\n\tSSID: One\nBSS cc:dd(on wlan0)\n\tsignal: -71.00 dBm\n\tSSID: \n"
        self.assertEqual(scanners.parse_iw(text), [("One", -52.0), ("", -71.0)])


if __name__ == "__main__":
    unittest.main()
