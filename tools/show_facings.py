"""Display source sprites at 6x zoom so we can see facing direction."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["squirrel", "dog-0", "chicken", "hedgehog", "rabbit", "cow-0",
           "sheep", "goat", "horse", "donkey", "alligator", "duck",
           "alpaca", "cow-1", "pig", "deer", "bear"]

CELL = 160
LABEL_H = 18
COLS = 4
ROWS = (len(ANIMALS) + COLS - 1) // COLS
font = ImageFont.load_default()
W = COLS * (CELL + 6) + 10
H = ROWS * (CELL + LABEL_H) + 10
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

for i, a in enumerate(ANIMALS):
    r = i // COLS
    c = i % COLS
    x = 5 + c * (CELL + 6)
    y = 5 + r * (CELL + LABEL_H)
    draw.text((x + 4, y), a, fill=(0, 0, 0), font=font)
    sheet.paste(composite(f"/Users/Pierre/Downloads/animals/{a}/0.png"),
                (x, y + LABEL_H))

sheet.save("tools/out/facings.png")
print(sheet.size)
