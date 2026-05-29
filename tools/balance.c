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
static void balance_curve(void) {
    const int SEEDS = 500;
    printf("\n== DIFFICULTY CURVE ==\n");
    printf("Competent player (unbumped) vs the turn-matched, streak-bumped enemy.\n");
    printf("Each row = %d seeds x2 orderings. Player win%% well under 50%% => climb escalates.\n\n",
           SEEDS);
    printf("turn  enemyBump   playerWin%%   draw%%   loss%%    avgPlayerStat  avgEnemyStat\n");
    printf("----  ---------   ----------   -----   -----    -------------  ------------\n");
    for (int turn = 1; turn <= 10; turn++) {
        int W = turn - 1;                  // wins roughly track turns survived
        int win = 0, loss = 0, draw = 0;
        long pstat = 0, estat = 0;
        for (int s = 0; s < SEEDS; s++) {
            uint32_t seedP = rng_next(), seedE = rng_next();
            Team P, E;
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
        printf("%4d  %9d   %9.1f%%   %5.1f   %5.1f    %13.1f  %12.1f\n",
               turn, W,
               100.0 * win / games, 100.0 * draw / games, 100.0 * loss / games,
               (double)pstat / SEEDS, (double)estat / SEEDS);
    }
    // Streak bump caps at +3 (ai.c: streakBump = wins/2, clamped 3) — turns
    // 7..10 share the same bump, so watch whether win% flattens there.
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

int main(int argc, char** argv) {
    g_sys.logToConsole = stub_log;
    g_sys.error        = stub_log;
    g_file.open        = stub_open;
    g_api.system = &g_sys;
    g_api.file   = &g_file;
    pd = &g_api;

    rng_seed(0xBA1A9CE5);    // fixed seed => reproducible reports

    int doCurve = 1, doPets = 1;
    if (argc > 1) {
        doCurve = (argv[1][0] == 'c');
        doPets  = (argv[1][0] == 'p');
        if (argv[1][0] == 'a') { doCurve = doPets = 1; }
    }
    if (doCurve) balance_curve();
    if (doPets)  balance_pets();
    return 0;
}
