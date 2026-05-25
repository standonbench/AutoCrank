"""Compare the three ways a sprite can end up at 64x64:
  1. Native 32x32 source, blitted at 2x (what the renderer does today)
  2. Source upscaled to 64x64 (Lanczos) then 1-bit dithered at 64x64
  3. Source kept at 32x32, dithered at 32x32 (what we ship), nearest-2x to 64x64
"""
import subprocess
from pathlib import Path
from PIL import Image, ImageEnhance, ImageOps

ROOT = Path(__file__).resolve().parent.parent
PY = ROOT / "tools/.venv/bin/python"
T1B = ROOT / "tools/to_1bit.py"
OUT = ROOT / "tools/out/resolution"
OUT.mkdir(parents=True, exist_ok=True)

ANIMALS = [
    ("chicken", False),
    ("fox", True),
    ("cow-1", True),
    ("pig", True),
    ("hedgehog", True),
]

for animal, flip in ANIMALS:
    src = Path(f"/Users/Pierre/Downloads/animals/{animal}/0.png")
    im = Image.open(src).convert("RGBA")
    # apply contrast like install script
    r, g, b, a = im.split()
    rgb = Image.merge("RGB", (r, g, b))
    boosted = ImageEnhance.Contrast(rgb).enhance(3.0)
    pre = Image.merge("RGBA", (*boosted.split(), a))
    if flip:
        pre = ImageOps.mirror(pre)

    # --- 32x32 native dither (what we ship) ---
    tmp32 = OUT / f"_{animal}_pre32.png"
    pre.save(tmp32)
    subprocess.run([str(PY), str(T1B), str(tmp32),
                    "--dither", "bayer8", "--threshold", "128",
                    "--preserve-rare", "4", "--out", str(OUT)],
                   check=True, capture_output=True)
    sprite32 = OUT / f"{tmp32.stem}_1bit.png"
    sprite32.replace(OUT / f"{animal}_32.png")

    # --- 64x64 Lanczos-upscaled + dithered at 64x64 ---
    pre64 = pre.resize((64, 64), Image.LANCZOS)
    tmp64 = OUT / f"_{animal}_pre64.png"
    pre64.save(tmp64)
    subprocess.run([str(PY), str(T1B), str(tmp64),
                    "--dither", "bayer8", "--threshold", "128",
                    "--preserve-rare", "4", "--out", str(OUT)],
                   check=True, capture_output=True)
    sprite64 = OUT / f"{tmp64.stem}_1bit.png"
    sprite64.replace(OUT / f"{animal}_64.png")

# build a contact sheet
from PIL import ImageDraw, ImageFont

CELL = 192
LABEL_H = 22
font = ImageFont.load_default()
W = 80 + 3 * CELL
H = LABEL_H + len(ANIMALS) * (CELL + 4)
sheet = Image.new("RGB", (W, H), (240, 240, 240))
draw = ImageDraw.Draw(sheet)

draw.text((80 + 8, 4), "source (32 color)", fill=(0, 0, 0), font=font)
draw.text((80 + CELL + 8, 4), "ship now: 32 dithered, 2x nearest to 64", fill=(0, 0, 0), font=font)
draw.text((80 + 2 * CELL + 8, 4), "option B: 64 Lanczos + dither at 64", fill=(0, 0, 0), font=font)

def composite(p, cell):
    im = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.paste(im, (0, 0), im)
    return bg.convert("RGB").resize((cell, cell), Image.NEAREST)

for i, (animal, _) in enumerate(ANIMALS):
    y = LABEL_H + i * (CELL + 4)
    draw.text((4, y + CELL // 2 - 6), animal, fill=(0, 0, 0), font=font)
    src_path = Path(f"/Users/Pierre/Downloads/animals/{animal}/0.png")
    sheet.paste(composite(src_path, CELL), (80, y))
    # "ship now" — show the 32x32 sprite at native pixel-doubled size for fair compare
    sheet.paste(composite(OUT / f"{animal}_32.png", CELL), (80 + CELL, y))
    sheet.paste(composite(OUT / f"{animal}_64.png", CELL), (80 + 2 * CELL, y))

# Clean temp files
for f in OUT.iterdir():
    if f.name.startswith("_") and f.suffix == ".png":
        f.unlink()

sheet.save(OUT / "grid.png")
print(sheet.size)
