// ---------------------------------------------------------------------------
// Headless balance harness  (P2.2 tooling).
//
// Runs thousands of instant battle simulations natively (no Simulator) and
// prints two reports that turn balancing from guesswork into data:
//
//   1. DIFFICULTY CURVE — a "competent player" (an unbumped ai_generate_opponent
//      team) vs the turn-matched, win-streak-bumped enemy it would actually
//      face, across turns 1..10. Shows whether the climb to 10 wins escalates
//      or stays flat, plus the average team-stat totals on each side.
//
//   2. PER-PET POWER — random 5v5 teams (all tiers, L1 base stats) round-robin;
//      reports each pet's win rate when present. Pets far from the field
//      average are over/under-powered. NOTE: random teammates mean synergy
//      pieces (summoners/buffers) read low here — judge those in context.
//
// Same pd-stub trick as tools/hosttest.c (the whole game compiles on host).
// Build + run:  sh tools/balance.sh
// ---------------------------------------------------------------------------
#include "game.h"
#include <stdio.h>
#include <stdarg.h>

static void stub_log(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); putchar('\n');
}
static SDFile* stub_open(const char* name, FileOptions mode) {
    (void)name; (void)mode; return NULL;
}

static struct playdate_sys  g_sys;
static struct playdate_file g_file;
static PlaydateAPI          g_api;
PlaydateAPI* pd = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void clear_team(Team* t) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        t->slots[i].id = EMPTY_PET; t->slots[i].level = 1; t->slots[i].xp = 0;
        t->slots[i].atk = 0; t->slots[i].hp = 0;
        t->slots[i].food = EMPTY_FOOD; t->slots[i].flags = 0;
    }
}

static void random_team(Team* t, int k, uint8_t maxTier) {
    clear_team(t);
    for (int i = 0; i < k && i < TEAM_SIZE; i++) {
        uint8_t id = pet_random_in_tier(maxTier);
        const PetDef* d = pet_def(id);
        t->slots[i].id = id;
        t->slots[i].level = 1;
        t->slots[i].atk = d ? d->baseAtk : 1;
        t->slots[i].hp  = d ? d->baseHp  : 1;
    }
}

static long team_stat(const Team* t) {
    long s = 0;
    for (int i = 0; i < TEAM_SIZE; i++)
        if (t->slots[i].id != EMPTY_PET) s += t->slots[i].atk + t->slots[i].hp;
    return s;
}

// 0 = side0 wins, 1 = side1 wins, 2 = draw
static int sim_match(const Team* a, const Team* b) {
    game.battle.teams[0] = *a;
    game.battle.teams[1] = *b;
    game.battle.inBattle = true;
    battle_simulate();
    game.battle.inBattle = false;
    return game.battleResult;
}

// ---------------------------------------------------------------------------
// Report 1: difficulty curve
// ---------------------------------------------------------------------------
static void curve_pass(const char* label, int lives) {
    const int SEEDS = 500;
    game.lives = (uint8_t)lives;   // enemy bump reads this (comeback at <=2)
    printf("\n-- %s --\n", label);
    printf("turn  playerWin%%   draw%%   loss%%    avgPlayerStat  avgEnemyStat\n");
    printf("----  ----------   -----   -----    -------------  ------------\n");
    for (int turn = 1; turn <= 10; turn++) {
        int W = turn - 1;                  // wins roughly track turns survived
        int win = 0, loss = 0, draw = 0;
        long pstat = 0, estat = 0;
        for (int s = 0; s < SEEDS; s++) {
            uint32_t seedP = rng_next(), seedE = rng_next();
            Team P, E;
            game.lives = (uint8_t)lives;
            game.wins = 0;            ai_generate_opponent(&P, (uint8_t)turn, seedP);
            game.wins = (uint8_t)W;   ai_generate_opponent(&E, (uint8_t)turn, seedE);
            pstat += team_stat(&P); estat += team_stat(&E);
            // Player as side0, then as side1, to cancel any side bias.
            int r1 = sim_match(&P, &E);
            if (r1 == 0) win++; else if (r1 == 1) loss++; else draw++;
            int r2 = sim_match(&E, &P);
            if (r2 == 1) win++; else if (r2 == 0) loss++; else draw++;
        }
        int games = SEEDS * 2;
        printf("%4d   %9.1f%%   %5.1f   %5.1f    %13.1f  %12.1f\n",
               turn,
               100.0 * win / games, 100.0 * draw / games, 100.0 * loss / games,
               (double)pstat / SEEDS, (double)estat / SEEDS);
    }
}

