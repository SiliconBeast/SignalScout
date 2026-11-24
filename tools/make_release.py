#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write the OTA manifest for a release.

  python3 tools/make_release.py firmware/.pio/build/device/firmware.bin out/ [--notes "..."] [--min 1.0.0]

Copies firmware.bin into out/ and writes out/manifest.json with the version
from firmware/include/board.h, the size and the SHA-256. Upload both files
as assets of the GitHub release named v<version>; the device reads
releases/latest/download/manifest.json.
"""
import argparse, hashlib, json, os, re, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "SiliconBeast/SignalScout"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bin"); ap.add_argument("out")
    ap.add_argument("--notes", default=""); ap.add_argument("--min", default="1.0.0"); ap.add_argument("--channel", default="stable")
    a = ap.parse_args()
    board = open(os.path.join(ROOT, "firmware/include/board.h")).read()
    version = re.search(r'#define SS_FW_VERSION "([^"]+)"', board).group(1)
    data = open(a.bin, "rb").read()
    if data[:1] != b"\xE9":
        raise SystemExit("not an ESP32 app image (missing 0xE9 magic)")
    os.makedirs(a.out, exist_ok=True)
    shutil.copyfile(a.bin, os.path.join(a.out, "firmware.bin"))
    m = {
        "version": version, "channel": a.channel,
        "url": f"https://github.com/{REPO}/releases/download/v{version}/firmware.bin",
        "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "min_version": a.min, "notes": a.notes[:95],
    }
    with open(os.path.join(a.out, "manifest.json"), "w") as f:
        json.dump(m, f, indent=1)
    print(json.dumps(m, indent=1))

if __name__ == "__main__":
    main()
