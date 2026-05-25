"""Grid: rows = animals, cols = methods. Source + 5 methods."""
from PIL import Image, ImageDraw, ImageFont

ANIMALS = ["penguin", "fox", "cow-0", "pig"]
METHODS = [
    ("source", None),
    ("threshold 128", "thr128.png"),
    ("atkinson", "atkinson.png"),
    ("bayer8", "bayer8.png"),
    ("atkinson +rare", "atkinson_rare.png"),
    ("threshold 200", "thr200.png"),
]
CELL = 160
LABEL_H = 22
LEFT = 78

font = ImageFont.load_default()
cols = len(METHODS)
rows = len(ANIMALS)
W = LEFT + cols * CELL
H = LABEL_H + rows * CELL
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

for ci, (h, _) in enumerate(METHODS):
    draw.text((LEFT + ci * CELL + 6, 5), h, fill=(0, 0, 0), font=font)

def composite(p, cell):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((cell, cell), Image.NEAREST)

for ri, a in enumerate(ANIMALS):
    draw.text((4, LABEL_H + ri * CELL + CELL // 2 - 6), a,
              fill=(0, 0, 0), font=font)
    for ci, (_, fname) in enumerate(METHODS):
        if fname is None:
            p = f"/Users/Pierre/Downloads/animals/{a}/0.png"
        else:
            p = f"tools/out/methods/{a}/{fname}"
        sheet.paste(composite(p, CELL),
                    (LEFT + ci * CELL, LABEL_H + ri * CELL))

sheet.save("tools/out/methods/grid.png")
print("wrote tools/out/methods/grid.png", sheet.size)
