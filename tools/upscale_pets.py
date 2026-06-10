"""Scale2x (EPX) upscale of the 32x32 pet sprites to 64x64.

Reads Source/images/pets/pet_NN.png, classifies every pixel as
transparent / black / white, runs Scale2x on the class grid — staircased
outlines smooth out while checkerboard dither interiors are preserved
verbatim (Scale2x leaves them untouched by design) — and writes
Source/images/pets/pet64_NN.png as RGBA with pure B/W + alpha, the same
convention to_1bit.py emits, so `pdc` keeps the transparency mask.

Also writes a contact sheet to tools/out/pet64_contact.png: for each pet,
the original nearest-doubled (left) next to the Scale2x result (right).
Eyeball it before shipping; delete the pet64_*.png files to revert (the
loader falls back to the 32px originals).

Usage: python3.11 tools/upscale_pets.py
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PETS_DIR = ROOT / "Source/images/pets"
OUT_DIR = ROOT / "tools/out"

TRANS, BLACK, WHITE = 0, 1, 2


def classify(img: Image.Image):
    """RGBA image -> 2D grid of TRANS/BLACK/WHITE classes."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    grid = [[TRANS] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 128:
                grid[y][x] = TRANS
            else:
                grid[y][x] = BLACK if (r + g + b) // 3 < 128 else WHITE
    return grid


def scale2x(grid):
    """Classic Scale2x/EPX on the class grid."""
    h, w = len(grid), len(grid[0])
    out = [[TRANS] * (w * 2) for _ in range(h * 2)]
    for y in range(h):
        for x in range(w):
            p = grid[y][x]
            a = grid[y - 1][x] if y > 0 else p          # above
            d = grid[y + 1][x] if y < h - 1 else p      # below
            c = grid[y][x - 1] if x > 0 else p          # left
            b = grid[y][x + 1] if x < w - 1 else p      # right
            e0 = e1 = e2 = e3 = p
            if c == a and c != d and a != b:
                e0 = a
            if a == b and a != c and b != d:
                e1 = b
            if d == c and d != b and c != a:
                e2 = c
            if b == d and b != a and d != c:
                e3 = b
            out[y * 2][x * 2] = e0
            out[y * 2][x * 2 + 1] = e1
            out[y * 2 + 1][x * 2] = e2
            out[y * 2 + 1][x * 2 + 1] = e3
    return out


def to_image(grid) -> Image.Image:
    h, w = len(grid), len(grid[0])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for y in range(h):
        for x in range(w):
            if grid[y][x] == BLACK:
                px[x, y] = (0, 0, 0, 255)
            elif grid[y][x] == WHITE:
                px[x, y] = (255, 255, 255, 255)
    return img


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    sources = sorted(PETS_DIR.glob("pet_[0-9][0-9].png"))
    if not sources:
        raise SystemExit(f"no pet_NN.png sprites found in {PETS_DIR}")

    cell, pad = 64, 6
    cols = 6  # pets per row; each pet takes two cells (before | after)
    rows = (len(sources) + cols - 1) // cols
    sheet = Image.new("RGB",
                      (cols * (cell * 2 + pad * 3),
                       rows * (cell + pad * 2)),
                      (128, 128, 128))

    for i, src_path in enumerate(sources):
        src = Image.open(src_path)
        big = to_image(scale2x(classify(src)))
        dst = PETS_DIR / f"pet64_{src_path.stem.split('_')[1]}.png"
        big.save(dst)

        nearest = src.convert("RGBA").resize((cell, cell), Image.NEAREST)
        cx = (i % cols) * (cell * 2 + pad * 3) + pad
        cy = (i // cols) * (cell + pad * 2) + pad
        sheet.paste(nearest, (cx, cy), nearest)
        sheet.paste(big, (cx + cell + pad, cy), big)
        print(f"{src_path.name} -> {dst.name}")

    sheet_path = OUT_DIR / "pet64_contact.png"
    sheet.save(sheet_path)
    print(f"contact sheet: {sheet_path}")


if __name__ == "__main__":
    main()
