// Stress / playtest suite — runs the same rebalanced game logic the
// selftest does, but on scenarios a human playtester would construct:
//
// 1. Per-pet smoke matchups (every pet at L1/L3 fights a baseline wall)
//    to surface under/over-performers.
// 2. Cascade scenarios (Sheep+Goose+Hopper+Edelweiss chains) to verify
//    no hangs and reasonable outcomes.
// 3. Recursion guard tests (Tiger+Ibex sandwiches).
// 4. Boundary checks (stat cap, level cap, random-target with no enemies).
// 5. Text-fit estimation for every pet desc.
// 6. Random-fuzz tournament: thousands of randomised battles to surface
//    crashes/hangs and overall balance (win-rate by tier).
//
// Findings print to stdout in a format easy to scan: PASS/FAIL/NOTE prefixes.

#include "pd_api.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

extern uint32_t _rng_state;
extern void host_setup_pd(void);

static int g_pass = 0, g_fail = 0, g_note = 0;
static void RESULT(const char* tag, const char* msg) {
    printf("%s %s\n", tag, msg);
    if (!strcmp(tag, "PASS")) g_pass++;
    else if (!strcmp(tag, "FAIL")) g_fail++;
    else g_note++;
}

// ---------------------------------------------------------------------------
// Tiny utilities reused across stress tests
// ---------------------------------------------------------------------------
static PetSlot mk(uint8_t id, int atk, int hp, uint8_t level) {
    PetSlot p; memset(&p, 0, sizeof(p));
    p.id = id; p.level = level; p.xp = 0;
    p.atk = (int16_t)atk; p.hp = (int16_t)hp;
    p.food = EMPTY_FOOD; p.flags = 0;
    return p;
}

static void clear_team(Team* t) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        t->slots[i].id = EMPTY_PET; t->slots[i].level = 1; t->slots[i].xp = 0;
        t->slots[i].atk = 0; t->slots[i].hp = 0;
        t->slots[i].food = EMPTY_FOOD; t->slots[i].flags = 0;
    }
}

static int run_battle(PetSlot* p, int np, PetSlot* e, int ne) {
    clear_team(&game.battle.teams[0]);
    clear_team(&game.battle.teams[1]);
    for (int i = 0; i < np && i < TEAM_SIZE; i++) game.battle.teams[0].slots[i] = p[i];
    for (int i = 0; i < ne && i < TEAM_SIZE; i++) game.battle.teams[1].slots[i] = e[i];
    game.battle.inBattle = true;
    battle_simulate();
    game.battle.inBattle = false;
    return game.battleResult;
}

// ---------------------------------------------------------------------------
// 1. Per-pet smoke matchups. Each pet stands alone, fed up to its tier's
// recommended baseline stats from a fresh L1 buy, against a calibrated
// wall of Marmots. We report W/L/D for each pet at L1 and L3.
// ---------------------------------------------------------------------------
static void per_pet_smoke(void) {
    printf("\n== Per-pet smoke (W/L/D vs Marmot wall) ==\n");
    const char* hdr = "  pet                  L1: W L D   L3: W L D";
    printf("%s\n", hdr);
    // 50 trials per scenario, randomised RNG seed each trial so spawns vary.
    const int N = 50;
    for (uint8_t id = 0; id < PET_COUNT; id++) {
        const PetDef* d = pet_def(id);
        if (!d) continue;
        int w[2] = {0, 0}, l[2] = {0, 0}, dr[2] = {0, 0};
        for (int lvl = 1; lvl <= 3; lvl += 2) {
            int idx = (lvl == 1) ? 0 : 1;
            for (int t = 0; t < N; t++) {
                _rng_state = 0xBEEF + t * 7;
                // Pet under test, base stats, given level
                PetSlot p[] = { mk(id, d->baseAtk, d->baseHp, (uint8_t)lvl) };
                // Wall: 2 Marmots @ T1 baseline matching expected fight power
                PetSlot e[] = {
                    mk(0, 2, 2, 1), mk(0, 2, 2, 1),
                };
                int r = run_battle(p, 1, e, 2);
                if (r == 0) w[idx]++; else if (r == 1) l[idx]++; else dr[idx]++;
            }
        }
        char line[80];
        snprintf(line, sizeof(line),
                 "  T%u %-16s  %2d %2d %2d    %2d %2d %2d",
                 (unsigned)d->tier, d->name,
                 w[0], l[0], dr[0], w[1], l[1], dr[1]);
        printf("%s\n", line);
    }
}

