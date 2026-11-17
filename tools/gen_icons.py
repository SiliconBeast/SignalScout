#!/usr/bin/env python3
"""SignalOS icon set: one vector description per glyph, rendered three ways.

  assets/icons/adventure/<name>.png   full-colour app tiles (176 px, for docs / store)
  assets/icons/hackerspace/<name>.png phosphor-on-black tiles (176 px)
  assets/icons/sheet.png              contact sheet of both sets
  firmware/lib/ss_ui/icons_gen.c/.h   A8 alpha masks at the sizes the firmware draws

The firmware draws the tile (gradient, radius, profile colours) with LVGL and
recolours the A8 glyph, so one glyph serves both profiles and any accent.
Everything is drawn at 4x and downsampled for clean anti-aliasing.

Usage: python3 tools/gen_icons.py
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SS = 4  # supersampling

class Pen:
    """Drawing in a 0..100 box, stroke widths in the same units."""
    def __init__(self, n, weight=1.0):
        self.n = n * SS; self.k = self.n / 100.0; self.w = weight
        self.im = Image.new("L", (self.n, self.n), 0); self.d = ImageDraw.Draw(self.im)
    def P(self, x, y): return (x * self.k, y * self.k)
    def dot(self, x, y, r, v=255):
        r *= self.k; cx, cy = self.P(x, y); self.d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=v)
    def line(self, pts, w, v=255, caps=True):
        w2 = w * self.w; pp = [self.P(*p) for p in pts]
        self.d.line(pp, fill=v, width=max(1, int(round(w2 * self.k))), joint="curve")
        if caps:
            for p in pts: self.dot(p[0], p[1], w2 / 2, v)
    def arc(self, cx, cy, r, a0, a1, w, v=255, caps=True):  # degrees, 0 = east, clockwise (screen)
        w2 = w * self.w; r2 = r + w2 / 2
        box = (*self.P(cx - r2, cy - r2), *self.P(cx + r2, cy + r2))
        self.d.arc(box, a0, a1, fill=v, width=max(1, int(round(w2 * self.k))))
        if caps:
            for a in (a0, a1):
                t = math.radians(a); self.dot(cx + r * math.cos(t), cy + r * math.sin(t), w2 / 2, v)
    def ring(self, cx, cy, r, w, v=255): self.arc(cx, cy, r, 0, 360, w, v, caps=False)
    def poly(self, pts, v=255): self.d.polygon([self.P(*p) for p in pts], fill=v)
    def rrect(self, x0, y0, x1, y1, r, w=None, v=255):
        b = (*self.P(x0, y0), *self.P(x1, y1))
        if w is None: self.d.rounded_rectangle(b, radius=r * self.k, fill=v)
        else: self.d.rounded_rectangle(b, radius=r * self.k, outline=v, width=max(1, int(round(w * self.w * self.k))))
    def curve(self, f, t0, t1, w, steps=60, v=255):
        self.line([f(t0 + (t1 - t0) * i / steps) for i in range(steps + 1)], w, v)
    def done(self, n): return self.im.resize((n, n), Image.LANCZOS)

W = 8.5  # default stroke

def cloud(p, ox=0, oy=0, s=1.0, v=255):
    for (x, y, r) in ((40, 58, 17), (60, 50, 20), (76, 62, 13)):
        p.dot(ox + 50 + (x - 50) * s, oy + 50 + (y - 50) * s, r * s, v)
    p.rrect(ox + 50 + (26 - 50) * s, oy + 50 + (58 - 50) * s, ox + 50 + (88 - 50) * s, oy + 50 + (75 - 50) * s, 8.5 * s, v=v)

# ------------------------------------------------------------------ glyphs
def g_wifi(p):
    for i, r in enumerate((15, 33, 51)): p.arc(50, 80, r, 228, 312, W)
    p.dot(50, 80, 7.5)
def g_devices(p):  # a BLE tag radiating
    p.rrect(18, 26, 50, 78, 10, W)
    p.dot(34, 64, 5)
    for r in (16, 30): p.arc(52, 50, r, 305, 55, W)
def g_channels(p):
    for cx, h in ((30, 34), (50, 56), (72, 40)):
        p.curve(lambda t, cx=cx, h=h: (cx + t, 80 - h * math.exp(-(t / 11.0) ** 2)), -26, 26, W * 0.8)
def g_sweep(p):
    pts = [(10, 76), (20, 70), (26, 76), (34, 66), (40, 74), (48, 52), (56, 18), (64, 56), (70, 70), (78, 64), (84, 76), (90, 72)]
    p.line(pts, W * 0.85)
    for y in range(14, 90, 12): p.line([(80, y), (80, y + 5)], W * 0.5)
def g_position(p):
    p.ring(50, 50, 25, W); p.dot(50, 50, 8)
    for a in (0, 90, 180, 270):
        t = math.radians(a); p.line([(50 + 32 * math.cos(t), 50 + 32 * math.sin(t)), (50 + 44 * math.cos(t), 50 + 44 * math.sin(t))], W)
def g_navigate(p):
    p.poly([(16, 50), (84, 16), (50, 84), (44, 56)])
def g_mesh(p):
    nodes = [(50, 20), (20, 74), (80, 74), (50, 54)]
    for a in range(4):
        for b in range(a + 1, 4): p.line([nodes[a], nodes[b]], W * 0.55)
    for (x, y) in nodes[:3]: p.dot(x, y, 10)
    p.dot(50, 54, 7)
def g_weather(p):
    p.dot(38, 36, 15)
    for a in range(0, 360, 45):
        t = math.radians(a); p.line([(38 + 22 * math.cos(t), 36 + 22 * math.sin(t)), (38 + 29 * math.cos(t), 36 + 29 * math.sin(t))], W * 0.7)
    cloud(p, 2, 8, 0.95, 0)  # knock-out gap around the cloud
    cloud(p, 4, 10, 0.80)
def g_console(p):
    p.rrect(10, 18, 90, 82, 12, W * 0.9)
    p.line([(26, 38), (40, 50), (26, 62)], W)
    p.line([(48, 63), (70, 63)], W)
def g_torch(p):
    p.poly([(28, 34), (72, 34), (62, 52), (38, 52)])
    p.rrect(38, 50, 62, 88, 6)
    p.dot(50, 62, 4, 0)
    for a in (-60, -90, -120):
        t = math.radians(a); p.line([(50 + 16 * math.cos(t), 28 + 16 * math.sin(t)), (50 + 26 * math.cos(t), 28 + 26 * math.sin(t))], W * 0.7)
def g_updates(p):
    p.arc(50, 50, 34, -50, 220, W)
    t = math.radians(-50); ex, ey = 50 + 34 * math.cos(t), 50 + 34 * math.sin(t)
    p.poly([(ex - 13, ey - 4), (ex + 9, ey - 12), (ex + 5, ey + 11)])
    p.line([(50, 32), (50, 62)], W); p.line([(38, 52), (50, 64), (62, 52)], W)
def g_settings(p):
    for i, kx in enumerate((64, 34, 54)):
        y = 26 + i * 24; p.line([(16, y), (84, y)], W * 0.8); p.dot(kx, y, 10)
        p.dot(kx, y, 4, 0)
def g_level(p):
    p.rrect(8, 34, 92, 66, 16, W * 0.85)
    for x in (40, 72): p.line([(x, 42), (x, 58)], W * 0.45)
    p.dot(56, 50, 9)
def g_sos(p):
    p.poly([(50, 10), (88, 26), (84, 58), (50, 90), (16, 58), (12, 26)])
    p.poly([(50, 20), (78, 32), (75, 56), (50, 80), (25, 56), (22, 32)], v=0)
    p.line([(50, 32), (50, 58)], W); p.dot(50, 70, 5.5)
def g_mountain(p):  # Adventure profile
    p.poly([(6, 82), (38, 30), (54, 54), (64, 42), (94, 82)])
    p.poly([(32, 40), (38, 30), (45, 41), (40, 46), (36, 42)], v=0)
    p.dot(74, 22, 8)
def g_terminal(p):  # Hackerspace profile
    p.line([(14, 34), (36, 50), (14, 66)], W * 1.1)
    p.line([(46, 70), (86, 70)], W * 1.1)
def g_bell(p):
    p.poly([(26, 70), (30, 40), (40, 26), (50, 22), (60, 26), (70, 40), (74, 70)])
    p.rrect(16, 66, 84, 76, 5); p.dot(50, 84, 7)
def g_vibrate(p):
    p.rrect(32, 14, 68, 86, 9, W * 0.85)
    for x in (16, 84): p.line([(x, 34), (x, 66)], W * 0.8)
def g_speaker(p):
    p.poly([(14, 38), (32, 38), (54, 18), (54, 82), (32, 62), (14, 62)])
    p.arc(56, 50, 16, -50, 50, W * 0.8); p.arc(56, 50, 30, -50, 50, W * 0.8)
def g_sd(p):
    p.poly([(26, 10), (64, 10), (80, 26), (80, 90), (26, 90)])
    for x in (38, 50, 62): p.rrect(x - 4, 18, x + 4, 34, 2, v=0)
def g_sled(p):  # stacked layers = UNO Q sled
    p.poly([(50, 14), (92, 34), (50, 54), (8, 34)])
    p.line([(8, 52), (50, 72), (92, 52)], W * 0.9)
    p.line([(8, 68), (50, 88), (92, 68)], W * 0.9)
def g_lock(p):
    p.arc(50, 40, 20, 180, 360, W * 1.1, caps=False); p.line([(30, 40), (30, 48)], W * 1.1, caps=False); p.line([(70, 40), (70, 48)], W * 1.1, caps=False)
    p.rrect(18, 46, 82, 90, 10); p.dot(50, 64, 6, 0); p.rrect(47, 64, 53, 78, 2, v=0)
def g_brightness(p):
    p.dot(50, 50, 16)
    for a in range(0, 360, 45):
        t = math.radians(a); p.line([(50 + 26 * math.cos(t), 50 + 26 * math.sin(t)), (50 + 36 * math.cos(t), 50 + 36 * math.sin(t))], W * 0.8)
def g_pin(p):
    p.dot(50, 40, 28); p.poly([(26, 54), (74, 54), (50, 92)]); p.dot(50, 40, 11, 0)
def g_web(p):
    p.ring(50, 50, 38, W * 0.75)
    p.arc(50, 50, 38, 0, 360, W * 0.75, caps=False)
    p.d.ellipse((*p.P(32, 12), *p.P(68, 88)), outline=255, width=int(W * 0.75 * p.k))
    p.line([(14, 50), (86, 50)], W * 0.75, caps=False)
def g_chevron(p): p.line([(36, 20), (66, 50), (36, 80)], W * 1.3)
def g_arrow_up(p): p.poly([(50, 8), (86, 80), (50, 64), (14, 80)])
def g_batt(p):
    p.rrect(6, 26, 84, 74, 10, W * 0.8); p.rrect(86, 40, 96, 60, 3)
# weather conditions (WMO groups)
def w_clear(p):
    p.dot(50, 50, 20)
    for a in range(0, 360, 45):
        t = math.radians(a); p.line([(50 + 29 * math.cos(t), 50 + 29 * math.sin(t)), (50 + 40 * math.cos(t), 50 + 40 * math.sin(t))], W * 0.8)
def w_cloud(p): cloud(p, 0, 0, 1.05)
def w_partly(p): g_weather(p)
def w_rain(p):
    cloud(p, 0, -14, 0.95)
    for x in (34, 52, 70): p.line([(x, 70), (x - 6, 88)], W * 0.8)
def w_snow(p):
    cloud(p, 0, -14, 0.95)
    for x, y in ((34, 76), (52, 86), (70, 76)): p.dot(x, y, 5.5)
def w_storm(p):
    cloud(p, 0, -16, 0.95)
    p.poly([(54, 58), (38, 80), (50, 80), (44, 98), (64, 72), (52, 72), (60, 58)])
def w_fog(p):
    for i, (a, b) in enumerate(((14, 80), (24, 90), (10, 72), (28, 86))): p.line([(a, 28 + i * 15), (b, 28 + i * 15)], W * 0.9)

GLYPHS = {n[2:]: f for n, f in globals().items() if n.startswith("g_")}
WEATHER = {n[2:]: f for n, f in globals().items() if n.startswith("w_")}

# App tiles: glyph, two-stop gradient (top, bottom), glyph colour.
APPS = [
    ("wifi", "Wi-Fi", 0x2F7BFF, 0x00C2FF, 0xFFFFFF), ("devices", "Bluetooth", 0x5B4BFF, 0x9A7BFF, 0xFFFFFF),
    ("channels", "Channels", 0x0077E6, 0x2FE0C8, 0xFFFFFF), ("sweep", "Sweep", 0x7A2CFF, 0xD45CFF, 0xFFFFFF),
    ("position", "Position", 0x0F9F5A, 0x5CE08A, 0xFFFFFF), ("navigate", "Navigate", 0x00927C, 0x3DE0B5, 0xFFFFFF),
    ("mesh", "Mesh", 0x4C9A12, 0xA3E635, 0xFFFFFF), ("weather", "Weather", 0xFF8A1C, 0xFFD166, 0xFFFFFF),
    ("console", "Console", 0x1C1F26, 0x3A3F4B, 0x39FF88), ("torch", "Torch", 0xFFB020, 0xFFE59A, 0x2A1A00),
    ("level", "Level", 0xFF4A2E, 0xFF9A3C, 0xFFFFFF), ("updates", "Updates", 0xFF3D7F, 0xFF8A5C, 0xFFFFFF),
    ("settings", "Settings", 0x4A505B, 0x8C939E, 0xFFFFFF), ("sos", "SOS", 0xE5251A, 0xFF5A4E, 0xFFFFFF),
]
HACK_BG, HACK_EDGE, HACK_FG = 0x0A0F0C, 0x1D3A28, 0x39FF88

# Sizes the firmware draws: (suffix, px, stroke weight, names)
FW = [
    ("52", 52, 1.0, [a[0] for a in APPS] + ["mountain", "terminal"]),
    ("30", 30, 1.15, ["wifi", "devices", "mesh", "position", "sd", "vibrate", "speaker", "torch", "brightness", "mountain", "terminal", "web", "bell", "lock", "level", "sweep", "pin", "updates", "sos", "sled"]),
    ("20", 20, 1.35, ["wifi", "devices", "mesh", "position", "sd", "sled", "bell", "updates", "web", "sos", "chevron", "lock", "pin", "arrow_up", "navigate"]),
]
FW_WEATHER = [("40", 40, 1.0), ("96", 96, 1.0)]

def render(fn, px, weight=1.0):
    p = Pen(px, weight); fn(p); return p.done(px)

def hexrgb(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255)

def tile(glyph, top, bot, fg, n=176, hack=False):
    S = n * SS; im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    grad = Image.new("RGBA", (1, S))
    for y in range(S):
        t = y / (S - 1); c0, c1 = (hexrgb(HACK_BG), hexrgb(HACK_BG)) if hack else (hexrgb(top), hexrgb(bot))
        grad.putpixel((0, y), tuple(int(c0[i] + (c1[i] - c0[i]) * t) for i in range(3)) + (255,))
    grad = grad.resize((S, S))
    mask = Image.new("L", (S, S), 0); r = int(S * 0.30)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), radius=r, fill=255)
    im.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(im)
    if hack: d.rounded_rectangle((SS, SS, S - 1 - SS, S - 1 - SS), radius=r, outline=hexrgb(HACK_EDGE) + (255,), width=3 * SS)
    else:  # soft top sheen
        sheen = Image.new("L", (S, S), 0); ImageDraw.Draw(sheen).ellipse((-S * 0.3, -S * 0.9, S * 1.3, S * 0.42), fill=34)
        sheen = Image.composite(sheen, Image.new("L", (S, S), 0), mask)
        im = Image.composite(Image.new("RGBA", (S, S), (255, 255, 255, 255)), im, sheen).copy(); im.putalpha(mask)
    g = int(S * 0.59); gm = render(glyph, g // SS * 1, 1.0).resize((g, g), Image.LANCZOS)
    col = Image.new("RGBA", (g, g), hexrgb(HACK_FG if hack else fg) + (255,))
    if hack:  # phosphor glow
        glow = gm.filter(ImageFilter.GaussianBlur(S * 0.02)).point(lambda v: v * 0.55)
        im.paste(Image.new("RGBA", (g, g), hexrgb(HACK_FG) + (255,)), ((S - g) // 2, (S - g) // 2), glow)
    im.paste(col, ((S - g) // 2, (S - g) // 2), gm)
    return im.resize((n, n), Image.LANCZOS)

def main():
    out_a = os.path.join(ROOT, "assets/icons/adventure"); out_h = os.path.join(ROOT, "assets/icons/hackerspace")
    os.makedirs(out_a, exist_ok=True); os.makedirs(out_h, exist_ok=True)
    tiles = []
    for name, label, top, bot, fg in APPS:
        a = tile(GLYPHS[name], top, bot, fg); h = tile(GLYPHS[name], top, bot, fg, hack=True)
        a.save(os.path.join(out_a, name + ".png")); h.save(os.path.join(out_h, name + ".png")); tiles.append((a, h, label))
    # contact sheet
    cols, cell = 7, 200; rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell + 40, rows * 2 * cell + 60), (0, 0, 0, 255))
    for i, (a, h, _) in enumerate(tiles):
        x, y = 20 + (i % cols) * cell + 12, 20 + (i // cols) * cell
        sheet.paste(a, (x, y), a); sheet.paste(h, (x, y + rows * cell + 20), h)
    sheet.save(os.path.join(ROOT, "assets/icons/sheet.png"))
    # firmware masks
    lines_c = ["// Generated by tools/gen_icons.py. Do not edit.", "#include \"icons_gen.h\"", ""]
    lines_h = ["// Generated by tools/gen_icons.py. Do not edit.", "#pragma once", "#include <lvgl.h>", "#ifdef __cplusplus", "extern \"C\" {", "#endif"]
    def emit(sym, im):
        w, h = im.size; data = im.tobytes()
        lines_c.append("static const uint8_t %s_px[%d] = {" % (sym, len(data)))
        for i in range(0, len(data), 24): lines_c.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
        lines_c.append("};")
        lines_c.append("const lv_image_dsc_t %s = { .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8, .flags = 0, .w = %d, .h = %d, .stride = %d, .reserved_2 = 0 }, .data_size = %d, .data = %s_px, .reserved = NULL };" % (sym, w, h, w, len(data), sym))
        lines_h.append("extern const lv_image_dsc_t %s;" % sym)
    for suf, px, wt, names in FW:
        for n in names: emit("ic%s_%s" % (suf, n), render(GLYPHS[n], px, wt))
    for suf, px, wt in FW_WEATHER:
        for n in WEATHER: emit("wx%s_%s" % (suf, n), render(WEATHER[n], px, wt))
    lines_h += ["#ifdef __cplusplus", "}", "#endif"]
    d = os.path.join(ROOT, "firmware/lib/ss_ui")
    open(os.path.join(d, "icons_gen.c"), "w").write("\n".join(lines_c) + "\n")
    open(os.path.join(d, "icons_gen.h"), "w").write("\n".join(lines_h) + "\n")
    print("icons:", len(APPS), "tiles x2,", sum(len(f[3]) for f in FW) + len(WEATHER) * len(FW_WEATHER), "firmware masks")

if __name__ == "__main__":
    main()
