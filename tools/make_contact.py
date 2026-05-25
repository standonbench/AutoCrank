"""Build a contact sheet: rows = animals, cols = dither modes."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["cow-0", "cat", "fox", "pig", "penguin", "elephant"]
DITHERS = ["threshold", "atkinson", "bayer8", "none"]
CELL = 128       # 32 * 4 upscale
LABEL_H = 18
LEFT = 70        # label column for animal name

font = ImageFont.load_default()

cols = len(DITHERS)
rows = len(ANIMALS)
W = LEFT + cols * CELL
H = LABEL_H + rows * CELL

sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

for ci, d in enumerate(DITHERS):
    x = LEFT + ci * CELL + CELL // 2 - 20
    draw.text((x, 3), d, fill=(0, 0, 0), font=font)

for ri, a in enumerate(ANIMALS):
    draw.text((4, LABEL_H + ri * CELL + CELL // 2 - 6), a,
              fill=(0, 0, 0), font=font)
    for ci, d in enumerate(DITHERS):
        p = Path(f"tools/out/animals/{a}/0_{d}.png")
        if not p.exists():
            continue
        im = Image.open(p).convert("RGBA")
        # composite on white so transparency is visible
        bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
        bg.paste(im, (0, 0), im)
        rgb = bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)
        sheet.paste(rgb, (LEFT + ci * CELL, LABEL_H + ri * CELL))

sheet.save("tools/out/animals/contact.png")
print("wrote tools/out/animals/contact.png")