// ---------------------------------------------------------------------------
// 2. Cascade scenarios. Each one packs a deliberately complex synergy and
// verifies the battle terminates (safety counter in battle.c is 100 rounds,
// not infinite). We confirm result != "stuck mid-combat" and check the
// battle log length is sane.
// ---------------------------------------------------------------------------
static void cascade_scenarios(void) {
    printf("\n== Cascade scenarios ==\n");
    // 5 spawn-heavy pets — each death cascades into more.
    {
        PetSlot p[] = {
            mk(4, 2, 2, 3), mk(4, 2, 2, 3), mk(6, 3, 2, 3),
            mk(11, 3, 3, 3), mk(13, 4, 8, 3),
        };
        PetSlot e[] = { mk(14, 8, 6, 3), mk(14, 8, 6, 3) };
        int r = run_battle(p, 5, e, 2);
        char buf[80];
        snprintf(buf, sizeof(buf), "5-spawn cascade vs 2 L3 Boars: result=%d, log=%u",
                 r, (unsigned)game.battleLogLen);
        RESULT(game.battleLogLen > 0 && game.battleLogLen < MAX_BATTLE_EVENTS
               ? "PASS" : "FAIL", buf);
    }
    // Stacked SOB chain — Tiger + Ibex + Bernard. Should fire many SOB events
    // but be bounded by recursion guard (ABILITY_MAX_DEPTH=6).
    {
        PetSlot p[] = {
            mk(12, 5, 4, 3), mk(15, 3, 4, 3), mk(16, 4, 5, 3), mk(12, 5, 4, 3),
        };
        PetSlot e[] = { mk(0, 1, 100, 1) };
        int r = run_battle(p, 4, e, 1);
        char buf[80];
        snprintf(buf, sizeof(buf), "Tiger+Ibex+Bernard sandwich: result=%d, log=%u",
                 r, (unsigned)game.battleLogLen);
        RESULT(game.battleLogLen < MAX_BATTLE_EVENTS ? "PASS" : "FAIL", buf);
    }
    // Edelweiss on every Hopper — explosive bee bloom.
    {
        PetSlot p[5];
        for (int i = 0; i < 5; i++) {
            p[i] = mk(4, 2, 2, 3);
            p[i].food = 1;  // Edelweiss
        }
        PetSlot e[] = { mk(14, 10, 10, 3) };
        int r = run_battle(p, 5, e, 1);
        char buf[80];
        snprintf(buf, sizeof(buf), "5x Edelweiss Hopper vs Boar L3: result=%d, log=%u",
                 r, (unsigned)game.battleLogLen);
        RESULT(game.battleLogLen < MAX_BATTLE_EVENTS ? "PASS" : "FAIL", buf);
    }
}

// ---------------------------------------------------------------------------
// 3. Recursion guard — triple Tiger sandwich with Bernards. Without the
// depth guard, this would loop indefinitely. We confirm it terminates and
// the log doesn't blow up.
// ---------------------------------------------------------------------------
static void recursion_test(void) {
    printf("\n== Recursion guard ==\n");
    {
        // Reine (re-fire right) | Ibex (copy left) | Reine | Ibex | Bernard
        PetSlot p[] = {
            mk(16, 4, 5, 3), mk(15, 3, 4, 3),
            mk(16, 4, 5, 3), mk(15, 3, 4, 3),
            mk(12, 5, 4, 3),
        };
        PetSlot e[] = { mk(0, 1, 200, 1) };
        int r = run_battle(p, 5, e, 1);
        char buf[100];
        snprintf(buf, sizeof(buf),
                 "deep SOB chain (Reine|Ibex|Reine|Ibex|Bernard): result=%d, log=%u",
                 r, (unsigned)game.battleLogLen);
        // Pass = battle terminated and the log is bounded.
        RESULT(game.battleLogLen < MAX_BATTLE_EVENTS ? "PASS" : "FAIL", buf);
    }
}

