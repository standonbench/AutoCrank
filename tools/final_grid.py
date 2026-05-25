from PIL import Image, ImageDraw, ImageFont

ANIMALS = [("penguin", 128), ("fox", 128), ("cow-1", 128), ("pig", 140)]
CELL = 192
LABEL_H = 22

font = ImageFont.load_default()
W = len(ANIMALS) * 2 * CELL
H = LABEL_H + CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

for i, (a, t) in enumerate(ANIMALS):
    x0 = i * 2 * CELL
    draw.text((x0 + 6, 5), f"{a} (source)", fill=(0, 0, 0), font=font)
    draw.text((x0 + CELL + 6, 5), f"{a} 1-bit thr{t}", fill=(0, 0, 0), font=font)
    sheet.paste(composite(f"/Users/Pierre/Downloads/animals/{a}/0.png"), (x0, LABEL_H))
    sheet.paste(composite(f"tools/out/final/{a}.png"), (x0 + CELL, LABEL_H))

sheet.save("tools/out/final/grid.png")
print(sheet.size)
