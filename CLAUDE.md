# AutoCrank — Playdate autobattler

A pocket-sized Super Auto Pets-inspired game. Win 10 in a row, lose 5 lives.

## Build & Run
```
make                    # zero warnings required
make run                # build + open Simulator
open AutoCrank.pdx      # launch in Simulator manually
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
| `src/render.c` | Pet card, top bar, shop screen, battle screen |
| `src/images.c` | LCDBitmap loader for pet/food sprites |
| `src/pets.c` | 18 pet definitions + ability handlers, 6 food defs |
| `src/triggers.c` | Trigger event queue + drain in attack-order |
| `src/shop.c` | Buy/sell/roll/freeze/combine + tier schedule |
| `src/battle.c` | Two-pass: simulate → event log → animated playback |
| `src/ai.c` | Opponent team generator, scales with turn |
| `src/ui.c` | Title, gameover, victory, pre-battle, result screens |
| `src/save.c` | Versioned save (V1 = lifetime stats + run resume) |
| `src/sound.c` | SFX stubs (synth implementation pending) |
| `src/mathutil.h` | XORShift RNG, clamp helpers |
| `src/debug.h` | DLOG/ASSERT (off in release) |

## Key Patterns
- GFX macros: `GFX_FILL`, `GFX_RECT`, `GFX_ELLIPSE`, `GFX_CIRCLE`, `GFX_LINE`, `GFX_PUSH`/`POP`
- Draw modes: `kDrawModeCopy` (default), `kDrawModeFillWhite` (text on black bars), `kDrawModeInverted` for outline/flash effects (NOT `kDrawModeXOR` — Playdate quirk)
- State machine: `game.state` — TITLE, SHOP, PRE_BATTLE, BATTLE, RESULT, GAMEOVER, VICTORY
- One Game struct (`game`) holds everything (lifetime stats, current run, shop, battle)
- Pre-allocated, no malloc in hot path
- Shop cursor = single uint8 indexing across rows: shop pets / shop foods / team / [roll, end turn]

## Trigger system
Pets have a `handlers[NUM_TRIGGER_KINDS]` table. The trigger queue (`triggers.c`)
drains events in attack-order, and handlers can push more events — that's how
synergies cascade (Cricket faints → spawns Zombie → Horse buffs it → Ox gets +atk).

## Battle two-pass design
1. `battle_simulate()` runs the whole fight instantly, populating `game.battleLog` (BattleEvent[256]).
2. `battle_playback_step()` chews through the log at `crankSpeed × baseRate` events/sec for animation.
Crank in battle = 1×–8× speed multiplier. A button skips to result.

## Sprite assets
- Pets: `Source/images/pets/pet_NN.png` (16×16, 1-bit, NN = 00-17). Sourced from Kenney 1-bit pack (CC0).
- Foods: `Source/images/food/food_NN.png` (16×16, NN = 00-05).
- `pdc` auto-converts every PNG in `Source/` into `.pdi` on build.
- Renderer scales pet sprites 2× to 32×32 in-game.
- Missing sprites render a stub (letter inside dotted box) — game still works without art.
- UI / FX are drawn procedurally with GFX macros.

## Controls
- D-pad: navigate cursor (up/down switches rows, left/right within row)
- A: buy from shop / pick-up team pet for reorder / roll / end turn
- B: freeze on shop slots / sell on team slots
- Crank (in shop, with team pet picked up): rotate to shift slot
- Crank (in battle): rotate to fast-forward animation (1×–8×)
- A (in battle): skip to result

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

## Roster (18 pets, 6 per tier — see plan)
T1: Mouse, Dog, Frog, Bee, Cricket, Cow
T2: Sheep, Hedgehog, Horse, Ox, Snake, Spider
T3: Crocodile, Whale, Boar, Parrot, Tiger, Dragon

## Style
- Procedural UI/FX in CrankSurvivor style (dot eyes, no eyebrows on any procedural sprite)
- Pet sprites from Kenney 1-bit pack, 16×16 → drawn 2× = 32×32

## Gotchas
- LSP errors about `pd_api.h` not found are FALSE POSITIVES — ignore
- Use `kDrawModeInverted`, NOT `kDrawModeXOR`, for outline/flash effects
- Format `uint32_t` with `%lu` and cast: `printf("%lu", (unsigned long)x)` (else -Wformat warning)
- Anonymous structs cannot be assigned to each other — typedef them
- Playdate refresh rate must be set explicitly (default is 20fps)
