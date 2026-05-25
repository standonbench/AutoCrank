"""Convert arbitrary images (photos, art) into Playdate-friendly 1-bit pixel art.

Sister tool to to_1bit.py — that one targets game sprites (color preservation,
sheet slicing). This one targets WHOLE images: full-screen scenes (400x240),
or fixed-square sprites cropped from photos (16, 32, etc).

Examples
--------
# Full-screen 400x240, default tone dither, output next to source:
python tools/pixelize.py photo.jpg

# Make a 32x32 icon from a photo, center-cropped, with Atkinson dither:
python tools/pixelize.py photo.jpg --size 32 --dither atkinson

# Dump every dither side-by-side at 4x zoom for picking the best look:
python tools/pixelize.py photo.jpg --compare --zoom 4 --out out/

# Render the 8-level grey ramp itself (the "inspo strip"):
python tools/pixelize.py --show-ramp --out out/

Adjustments
-----------
--brightness, --contrast, --gamma  -- applied to luma before dithering.
--invert                           -- swap black <-> white at the very end.
"""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass, field
from typing import Optional

from PIL import Image, ImageEnhance, ImageOps


# ---------- dither kernels ------------------------------------------------

BAYER4 = [
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5],
]

BAYER8 = [
    [ 0, 32,  8, 40,  2, 34, 10, 42],
    [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44,  4, 36, 14, 46,  6, 38],
    [60, 28, 52, 20, 62, 30, 54, 22],
    [ 3, 35, 11, 43,  1, 33,  9, 41],
    [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47,  7, 39, 13, 45,  5, 37],
    [63, 31, 55, 23, 61, 29, 53, 21],
]

# Hand-tuned 4x4 tone tiles. Each tile picks a fixed set of pixels to be WHITE.
# Coverage steps roughly match the inspo strip (12/25/37/50/62/75/87 %),
# but the patterns themselves are chosen so they tile cleanly and read as a
# single perceptual grey rather than as visible structure.
#
# Index 0 = pure black, index 8 = pure white. Each tile is a 4x4 bitmap
# (row-major) of 0/1; the cell is white where the value is 1.
TONE_TILES = [
    # 0% — black
    [0,0,0,0,
     0,0,0,0,
     0,0,0,0,
     0,0,0,0],
    # 12% — 2/16 (sparse offset dots)
    [1,0,0,0,
     0,0,0,0,
     0,0,1,0,
     0,0,0,0],
    # 25% — 4/16 (bayer-ish quarter)
    [1,0,0,0,
     0,0,1,0,
     0,0,0,0,
     0,1,0,0],   # last row hits the missing column
    # 37% — 6/16
    [1,0,1,0,
     0,0,0,0,
     1,0,1,0,
     0,1,0,1],
    # 50% — checkerboard
    [1,0,1,0,
     0,1,0,1,
     1,0,1,0,
     0,1,0,1],
    # 62% — inverse of 37%
    [1,1,1,1,
     1,0,1,0,
     1,1,1,1,
     1,0,1,0],
    # 75% — inverse of 25%
    [1,1,1,1,
     1,1,0,1,
     1,1,1,1,
     1,0,1,1],
    # 87% — inverse of 12%
    [1,1,1,1,
     1,1,1,1,
     1,1,0,1,
     1,1,1,1],
    # 100% — white
    [1,1,1,1,
     1,1,1,1,
     1,1,1,1,
     1,1,1,1],
]
TONE_LEVELS = len(TONE_TILES)  # 9 levels: 0..8


# ---------- options -------------------------------------------------------

@dataclass
class Options:
    size: tuple[int, int] = (400, 240)
    fit: str = "cover"          # cover | contain | stretch
    dither: str = "tone"        # tone | bayer4 | bayer8 | atkinson | floyd | threshold
    threshold: int = 128
    brightness: float = 1.0
    contrast: float = 1.0
    gamma: float = 1.0
    invert: bool = False
    zoom: int = 1               # NEAREST upscale of output (for inspection)
    bg: int = 0                 # letterbox fill (0=black, 255=white) when fit=contain
    crop_x: float = 0.5         # cover-mode horizontal anchor (0=left, 1=right)
    crop_y: float = 0.5         # cover-mode vertical anchor (0=top, 1=bottom)


