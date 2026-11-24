#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Regenerate tests/vectors/*.csv from the Python reference implementations."""
import os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, "sled", "signalscout_sled", "python"))
import correlate, ssproto

rows = ["# measured_dbm,floor_dbm,known_sum_dbm,state,excess_db,expected_dbm,confidence"]
for m in (-70, -63, -61.5, -58, -52, -47.3, -40, -31, -20):
    for fl in (-64, -60):
        for known in (-200, -75, -55.2, -48, -35):
            r = correlate.classify(m, fl, known)
            rows.append(f"{m},{fl},{known},{r.state},{r.excess_db:.4f},{r.expected_dbm:.4f},{r.confidence}")
open(os.path.join(root, "tests/vectors/correlate.csv"), "w").write("\n".join(rows) + "\n")

frames = ["# type,seq,payload_hex,frame_hex"]
for t, s, p in [(1, 0, bytes([2, 1, 0, 1])), (2, 1, b""), (0x10, 7, ssproto.RF_SAMPLE.pack(123456, -5234, -5500, -4900, -6400)),
                (0x11, 255, ssproto.VERDICT.pack(2, 87, 1820, -5310, 9, 7)), (0x20, 9, ssproto.SCAN_SUMMARY.pack(9, 7, -4800, -5200, -4610))]:
    frames.append(f"{t},{s},{p.hex()},{ssproto.encode(t, s, p).hex()}")
open(os.path.join(root, "tests/vectors/proto.csv"), "w").write("\n".join(frames) + "\n")
print("vectors written")
