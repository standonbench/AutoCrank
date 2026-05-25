"""Convert a high-contrast subject image (silhouette, line art, or B&W illustration
on transparent/uniform background) into a clean 1-bit pixel-art sprite.

Pipeline:
  1. Composite source onto white (so transparent -> white, "outside subject").
  2. Convert to luma; use alpha (or luma != white) to find subject bbox.
  3. Tight-crop, then square-pad with white.
  4. Lanczos downscale to N x N.
  5. Threshold luma to 1-bit: dark -> black opaque, light -> transparent.
     White interior regions (spots, highlights) are preserved as transparent.
  6. Save as RGBA. Also save an opaque-white preview for inline viewing.

Usage:
  python tools/sprite_from_silhouette.py in.png --size 32 --out out.png
"""
from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image


def sprite_from_image(src: Path, size: int, out: Path,
                      threshold: int = 128, zoom: int = 8) -> None:
    im = Image.open(src).convert("RGBA")

    # 1. Composite on white so transparency becomes "outside subject" = white.
    white = Image.new("RGBA", im.size, (255, 255, 255, 255))
    composed = Image.alpha_composite(white, im).convert("RGB")

    # 2. Subject bbox: prefer source alpha; fall back to "not pure white".
    alpha = im.split()[-1]
    if alpha.getbbox() is not None and alpha.getextrema() != (255, 255):
        bbox = alpha.getbbox()
    else:
        # Find bbox where luma is not max.
        gray = composed.convert("L")
        mask = gray.point(lambda v: 255 if v < 250 else 0, mode="L")
        bbox = mask.getbbox()
        if bbox is None:
            raise SystemExit("empty image — no subject found")

    cropped = composed.crop(bbox)

    # 3. Square-pad with white so aspect ratio survives downscale.
    w, h = cropped.size
    side = max(w, h)
    sq = Image.new("RGB", (side, side), (255, 255, 255))
    sq.paste(cropped, ((side - w) // 2, (side - h) // 2))

    # 4. Lanczos downscale.
    small = sq.resize((size, size), Image.LANCZOS).convert("L")

    # 5. Threshold to 1-bit. Dark -> black opaque, light -> transparent.
    rgba = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = small.load()
    op = rgba.load()
    for y in range(size):
        for x in range(size):
            if px[x, y] < threshold:
                op[x, y] = (0, 0, 0, 255)

    rgba.save(out)
    print(f"wrote {out}")

    if zoom > 1:
        big = rgba.resize((size * zoom, size * zoom), Image.NEAREST)
        bg = Image.new("RGBA", big.size, (255, 255, 255, 255))
        bg.paste(big, (0, 0), big)
        zp = out.with_name(out.stem + f"_{zoom}x" + out.suffix)
        bg.convert("RGB").save(zp)
        print(f"wrote {zp}")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("src", type=Path)
    p.add_argument("--size", type=int, default=32)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--threshold", type=int, default=128,
                   help="0-255: luma cutoff for black vs transparent")
    p.add_argument("--zoom", type=int, default=8)
    a = p.parse_args()
    sprite_from_image(a.src, a.size, a.out, a.threshold, a.zoom)


if __name__ == "__main__":
    main()
