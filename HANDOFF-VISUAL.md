# AutoCrank — Visual QA Session Brief (2026-05-30)

**This session is VISUAL-first** and needs **computer-use (Simulator screen
control)**. The previous session did all the offline/logic work and committed
it; what's left genuinely needs eyes (and your ears) on the running game.
Full state/history: `HANDOFF.md`. Plan: `~/.claude/plans/review-the-game-s-repo-witty-tide.md`.

## State (don't re-do this)
- **`master`, 12 commits, NOT pushed.** `make` zero-warnings, `sh tools/runtests.sh` **56/56**.
- Already done & committed: critical save-load fix, system-menu rework
  (Sound + dev Cheat/Scene, gated), fun-first roster rebalance. See `HANDOFF.md`.

## Operational playbook — driving the Simulator (READ THIS, it saves hours)
- **Launch:** `make run`. Then `request_access` for bundle **`date.play.simulator`**
  (grants at "full" tier). Bring it forward with `open_application date.play.simulator`.
  If the window doesn't appear (esp. after a quit), open the macOS **Window menu**
  (top bar) and pick "Playdate Simulator" to restore it.
- **Controls** (Controls menu = "Arrows A S"): Arrows = D-pad, **keyboard `s` =
  Playdate A** (buy/select/confirm), **keyboard `a` = Playdate B** (back/freeze/sell).
  Use **`hold_key` ~0.2s** — instant `key`/`left_click` taps are too brief for the
  30fps button poll and silently drop. On-screen buttons need a press-hold
  (`mouse_down` → `wait 0.15-0.25` → `mouse_up`). Coords: dpad-down ~(572,388),
  A ~(829,340), B ~(737,340), MENU ~(907,94).
- **System menu:** press-HOLD the device MENU button (~907,94) to open (a quick
  click is too brief). dpad Up/Down between items, Left/Right to change an option
  value, `s` to toggle a checkmark. **Callbacks fire on menu CLOSE** — set the
  value, then close (press-hold MENU again) to apply it. Items:
  `Sound` (checkmark) · `Cheat` (-, +5g, Turn 5, Turn 9, Win, Reset) ·
  `Scene` (-, PreBattle, Win, Loss, Draw, GameOver, Victory, F:Dmg, F:Copy,
  F:Death, FPS). Cheat/Scene snap back to "-" after firing. **FPS** toggles the
  on-screen FPS/debug overlay.
  - Meaty battle on demand: **Cheat → Turn 9** (close to apply: sets turn 9 / 8
    wins / strong AI), then **Scene → PreBattle** (close: curated team
    Calf/Highland/Goat/Bear/Fox vs a turn-9 opponent). You can set both in one
    open session — Cheat fires before Scene (registration order), so the opponent
    is turn-9.
- **Zoom the LCD:** region `[487, 38, 885, 290]` ≈ native 400×240. Zoom sub-regions
  for pixel detail (e.g. menu `[690,48,875,275]`, top bar `[487,45,885,75]`).

