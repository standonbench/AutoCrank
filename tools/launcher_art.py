#!/usr/bin/env python3
"""Generate all Playdate launcher assets for AutoCrank.

Procedural 1-bit "scherenschnitt festival poster" art, reusing the game's
own pet sprites and Sasser Slab font sheet so the launcher matches the
in-game look exactly. Run with a Pillow-equipped python:

    python3 -m venv /tmp/pdart && /tmp/pdart/bin/pip install Pillow
    /tmp/pdart/bin/python tools/launcher_art.py

Outputs into Source/launcher/ (pdxinfo imagePath=launcher):
    card.png (350x155)            card-pressed.png
    card-highlighted/1..16.png    icon.png (32x32)
    icon-pressed.png              icon-highlighted/1..8.png
    launchImage.png (400x240)     launchImages/1..12.png
    wrapping-pattern.png (400x240)
"""
import math
import os
from PIL import Image, ImageChops, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Source")
OUT = os.path.join(SRC, "launcher")
PETS = os.path.join(SRC, "images", "pets")
FNT = os.path.join(SRC, "fonts", "Sasser-Slab-Bold.fnt")
FNT_TABLE = os.path.join(SRC, "fonts", "Sasser-Slab-Bold-table-18-18.png")

BLACK, WHITE = 0, 255
BAYER4 = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))

# ---------------------------------------------------------------------------
# canvas helpers
# ---------------------------------------------------------------------------

def new_canvas(w, h, color=WHITE):
    return Image.new("L", (w, h), color)


_dither_cache = {}

def dither_mask(size, level16):
    """Full-size L mask: 255 where the Bayer cell fires for this density."""
    key = (size, level16)
    if key not in _dither_cache:
        m = Image.new("L", size, 0)
        px = m.load()
        for y in range(size[1]):
            row = BAYER4[y % 4]
            for x in range(size[0]):
                if row[x % 4] < level16:
                    px[x, y] = 255
        _dither_cache[key] = m
    return _dither_cache[key]


def poly(canvas, pts, color=BLACK, level16=16):
    """Fill polygon; level16<16 gives a Bayer-dithered fill."""
    mask = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(mask).polygon(pts, fill=255)
    if level16 < 16:
        mask = ImageChops.multiply(mask, dither_mask(canvas.size, level16))
    canvas.paste(color, (0, 0), mask)


def rect(canvas, x, y, w, h, color=BLACK, level16=16):
    poly(canvas, [(x, y), (x + w - 1, y), (x + w - 1, y + h - 1), (x, y + h - 1)],
         color, level16)


def chamfer_panel(canvas, x, y, w, h, color=BLACK, ch=4):
    """Filled panel with cut corners — matches th_panel's silhouette."""
    poly(canvas, [(x + ch, y), (x + w - 1 - ch, y), (x + w - 1, y + ch),
                  (x + w - 1, y + h - 1 - ch), (x + w - 1 - ch, y + h - 1),
                  (x + ch, y + h - 1), (x, y + h - 1 - ch), (x, y + ch)], color)


def line(canvas, x0, y0, x1, y1, color=BLACK, width=1):
    ImageDraw.Draw(canvas).line([(x0, y0), (x1, y1)], fill=color, width=width)


def circle(canvas, cx, cy, r, color=BLACK, fill=True, level16=16):
    if fill:
        mask = Image.new("L", canvas.size, 0)
        ImageDraw.Draw(mask).ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        if level16 < 16:
            mask = ImageChops.multiply(mask, dither_mask(canvas.size, level16))
        canvas.paste(color, (0, 0), mask)
    else:
        ImageDraw.Draw(canvas).ellipse([cx - r, cy - r, cx + r, cy + r],
                                       outline=color)


# ---------------------------------------------------------------------------
# sprites
# ---------------------------------------------------------------------------

_sprite_cache = {}

def load_sprite(pet_id, big=True):
    name = ("pet64_%02d.png" if big else "pet_%02d.png") % pet_id
    if name not in _sprite_cache:
        _sprite_cache[name] = Image.open(os.path.join(PETS, name)).convert("RGBA")
    return _sprite_cache[name]


