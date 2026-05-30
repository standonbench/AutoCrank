# AutoCrank — Session Handoff (2026-05-30)

Polish pass toward a fun/polished/publishable game. This note says exactly
where to pick up and what still needs testing. Full plan:
`~/.claude/plans/review-the-game-s-repo-witty-tide.md`.

## State
- **8 commits on `master`** (NOT pushed) atop `a77ff82`.
- **Build clean** (`make`, zero warnings) and **56/56 self-tests pass** (`sh tools/runtests.sh`).
- Working tree clean.

```
61ddb48 Add how-to-play onboarding (P3.1)
990581f Add Sound on/off setting + bump save to V5
1db0708 balance harness: add synergy-aware archetype round-robin
731f5dd Rebalance difficulty curve + fix Haflinger mis-stat
697ed6a Add balance harness: difficulty-curve + per-pet win-rate reports
e82d6cc Docs + cleanup: refresh CLAUDE.md; drop dead BEV_ATTACK hook
c800428 Harden battle robustness: log-truncation terminator + tests
44064e2 Battle FX: finish projectiles, combat audio, screen-shake, directional KO/spawn FX
b92932f Add headless host test harness
```

## Done this pass
- **Combat feel:** projectile spawn-position hardening; **combat audio** (clash + projectile impact — battles were previously silent); battlefield screen-shake on weighty hits; directional debris replacing the old symmetric spoke-bursts on KO/spawn.
- **Robustness:** always-land `BEV_RESULT` truncation terminator; determinism + truncation + Deer/Fox regression tests.
- **Tooling:** headless test harness (`runtests.sh`) and **balance harness** (`balance.sh` → difficulty-curve, per-pet, synergy reports).
- **Difficulty:** the old win-streak bump was a death-spiral (baseline player ~1–7% win from turn 3). Reshaped to a gentle, HP-biased bump + a ≤2-lives comeback mercy. Haflinger fixed (T2 with T1 base stats, 2/2→3/3).
- **UX:** Sound on/off setting (Playdate system menu, persisted via save **V5**); how-to-play onboarding (`STATE_HELP`, auto-shown once on first PLAY, re-readable from new title **HELP** entry).

## NEEDS TESTING — Simulator visual/audio QA (the main gap)
Everything below is build-clean + logic-verified but **never seen/heard**. Needs
computer-use to drive the Playdate Simulator (it dropped mid-session and needs a
**fresh Claude Code session** to relaunch). Launch with `make run`, then check:

1. **Battle FX** — projectiles arc from attacker to target; debris ejects in the
   right direction on clash/impact; **screen-shake** fires on lethal clash / big
   AoE but is suppressed at high crank speed; KO fly-off + spawn "materialize"
   wisps read well (no leftover radial star-bursts). Check at **1× and 8× crank**.
2. **Combat audio** — clash contact + projectile impacts now make a thud; a
   multi-target salvo doesn't machine-gun the SFX; faint still sounds.
3. **Sound setting** — system menu "Sound" toggles SFX and persists across relaunch.
4. **How-to-play** — first-ever PLAY shows the HELP card then continues into the
   team-name picker; title **HELP** re-opens it; the 5-card title menu
   (PLAY/HALL/ROSTER/TEST/HELP) isn't cramped and doesn't collide with the cows/stats.
5. **Difficulty FEEL** — play a few full runs (use system-menu Cheats: +5g / Turn 5
   / Win to fast-forward). Confirm the climb feels fair-but-escalating and a bad
   patch is recoverable (comeback). Tune `src/ai.c` streak bump if it feels off;
   re-check with `sh tools/balance.sh curve`.

## Held / remaining
- **Roster rebalance (held for playtest):** the synergy harness shows snipe-amplify
  (Fox 81%, Deer 78% via copy/re-fire of Alpaca's snipe) dominates and the
  summon/buff archetype is non-viable (Pup 4%, Haflinger 6%, summoners 16–27%).
  Decide a direction with playtest feel; `sh tools/balance.sh synergy` guides it.
- **P3.2 shop clarity** (offline-doable, not started): telegraph "buying this
  combines into slot N → L2" and explain when A is inert (team full).
- **P3.3 / P3.5:** 1-bit readability at 1×/8×, transition timing — need the Simulator.

## Key invariants (don't break)
- **Determinism:** playback/render/sound must NEVER touch the sim RNG (`_rng_state`);
  FX use `fx_hash`. There's a self-test guarding this.
- Pet **display names ≠ handler names** (e.g. `pet_mouse_faint` = Squirrel) — see CLAUDE.md.
- `make` must stay zero-warnings; keep `sh tools/runtests.sh` at 56/56.

## Verify quickly
```
make                     # zero warnings
sh tools/runtests.sh     # 56/56
sh tools/balance.sh      # difficulty + per-pet + synergy reports
make run                 # launch Simulator (needs computer-use for QA)
```

---

## New-session prompt (paste into a fresh Claude Code session)

> Continue the AutoCrank polish effort. Read `HANDOFF.md`, the plan at
> `~/.claude/plans/review-the-game-s-repo-witty-tide.md`, and `git log a77ff82..HEAD`
> to get oriented (the progress memory should auto-load). Everything is committed on
> `master`, build-clean, 56/56 tests pass.
>
> Primary task: drive the **Playdate Simulator** (`make run`) to do the visual/audio
> QA that was blocked last session — verify the battle FX (projectiles, debris,
> screen-shake) at 1× and 8× crank, the combat audio, the Sound system-menu toggle,
> the how-to-play screen + 5-card title menu, and play a few runs (use the system-menu
> Cheats to fast-forward) to judge the new difficulty feel. Fix anything that looks/
> sounds/feels off, keeping `make` warning-free and `sh tools/runtests.sh` at 56/56.
>
> Then, if time: the playtest-informed **roster rebalance** (synergy harness shows
> snipe-amplify OP / summon-buff weak — `sh tools/balance.sh synergy`), and **P3.2
> shop clarity** (telegraph combine target / "team full"). Don't push without my OK.
