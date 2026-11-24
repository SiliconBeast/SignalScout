#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Copy the shared protocol source into the sled sketch (Arduino sketches cannot
include files from outside their folder). `--check` fails if the copies differ."""
import filecmp, os, shutil, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
pairs = [(f"firmware/lib/ss_core/{n}", f"sled/signalscout_sled/sketch/{n}") for n in ("proto.h", "proto.cpp")]
bad = 0
for src, dst in pairs:
    s, d = os.path.join(root, src), os.path.join(root, dst)
    if "--check" in sys.argv:
        if not os.path.exists(d) or not filecmp.cmp(s, d, shallow=False):
            print(f"out of sync: {dst} (run tools/sync_shared.py)"); bad = 1
    else:
        shutil.copyfile(s, d); print(f"copied {src} -> {dst}")
sys.exit(bad)