static void balance_curve(void) {
    printf("\n== DIFFICULTY CURVE ==\n");
    printf("Competent player (unbumped) vs the turn-matched, win-streak-bumped enemy.\n");
    printf("Each row = 500 seeds x2 orderings. Baseline-skill player; a real (better)\n");
    printf("player wins more. Lower player-win%% = harder climb; compare the two passes\n");
    printf("to see the <=2-lives comeback mercy ease the enemy.\n");
    curve_pass("HEALTHY (5 lives) — streaker challenge", 5);
    curve_pass("ON THE ROPES (2 lives) — comeback mercy active", 2);
}

// ---------------------------------------------------------------------------
// Report 2: per-pet power
// ---------------------------------------------------------------------------
static void balance_pets(void) {
    const int ITER = 6000;
    long wins[PET_COUNT] = {0}, games[PET_COUNT] = {0};
    int s0win = 0, dr = 0;
    for (int n = 0; n < ITER; n++) {
        Team P, E;
        random_team(&P, 5, MAX_TIER);
        random_team(&E, 5, MAX_TIER);
        int r = sim_match(&P, &E);
        if (r == 0) s0win++; else if (r == 2) dr++;
        for (int i = 0; i < TEAM_SIZE; i++) {
            uint8_t id = P.slots[i].id;
            if (id < PET_COUNT) { games[id]++; if (r == 0) wins[id]++; }
        }
        for (int i = 0; i < TEAM_SIZE; i++) {
            uint8_t id = E.slots[i].id;
            if (id < PET_COUNT) { games[id]++; if (r == 1) wins[id]++; }
        }
    }

    // Sort pet indices by win rate, descending (selection sort; PET_COUNT=18).
    int idx[PET_COUNT];
    for (int i = 0; i < PET_COUNT; i++) idx[i] = i;
    for (int i = 0; i < PET_COUNT; i++) {
        int best = i;
        for (int j = i + 1; j < PET_COUNT; j++) {
            double rj = games[idx[j]] ? (double)wins[idx[j]] / games[idx[j]] : 0.0;
            double rb = games[idx[best]] ? (double)wins[idx[best]] / games[idx[best]] : 0.0;
            if (rj > rb) best = j;
        }
        int tmp = idx[i]; idx[i] = idx[best]; idx[best] = tmp;
    }

    printf("\n== PER-PET POWER (random 5v5, all tiers, L1 base stats) ==\n");
    printf("%d battles. Sanity: side0 win%% %.1f, draw%% %.1f (want ~symmetric).\n",
           ITER, 100.0 * s0win / ITER, 100.0 * dr / ITER);
    printf("Synergy pieces (summoners/buffers) read low with random teammates — judge in context.\n\n");
    printf("rank  pet          tier   base   winrate   games\n");
    printf("----  -----------  ----   ----   -------   -----\n");
    for (int k = 0; k < PET_COUNT; k++) {
        int id = idx[k];
        const PetDef* d = pet_def((uint8_t)id);
        double wr = games[id] ? 100.0 * (double)wins[id] / games[id] : 0.0;
        printf("%4d  %-11s  T%d    %2d/%-2d  %6.1f%%  %6ld\n",
               k + 1, pet_name((uint8_t)id), d ? d->tier : 0,
               d ? d->baseAtk : 0, d ? d->baseHp : 0, wr, games[id]);
    }
}

