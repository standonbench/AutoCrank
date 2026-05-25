"""Stitch all 15 animal animations into one sheet (rows = animal, cols = frame)."""
import os
import sys
from PIL import Image, ImageDraw

root = sys.argv[1] if len(sys.argv) > 1 else "tools/test_output/final"
out_path = sys.argv[2] if len(sys.argv) > 2 else "tools/test_output/all_animals.png"
display_scale = int(sys.argv[3]) if len(sys.argv) > 3 else 3

animals = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
# Discover frame count and cell size from first animal's first frame.
first_animal = animals[0]
first_frames = sorted(f for f in os.listdir(os.path.join(root, first_animal)) if f.endswith(".png"))
sample = Image.open(os.path.join(root, first_animal, first_frames[0]))
cw, ch = sample.size
cell_w, cell_h = cw * display_scale, ch * display_scale

pad = 8
label_w = 130
frame_count = len(first_frames)

sheet_w = label_w + pad + frame_count * (cell_w + pad)
sheet_h = pad + len(animals) * (cell_h + pad)
sheet = Image.new("RGB", (sheet_w, sheet_h), (40, 40, 40))
draw = ImageDraw.Draw(sheet)

for row, name in enumerate(animals):
    y = pad + row * (cell_h + pad)
    draw.text((6, y + cell_h // 2 - 6), name, fill=(255, 255, 255))
    frames = sorted(f for f in os.listdir(os.path.join(root, name)) if f.endswith(".png"))
    for col, fname in enumerate(frames):
        img = Image.open(os.path.join(root, name, fname)).convert("RGBA")
        bg = Image.new("RGBA", img.size, (120, 120, 120, 255))
        bg.alpha_composite(img)
        big = bg.resize((cell_w, cell_h), Image.NEAREST)
        sheet.paste(big, (label_w + pad + col * (cell_w + pad), y))

sheet.save(out_path)
print(out_path)
