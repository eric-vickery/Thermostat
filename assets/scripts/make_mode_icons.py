#!/usr/bin/env python3
"""50x50 white-on-transparent mode icons, drawn at 8x and downsampled."""
# Needs Pillow (pip install pillow). Run from anywhere: python3 assets/scripts/make_mode_icons.py
# Afterwards re-add the changed PNGs as bitmaps in Thermostat.eez-project.
import math
import os
import tempfile
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
S, N = 8, 50
W = (255, 255, 255, 255)

def canvas():
    im = Image.new("RGBA", (N * S, N * S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)

def px(v): return v * S

def save(im, name):
    im.resize((N, N), Image.LANCZOS).save(f"{OUT}/{name}.png")

def line_round(d, p0, p1, w):
    d.line([p0, p1], fill=W, width=w)
    r = w / 2
    for x, y in (p0, p1):
        d.ellipse([x - r, y - r, x + r, y + r], fill=W)

# Manual: gauge - open 270 degree arc with a pointer
im, d = canvas()
c = px(25)
r = px(19)
d.arc([c - r, c - r, c + r, c + r], start=135, end=405, fill=W, width=px(4))
for ang in (135, 405):
    a = math.radians(ang)
    x, y = c + r * math.cos(a), c + r * math.sin(a)
    t = px(2)
    d.ellipse([x - t, y - t, x + t, y + t], fill=W)
a = math.radians(-50)
line_round(d, (c, c), (c + px(13) * math.cos(a), c + px(13) * math.sin(a)), px(4))
d.ellipse([c - px(4), c - px(4), c + px(4), c + px(4)], fill=W)
save(im, "mode_manual")

# Schedule: calendar
im, d = canvas()
d.rounded_rectangle([px(7), px(10), px(43), px(44)], radius=px(4), outline=W, width=px(3))
d.rectangle([px(7), px(10), px(43), px(19)], fill=W)
for x in (16, 34):
    d.rounded_rectangle([px(x - 1.5), px(5), px(x + 1.5), px(14)], radius=px(1.5), fill=W)
for row in range(2):
    for col in range(3):
        x, y = 15 + col * 10, 26 + row * 9
        d.rounded_rectangle([px(x - 2), px(y - 2), px(x + 2), px(y + 2)], radius=px(1), fill=W)
save(im, "mode_schedule")

# Off: power symbol
im, d = canvas()
c = px(25)
r = px(15)
d.arc([c - r, c - r + px(2), c + r, c + r + px(2)], start=-60, end=240, fill=W, width=px(4))
for ang in (-60, 240):
    a = math.radians(ang)
    x, y = c + r * math.cos(a), c + px(2) + r * math.sin(a)
    t = px(2)
    d.ellipse([x - t, y - t, x + t, y + t], fill=W)
line_round(d, (c, px(7)), (c, px(23)), px(4))
save(im, "mode_off")

# Contact sheet on the display background for a quick look
sheet = Image.new("RGBA", (50 * 3 * 4 + 40, 50 * 4 + 20), (10, 17, 40, 255))
for i, n in enumerate(("mode_manual", "mode_schedule", "mode_off")):
    ic = Image.open(f"{OUT}/{n}.png").resize((200, 200), Image.NEAREST)
    sheet.alpha_composite(ic, (10 + i * 210, 10))
preview = os.path.join(tempfile.gettempdir(), "thermostat_mode_icons_preview.png")
sheet.save(preview)
print("preview:", preview)