// ---------------------------------------------------------------------------
// Report 3: per-pet power IN A SUPPORT DECK (synergy-aware)
//
// The random-team report undersells synergy pets — a buffer with no summoner,
// a copy/re-fire pet with no SOB neighbor, etc. does nothing. Here each pet is
// dropped into a hand-picked deck that activates it, then run against a strong
// stat-ball benchmark. A pet that's weak in random teams but strong here is
// just synergy-dependent (fine); one that's weak in BOTH is genuinely weak.
//
// Partner ids per pet (judgment-based, kept in sync with pets.c abilities).
// Deer(15, copies LEFT) / Fox(16, re-fires RIGHT) need a neighbour on a side,
// so their decks are positioned explicitly in supportive_team().
// ---------------------------------------------------------------------------
static const int8_t PARTNERS[PET_COUNT][4] = {
    /*0  Squirrel faint-dmg */ {  7, 13, 14, 12 },
    /*1  Pup on-summon buff */ {  6,  4, 11,  9 },   // summoners + faint payoff
    /*2  Hen EOT(shop)      */ { 14, 13, 12,  7 },   // out-of-combat value
    /*3  Hedgehog SOB +atk  */ {  0,  7,  4, 14 },   // wide team to buff
    /*4  Rabbit summoner    */ {  1,  8,  9, 14 },   // buffers + faint payoff
    /*5  Calf buy(shop)     */ { 14, 13, 12,  7 },   // out-of-combat value
    /*6  Sheep summoner     */ {  1,  8,  9, 14 },
    /*7  Goat faint-AoE     */ {  0, 13, 14, 12 },
    /*8  Haflinger on-summon*/ {  6,  4, 11,  9 },
    /*9  Donkey on-faint    */ {  6,  4,  0,  7 },   // faint-prone allies
    /*10 Alligator on-atk   */ { 14, 12, 13, 17 },   // many attackers
    /*11 Duck summoner      */ {  1,  8,  9, 14 },
    /*12 Alpaca SOB-snipe   */ { 15, 16, 13, 14 },   // copy/re-fire amplify
    /*13 Highland faint-AoE */ { 14, 12,  7, 17 },
    /*14 Boar before-atk    */ { 13, 12, 17,  7 },
    /*15 Deer copy-left     */ { 13, 14, 17,  7 },   // 12 placed to its LEFT
    /*16 Fox re-fire-right  */ { 13, 14, 17,  7 },   // 12 placed to its RIGHT
    /*17 Bear buy(shop)     */ { 14, 13, 12,  7 },   // out-of-combat value
};
static int is_shop_phase(uint8_t id) { return id == 2 || id == 5 || id == 17; }

static void put(Team* t, int slot, uint8_t id) {
    const PetDef* d = pet_def(id);
    t->slots[slot].id = id; t->slots[slot].level = 1;
    t->slots[slot].atk = d ? d->baseAtk : 1;
    t->slots[slot].hp  = d ? d->baseHp  : 1;
    t->slots[slot].food = EMPTY_FOOD; t->slots[slot].flags = 0;
}

static void supportive_team(Team* t, uint8_t pet) {
    clear_team(t);
    if (pet == 15) {                       // Deer copies its LEFT (slot-1) neighbour
        put(t, 0, 12); put(t, 1, 15);      // Alpaca left of Deer
        for (int i = 0; i < 3; i++) put(t, 2 + i, (uint8_t)PARTNERS[15][i]);
    } else if (pet == 16) {                // Fox re-fires its RIGHT (slot+1) neighbour
        put(t, 0, 16); put(t, 1, 12);      // Alpaca right of Fox
        for (int i = 0; i < 3; i++) put(t, 2 + i, (uint8_t)PARTNERS[16][i]);
    } else {
        put(t, 0, pet);
        for (int i = 0; i < 4; i++) put(t, 1 + i, (uint8_t)PARTNERS[pet][i]);
    }
}

