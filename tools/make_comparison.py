"""Stitch a comparison grid for the dither outputs.

Usage:
  python tools/make_comparison.py tools/test_output/cat MeowingCat 4 \
      tools/test_output/cat_compare.png
"""
import os
import sys
from PIL import Image, ImageDraw

src_dir, base, frame_count, out_path = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
display_scale = int(sys.argv[5]) if len(sys.argv) > 5 else 6
algos = ["threshold", "bayer4", "bayer8", "atkinson"]
scale = display_scale
pad = 6
label_w = 70

# Load all frames, find cell size from the first one.
first = Image.open(os.path.join(src_dir, f"{base}_0_0_{algos[0]}.png"))
cw, ch = first.size
cell_w, cell_h = cw * scale, ch * scale

sheet_w = label_w + pad + frame_count * (cell_w + pad)
sheet_h = pad + len(algos) * (cell_h + pad)
# White-on-black so dither pattern is clearly visible on transparent regions.
sheet = Image.new("RGB", (sheet_w, sheet_h), (40, 40, 40))
draw = ImageDraw.Draw(sheet)

for row, algo in enumerate(algos):
    y = pad + row * (cell_h + pad)
    draw.text((4, y + cell_h // 2 - 6), algo, fill=(255, 255, 255))
    for col in range(frame_count):
        path = os.path.join(src_dir, f"{base}_0_{col}_{algo}.png")
        img = Image.open(path).convert("RGBA")
        # Composite onto a mid-gray so we can see both black & white pixels and alpha.
        bg = Image.new("RGBA", img.size, (120, 120, 120, 255))
        bg.alpha_composite(img)
        big = bg.resize((cell_w, cell_h), Image.NEAREST)
        sheet.paste(big, (label_w + pad + col * (cell_w + pad), y))

sheet.save(out_path)
print(out_path)
