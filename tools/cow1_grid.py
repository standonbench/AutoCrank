from PIL import Image, ImageDraw, ImageFont

COLS = [
    ("source", "/Users/Pierre/Downloads/animals/cow-1/0.png"),
    ("thr 100", "tools/out/cow1_tune/thr100.png"),
    ("thr 128", "tools/out/cow1_tune/thr128.png"),
    ("thr 150", "tools/out/cow1_tune/thr150.png"),
    ("thr 170", "tools/out/cow1_tune/thr170.png"),
    ("thr 190", "tools/out/cow1_tune/thr190.png"),
    ("atkinson", "tools/out/cow1_tune/atkinson.png"),
    ("bayer8", "tools/out/cow1_tune/bayer8.png"),
]
CELL = 160
LABEL_H = 22

font = ImageFont.load_default()
W = len(COLS) * CELL
H = LABEL_H + CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

def composite(p):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((CELL, CELL), Image.NEAREST)

for ci, (label, path) in enumerate(COLS):
    draw.text((ci * CELL + 6, 5), label, fill=(0, 0, 0), font=font)
    sheet.paste(composite(path), (ci * CELL, LABEL_H))

sheet.save("tools/out/cow1_tune/grid.png")
print(sheet.size)
