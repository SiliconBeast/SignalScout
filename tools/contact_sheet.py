#!/usr/bin/env python3
"""Tile docs/screenshots/*.png into contact sheets (docs/screenshots/sheet_N.png)."""
import glob, os, sys
from PIL import Image
d = sys.argv[1] if len(sys.argv) > 1 else "docs/screenshots"
per = int(sys.argv[2]) if len(sys.argv) > 2 else 5
files = sorted(f for f in glob.glob(os.path.join(d, "[0-9]*.png")))
for n in range(0, len(files), per):
    ims = [Image.open(f).convert("RGB") for f in files[n:n + per]]
    w, h = ims[0].size; gap = 14
    sheet = Image.new("RGB", (len(ims) * (w + gap) + gap, h + 2 * gap), (38, 40, 46))
    for i, im in enumerate(ims):
        sheet.paste(im, (gap + i * (w + gap), gap))
    sheet.save(os.path.join(d, f"sheet_{n // per + 1}.png"))
