# Backlog

Outstanding work — not yet scheduled. Newest items at the top.

## Shop: cut shop pets from 5 to 3
Five options is too much choice and waters down the strategy — every roll
should be a small dilemma. Reduce `MAX_SHOP_PETS` from 5 to 3 in
`src/game.h`. Knock-on effects to audit:
- Shop cursor layout & arrows in `src/render.c` (`render_shop_screen` uses a
  5-wide `colX[5]` for both shop and team rows — split into separate
  layouts).
- Cursor indexing in `src/game.c` (cursor positions 0–4 are shop pets;
  shifts every downstream slot index).
- Tier-schedule pacing — fewer pets/roll may want a small gold or roll-cost
  adjustment to keep average pets-seen-per-turn similar.
- Test that frozen pets still survive end-of-turn refill correctly.

## Pet info card on hover/select
Pet names and abilities are invisible right now — players can't tell what
they're buying. Add a floating card that appears when the cursor is on a
shop pet or team pet, like Super Auto Pets. Card should show:
- Pet sprite + name + tier badge
- Base ATK / HP (or current ATK/HP for team pets)
- One-line ability text by trigger (e.g. "On faint → +2 ATK to a friend")
- Cost badge for shop pets

Implementation notes:
- Add `const char* description` to `PetDef` in `src/game.h`; fill in
  `src/pets.c` for all 18 pets.
- Card renderer in `src/render.c` — anchored near the cursor, flipped
  side when it would clip the screen edge.
- Foods need the same treatment (`FoodDef` already has a name — add
  `description`).

## Replace placeholder sprites with real free art
The procedural creatures in `src/creatures.c` are placeholders. Time to
swap in a real free sprite pack and convert to 1-bit for the Playdate.
**See sprite research at the bottom of this file — present top choices
before committing to one.**

Steps once a pack is chosen:
1. Drop source PNGs into `Source/images/pets/pet_NN.png` (currently
   serving stubs — the loader already resolves them).
2. Pick a B/W conversion: threshold, Bayer dither, Floyd-Steinberg, or
   manual cleanup per sprite. The pack's complexity will dictate.
3. Delete (or keep as fallback) `src/creatures.c` — the renderer already
   prefers `LCDBitmap` via `image_pet()` when present.
4. Reskin food sprites with the same pack/style if compatible.

---

## Sprite pack research — top choices

AutoCrank's 18-pet roster (cow, dog, hen, bee, grasshopper, calf, sheep,
goat, horse, ox, snake, goose, st-bernard, highland cow, boar, ibex,
queen cow, bull) needs side-profile or top-down sprites that can land on
Playdate's 1-bit 400×240 panel. Four candidates, ranked by how much work
stands between the pack and a finished build.

### 1. Intergenic 1-Bit Animals Pack — _strongest fit_
[intergenic.itch.io/1-bit-animals-pack](https://intergenic.itch.io/1-bit-animals-pack)
Hand-drawn 1-bit pixel-art animals, purpose-built for retro panels.
**Drop-in for Playdate — zero color conversion.**
- Pros: native 1-bit aesthetic matches the Playdate exactly; the look
  is already what we want; no dither tuning, no thresholding.
- Cons: itch.io page rate-limited during research — couldn't confirm
  the full animal roster or license terms. Typical itch packs are
  CC-BY or small fee. Coverage of AutoCrank's specific 18 may be
  partial; gaps would need custom drawing in the same style.
- Risk: medium. Recommend buying/downloading and auditing roster
  coverage before committing.

### 2. Kenney Animal Pack Redux + 1-bit conversion — _safest, most work_
[kenney.nl/assets/animal-pack-redux](https://kenney.nl/assets/animal-pack-redux)
240 colored animal sprites, CC0 license.
- Pros: CC0 (zero licensing risk); huge roster almost certainly covers
  all 18 pets; professional consistent style; free.
- Cons: requires color → 1-bit conversion. Kenney's chibi style is
  busy and won't threshold cleanly — needs per-sprite manual touch-up
  in Aseprite/Pixen (Pixen has Floyd-Steinberg and Atkinson
  dithering built-in, recommended by the Playdate community).
- Risk: low quality risk, medium time cost (~1 hr per sprite × 18 =
  one focused day).

### 3. Pixel Farm RPG (16x16) by exclusiveOlive — _right size, right theme_
[exclusiveolive.itch.io/pixel-farm-rpg](https://exclusiveolive.itch.io/pixel-farm-rpg)
16×16 colored farm-themed pack — matches AutoCrank's current sprite
resolution.
- Pros: already the right size (no resampling artifacts on conversion);
  farm theme aligns with the Alpine setting.
- Cons: color → 1-bit conversion needed; roster coverage of
  AutoCrank's specific animals unverified; license/price unclear from
  search.
- Risk: medium. Cheap to evaluate.

### 4. Keep procedural creatures + improve `creatures.c` — _bonus option_
- Pros: zero licensing; total control; Reine's bell pattern proves
  the procedural approach can give each pet character; no asset
  pipeline complexity.
- Cons: still looks placeholder-y; per-creature tuning is slow; 18
  distinct silhouettes is a stretch for the `quadruped(...)` helper.
- Risk: high quality risk for a polished release; great for a v0
  prototype, weak for a v1.

### Recommendation
**Try #1 first** (Intergenic) — if the roster covers and the license
fits, that's a few-hour drop-in. **Fall back to #2** (Kenney Redux +
manual 1-bit conversion in Pixen) — slower but guaranteed. Don't pursue
#3 or #4 unless #1 and #2 both fall through.

For the conversion step (only needed for #2 or #3):
[Playdate dev forum: 1-bit art tools](https://devforum.play.date/t/1-bit-art-tools/3524)
recommends Pixen (Mac) for Atkinson dithering — that gives the
authentic Mac-classic look that suits the Playdate panel better than
Floyd-Steinberg. Threshold-only conversion is fast but loses gradients.

