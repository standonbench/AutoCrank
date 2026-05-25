"""Convert colored pixel-art sprites to Playdate-friendly 1-bit PNGs.

Pipeline per pixel:
  alpha < threshold     -> transparent
  else                  -> luma -> dither (none | threshold | bayer4 | bayer8 | atkinson)
                                -> black or white, fully opaque

Outputs an indexed PNG (mode "1" + transparency) so `pdc` keeps the alpha mask.

Examples
--------
# Convert a single strip / sprite, write next to it with _1bit suffix:
python tools/to_1bit.py "Basic Asset Pack 3/Basic Animal Animations/Meowing Cat/MeowingCat.png"

# Pick a different dither and dump all algorithms side-by-side for comparison:
python tools/to_1bit.py path/to/foo.png --compare --out tools/test_output

# Slice a sheet into frames first (useful for the 64x16 animation strips):
python tools/to_1bit.py path/to/strip.png --slice 16x16 --out tools/test_output/frames
"""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass
from typing import Iterable

from PIL import Image


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


def _bayer_threshold(matrix, size, luma, x, y):
    # Map 0..size-1 cell value to 0..255, plus half a step so the midtone
    # (luma == cell threshold) still rounds toward white half the time.
    cell = matrix[y % size][x % size]
    return int((cell + 0.5) * 255 / (size * size))


# ---------- core conversion -----------------------------------------------

@dataclass
class Options:
    dither: str = "bayer4"   # none | threshold | bayer4 | bayer8 | atkinson
    threshold: int = 128     # for "threshold" mode
    alpha_cutoff: int = 128  # below this -> transparent
    invert: bool = False     # swap black <-> white
    # Feature preservation — overrides applied AFTER dithering so they always win.
    preserve_rare: int = 0   # colors used in <= N pixels are forced to black (0 = off)
    force_black: list = None # list of (r,g,b) tuples that always map to black
    force_white: list = None # list of (r,g,b) tuples that always map to white
    upscale: int = 1         # integer scale factor applied before dithering
    upscale_mode: str = "lanczos"  # nearest | lanczos


def to_1bit(src: Image.Image, opts: Options) -> Image.Image:
    """Return a mode-"1" image with a transparency mask matching src.alpha."""
    src = src.convert("RGBA")

    # Build feature-preservation mask from the ORIGINAL palette (before upscale),
    # otherwise Lanczos blending creates hundreds of in-between colors that all
    # match "rare" by accident and the whole sprite turns black.
    force_black_colors = set(tuple(c) for c in (opts.force_black or []))
    force_white_colors = set(tuple(c) for c in (opts.force_white or []))
    if opts.preserve_rare > 0:
        from collections import Counter
        counts = Counter()
        opx = src.load()
        for y in range(src.size[1]):
            for x in range(src.size[0]):
                r, g, b, a = opx[x, y]
                if a >= opts.alpha_cutoff:
                    counts[(r, g, b)] += 1
        for col, n in counts.items():
            if n <= opts.preserve_rare:
                force_black_colors.add(col)

    # Rewrite force-black/force-white pixels in the SOURCE before upscale.
    # This way the smooth Lanczos resampler blends them with neighbors and
    # the dither produces naturally-textured edges, instead of leaving a
    # chunky NEAREST mask block stamped on top of the dithered output.
    if force_black_colors or force_white_colors:
        opx = src.load()
        for y in range(src.size[1]):
            for x in range(src.size[0]):
                r, g, b, a = opx[x, y]
                if a < opts.alpha_cutoff:
                    continue
                c = (r, g, b)
                if c in force_black_colors:
                    opx[x, y] = (0, 0, 0, a)
                elif c in force_white_colors:
                    opx[x, y] = (255, 255, 255, a)

    if opts.upscale != 1:
        new_size = (src.size[0] * opts.upscale, src.size[1] * opts.upscale)
        resample = Image.NEAREST if opts.upscale_mode == "nearest" else Image.LANCZOS
        src = src.resize(new_size, resample)
    w, h = src.size
    rgba = src.load()

    # Luma in linear-ish space (Rec.601); good enough for pixel art.
    luma = Image.new("L", (w, h))
    lpx = luma.load()
    mask = Image.new("L", (w, h))
    mpx = mask.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = rgba[x, y]
            lpx[x, y] = (r * 77 + g * 150 + b * 29) >> 8
            mpx[x, y] = 255 if a >= opts.alpha_cutoff else 0

    if opts.dither == "atkinson":
        out = _atkinson(luma)
    elif opts.dither == "none" or opts.dither == "threshold":
        out = luma.point(lambda v, t=opts.threshold: 255 if v >= t else 0, mode="1")
    elif opts.dither in ("bayer4", "bayer8"):
        matrix = BAYER4 if opts.dither == "bayer4" else BAYER8
        size = 4 if opts.dither == "bayer4" else 8
        out = Image.new("1", (w, h))
        opx = out.load()
        lpx = luma.load()
        for y in range(h):
            for x in range(w):
                t = _bayer_threshold(matrix, size, lpx[x, y], x, y)
                opx[x, y] = 1 if lpx[x, y] >= t else 0
    else:
        raise ValueError(f"unknown dither {opts.dither!r}")

    if opts.invert:
        out = out.point(lambda v: 0 if v else 255, mode="1")

    # Glue the alpha mask back on.  Mode "1" doesn't natively carry alpha, so
    # save as palette + transparency via PNG: keep two images, the caller will
    # composite at save time.
    out.info["mask"] = mask
    return out