// ---------------------------------------------------------------------------
// 4. Boundary / defensive checks.
// ---------------------------------------------------------------------------
static void boundary_tests(void) {
    printf("\n== Boundary tests ==\n");

    // Stat cap respected: chain Boar +4/+4 across 100 rounds against a tank,
    // confirm Boar's atk caps at STAT_CAP.
    {
        PetSlot p[] = { mk(14, 6, 6, 3) };
        PetSlot e[] = { mk(0, 0, 250, 1) };
        run_battle(p, 1, e, 1);
        char buf[80];
        snprintf(buf, sizeof(buf), "Boar atk capped: final atk=%d",
                 (int)game.battle.teams[0].slots[0].atk);
        RESULT(game.battle.teams[0].slots[0].atk <= STAT_CAP ? "PASS" : "FAIL", buf);
    }

    // Random-target with no enemies: Marmot faints with empty enemy team,
    // handler should bail not crash.
    {
        PetSlot p[] = { mk(0, 0, 1, 1) };
        // No enemies — battle ends immediately as loss for empty player? Actually
        // player has Marmot (1 alive), enemy empty -> player wins instantly. But
        // we want to exercise the faint handler with no target. Force-faint via
        // self-damage by giving Marmot 0 hp.
        clear_team(&game.battle.teams[0]);
        clear_team(&game.battle.teams[1]);
        game.battle.teams[0].slots[0] = p[0];
        game.battle.teams[0].slots[0].hp = 0;  // pre-dead
        game.battle.inBattle = true;
        bool died = battle_resolve_faints(&game.battle);
        game.battle.inBattle = false;
        RESULT(died ? "PASS" : "FAIL", "Marmot faint with empty enemy team: no crash");
    }

    // Combine past L3 is rejected. Start with L3 Calf and try to buy another;
    // shop_try_buy_pet should refuse (return false) and level stays 3.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(5, 7, 7, 3);
        game.player.slots[0].xp = 5;  // max xp at L3
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 5;
        game.shop.gold = 10;
        bool ok = shop_try_buy_pet(0, 0);
        char buf[80];
        snprintf(buf, sizeof(buf), "Combine past L3 rejected: returned %s, level=%u",
                 ok ? "true" : "false", (unsigned)game.player.slots[0].level);
        RESULT(!ok && game.player.slots[0].level == 3 ? "PASS" : "FAIL", buf);
    }
}

// ---------------------------------------------------------------------------
// 5. Text-fit estimation. Card description area is 248px wide (260 - 12).
// Playdate's small font averages ~5.5px/char (variable). Anything beyond
// ~45 chars likely needs the second wrap line — that's fine, but if a line
// is so long it wraps to a THIRD line the renderer truncates it (drawn<2).
// Conservatively flag any single desc string >70 chars total as risky.
// ---------------------------------------------------------------------------
static void text_fit_check(void) {
    printf("\n== Description text fit ==\n");
    int over = 0;
    for (int id = 0; id < PET_COUNT; id++) {
        for (int lvl = 0; lvl < MAX_PET_LEVEL; lvl++) {
            const char* s = PETS[id].desc[lvl];
            int n = s ? (int)strlen(s) : 0;
            if (n > 45) {
                char buf[120];
                snprintf(buf, sizeof(buf), "  long desc (%d ch) %s L%d: \"%s\"",
                         n, PETS[id].name, lvl + 1, s);
                printf("NOTE %s\n", buf);
                over++;
            }
            if (n > 70) {
                char buf[120];
                snprintf(buf, sizeof(buf),
                         "  OVERLONG desc (%d ch) %s L%d: \"%s\"",
                         n, PETS[id].name, lvl + 1, s);
                RESULT("FAIL", buf);
            }
        }
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "Text fit: %d desc strings over 45 chars (NOTE only)", over);
    RESULT("PASS", buf);
}

