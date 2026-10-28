#!/usr/bin/env python3
"""Favicon set from the mode_manual gauge: same geometry, redrawn at high resolution
(white gauge on the UI's navy, rounded square), then downsampled to each size."""
# Needs Pillow (pip install pillow). Run from anywhere: python3 assets/scripts/make_favicon.py
# Afterwards copy favicon.ico, favicon-32x32.png and apple-touch-icon.png into web/src/assets/.
import math
import os
import tempfile
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "favicon")
os.makedirs(OUT, exist_ok=True)
BIG = 1024
NAVY = (10, 17, 40, 255)      # #0A1128, the Main container background
WHITE = (255, 255, 255, 255)

def render():
    im = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, BIG - 1, BIG - 1], radius=int(BIG * 0.22), fill=NAVY)

    # mode_manual geometry lives on a 50 unit grid; draw it centred at ~72% of the tile
    u = BIG * 0.72 / 50
    ox = oy = (BIG - 50 * u) / 2
    P = lambda v: v * u
    c = ox + P(25)
    cy = oy + P(25)
    r = P(19)
    d.arc([c - r, cy - r, c + r, cy + r], start=135, end=405, fill=WHITE, width=int(P(4)))
    for ang in (135, 405):
        a = math.radians(ang)
        x, y = c + r * math.cos(a), cy + r * math.sin(a)
        t = P(2)
        d.ellipse([x - t, y - t, x + t, y + t], fill=WHITE)
    a = math.radians(-50)
    ex, ey = c + P(13) * math.cos(a), cy + P(13) * math.sin(a)
    d.line([(c, cy), (ex, ey)], fill=WHITE, width=int(P(4)))
    t = P(2)
    d.ellipse([ex - t, ey - t, ex + t, ey + t], fill=WHITE)
    t = P(4)
    d.ellipse([c - t, cy - t, c + t, cy + t], fill=WHITE)
    return im

big = render()
def size(n): return big.resize((n, n), Image.LANCZOS)

size(16).save(f"{OUT}/favicon-16x16.png")
size(32).save(f"{OUT}/favicon-32x32.png")
size(180).save(f"{OUT}/apple-touch-icon.png")
size(192).save(f"{OUT}/icon-192.png")
size(512).save(f"{OUT}/icon-512.png")
big.resize((256, 256), Image.LANCZOS).save(
    f"{OUT}/favicon.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

# contact sheet: light and dark backgrounds, actual sizes enlarged
sheet = Image.new("RGBA", (560, 200), (255, 255, 255, 255))
sheet.paste((30, 30, 30, 255), (280, 0, 560, 200))
x = 12
for n in (16, 32, 64, 128):
    ic = size(n)
    sheet.alpha_composite(ic, (x, 20)); sheet.alpha_composite(ic, (x + 280, 20))
    x += n + 12
preview = os.path.join(tempfile.gettempdir(), "thermostat_favicon_preview.png")
sheet.save(preview)
print("preview:", preview)
