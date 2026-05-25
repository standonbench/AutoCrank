"""Install pet sprites with contrast boost + facing normalization.

Pipeline per pet:
  1. Open source PNG from /Users/Pierre/Downloads/animals/<animal>/0.png
  2. Boost RGB contrast x3.0 (alpha untouched)
  3. Mirror horizontally if source faces left, so all sprites face right
  4. Save to a temp file
  5. Run to_1bit.py: bayer8 dither, threshold 128, preserve-rare 4
  6. Move output to Source/images/pets/pet_NN.png
"""
import subprocess
import sys
from pathlib import Path
from PIL import Image, ImageOps, ImageEnhance

ROOT = Path(__file__).resolve().parent.parent
ANIMALS = Path("/Users/Pierre/Downloads/animals")
PETS_DIR = ROOT / "Source/images/pets"
TMP_DIR = ROOT / "tools/out/_install_tmp"
TMP_DIR.mkdir(parents=True, exist_ok=True)
PY_BIN = ROOT / "tools/.venv/bin/python"
T1B = ROOT / "tools/to_1bit.py"

# pet id -> (display name, animal source dir, must_flip)
MAPPING = [
    (0,  "Squirrel",  "squirrel",  False),
    (1,  "Pup",       "dog-0",     True),
    (2,  "Hen",       "chicken",   False),
    (3,  "Hedgehog",  "hedgehog",  True),
    (4,  "Rabbit",    "rabbit",    True),
    (5,  "Calf",      "cow-0",     True),
    (6,  "Sheep",     "sheep",     True),
    (7,  "Goat",      "goat",      True),
    (8,  "Haflinger", "horse",     True),
    (9,  "Donkey",    "donkey",    False),
    (10, "Alligator", "alligator", True),
    (11, "Duck",      "duck",      False),
    (12, "Alpaca",    "alpaca",    True),
    (13, "Highland",  "cow-1",     True),
    (14, "Boar",      "pig",       True),
    (15, "Deer",      "deer",      True),
    (16, "Fox",       "fox",       True),
    (17, "Bear",      "bear",      True),
]

CONTRAST = 3.0

for pid, name, animal, flip in MAPPING:
    src = ANIMALS / animal / "0.png"
    if not src.exists():
        print(f"!! missing source: {src}", file=sys.stderr)
        sys.exit(1)

    im = Image.open(src).convert("RGBA")
    r, g, b, a = im.split()
    rgb = Image.merge("RGB", (r, g, b))
    boosted = ImageEnhance.Contrast(rgb).enhance(CONTRAST)
    out = Image.merge("RGBA", (*boosted.split(), a))
    if flip:
        out = ImageOps.mirror(out)

    tmp = TMP_DIR / f"_{animal}.png"
    out.save(tmp)

    subprocess.run(
        [str(PY_BIN), str(T1B), str(tmp),
         "--dither", "bayer8",
         "--threshold", "128",
         "--preserve-rare", "4",
         "--out", str(PETS_DIR)],
        check=True, capture_output=True,
    )
    produced = PETS_DIR / f"{tmp.stem}_1bit.png"
    dst = PETS_DIR / f"pet_{pid:02d}.png"
    produced.replace(dst)
    flip_tag = " (flipped)" if flip else ""
    print(f"pet_{pid:02d}.png  <-  {animal}/0.png  -> {name}{flip_tag}")

# Clean temp files
for f in TMP_DIR.iterdir():
    f.unlink()

print(f"\n{len(MAPPING)} sprites installed (contrast={CONTRAST}, all facing right).")
