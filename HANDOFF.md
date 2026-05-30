# AutoCrank — Session Handoff (updated 2026-05-30, session 2)

Polish pass toward a fun/polished/publishable game. Full plan:
`~/.claude/plans/review-the-game-s-repo-witty-tide.md`.

## State
- **11 commits on `master`** (NOT pushed). Build clean (`make`, zero warnings),
  `sh tools/runtests.sh` **56/56**. Working tree clean.
- Latest 3 (this session) on top of the earlier 9:
```
5fdd843 Rebalance roster for fun + counterplay (snipe-amplify, summon/buff)
d4151e2 Rework system menu within Playdate's 3-item limit; gate debug for release
1445e1c Fix critical save bug: load from Data folder, not the pdx bundle
```

## Done this session
- **CRITICAL save bug fixed.** `save_load` opened `save.bin` with `kFileRead`
  (pdx bundle, read-only); `save_write` writes it to the **Data folder**
  (`kFileWrite`). The save was never found on launch → every boot fell back to
  defaults: lifetime stats, run resume, hall of fame, and the settings byte
  (sound + how-to-play-seen) **never persisted**. Fix: `kFileReadData |
  kFileRead`. Verified in-Simulator (title shows saved RUNS; PLAY skips the
  already-seen how-to-play).
- **System menu reworked + made publishable.** Playdate caps custom menu items
  at 3; the harness registered 5, so **Cheat/Preview never appeared** (dead QA
  tooling) and debug items showed to players. Now `Sound` (every build) +
  dev-only `Cheat` (incl. a Turn 9 / 8-wins jump) + a consolidated `Scene`
  picker (preview jumps + forced teams + FPS), gated behind `DEBUG_BUILD`.
  Makefile fixed so `DEBUG_BUILD` reaches simulator builds (SDK omitted UDEFS).
- **Roster rebalance (fun-first, tier-aware).** Tamed the snipe-amplify
  *instant win* (Alpaca single-target every level 8/12/15; Fox re-fire capped
  ×2) → removes ~3 key pets and leaves a fight instead of deleting the board.
  Made the go-wide payoff rewarding (Haflinger/Hedgehog gain hp on their buffs;
  Rabbit respawns a real body). Pup intentionally untouched. Numbers only.
- **Simulator QA** (computer-use, until it disconnected): title 5-card menu +
  how-to-play + first-run onboarding + pre-battle screen all read well; Sound
  toggle round-trips; difficulty curve confirmed fair-but-escalating with a
  working comeback mercy (`sh tools/balance.sh curve`).
- **Battle FX: code-verified, not visually captured** — battles resolve in
  ~0.3s (level-based ability kills = zero-window events), so sub-second 1-bit
  FX can't be screenshot-sampled. Final eyeballing is a human task.

## Remaining
- **P3.2 shop clarity** (not started; needs the Simulator to verify the
  telegraph): in `render_shop_screen`, telegraph "buying this combines into
  slot N → L2/L3" and explain when A is inert (team full). Reuse
  `first_combinable_slot()` (game.c:445) / `first_empty_team_slot()`; adapt the
  hint strip (`render.c:827`) + `render_pet_info_card`. A combine may level-up
  or just add XP — show it accurately.
- **Rebalance feel-tuning:** the pass is a feel-first starting point. Fox/Deer
  still ~80% in the harness (that's the T3 ceiling; the round-robin can't
  measure counterability). If they feel oppressive in play, cap harder.
- **FX/audio:** human eyeball + ears in the Simulator (use `Scene → PreBattle`
  at 1× and 8× crank). P3.3/P3.5 readability + transition timing.

## How to verify / drive
```
make                     # zero warnings
sh tools/runtests.sh     # 56/56
sh tools/balance.sh      # curve + per-pet + synergy
make run                 # launch Simulator
```
- **System menu = hold MENU** (or click the device MENU button; a *press-hold*
  is needed — a quick click is too brief for the 30fps poll). Items: `Sound`,
  `Cheat` (-, +5g, Turn 5, Turn 9, Win, Reset), `Scene` (PreBattle, Win, Loss,
  Draw, GameOver, Victory, F:Dmg, F:Copy, F:Death, FPS). Callbacks fire on menu
  **close**, reading the final value.
- Sim keyboard: Arrows = D-pad, **S = A**, **A = B** (Controls menu: "Arrows A
  S"). `hold_key` (~0.2s) is reliable; instant key/click taps often drop.

## Key invariants (don't break)
- **Determinism:** playback/render/sound must NEVER touch `_rng_state`; FX use
  `fx_hash`. A self-test guards it.
- Pet **display names ≠ handler names** (`pet_mouse_faint` = Squirrel) — CLAUDE.md.
- `make` zero-warnings; `sh tools/runtests.sh` 56/56. Release build
  (`DEBUG_BUILD=0`) must stay clean — gated debug code is `#if DEBUG_BUILD`.

## New-session prompt
> Continue the AutoCrank polish effort. Read `HANDOFF.md`, the plan at
> `~/.claude/plans/review-the-game-s-repo-witty-tide.md`, and `git log
> a77ff82..HEAD` (progress memory auto-loads). 11 commits on `master` (not
> pushed), build-clean, 56/56. This session fixed a critical save-persistence
> bug, reworked the system menu (publishable + working QA tooling), and did a
> fun-first roster rebalance. Remaining: **P3.2 shop clarity** (telegraph
> combine target / "team full" — needs the Simulator to verify rendering),
> **rebalance feel-tuning** (play a few runs via Cheat/Scene; cap snipe-amplify
> harder if Fox/Deer feel oppressive), and **FX/audio eyeball+ears** in the
> Simulator. Keep `make` warning-free and `sh tools/runtests.sh` 56/56. Don't
> push without my OK.