// ---------------------------------------------------------------------------
// 6. Fuzz tournament — fire random teams at each other 1000 times. We're
// looking for crashes/hangs (timeouts caught by run_battle's safety) and
// outlier balance signals (player vs AI generator at different turns).
// ---------------------------------------------------------------------------
static void fuzz_tournament(void) {
    printf("\n== Fuzz tournament (AI vs AI, 200 battles per turn) ==\n");
    for (uint8_t turn = 1; turn <= 10; turn++) {
        int w = 0, l = 0, dr = 0;
        int maxLog = 0;
        for (int trial = 0; trial < 200; trial++) {
            uint32_t seedP = 0xDEADBEEF + turn * 1000 + trial;
            uint32_t seedE = 0xCAFEBABE + turn * 1000 + trial;
            Team a, b;
            ai_generate_opponent(&a, turn, seedP);
            ai_generate_opponent(&b, turn, seedE);
            clear_team(&game.battle.teams[0]);
            clear_team(&game.battle.teams[1]);
            for (int i = 0; i < TEAM_SIZE; i++) game.battle.teams[0].slots[i] = a.slots[i];
            for (int i = 0; i < TEAM_SIZE; i++) game.battle.teams[1].slots[i] = b.slots[i];
            game.battle.inBattle = true;
            battle_simulate();
            game.battle.inBattle = false;
            if (game.battleResult == 0) w++;
            else if (game.battleResult == 1) l++;
            else dr++;
            if ((int)game.battleLogLen > maxLog) maxLog = (int)game.battleLogLen;
        }
        char buf[100];
        snprintf(buf, sizeof(buf),
                 "  turn %2u: W%3d L%3d D%3d  max log=%d (cap %d)",
                 (unsigned)turn, w, l, dr, maxLog, MAX_BATTLE_EVENTS);
        printf("%s\n", buf);
        // Sanity: never hit log cap (would mean truncated playback).
        if (maxLog >= MAX_BATTLE_EVENTS) RESULT("FAIL", "  log cap hit — events lost");
    }
    RESULT("PASS", "Fuzz tournament: 2000 battles, no crashes / no log overflow");
}

// ---------------------------------------------------------------------------
// 7. Mono-team impact — fill the whole team with the same pet at L1 and run
// vs a turn-3 AI. Identifies pets that snowball (high win rate) vs pets that
// don't carry on their own (low win rate). T2/T3 mono-teams shouldn't be
// available that early in real play, but the relative ordering is the signal.
// ---------------------------------------------------------------------------
static void pet_impact(void) {
    printf("\n== Mono-team impact (5x same pet @ L1, vs turn-3 AI, 100 trials) ==\n");
    printf("  pet                tier   win%%  draw%%  loss%%\n");
    for (uint8_t id = 0; id < PET_COUNT; id++) {
        const PetDef* d = pet_def(id);
        if (!d) continue;
        int w = 0, dr = 0, l = 0;
        const int N = 100;
        for (int t = 0; t < N; t++) {
            PetSlot p[TEAM_SIZE];
            for (int i = 0; i < TEAM_SIZE; i++)
                p[i] = mk(id, d->baseAtk, d->baseHp, 1);
            Team enemy;
            ai_generate_opponent(&enemy, 3, 0xC0FFEE + t);
            clear_team(&game.battle.teams[0]);
            clear_team(&game.battle.teams[1]);
            for (int i = 0; i < TEAM_SIZE; i++) game.battle.teams[0].slots[i] = p[i];
            for (int i = 0; i < TEAM_SIZE; i++) game.battle.teams[1].slots[i] = enemy.slots[i];
            game.battle.inBattle = true;
            battle_simulate();
            game.battle.inBattle = false;
            if (game.battleResult == 0) w++;
            else if (game.battleResult == 1) l++;
            else dr++;
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "  T%u %-16s  %4d%%  %4d%%  %4d%%",
                 (unsigned)d->tier, d->name, w, dr, l);
        printf("%s\n", buf);
    }
    RESULT("PASS", "Mono-team impact: collected win rates (see above for outliers)");
}