def blit_sprite(canvas, pet_id, cx, feet_y, big=True, flip=False, halo=True):
    """Paste a pet sprite centered on cx with its bbox bottom at feet_y,
    with a 1px white halo (same trick as draw_creature_haloed)."""
    im = load_sprite(pet_id, big)
    if flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    a = im.getchannel("A").point(lambda v: 255 if v > 127 else 0)
    bbox = a.getbbox()
    x = cx - (bbox[0] + bbox[2]) // 2
    y = feet_y - bbox[3]
    if halo:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx or dy:
                    canvas.paste(WHITE, (x + dx, y + dy), a)
    ink = im.convert("L").point(lambda v: 255 if v > 127 else 0)
    canvas.paste(ink, (x, y), a)


# ---------------------------------------------------------------------------
# font — parse the game's own .fnt + glyph table
# ---------------------------------------------------------------------------

def load_font():
    glyphs, order, tracking = {}, [], 1
    for ln in open(FNT, encoding="utf-8").read().splitlines():
        if not ln.strip():
            continue
        if "=" in ln and "\t" not in ln:
            k, v = ln.split("=", 1)
            if k.strip() == "tracking":
                tracking = int(v)
            continue
        parts = ln.split("\t")
        ch = parts[0]
        if ch == "space":
            ch = " "
        order.append((ch, int(parts[-1])))
    table = Image.open(FNT_TABLE).convert("RGBA")
    cols = table.width // 18
    for i, (ch, w) in enumerate(order):
        gx, gy = (i % cols) * 18, (i // cols) * 18
        cell = table.crop((gx, gy, gx + 18, gy + 18))
        mask = cell.getchannel("A").point(lambda v: 255 if v > 127 else 0)
        glyphs[ch] = (mask, w)
    return glyphs, tracking


GLYPHS, TRACKING = load_font()
CAP_TOP = GLYPHS["A"][0].getbbox()[1]          # ink offset inside the cell
CAP_BOT = GLYPHS["A"][0].getbbox()[3]
CAP_H = CAP_BOT - CAP_TOP


def text_w(s, scale=1, tracking=None):
    t = TRACKING if tracking is None else tracking
    w = sum(GLYPHS[c][1] + t for c in s if c in GLYPHS)
    return (w - t) * scale if s else 0


def draw_text(canvas, x, y, s, color=WHITE, scale=1, tracking=None):
    t = TRACKING if tracking is None else tracking
    for c in s:
        if c not in GLYPHS:
            continue
        mask, w = GLYPHS[c]
        m = mask if scale == 1 else mask.resize(
            (18 * scale, 18 * scale), Image.NEAREST)
        canvas.paste(color, (x, y), m)
        x += (w + t) * scale


def draw_text_centered(canvas, cx, y, s, color=WHITE, scale=1, tracking=None):
    draw_text(canvas, cx - text_w(s, scale, tracking) // 2, y, s,
              color, scale, tracking)


# ---------------------------------------------------------------------------
# shared motifs
# ---------------------------------------------------------------------------

def garland(canvas, x0, x1, y_end, sag, phase=0.0, step=26):
    """Pennant string sagging between two points; phase animates flutter."""
    def string_y(x):
        t = (x - x0) / (x1 - x0)
        return y_end + sag * math.sin(math.pi * t)
    prev = None
    for x in range(x0, x1 + 1, 4):
        y = int(string_y(x))
        if prev:
            line(canvas, prev[0], prev[1], x, y)
        prev = (x, y)
    i = 0
    for x in range(x0 + step // 2, x1 - step // 2, step):
        y = int(string_y(x))
        sway = round(2.0 * math.sin(2 * math.pi * (phase + i * 0.18)))
        tip = (x + sway, y + 13)
        pts = [(x - 6, y), (x + 6, y), tip]
        if i % 2 == 0:
            poly(canvas, pts, BLACK)
        else:
            poly(canvas, pts, BLACK, level16=6)
            ImageDraw.Draw(canvas).polygon(pts, outline=BLACK)
        i += 1


def sun(canvas, cx, cy, r):
    circle(canvas, cx, cy, r, BLACK, fill=True, level16=7)
    circle(canvas, cx, cy, r, BLACK, fill=False)
    for k in range(8):
        a = k * math.pi / 4 + math.pi / 8
        x0 = cx + int((r + 3) * math.cos(a))
        y0 = cy + int((r + 3) * math.sin(a))
        x1 = cx + int((r + 7) * math.cos(a))
        y1 = cy + int((r + 7) * math.sin(a))
        line(canvas, x0, y0, x1, y1)


def rosette(canvas, cx, cy, r, color=WHITE):
    """Prize rosette with ribbon tails, white-on-black by default."""
    line(canvas, cx - 3, cy + r - 2, cx - 5, cy + r + 6, color)
    line(canvas, cx + 3, cy + r - 2, cx + 5, cy + r + 6, color)
    circle(canvas, cx, cy, r, color, fill=False)
    circle(canvas, cx, cy, max(1, r - 3), color, fill=False)
    canvas.paste(color, (cx, cy), Image.new("L", (1, 1), 255))


def edelweiss(canvas, cx, cy, color=WHITE):
    """Tiny 5-px star flower."""
    for dx, dy in ((0, 0), (-2, 0), (2, 0), (0, -2), (0, 2)):
        canvas.paste(color, (cx + dx, cy + dy), Image.new("L", (1, 1), 255))


def cowbell(canvas, cx, cy, s=10):
    """Outline cowbell: strap loop, trapezoid body, clapper dot."""
    d = ImageDraw.Draw(canvas)
    d.rectangle([cx - 3, cy - s, cx + 3, cy - s + 4], outline=BLACK)
    d.polygon([(cx - s // 2 - 1, cy - s + 4), (cx + s // 2 + 1, cy - s + 4),
               (cx + s - 2, cy + s - 4), (cx - s + 2, cy + s - 4)],
              outline=BLACK)
    d.line([cx - s + 1, cy + s - 3, cx + s - 1, cy + s - 3], fill=BLACK)
    d.ellipse([cx - 2, cy + s - 2, cx + 2, cy + s + 2], fill=BLACK)


def edelweiss_bloom(canvas, cx, cy, r=9):
    """Bigger line-art edelweiss for the wrapping paper."""
    d = ImageDraw.Draw(canvas)
    for k in range(7):
        a = k * 2 * math.pi / 7 - math.pi / 2
        px_, py_ = cx + (r + 2) * math.cos(a), cy + (r + 2) * math.sin(a)
        mx, my = cx + 3 * math.cos(a), cy + 3 * math.sin(a)
        side = 2.6
        sa, ca = math.sin(a), math.cos(a)
        d.polygon([(mx - side * sa, my + side * ca), (px_, py_),
                   (mx + side * sa, my - side * ca)], outline=BLACK)
    d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], outline=BLACK)
    canvas.paste(BLACK, (cx, cy), Image.new("L", (1, 1), 255))


def rosette_medal(canvas, cx, cy, r=9):
    """Line-art prize rosette for the wrapping paper (black on white)."""
    d = ImageDraw.Draw(canvas)
    for k in range(10):
        a = k * math.pi / 5
        x = cx + int((r + 2) * math.cos(a))
        y = cy + int((r + 2) * math.sin(a))
        d.ellipse([x - 2, y - 2, x + 2, y + 2], outline=BLACK)
    d.ellipse([cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3], outline=BLACK)
    d.line([cx - 3, cy + r + 1, cx - 5, cy + r + 7], fill=BLACK)
    d.line([cx + 3, cy + r + 1, cx + 5, cy + r + 7], fill=BLACK)


def ridge(canvas, pts, bottom, color=BLACK, level16=16):
    poly(canvas, list(pts) + [(pts[-1][0], bottom), (pts[0][0], bottom)],
         color, level16)


# ---------------------------------------------------------------------------
# CARD — 350x155
# ---------------------------------------------------------------------------
CARD_W, CARD_H = 350, 155

def render_card(phase=None, pressed=False):
    """phase None = static; else 0..1 animation phase."""
    c = new_canvas(CARD_W, CARD_H)
    dy = 2 if pressed else 0

    # far dithered ridge + near black alp
    ridge(c, [(0, 96), (45, 70), (95, 100), (150, 62), (205, 98),
              (255, 72), (305, 102), (350, 80)], CARD_H, BLACK, 4)
    ground = [(0, 124), (40, 116), (80, 124), (120, 112), (175, 122),
              (230, 112), (270, 122), (310, 114), (350, 120)]
    ridge(c, [(x, y + dy) for x, y in ground], CARD_H)

    # the duel: two queens face off on the alp
    t = 0.0 if phase is None else phase
    hopL = round(5 * math.sin(math.pi * min(1.0, max(0.0, t * 2)))) \
        if phase is not None and t < 0.5 else 0
    hopR = round(5 * math.sin(math.pi * min(1.0, max(0.0, (t - 0.5) * 2)))) \
        if phase is not None and t >= 0.5 else 0
    blit_sprite(c, 13, 78, 146 + dy - hopL, big=True, flip=False)
    blit_sprite(c, 13, 272, 146 + dy - hopR, big=True, flip=True)
    rosette(c, 175, 130 + dy, 7, WHITE)
    for ex, ey in ((30, 138), (140, 142), (215, 140), (322, 136)):
        edelweiss(c, ex, ey + dy, WHITE)

    # title banner with the sub-chip locked onto its bottom edge
    bw = text_w("AUTOCRANK", 2, 0) + 28
    bx = (CARD_W - bw) // 2
    by = 24 + dy
    bh = CAP_H * 2 + 14
    chamfer_panel(c, bx, by, bw, bh, BLACK, ch=5)
    draw_text_centered(c, CARD_W // 2, by + (bh - CAP_H * 2) // 2 - CAP_TOP * 2,
                       "AUTOCRANK", WHITE, 2, 0)
    ch_h = CAP_H + 8
    sw = text_w("COMBAT DES REINES", 1) + 16
    sx = (CARD_W - sw) // 2
    sy = by + bh - 3
    rect(c, sx, sy, sw, ch_h, WHITE)
    ImageDraw.Draw(c).rectangle([sx, sy, sx + sw - 1, sy + ch_h - 1],
                                outline=BLACK)
    draw_text_centered(c, CARD_W // 2, sy + (ch_h - CAP_H) // 2 - CAP_TOP,
                       "COMBAT DES REINES", BLACK, 1)

    garland(c, 4, CARD_W - 5, 8 + dy, 7, phase=t)

    # chamfered border frame
    d = ImageDraw.Draw(c)
    for inset in (0, 1):
        d.polygon([(5, inset), (CARD_W - 6, inset), (CARD_W - 1 - inset, 5),
                   (CARD_W - 1 - inset, CARD_H - 6), (CARD_W - 6, CARD_H - 1 - inset),
                   (5, CARD_H - 1 - inset), (inset, CARD_H - 6), (inset, 5)],
                  outline=BLACK)
    return c


# ---------------------------------------------------------------------------
# ICON — 32x32
# ---------------------------------------------------------------------------

def render_icon(angle=0.0, pressed=False):
    """Bold cowbell on white; angle (degrees) swings it for the
    highlighted animation, clapper lagging the other way."""
    c = new_canvas(32, 32)
    px, py = 16, 7                                    # pivot at the strap
    a = math.radians(angle)
    ca, sa = math.cos(a), math.sin(a)

    def rot(rx, ry):
        return (px + rx * ca - ry * sa, py + rx * sa + ry * ca)

    body = [(4, 1), (5, 6), (6, 11), (8, 15), (11, 16), (11, 21),
            (3, 21), (3, 19), (-3, 19), (-3, 21), (-11, 21), (-11, 16),
            (-8, 15), (-6, 11), (-5, 6), (-4, 1)]
    ImageDraw.Draw(c).polygon([rot(x, y) for x, y in body], fill=BLACK)
    ImageDraw.Draw(c).line([rot(-5.5, 12), rot(5.5, 12)], fill=WHITE)
    if angle:                                          # clapper swings out
        cx, cy = rot(-6.0 * math.sin(a * 1.6), 20)
        ImageDraw.Draw(c).ellipse([cx - 2, cy - 2, cx + 2, cy + 2],
                                  fill=BLACK)
    circle(c, px, py, 4, BLACK, fill=True)             # strap loop
    circle(c, px, py, 2, WHITE, fill=True)
    ImageDraw.Draw(c).rectangle([0, 0, 31, 31], outline=BLACK)
    if pressed:
        c = c.point(lambda v: 255 - v)
    return c


# ---------------------------------------------------------------------------
# LAUNCH IMAGE — 400x240 festival poster (bridges into the title screen)
# ---------------------------------------------------------------------------
LW, LH = 400, 240

def render_launch():
    c = new_canvas(LW, LH)

    sun(c, 352, 116, 16)
    ridge(c, [(0, 130), (55, 96), (115, 134), (180, 88), (245, 130),
              (305, 100), (360, 136), (400, 112)], LH, BLACK, 4)
    ridge(c, [(0, 158), (60, 140), (130, 160), (200, 136), (270, 158),
              (330, 142), (400, 156)], LH, BLACK, 9)
    ground = [(0, 196), (50, 188), (100, 196), (160, 184), (220, 194),
              (280, 184), (340, 194), (400, 188)]
    ridge(c, ground, LH)

    # procession walking home (right to left), the queen leading
    blit_sprite(c, 13, 70, 232, big=True, flip=True)
    blit_sprite(c, 5, 140, 228, big=False, flip=True)
    blit_sprite(c, 6, 190, 230, big=False, flip=True)
    blit_sprite(c, 1, 240, 227, big=False, flip=True)
    blit_sprite(c, 2, 285, 230, big=False, flip=True)
    blit_sprite(c, 4, 330, 226, big=False, flip=True)
    for ex, ey in ((30, 206), (120, 210), (210, 206), (300, 210), (375, 204)):
        edelweiss(c, ex, ey, WHITE)

    # title band — slab type on black, like the in-game banner
    band_h = CAP_H * 2 + 16
    rect(c, 0, 22, LW, band_h, BLACK)
    line(c, 0, 22 + band_h + 2, LW - 1, 22 + band_h + 2, BLACK)
    draw_text_centered(c, LW // 2, 22 + 8 - CAP_TOP * 2,
                       "COMBAT DES REINES", WHITE, 2, 0)
    ch_h = CAP_H + 8
    sw = text_w("ALPINE AUTOBATTLER", 1) + 18
    sx = (LW - sw) // 2
    sy = 22 + band_h + 8
    rect(c, sx, sy, sw, ch_h, BLACK)
    draw_text_centered(c, LW // 2, sy + 4 - CAP_TOP,
                       "ALPINE AUTOBATTLER", WHITE, 1)

    garland(c, -8, LW + 8, 6, 8)
    return c


def render_launch_frames(n=12):
    """Ridge-curtain reveal: black curtain with a jagged alpine bottom edge
    lifts to expose the poster — echoes the in-game scene wipe."""
    poster = render_launch()
    frames = []
    teeth = [(0, 0), (30, 26), (60, 4), (95, 30), (130, 8), (165, 32),
             (200, 6), (235, 30), (270, 4), (305, 28), (340, 8),
             (370, 30), (400, 0)]
    for i in range(n):
        f = poster.copy()
        t = (i + 1) / n
        lift = int((LH + 40) * (1 - (1 - t) * (1 - t)))     # ease-out
        edge = LH - lift
        if edge > -40:
            pts = [(x, edge + h) for x, h in teeth]
            poly(f, [(0, -1), (LW, -1)] + pts[::-1], BLACK)
        frames.append(f)
    return frames


# ---------------------------------------------------------------------------
# WRAPPING PAPER — 400x240 seamless tile (the packaging)
# ---------------------------------------------------------------------------

def render_wrapping():
    c = new_canvas(LW, LH)
    cw, chh = 50, 48                                     # 8x5 cell grid
    motifs = (cowbell, edelweiss_bloom, rosette_medal)
    for row in range(LH // chh):
        for col in range(LW // cw):
            cx = (col * cw + (row % 2) * (cw // 2) + cw // 2) % LW
            cy = row * chh + chh // 2
            m = motifs[(col + 2 * row) % 3]
            for ox in (0, -LW, LW):                       # wrap for seamless
                m(c, cx + ox, cy)
    # diamond dots between motifs
    d = ImageDraw.Draw(c)
    for row in range(LH // chh):
        for col in range(LW // cw):
            cx = (col * cw + ((row + 1) % 2) * (cw // 2) + cw // 2) % LW
            cy = row * chh + chh // 2
            d.polygon([(cx, cy - 2), (cx + 2, cy), (cx, cy + 2), (cx - 2, cy)],
                      fill=BLACK)
    return c


# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------

def save(img, *path):
    p = os.path.join(OUT, *path)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    img.convert("1").save(p)
    print("wrote", os.path.relpath(p, ROOT))


def main():
    save(render_card(), "card.png")
    save(render_card(pressed=True), "card-pressed.png")
    for i in range(16):
        save(render_card(phase=i / 16.0), "card-highlighted", f"{i + 1}.png")
    save(render_icon(), "icon.png")
    save(render_icon(pressed=True), "icon-pressed.png")
    swings = (0, 10, 16, 10, 0, -10, -16, -10)
    for i, s in enumerate(swings):
        save(render_icon(angle=s), "icon-highlighted", f"{i + 1}.png")
    save(render_launch(), "launchImage.png")
    for i, f in enumerate(render_launch_frames()):
        save(f, "launchImages", f"{i + 1}.png")
    save(render_wrapping(), "wrapping-pattern.png")


if __name__ == "__main__":
    main()
