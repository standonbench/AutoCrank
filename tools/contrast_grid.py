"""Grid: rows = animals, cols = source + 4 contrast levels of 1-bit output."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["squirrel", "dog-0", "rabbit", "alpaca", "deer", "cow-1", "hedgehog", "bear"]
CONTRASTS = [1.0, 1.5, 2.0, 3.0]

CELL = 128
LABEL_H = 22
LEFT = 78
font = ImageFont.load_default()

cols = 1 + len(CONTRASTS)
W = LEFT + cols * CELL
H = LABEL_H + len(ANIMALS) * CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

draw.text((LEFT + 8, 5), "source", fill=(0, 0, 0), font=font)
for i, c in enumerate(CONTRASTS):
    draw.text((LEFT + (i + 1) * CELL + 6, 5),
              f"contrast {c:.1f}{'  (current)' if c == 1.0 else ''}",
              fill=(0, 0, 0), font=font)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

for ri, a in enumerate(ANIMALS):
    draw.text((4, LABEL_H + ri * CELL + CELL // 2 - 6), a,
              fill=(0, 0, 0), font=font)
    sheet.paste(composite(f"/Users/Pierre/Downloads/animals/{a}/0.png"),
                (LEFT, LABEL_H + ri * CELL))
    for ci, c in enumerate(CONTRASTS):
        sheet.paste(composite(f"tools/out/contrast/{a}_c{c:.1f}.png"),
                    (LEFT + (ci + 1) * CELL, LABEL_H + ri * CELL))

sheet.save("tools/out/contrast/grid.png")
print(sheet.size)
