from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["fox", "penguin", "cow-1", "pig", "tiger", "rabbit"]
CELL = 160
LABEL_H = 18
GAP = 6

font = ImageFont.load_default()
W = 60 + 2 * CELL + GAP
H = LABEL_H + len(ANIMALS) * (CELL + 8)
sheet = Image.new("RGB", (W, H), (245, 245, 245))
draw = ImageDraw.Draw(sheet)
draw.text((60 + 4, 2), "frame 0", fill=(0, 0, 0), font=font)
draw.text((60 + CELL + GAP + 4, 2), "frame 1", fill=(0, 0, 0), font=font)

for i, a in enumerate(ANIMALS):
    y = LABEL_H + i * (CELL + 8)
    draw.text((4, y + CELL // 2 - 6), a, fill=(0, 0, 0), font=font)
    f0 = Image.open(f"tools/out/frames/{a}_0.png").resize((CELL, CELL))
    f1 = Image.open(f"tools/out/frames/{a}_1.png").resize((CELL, CELL))
    sheet.paste(f0, (60, y))
    sheet.paste(f1, (60 + CELL + GAP, y))

sheet.save("tools/out/frames/grid.png")
print(sheet.size)