## The FX-capture problem (why last session couldn't screenshot the FX)
Battles resolve in **~0.3s real-time** — kills come from level-based ability
damage (snipes/AoE) that collapse to **zero-window events** no slowdown can
stretch. Static screenshots are too coarse for sub-second 1-bit FX. Techniques,
best first:
1. **TRY THE SCREEN RECORDER FIRST (untried).** Toolbar has a record-looking
   button **`▶O` at ~(710,438)** (distinct from the camera/screenshot `📷` at
   ~(679,438)). If it records a GIF/video: record a battle, find the output
   (check `~/Desktop` and the Simulator's save dir), then `ffmpeg`/`sips` the
   frames out and `Read` them — **full-battle frame-by-frame FX**. This is the
   most promising path and was not explored.
2. **Watch a long CLASH battle**, not a snipe one. A real turn-1/2 fight (3/3 vs
   3/3, many clashes) lasts much longer than a snipe alpha-strike. Or temporarily
   slow playback for QA: `battle.c` `battle_playback_step` →
   `game.battleClock += (float)speed * 0.2f;` (revert before commit). NB this
   stretches *clash* battles but not zero-window snipe battles.
3. **Capture in ONE `computer_batch`** to beat the ~2-3s model round-trip:
   menu-close + `wait ~2.8` (out the 3s pre-battle intro) + a dense burst of
   `screenshot`s, all in a single batch — otherwise the battle finishes between
   your separate calls.
4. **Screen-shake is 3-4 frames (~0.1s)** — essentially uncatchable by
   screenshot; confirm it on a recording or accept code-verification
   (`render.c` `battle_shake`, auto-suppressed at `battleSpeed>3`).

## Visual task checklist (priority order)
**A. Battle FX at 1× and 8× crank** (the originally-blocked item)
   - Projectiles arc attacker→target; directional debris on clash/impact (NOT
     symmetric spoke/star bursts — see [[feedback-fx-radial-spokes]]); screen
     -shake on lethal / big-AoE hits and **suppressed** at high crank; KO fly-off
     + spawn "materialize" wisps read well; no leftover radial star-bursts; the
     1-bit screen never turns to mush at 8×. Crank in the Simulator = drag the
     docked crank slider (bottom-right of the control panel) to fast-forward.
**B. Rebalance FEEL** (play 2-3 runs via Cheat→Turn 9 + Scene→PreBattle, and a
   real run from turn 1)
   - Does snipe-amplify still feel oppressive? Fox/Deer + Alpaca were just tamed
     (Alpaca single-target every level; Fox re-fire capped ×2) so it removes ~3
     pets and leaves a fight. If it STILL deletes you, cap harder: Fox ×1, trim
     Alpaca damage, or Deer copy-left-only. Re-check `sh tools/balance.sh synergy`.
   - Does summon/buff feel rewarding now (Haflinger/Hedgehog gained hp; Rabbit
     respawns a real body; Pup grows off summons early)? Each pet should "give
     ideas," none should auto-win. Tune numbers in `pets.c` (update the matching
     `selftest.c` assertions in lockstep — they hard-code the numbers).
**C. P3.2 shop clarity** (implement + verify it renders cleanly)
   - Telegraph "buying this shop pet combines into team slot N → L2/L3" (highlight
     the destination slot) and explain when A is inert (team full, no combine).
   - Reuse `first_combinable_slot(petId)` (game.c:445) / `first_empty_team_slot()`;
     adapt the hint strip (`render.c:827`) + `render_pet_info_card`. A combine may
     level-up OR just add XP — show it accurately. **Verify no text/box overlap.**
**D. Audio** (your ears)
   - Combat clash/impact/faint SFX fire at the visual moment and don't
     machine-gun on multi-target salvos; the **Sound** menu toggle actually mutes
     and persists across relaunch (now that saves load). Code is RNG-free
     `sfx_attack` + hooks in `render.c`.
**E. P3.3 / P3.5 polish**
   - 1-bit readability at 1× and 8×; transition timing (no dead air result→shop;
     pre-battle intro ~3s, skippable with A; repeat viewings don't drag).

## Invariants (don't break)
- **Determinism:** playback/render/sound must NEVER touch `_rng_state`; FX use
  `fx_hash`. A self-test guards it.
- `make` zero-warnings; `sh tools/runtests.sh` **56/56**; release build
  (`DEBUG_BUILD=0`) stays clean (debug code is `#if DEBUG_BUILD`).
- Pet **display names ≠ handler names** (`pet_mouse_faint` = Squirrel) — CLAUDE.md.
- **Don't push** without the user's OK. Commit freely on `master`.

---

## Pickup prompt (paste into the new session)
> Continue AutoCrank — this is the **visual QA session** with Simulator access.
> Read `HANDOFF-VISUAL.md` first (it has the full Simulator-driving playbook and
> the FX-capture techniques), then `HANDOFF.md` for history. State: `master`, 12
> commits, not pushed, `make` clean, 56/56 tests. Launch with `make run` and
> request computer-use access for bundle `date.play.simulator`.
>
> Focus, in order: **(A)** eyeball the battle FX at 1× and 8× crank (try the
> Simulator's screen recorder `▶O` first to grab full-battle frames — see the
> brief); **(B)** play a few runs (Cheat→Turn 9, Scene→PreBattle) and judge the
> new rebalance FEEL — is snipe-amplify (Fox/Deer/Alpaca) still oppressive? does
> summon/buff feel rewarding? cap/tune in `pets.c` if needed (update
> `selftest.c` in lockstep); **(C)** implement + visually verify **P3.2 shop
> clarity** (telegraph combine → L2/L3 + "team full" inert-A); **(D)** I'll
> listen for the combat audio + Sound toggle. Keep `make` warning-free and
> `sh tools/runtests.sh` 56/56. Don't push without my OK.
