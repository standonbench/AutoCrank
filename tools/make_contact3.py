"""Triptych: source / threshold=128+atkinson+rare / threshold=200 silhouette."""
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["cow-0", "cat", "fox", "pig", "penguin", "elephant"]
CELL = 192
LABEL_H = 18
LEFT = 70

font = ImageFont.load_default()
cols = 3
rows = len(ANIMALS)
W = LEFT + cols * CELL
H = LABEL_H + rows * CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

headers = ["source", "atkinson + rare", "silhouette (thr 200)"]
for ci, h in enumerate(headers):
    draw.text((LEFT + ci * CELL + 8, 3), h, fill=(0, 0, 0), font=font)

def composite_to(p, cell):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((cell, cell), Image.NEAREST)

for ri, a in enumerate(ANIMALS):
    draw.text((4, LABEL_H + ri * CELL + CELL // 2 - 6), a,
              fill=(0, 0, 0), font=font)
    sheet.paste(composite_to(f"/Users/Pierre/Downloads/animals/{a}/0.png", CELL),
                (LEFT, LABEL_H + ri * CELL))
    sheet.paste(composite_to(f"tools/out/animals3/{a}/0_1bit.png", CELL),
                (LEFT + CELL, LABEL_H + ri * CELL))
    sheet.paste(composite_to(f"tools/out/animals2/{a}/0_1bit.png", CELL),
                (LEFT + 2 * CELL, LABEL_H + ri * CELL))

sheet.save("tools/out/animals3/contact.png")
print("wrote tools/out/animals3/contact.png")
