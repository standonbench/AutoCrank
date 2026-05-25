"""Stitch the full sprite-sheet outputs into one labelled column."""
import os
import sys
from PIL import Image, ImageDraw

algos = ["threshold", "bayer4", "bayer8", "atkinson"]
src_dir = sys.argv[1] if len(sys.argv) > 1 else "tools/test_output/sheet"
out_path = sys.argv[2] if len(sys.argv) > 2 else "tools/test_output/sheet_compare.png"
scale = 4
pad = 8
label_w = 70

first = Image.open(os.path.join(src_dir, f"Basic Sprites 1x_{algos[0]}.png"))
w, h = first.size
cw, ch = w * scale, h * scale

sheet = Image.new("RGB", (label_w + pad + cw + pad, pad + len(algos) * (ch + pad)), (40, 40, 40))
draw = ImageDraw.Draw(sheet)

for row, algo in enumerate(algos):
    y = pad + row * (ch + pad)
    draw.text((4, y + ch // 2 - 6), algo, fill=(255, 255, 255))
    img = Image.open(os.path.join(src_dir, f"Basic Sprites 1x_{algo}.png")).convert("RGBA")
    bg = Image.new("RGBA", img.size, (120, 120, 120, 255))
    bg.alpha_composite(img)
    big = bg.resize((cw, ch), Image.NEAREST)
    sheet.paste(big, (label_w + pad, y))

sheet.save(out_path)
print(out_path)
