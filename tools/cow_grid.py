from PIL import Image, ImageDraw, ImageFont

THRS = [128, 150, 170, 180, 190, 200]
CELL = 192
LABEL_H = 22
LEFT = 0

font = ImageFont.load_default()
cols = len(THRS) + 1
W = LEFT + cols * CELL
H = LABEL_H + CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

draw.text((LEFT + 8, 5), "source", fill=(0, 0, 0), font=font)
sheet.paste(composite("/Users/Pierre/Downloads/animals/cow-0/0.png"),
            (LEFT, LABEL_H))
for ci, t in enumerate(THRS):
    x = LEFT + (ci + 1) * CELL
    draw.text((x + 8, 5), f"thr {t}", fill=(0, 0, 0), font=font)
    sheet.paste(composite(f"tools/out/cow_tune/thr{t}.png"), (x, LABEL_H))

sheet.save("tools/out/cow_tune/grid.png")
print("wrote tools/out/cow_tune/grid.png", sheet.size)