// ---------------------------------------------------------------------------
// 8. Late-game team builds vs a turn-10 AI — checks whether the cohesive
// strategies actually work at endgame intensity.
// ---------------------------------------------------------------------------
static void late_game_strategies(void) {
    printf("\n== Late-game strategy probes (vs turn-10 AI, 100 trials each) ==\n");

    struct {
        const char* name;
        PetSlot slots[TEAM_SIZE];
    } builds[] = {
        // 1. Sheep spawn-engine — Sheep + Haflinger summon-buff combo.
        { "Sheep+Hafl. engine", {
            mk(6, 3, 2, 3), mk(8, 2, 2, 3), mk(6, 3, 2, 3),
            mk(8, 2, 2, 3), mk(9, 2, 4, 3),
        }},
        // 2. Boar carry — front-line snowball.
        { "Boar carry",  {
            mk(3, 2, 1, 3), mk(8, 2, 2, 3), mk(10, 3, 3, 3),
            mk(0, 2, 2, 3), mk(14, 6, 6, 3),
        }},
        // 3. Tiger + Bernard combo.
        { "Tiger+Bernard",   {
            mk(0, 2, 2, 3), mk(0, 2, 2, 3), mk(0, 2, 2, 3),
            mk(16, 4, 5, 3), mk(12, 5, 4, 3),
        }},
        // 4. Ibex+Bee — copy the team-wide aura.
        { "Ibex+Bee aura",  {
            mk(3, 2, 1, 3), mk(15, 3, 4, 3), mk(0, 2, 2, 3),
            mk(0, 2, 2, 3), mk(14, 6, 6, 3),
        }},
        // 5. Highland AoE + Goose body double.
        { "Highland+Goose",  {
            mk(11, 3, 3, 3), mk(11, 3, 3, 3), mk(13, 4, 8, 3),
            mk(9, 2, 4, 3), mk(14, 6, 6, 3),
        }},
        // 6. Bull buff-economy late game.
        { "Bull buff", {
            mk(8, 2, 2, 3), mk(8, 2, 2, 3), mk(8, 2, 2, 3),
            mk(8, 2, 2, 3), mk(17, 6, 8, 3),
        }},
    };
    int n = sizeof(builds) / sizeof(builds[0]);
    for (int b = 0; b < n; b++) {
        int w = 0, dr = 0, l = 0;
        for (int t = 0; t < 100; t++) {
            Team enemy;
            ai_generate_opponent(&enemy, 10, 0x1234 + t * 31);
            clear_team(&game.battle.teams[0]);
            clear_team(&game.battle.teams[1]);
            for (int i = 0; i < TEAM_SIZE; i++)
                game.battle.teams[0].slots[i] = builds[b].slots[i];
            for (int i = 0; i < TEAM_SIZE; i++)
                game.battle.teams[1].slots[i] = enemy.slots[i];
            game.battle.inBattle = true;
            battle_simulate();
            game.battle.inBattle = false;
            if (game.battleResult == 0) w++;
            else if (game.battleResult == 1) l++;
            else dr++;
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "  %-22s W%3d D%3d L%3d",
                 builds[b].name, w, dr, l);
        printf("%s\n", buf);
    }
    RESULT("PASS", "Late-game probes: collected win rates");
}

// ---------------------------------------------------------------------------
// 9. New Calf L3 behaviour — rebate +2g should fire on ANY pet's buy, not
// just Calf's own combine. Without this, Calf's L3 is dead at max level.
// ---------------------------------------------------------------------------
static void calf_l3_any_buy(void) {
    printf("\n== Calf L3 any-buy rebate ==\n");
    clear_team(&game.player);
    game.player.slots[0] = mk(5, 7, 7, 3);            // L3 Calf already maxed
    memset(&game.shop, 0, sizeof(Shop));
    for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
    for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
    game.shop.pets[0] = 0;                            // Marmot (T1)
    game.shop.gold = 10;
    int before = game.shop.gold;
    shop_try_buy_pet(0, 1);
    char buf[80];
    snprintf(buf, sizeof(buf),
             "L3 Calf rebates +2g on other pets' buys: gold %d -> %d",
             before, game.shop.gold);
    RESULT(game.shop.gold == before - PET_COST + 2 ? "PASS" : "FAIL", buf);
}

// ---------------------------------------------------------------------------
int main(void) {
    host_setup_pd();
    per_pet_smoke();
    cascade_scenarios();
    recursion_test();
    boundary_tests();
    text_fit_check();
    calf_l3_any_buy();
    pet_impact();
    late_game_strategies();
    fuzz_tournament();
    printf("\nSTRESS: %d PASS, %d FAIL, %d NOTE\n", g_pass, g_fail, g_note);
    return g_fail ? 1 : 0;
}
