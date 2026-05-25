"""Side-by-side: source colored sprite vs threshold=200 1-bit."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["cow-0", "cat", "fox", "pig", "penguin", "elephant"]
CELL = 192
LABEL_H = 18
LEFT = 70

font = ImageFont.load_default()
cols = 2
rows = len(ANIMALS)
W = LEFT + cols * CELL
H = LABEL_H + rows * CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

for ci, h in enumerate(["source (32x32)", "threshold=200"]):
    draw.text((LEFT + ci * CELL + 8, 3), h, fill=(0, 0, 0), font=font)

for ri, a in enumerate(ANIMALS):
    draw.text((4, LABEL_H + ri * CELL + CELL // 2 - 6), a,
              fill=(0, 0, 0), font=font)
    src = Image.open(f"/Users/Pierre/Downloads/animals/{a}/0.png").convert("RGBA")
    bg = Image.new("RGBA", src.size, (255, 255, 255, 255))
    bg.paste(src, (0, 0), src)
    src_big = bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)
    sheet.paste(src_big, (LEFT, LABEL_H + ri * CELL))

    one = Image.open(f"tools/out/animals2/{a}/0_1bit.png").convert("RGBA")
    bg2 = Image.new("RGBA", one.size, (255, 255, 255, 255))
    bg2.paste(one, (0, 0), one)
    one_big = bg2.convert("RGB").resize((CELL, CELL), Image.NEAREST)
    sheet.paste(one_big, (LEFT + CELL, LABEL_H + ri * CELL))

sheet.save("tools/out/animals2/contact.png")
print("wrote tools/out/animals2/contact.png")
