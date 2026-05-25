"""Show installed pet sprites with their new names side-by-side with source."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

MAPPING = [
    (0,  "Squirrel",  "squirrel"),
    (1,  "Pup",       "dog-0"),
    (2,  "Hen",       "chicken"),
    (3,  "Hedgehog",  "hedgehog"),
    (4,  "Rabbit",    "rabbit"),
    (5,  "Calf",      "cow-0"),
    (6,  "Sheep",     "sheep"),
    (7,  "Goat",      "goat"),
    (8,  "Haflinger", "horse"),
    (9,  "Donkey",    "donkey"),
    (10, "Alligator", "alligator"),
    (11, "Duck",      "duck"),
    (12, "Alpaca",    "alpaca"),
    (13, "Highland",  "cow-1"),
    (14, "Boar",      "pig"),
    (15, "Deer",      "deer"),
    (16, "Fox",       "fox"),
    (17, "Bear",      "bear"),
]

CELL = 96
LABEL_H = 28
COLS = 3
ROW_H = LABEL_H + CELL
N = len(MAPPING)
ROWS = (N + COLS - 1) // COLS

font = ImageFont.load_default()
W = COLS * (CELL * 2 + 20) + 20
H = ROWS * ROW_H + 20

sheet = Image.new("RGB", (W, H), (245, 245, 245))
draw = ImageDraw.Draw(sheet)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

placeholder = Image.new("RGB", (CELL, CELL), (255, 255, 255))
pd_draw = ImageDraw.Draw(placeholder)
pd_draw.text((CELL // 2 - 30, CELL // 2 - 6), "procedural", fill=(120, 120, 120), font=font)

for i, (pid, name, animal) in enumerate(MAPPING):
    r = i // COLS
    c = i % COLS
    x = 10 + c * (CELL * 2 + 20)
    y = 10 + r * ROW_H
    label = f"#{pid:02d}  {name}"
    if animal != "(procedural)":
        label += f"  (from {animal})"
    draw.text((x, y), label, fill=(0, 0, 0), font=font)

    src_path = Path(f"/Users/Pierre/Downloads/animals/{animal}/0.png")
    dst_path = Path(f"Source/images/pets/pet_{pid:02d}.png")

    if src_path.exists():
        sheet.paste(composite(src_path), (x, y + LABEL_H))
    else:
        sheet.paste(placeholder, (x, y + LABEL_H))

    if dst_path.exists():
        sheet.paste(composite(dst_path), (x + CELL + 6, y + LABEL_H))
    else:
        sheet.paste(placeholder, (x + CELL + 6, y + LABEL_H))

sheet.save("tools/out/installed_grid.png")
print(sheet.size)
