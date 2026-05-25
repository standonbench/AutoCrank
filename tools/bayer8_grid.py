"""Final grid: all 32 animals, source on left, bayer8 1-bit on right, in 4 columns."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SRC_DIR = Path("/Users/Pierre/Downloads/animals")
OUT_DIR = Path("tools/out/bayer8_all")

animals = sorted([p.name for p in SRC_DIR.iterdir()
                  if p.is_dir() and not p.name.startswith(".")])

CELL = 96            # 32 * 3
LABEL_H = 14
PAIR_GAP = 4
GROUP_W = CELL * 2 + PAIR_GAP
COLS = 4             # 4 pairs per row
N = len(animals)
ROWS = (N + COLS - 1) // COLS

font = ImageFont.load_default()
W = COLS * GROUP_W + (COLS - 1) * 20 + 20
H = ROWS * (CELL + LABEL_H) + 20

sheet = Image.new("RGB", (W, H), (245, 245, 245))
draw = ImageDraw.Draw(sheet)

def composite(p, cell):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((cell, cell), Image.NEAREST)

for i, name in enumerate(animals):
    r = i // COLS
    c = i % COLS
    x = 10 + c * (GROUP_W + 20)
    y = 10 + r * (CELL + LABEL_H)
    draw.text((x + 4, y), name, fill=(0, 0, 0), font=font)
    sheet.paste(composite(SRC_DIR / name / "0.png", CELL), (x, y + LABEL_H))
    sheet.paste(composite(OUT_DIR / f"{name}.png", CELL), (x + CELL + PAIR_GAP, y + LABEL_H))

sheet.save("tools/out/bayer8_all/grid.png")
print(sheet.size, "->", "tools/out/bayer8_all/grid.png")