# ---------- size & fit ----------------------------------------------------

def parse_size(s: str) -> tuple[int, int]:
    s = s.lower().strip()
    if s == "full":
        return (400, 240)
    if "x" in s:
        w, h = s.split("x")
        return (int(w), int(h))
    n = int(s)
    return (n, n)


def fit_into(src: Image.Image, target: tuple[int, int], mode: str, bg: int,
             crop_x: float = 0.5, crop_y: float = 0.5) -> Image.Image:
    """Resize src into target box per fit mode. Returns RGBA at target size."""
    src = src.convert("RGBA")
    tw, th = target
    if mode == "stretch":
        return src.resize((tw, th), Image.LANCZOS)
    sw, sh = src.size
    src_ratio = sw / sh
    dst_ratio = tw / th
    if mode == "cover":
        # Scale so the SHORT side matches, then crop with anchor (default centered).
        if src_ratio > dst_ratio:
            new_h = th
            new_w = round(sw * th / sh)
        else:
            new_w = tw
            new_h = round(sh * tw / sw)
        scaled = src.resize((new_w, new_h), Image.LANCZOS)
        x = round((new_w - tw) * max(0.0, min(1.0, crop_x)))
        y = round((new_h - th) * max(0.0, min(1.0, crop_y)))
        return scaled.crop((x, y, x + tw, y + th))
    if mode == "contain":
        if src_ratio > dst_ratio:
            new_w = tw
            new_h = round(sh * tw / sw)
        else:
            new_h = th
            new_w = round(sw * th / sh)
        scaled = src.resize((new_w, new_h), Image.LANCZOS)
        canvas = Image.new("RGBA", (tw, th), (bg, bg, bg, 255))
        canvas.paste(scaled, ((tw - new_w) // 2, (th - new_h) // 2), scaled)
        return canvas
    raise ValueError(f"unknown fit mode {mode!r}")


# ---------- luma & adjustments --------------------------------------------

def to_luma(img: Image.Image, opts: Options) -> Image.Image:
    """RGBA -> L with brightness/contrast/gamma applied."""
    # Composite over solid grey first so semi-transparent pixels don't go pitch
    # black (which would dither as solid black).
    rgba = img.convert("RGBA")
    bg = Image.new("RGBA", rgba.size, (128, 128, 128, 255))
    bg.alpha_composite(rgba)
    gray = bg.convert("L")
    if opts.brightness != 1.0:
        gray = ImageEnhance.Brightness(gray).enhance(opts.brightness)
    if opts.contrast != 1.0:
        gray = ImageEnhance.Contrast(gray).enhance(opts.contrast)
    if opts.gamma != 1.0:
        lut = [min(255, max(0, int(255 * (i / 255) ** (1 / opts.gamma)))) for i in range(256)]
        gray = gray.point(lut)
    return gray


# ---------- dithers -------------------------------------------------------

def dither_threshold(luma: Image.Image, t: int) -> Image.Image:
    return luma.point(lambda v: 255 if v >= t else 0, mode="1")


def dither_bayer(luma: Image.Image, matrix, size: int) -> Image.Image:
    w, h = luma.size
    out = Image.new("1", (w, h))
    opx = out.load()
    lpx = luma.load()
    # Precompute thresholds for each cell in 0..255 range.
    thresh = [[int((matrix[y][x] + 0.5) * 255 / (size * size)) for x in range(size)] for y in range(size)]
    for y in range(h):
        ty = y % size
        for x in range(w):
            opx[x, y] = 1 if lpx[x, y] >= thresh[ty][x % size] else 0
    return out


def dither_atkinson(luma: Image.Image) -> Image.Image:
    """Atkinson — sparse error diffusion, classic 1-bit Mac/Playdate look."""
    w, h = luma.size
    data = luma.tobytes()
    rows = [list(data[y * w:(y + 1) * w]) for y in range(h)]
    out = Image.new("1", (w, h))
    opx = out.load()
    offsets = [(1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)]
    for y in range(h):
        for x in range(w):
            v = max(0, min(255, rows[y][x]))
            new = 255 if v >= 128 else 0
            opx[x, y] = 1 if new else 0
            err = (v - new) // 8
            for dx, dy in offsets:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    rows[ny][nx] += err
    return out


def dither_floyd(luma: Image.Image) -> Image.Image:
    """Floyd–Steinberg — uses Pillow's built-in (faster & well-tuned)."""
    return luma.convert("1", dither=Image.FLOYDSTEINBERG)


def dither_tone(luma: Image.Image) -> Image.Image:
    """Map each pixel's grey level to one of TONE_LEVELS hand-tuned 4x4 tiles.

    Result: pixel art with a small, perceptually-clean set of "shades" that
    tile cleanly. Looks like the inspo strip.
    """
    w, h = luma.size
    out = Image.new("1", (w, h))
    opx = out.load()
    lpx = luma.load()
    # Quantize to TONE_LEVELS-1 buckets. v=0 -> 0, v=255 -> TONE_LEVELS-1.
    step = 256 / TONE_LEVELS  # bucket width
    for y in range(h):
        row = y & 3
        for x in range(w):
            v = lpx[x, y]
            level = min(TONE_LEVELS - 1, int(v / step))
            tile = TONE_TILES[level]
            opx[x, y] = 1 if tile[row * 4 + (x & 3)] else 0
    return out


def apply_dither(luma: Image.Image, opts: Options) -> Image.Image:
    if opts.dither == "threshold":
        out = dither_threshold(luma, opts.threshold)
    elif opts.dither == "bayer4":
        out = dither_bayer(luma, BAYER4, 4)
    elif opts.dither == "bayer8":
        out = dither_bayer(luma, BAYER8, 8)
    elif opts.dither == "atkinson":
        out = dither_atkinson(luma)
    elif opts.dither == "floyd":
        out = dither_floyd(luma)
    elif opts.dither == "tone":
        out = dither_tone(luma)
    else:
        raise ValueError(f"unknown dither {opts.dither!r}")
    if opts.invert:
        out = ImageOps.invert(out.convert("L")).convert("1")
    return out


# ---------- pipeline ------------------------------------------------------

def pixelize(src: Image.Image, opts: Options) -> Image.Image:
    fitted = fit_into(src, opts.size, opts.fit, opts.bg, opts.crop_x, opts.crop_y)
    luma = to_luma(fitted, opts)
    out = apply_dither(luma, opts)
    if opts.zoom != 1:
        zw, zh = opts.size[0] * opts.zoom, opts.size[1] * opts.zoom
        out = out.resize((zw, zh), Image.NEAREST)
    return out


# ---------- ramp preview --------------------------------------------------

def render_ramp(cell_px: int = 32) -> Image.Image:
    """Render TONE_TILES as a horizontal strip, like the inspo image."""
    cols = TONE_LEVELS
    pad = 2
    label_h = 12
    w = cols * cell_px + (cols + 1) * pad
    h = cell_px + label_h + pad * 2
    canvas = Image.new("L", (w, h), 128)
    for i, tile in enumerate(TONE_TILES):
        cell = Image.new("1", (4, 4))
        cpx = cell.load()
        for ty in range(4):
            for tx in range(4):
                cpx[tx, ty] = 1 if tile[ty * 4 + tx] else 0
        scaled = cell.resize((cell_px, cell_px), Image.NEAREST).convert("L")
        canvas.paste(scaled, (pad + i * (cell_px + pad), pad))
    return canvas.convert("1")


# ---------- CLI -----------------------------------------------------------

ALL_DITHERS = ["tone", "bayer4", "bayer8", "atkinson", "floyd", "threshold"]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("src", nargs="?", help="input image (any format Pillow reads)")
    p.add_argument("--out", default=None, help="output dir (default: alongside src, or cwd)")
    p.add_argument("--size", default="full",
                   help="full (400x240) | 16 | 32 | WxH (e.g. 200x120)")
    p.add_argument("--fit", default=None, choices=["cover", "contain", "stretch"],
                   help="cover (default for full/sprites, crops to fill) | contain (letterbox) | stretch")
    p.add_argument("--bg", type=int, default=0, choices=[0, 255],
                   help="letterbox fill for --fit contain (0=black, 255=white)")
    p.add_argument("--dither", default="tone", choices=ALL_DITHERS,
                   help="tone = clever 8-level tile dither (default); others = standard algorithms")
    p.add_argument("--threshold", type=int, default=128, help="only used with --dither threshold")
    p.add_argument("--brightness", type=float, default=1.0)
    p.add_argument("--contrast", type=float, default=1.0)
    p.add_argument("--gamma", type=float, default=1.0)
    p.add_argument("--invert", action="store_true")
    p.add_argument("--zoom", type=int, default=1, help="NEAREST upscale of output, for inspection")
    p.add_argument("--crop-x", type=float, default=0.5,
                   help="cover-mode horizontal crop anchor (0=left, 0.5=center, 1=right)")
    p.add_argument("--crop-y", type=float, default=0.5,
                   help="cover-mode vertical crop anchor (0=top, 0.5=center, 1=bottom). "
                        "Use 0.2-0.3 to keep mountains/sky.")
    p.add_argument("--compare", action="store_true",
                   help="emit one output per dither algorithm (and a stitched grid)")
    p.add_argument("--show-ramp", action="store_true",
                   help="emit the 9-level tone ramp itself (no src needed)")
    args = p.parse_args()

    out_dir = args.out or (os.path.dirname(args.src) if args.src else ".") or "."
    os.makedirs(out_dir, exist_ok=True)

    if args.show_ramp:
        ramp = render_ramp()
        path = os.path.join(out_dir, "tone_ramp.png")
        ramp.save(path)
        print(path)
        if not args.src:
            return

    if not args.src:
        p.error("src is required unless --show-ramp is used alone")

    size = parse_size(args.size)
    # Default fit: cover (best for both full-screen photos and centered sprites).
    fit = args.fit or "cover"

    src = Image.open(args.src)
    base = os.path.splitext(os.path.basename(args.src))[0]

    dithers = ALL_DITHERS if args.compare else [args.dither]
    outputs: list[tuple[str, Image.Image]] = []
    for d in dithers:
        opts = Options(
            size=size, fit=fit, dither=d, threshold=args.threshold,
            brightness=args.brightness, contrast=args.contrast, gamma=args.gamma,
            invert=args.invert, zoom=args.zoom, bg=args.bg,
            crop_x=args.crop_x, crop_y=args.crop_y,
        )
        img = pixelize(src, opts)
        suffix = f"_{d}" if args.compare else ""
        name = f"{base}{suffix}_{size[0]}x{size[1]}.png"
        path = os.path.join(out_dir, name)
        img.save(path)
        print(path)
        outputs.append((d, img))

    # Stitched grid for visual comparison.
    if args.compare and outputs:
        pad = 6
        label_h = 14
        cell_w, cell_h = outputs[0][1].size
        cols = min(3, len(outputs))
        rows = (len(outputs) + cols - 1) // cols
        gw = cols * cell_w + (cols + 1) * pad
        gh = rows * (cell_h + label_h) + (rows + 1) * pad
        grid = Image.new("L", (gw, gh), 200)
        try:
            from PIL import ImageDraw
            draw = ImageDraw.Draw(grid)
        except Exception:
            draw = None
        for i, (name, img) in enumerate(outputs):
            r = i // cols
            c = i % cols
            x = pad + c * (cell_w + pad)
            y = pad + r * (cell_h + label_h + pad)
            grid.paste(img.convert("L"), (x, y))
            if draw:
                draw.text((x + 2, y + cell_h), name, fill=0)
        gpath = os.path.join(out_dir, f"{base}_compare_{size[0]}x{size[1]}.png")
        grid.save(gpath)
        print(gpath)


if __name__ == "__main__":
    main()