static void jitter(Team* t) {              // +0..1 per stat, samples build variance
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (t->slots[i].id == EMPTY_PET) continue;
        t->slots[i].atk = (int16_t)mini(t->slots[i].atk + rng_range(0, 1), STAT_CAP);
        t->slots[i].hp  = (int16_t)mini(t->slots[i].hp  + rng_range(0, 1), STAT_CAP);
    }
}

static void balance_synergy(void) {
    // Round-robin the 18 support decks against each other (self-calibrating:
    // no arbitrary benchmark; field average ~= (1 - draw)/2). Each deck is
    // headlined by its pet, so the win rate reads as "that archetype vs the
    // field of archetypes."
    const int K = 30;                          // iters per ordered pairing
    Team decks[PET_COUNT];
    for (int i = 0; i < PET_COUNT; i++) supportive_team(&decks[i], (uint8_t)i);
    long wins[PET_COUNT] = {0}, games[PET_COUNT] = {0};
    for (int i = 0; i < PET_COUNT; i++) {
        for (int j = 0; j < PET_COUNT; j++) {
            for (int k = 0; k < K; k++) {
                Team A = decks[i], B = decks[j];
                jitter(&A); jitter(&B);
                int r = sim_match(&A, &B);       // A=side0 (deck i), B=side1 (deck j)
                games[i]++; games[j]++;
                if (r == 0) wins[i]++; else if (r == 1) wins[j]++;
            }
        }
    }
    int idx[PET_COUNT];
    for (int i = 0; i < PET_COUNT; i++) idx[i] = i;
    for (int i = 0; i < PET_COUNT; i++) {
        int best = i;
        for (int j = i + 1; j < PET_COUNT; j++) {
            double rj = games[idx[j]] ? (double)wins[idx[j]] / games[idx[j]] : 0;
            double rb = games[idx[best]] ? (double)wins[idx[best]] / games[idx[best]] : 0;
            if (rj > rb) best = j;
        }
        int tmp = idx[i]; idx[i] = idx[best]; idx[best] = tmp;
    }
    printf("\n== ARCHETYPE POWER: support-deck round-robin (each pet headlines a deck) ==\n");
    printf("18 support decks, each vs every other (self-calibrating; field avg ~= 50%% minus\n");
    printf("draws). ~50%% = an average archetype; well below = weak even in its best deck.\n");
    printf("(shop) = gold/buff value realised in the shop, not combat — ignore its combat score.\n\n");
    printf("rank  pet          tier   winRate   note\n");
    printf("----  -----------  ----   -------   ----\n");
    for (int k = 0; k < PET_COUNT; k++) {
        int id = idx[k];
        const PetDef* d = pet_def((uint8_t)id);
        double wr = games[id] ? 100.0 * (double)wins[id] / games[id] : 0;
        printf("%4d  %-11s  T%d     %6.1f%%   %s\n",
               k + 1, pet_name((uint8_t)id), d ? d->tier : 0, wr,
               is_shop_phase((uint8_t)id) ? "(shop)" : "");
    }
}

int main(int argc, char** argv) {
    g_sys.logToConsole = stub_log;
    g_sys.error        = stub_log;
    g_file.open        = stub_open;
    g_api.system = &g_sys;
    g_api.file   = &g_file;
    pd = &g_api;

    rng_seed(0xBA1A9CE5);    // fixed seed => reproducible reports

    int doCurve = 1, doPets = 1, doSyn = 1;
    if (argc > 1) {
        char c = argv[1][0];
        doCurve = (c == 'c');
        doPets  = (c == 'p');
        doSyn   = (c == 's');
        if (c == 'a') { doCurve = doPets = doSyn = 1; }
    }
    if (doCurve) balance_curve();
    if (doPets)  balance_pets();
    if (doSyn)   balance_synergy();
    return 0;
}
