#!/usr/bin/env python3
"""Store banner and icons for BrewSession.

The app's own visual language, nothing borrowed: the teacup that fills as
the steep runs, the skinny 7x12 blocky digits, tea green and gold on black.
The banner leans on a real emery screenshot the way the Solfarer Atlas
banner does — the product sells itself; the art just sets the table.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

TEA = (0, 170, 85)        # GColorMayGreen, the app's accent
GOLD = (255, 170, 0)      # GColorChromeYellow
WHITE = (255, 255, 255)
DIM = (170, 170, 170)
BLACK = (0, 0, 0)

# The 7x12 skinny digits, straight from src/c/digits.c.
DIGITS = {
 '0': [0x3E,0x7F,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x7F,0x3E],
 '1': [0x78,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7F,0x7F],
 '2': [0x3E,0x7F,0x03,0x03,0x03,0x7F,0x7E,0x60,0x60,0x60,0x7F,0x7F],
 '3': [0x3E,0x7F,0x03,0x03,0x03,0x1F,0x1E,0x03,0x03,0x03,0x7F,0x3E],
 '4': [0x63,0x63,0x63,0x63,0x63,0x63,0x7F,0x7F,0x03,0x03,0x03,0x03],
 '5': [0x7F,0x7F,0x60,0x60,0x60,0x7E,0x7F,0x03,0x03,0x03,0x7F,0x3E],
 '6': [0x3E,0x7F,0x60,0x60,0x60,0x7E,0x7F,0x63,0x63,0x63,0x7F,0x3E],
 '7': [0x7F,0x7F,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03],
 '8': [0x3E,0x7F,0x63,0x63,0x63,0x3E,0x3E,0x63,0x63,0x63,0x7F,0x3E],
 '9': [0x3E,0x7F,0x63,0x63,0x63,0x7F,0x3F,0x03,0x03,0x03,0x03,0x03],
 ':': [0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00],
}
COLON_COLS = (2, 3)   # ink columns of the colon glyph


def futura(size, index=0):
    return ImageFont.truetype("/System/Library/Fonts/Supplemental/Futura.ttc",
                              size, index=index)


def blocky(d, s, x, y, scale, col=WHITE):
    """Draw a digit string; returns the x after the last glyph."""
    for ch in s:
        rows = DIGITS[ch]
        if ch == ':':
            l, r = COLON_COLS
        else:
            l, r = 0, 6
        for ri, row in enumerate(rows):
            for c in range(7):
                if row & (1 << (6 - c)):
                    d.rectangle([x + (c - l) * scale, y + ri * scale,
                                 x + (c - l + 1) * scale - 1,
                                 y + (ri + 1) * scale - 1], fill=col)
        x += (r - l + 2) * scale
    return x - scale


def teacup(d, cx, rim_y, r, lw, fill_pct=100, steam=True, line=WHITE):
    """The app's cup, scaled up: bowl, tea, rim, handle, saucer, steam."""
    if fill_pct > 0:
        d.pieslice([cx - r + lw, rim_y - r + lw, cx + r - lw, rim_y + r - lw],
                   0, 180, fill=TEA)
        if fill_pct < 100:
            cover = (r - lw) * (100 - fill_pct) // 100
            d.rectangle([cx - r, rim_y + 1, cx + r, rim_y + cover], fill=BLACK)
    d.arc([cx - r, rim_y - r, cx + r, rim_y + r], 0, 180, fill=line, width=lw)
    d.line([cx - r - lw, rim_y, cx + r + lw, rim_y], fill=line, width=lw)
    rh = r // 2
    d.arc([cx + r - rh, rim_y + lw, cx + r + rh, rim_y + 2 * rh + lw],
          -70, 70, fill=line, width=lw)
    d.line([cx - r - lw, rim_y + r + lw * 2, cx + r + lw, rim_y + r + lw * 2],
           fill=line, width=lw)
    d.line([cx - r + r // 3, rim_y + r + lw * 3 + 2,
            cx + r - r // 3, rim_y + r + lw * 3 + 2], fill=line, width=lw)
    if steam:
        for side in (-1, 1):
            x = cx + side * (r // 2)
            d.line([x, rim_y - r - lw, x, rim_y - r + r // 4], fill=line, width=lw)
            d.line([x + lw + 1, rim_y - r + r // 4 + lw + 2,
                    x + lw + 1, rim_y - r + r // 2 + lw + 2], fill=line, width=lw)


# ---------------------------------------------------------------- banner
W, H = 720, 320
img = Image.new("RGB", (W, H), BLACK)
d = ImageDraw.Draw(img)

# A real screen on the right, framed — the infusion-2 steep, mid-fill.
shot = Image.open(os.path.join(
    ROOT, "store/screenshots/emery_4_steeping.png")).convert("RGB")
bs = shot.resize((230, 262), Image.NEAREST)
bx, by = W - bs.width - 26, (H - bs.height) // 2
d.rectangle([bx - 3, by - 3, bx + bs.width + 2, by + bs.height + 2],
            outline=(60, 60, 60), width=3)
img.paste(bs, (bx, by))
d = ImageDraw.Draw(img)

# Left column: wordmark, what it is, and the cup mid-pour beside a time.
d.text((44, 30), "BREW", font=futura(56, index=2), fill=TEA)
d.text((208, 30), "SESSION", font=futura(56, index=2), fill=WHITE)
d.text((46, 100), "A TEA TIMER THAT KNOWS", font=futura(24, index=0), fill=GOLD)
d.text((46, 130), "WHICH INFUSION YOU'RE ON", font=futura(24, index=0), fill=GOLD)

teacup(d, 110, 230, 44, 5, fill_pct=65)
blocky(d, "2:30", 210, 196, 6, col=WHITE)
d.text((212, 278), "+25s each infusion", font=futura(18, index=1), fill=DIM)

img.save(os.path.join(ROOT, "store/banner_720x320.png"))


# ---------------------------------------------------------------- icons
def icon(size):
    im = Image.new("RGB", (size, size), BLACK)
    dd = ImageDraw.Draw(im)
    s = size / 48.0
    r = int(15 * s)
    lw = max(2, int(2.5 * s))
    teacup(dd, size // 2, int(size * 0.52), r, lw, fill_pct=100)
    return im


icon(48).save(os.path.join(ROOT, "store/icon_small_48.png"))
icon(144).save(os.path.join(ROOT, "store/icon_large_144.png"))
print("banner and icons written")
