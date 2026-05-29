# AutoCrank — Playdate autobattler

A pocket-sized Super Auto Pets-inspired game, themed as a Swiss-Alpine
"Combat des Reines." Win 10 battles to take the run; you have 5 lives. Wins
are cumulative (not a streak — best streak is tracked separately).

## Build & Run
```
make                    # zero warnings required
make run                # build + open Simulator
open AutoCrank.pdx      # launch in Simulator manually
sh tools/runtests.sh    # headless mechanics self-tests (no Simulator needed)
```

Same setup as sibling `CrankSurvivorC/` — `PLAYDATE_SDK_PATH` env or `~/.Playdate/config`.

## Platform
Playdate: ARM Cortex-M7, 400×240, 1-bit B/W, 30fps.

## Architecture
| File | Responsibility |
|------|----------------|
| `src/game.h` | All structs, enums, constants, GFX_* macros, extern decls |
| `src/game.c` | State machine, top-level update(), shop input handling |
| `src/main.c` | eventHandler, RNG seed, 30fps lock |
| `src/render.c` | Top bar, shop screen, battle screen + projectile/debris/shake FX |
| `src/creatures.c` | Procedural 1-bit creature art (fallback when a PNG is missing) |
| `src/images.c` | LCDBitmap loader for pet sprites (foods are procedural) |
| `src/pets.c` | 18 pet definitions + ability handlers, 6 food defs |
| `src/triggers.c` | Trigger event queue + drain in attack-order |
| `src/shop.c` | Buy/sell/roll/freeze/combine + tier schedule |
| `src/battle.c` | Two-pass: simulate → event log → animated playback |
| `src/ai.c` | Opponent generator — simulates a competent player's journey |
| `src/ui.c` | Title, gameover, victory, pre-battle, result, gallery, team-name, hall-of-fame |
| `src/names.c/.h` | Team-name word pools (`team_name_str`) |
| `src/save.c` | Versioned save (V4 = lifetime stats + run resume + names + hall of fame) |
| `src/sound.c` | Synth SFX (implemented). `sfx_attack` is the combat-hit thud |
| `src/selftest.c` | 56-case mechanics self-test (Title→TEST, or `tools/runtests.sh`) |
| `src/mathutil.h` | XORShift RNG, clamp helpers |
| `src/debug.h` | DLOG/ASSERT (off in release) |
| `tools/hosttest.c` | Host harness: stub `pd` + `main` to run selftest_run() natively |

## Key Patterns
- GFX macros: `GFX_FILL`, `GFX_RECT`, `GFX_ELLIPSE`, `GFX_CIRCLE`, `GFX_LINE`, `GFX_PUSH`/`POP`
- Draw modes: `kDrawModeCopy` (default), `kDrawModeFillWhite` (text on black bars), `kDrawModeInverted` for outline/flash effects (NOT `kDrawModeXOR` — Playdate quirk)
- State machine: `game.state` — TITLE, SHOP, PRE_BATTLE, BATTLE, RESULT, GAMEOVER, VICTORY, SELFTEST, GALLERY, TEAM_NAME, HALL_OF_FAME
- One Game struct (`game`) holds everything (lifetime stats, current run, shop, battle)
- Pre-allocated, no malloc in hot path
- Shop cursor = single uint8 indexing across rows: shop pets / shop foods / team / [roll, end turn]

## Trigger system
Pets have a `handlers[NUM_TRIGGER_KINDS]` table. The trigger queue (`triggers.c`)
drains events in attack-order, and handlers can push more events — that's how
synergies cascade (Cricket faints → spawns Zombie → Horse buffs it → Ox gets +atk).

## Battle two-pass design
1. `battle_simulate()` runs the whole fight instantly, populating `game.battleLog` (BattleEvent[1024]).
2. `battle_playback_step()` chews through the log at `crankSpeed × baseRate` events/sec for animation.
Crank in battle = 1×–8× speed multiplier; holding A floors speed to 4× (it does NOT skip).
Hits render as arcing projectiles + falling debris + a brief screen-shake; all FX use a
separate `fx_hash` and the playback path NEVER touches the sim `_rng_state` (determinism).

## Sprite assets
- Pets: `Source/images/pets/pet_NN.png` (**32×32**, 1-bit, NN = 00-17, Kenney-derived CC0).
  `PETS[i].spriteId` selects the file; `NO_SPRITE` forces the procedural drawer.
- Foods: **procedural only** (`render.c` `render_food_*`); `Source/images/food/` is empty.
- `pdc` auto-converts every PNG in `Source/` into `.pdi` on build.
- Pet sprites blit at native 32×32; the battle screen scales them by `BATTLE_SCALE`.
- Missing pet PNG → procedural `creatures.c` fallback, which allocates a 32×32 scratch
  bitmap PER FRAME (off the happy path; `images_init` warns if any sprite fails to load).
- UI / combat FX are drawn procedurally with GFX macros.

## Controls
- D-pad: navigate cursor (up/down switches rows, left/right within row)
- A: buy from shop / pick-up team pet for reorder / roll / end turn
- B: freeze on shop slots / sell on team slots
- Crank (in shop, with team pet picked up): rotate to shift slot
- Crank (in battle): rotate to fast-forward animation (1×–8×)
- A (in battle): hold to floor speed at 4× (does NOT skip to result)

## SAP-derived numbers
- Starting gold: 10 per turn
- Pet/food cost: 3 (Sleeping Pill: 1)
- Roll cost: 1
- Sell refund: 1 + (level - 1)
- Combine: 1 XP per duplicate buy. L2 needs 2 XP, L3 needs 5 cumulative
- Team: 5 slots, attack right-to-left
- Tier: T1 turn 1, T2 turn 3, T3 turn 5 (v0 caps at T3)
- Lives: 5; Wins to victory: 10
- Stat cap: 50

## Roster (18 pets, 6 per tier) — Swiss-Alpine display names
T1: Squirrel, Pup, Hen, Hedgehog, Rabbit, Calf
T2: Sheep, Goat, Haflinger, Donkey, Alligator, Duck
T3: Alpaca, Highland, Boar, Deer, Fox, Bear

**Name-mapping gotcha:** display names in `PETS[]` differ from the handler
function names, which keep the original prototype names — e.g.
`pet_mouse_faint` = Squirrel, `pet_dog_summon` = Pup, `pet_croc_sob` = Alpaca,
`pet_whale_faint` = Highland, `pet_tiger_sob` = Fox, `pet_parrot_sob` = Deer,
`pet_dragon_buy` = Bear. Some `pets.c` comments use yet a third set
(Marmot/Bernard/Ibex/Reine/Bull). Same pets — do not rename to "fix" them.

## Style
- Procedural UI/FX in CrankSurvivor style (dot eyes, no eyebrows on any procedural sprite)
- Combat FX are directional (projectiles/debris), not symmetric spoke-bursts (see memory)
- Pet sprites are 32×32 1-bit PNGs (Kenney-derived); `creatures.c` is the procedural fallback

## Gotchas
- LSP errors about `pd_api.h` not found are FALSE POSITIVES — ignore
- Use `kDrawModeInverted`, NOT `kDrawModeXOR`, for outline/flash effects
- Format `uint32_t` with `%lu` and cast: `printf("%lu", (unsigned long)x)` (else -Wformat warning)
- Anonymous structs cannot be assigned to each other — typedef them
- Playdate refresh rate must be set explicitly (default is 20fps)