def _atkinson(luma: Image.Image) -> Image.Image:
    """Atkinson dither — sparser than Floyd-Steinberg, looks great on Playdate."""
    w, h = luma.size
    src = [list(luma.getdata())[i * w:(i + 1) * w] for i in range(h)]
    out = Image.new("1", (w, h))
    opx = out.load()
    # Atkinson distributes 6/8 of the error (the other 2/8 vanishes — that's
    # the look).  Offsets relative to current pixel:
    offsets = [(1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)]
    for y in range(h):
        for x in range(w):
            v = max(0, min(255, src[y][x]))
            new = 255 if v >= 128 else 0
            opx[x, y] = 1 if new else 0
            err = (v - new) // 8
            for dx, dy in offsets:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    src[ny][nx] += err
    return out


def save_with_mask(img: Image.Image, path: str) -> None:
    """Save a mode-1 image with its transparency mask (attached as img.info['mask'])."""
    mask = img.info.get("mask")
    if mask is None:
        img.save(path)
        return
    # Promote to LA so PNG keeps the alpha channel; pdc handles indexed+alpha fine.
    rgba = Image.new("LA", img.size, (0, 0))
    bits = img.convert("L")
    rgba.putalpha(mask)
    # Compose: where mask says opaque, copy the 1-bit value; else stay transparent black.
    bpx = bits.load()
    rpx = rgba.load()
    mpx = mask.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            if mpx[x, y]:
                rpx[x, y] = (bpx[x, y], 255)
            else:
                rpx[x, y] = (0, 0)
    rgba.save(path)


# ---------- sheet slicing -------------------------------------------------

def slice_sheet(src: Image.Image, cell_w: int, cell_h: int) -> Iterable[tuple[int, int, Image.Image]]:
    w, h = src.size
    if w % cell_w or h % cell_h:
        print(f"warn: sheet {w}x{h} doesn't divide evenly by {cell_w}x{cell_h}", file=sys.stderr)
    cols = w // cell_w
    rows = h // cell_h
    for ry in range(rows):
        for cx in range(cols):
            box = (cx * cell_w, ry * cell_h, (cx + 1) * cell_w, (ry + 1) * cell_h)
            yield cx, ry, src.crop(box)


# ---------- CLI -----------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("src", help="input PNG (single sprite or sheet)")
    p.add_argument("--out", default=None, help="output dir (default: alongside src)")
    p.add_argument("--dither", default="bayer4",
                   choices=["none", "threshold", "bayer4", "bayer8", "atkinson"])
    p.add_argument("--threshold", type=int, default=128)
    p.add_argument("--alpha-cutoff", type=int, default=128)
    p.add_argument("--invert", action="store_true")
    p.add_argument("--preserve-rare", type=int, default=0, metavar="N",
                   help="force any color used in <=N pixels to black (catches eyes/markings). "
                        "Try 4 for 16x16 sprites.")
    p.add_argument("--force-black", action="append", default=[], metavar="R,G,B",
                   help="repeatable: force this exact source color to black")
    p.add_argument("--force-white", action="append", default=[], metavar="R,G,B",
                   help="repeatable: force this exact source color to white")
    p.add_argument("--upscale", type=int, default=1, metavar="N",
                   help="upscale source by NxN before dithering (try 2 or 4)")
    p.add_argument("--upscale-mode", default="lanczos", choices=["nearest", "lanczos"],
                   help="lanczos = smooth gradients (recommended), nearest = blocky")
    p.add_argument("--slice", default=None,
                   help="slice into WxH cells before converting, e.g. 16x16")
    p.add_argument("--compare", action="store_true",
                   help="emit one output per dither algorithm for visual comparison")
    args = p.parse_args()

    src = Image.open(args.src)
    base = os.path.splitext(os.path.basename(args.src))[0]
    out_dir = args.out or os.path.dirname(args.src) or "."
    os.makedirs(out_dir, exist_ok=True)

    if args.slice:
        cw, ch = (int(x) for x in args.slice.lower().split("x"))
        frames = list(slice_sheet(src, cw, ch))
    else:
        frames = [(0, 0, src)]

    modes = (["none", "threshold", "bayer4", "bayer8", "atkinson"]
             if args.compare else [args.dither])

    def parse_rgb(s):
        return tuple(int(x) for x in s.split(","))

    force_black = [parse_rgb(s) for s in args.force_black]
    force_white = [parse_rgb(s) for s in args.force_white]

    for mode in modes:
        opts = Options(dither=mode, threshold=args.threshold,
                       alpha_cutoff=args.alpha_cutoff, invert=args.invert,
                       preserve_rare=args.preserve_rare,
                       force_black=force_black, force_white=force_white,
                       upscale=args.upscale, upscale_mode=args.upscale_mode)
        for cx, ry, frame in frames:
            converted = to_1bit(frame, opts)
            if len(frames) > 1:
                name = f"{base}_{ry}_{cx}_{mode}.png" if args.compare else f"{base}_{ry}_{cx}.png"
            else:
                name = f"{base}_{mode}.png" if args.compare else f"{base}_1bit.png"
            path = os.path.join(out_dir, name)
            save_with_mask(converted, path)
            print(path)


if __name__ == "__main__":
    main()
