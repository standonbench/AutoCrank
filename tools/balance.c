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
// One curve table. winsOverride < 0 → wins = turn-1 (wins roughly track
// turns survived); otherwise every row uses that fixed win count (climax
// rows: what does the battle AT 9 wins look like on each turn?).
static void curve_pass(const char* label, int lives, int winsOverride,
                       int turnLo, int turnHi) {
    const int SEEDS = 500;
    printf("\n-- %s --\n", label);
    printf("turn  playerWin%%   draw%%   loss%%    avgPlayerStat  avgEnemyStat\n");
    printf("----  ----------   -----   -----    -------------  ------------\n");
    for (int turn = turnLo; turn <= turnHi; turn++) {
        int W = (winsOverride >= 0) ? winsOverride : turn - 1;
        int win = 0, loss = 0, draw = 0;
        long pstat = 0, estat = 0;
        for (int s = 0; s < SEEDS; s++) {
            uint32_t seedP = rng_next(), seedE = rng_next();
            Team P, E;
            ai_sim_team(&P, (uint8_t)turn, seedP);
            ai_sim_team(&E, (uint8_t)turn, seedE);
            ai_apply_handicap(&E, (uint8_t)W, (uint8_t)lives, (uint8_t)turn);
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
    rng_seed(0xBA1A9CE5u ^ 0x101);   // independent of other reports
    printf("\n== DIFFICULTY CURVE ==\n");
    printf("Policy player (no handicap) vs the turn-matched, handicapped enemy.\n");
    printf("Each row = 500 seeds x2 orderings. Policy-skill player; a real (better)\n");
    printf("player wins more. Gates: t1-2 loss<=22 (G1); t3-8 loss 28-40 (G2);\n");
    printf("CLIMAX rows loss 45-55 (G3); mercy passes >=8 / >=14 pts easier (G4).\n");
    curve_pass("HEALTHY (5 lives), wins=turn-1", 5, -1, 1, 10);
    curve_pass("ON THE ROPES (2 lives) — mercy", 2, -1, 1, 10);
    curve_pass("LAST LIFE (1 life) — deep mercy", 1, -1, 1, 10);
    curve_pass("CLIMAX — the battle at 9 wins (healthy)", 5, 9, 9, 12);
}

// ---------------------------------------------------------------------------
// Report 2: per-pet power
// ---------------------------------------------------------------------------
static void balance_pets(void) {
    rng_seed(0xBA1A9CE5u ^ 0x202);   // independent of other reports
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
    /*0  Squirrel buy(shop) */ { 14, 13, 12,  7 },   // out-of-combat value
    /*1  Pup on-summon buff */ {  6,  4,  9, 14 },   // summoners + faint payoff
    /*2  Hen EOT(shop)      */ { 14, 13, 12,  7 },   // out-of-combat value
    /*3  Hedgehog SOB +atk  */ { 13,  7,  4, 14 },   // wide team to buff
    /*4  Rabbit summoner    */ {  1,  8,  9, 14 },   // buffers + faint payoff
    /*5  Calf buy(shop)     */ { 14, 13, 12,  7 },   // out-of-combat value
    /*6  Sheep summoner     */ {  1,  8,  9, 14 },
    /*7  Goat faint-AoE     */ { 17, 13, 14, 12 },
    /*8  Haflinger on-summon*/ {  6,  4,  1,  9 },
    /*9  Donkey on-faint    */ {  6,  4, 13,  7 },   // faint-prone allies
    /*10 Alligator on-atk   */ { 14, 12, 13, 17 },   // many attackers
    /*11 Duck EOT(shop)     */ { 14, 13, 12,  7 },   // out-of-combat value
    /*12 Alpaca SOB-snipe   */ { 15, 16, 13, 14 },   // copy/re-fire amplify
    /*13 Highland faint-AoE */ { 14, 12,  7, 17 },
    /*14 Boar before-atk    */ { 13, 12, 17,  7 },
    /*15 Deer copy-left     */ { 13, 14, 17,  7 },   // 12 placed to its LEFT
    /*16 Fox re-fire-right  */ { 13, 14, 17,  7 },   // 12 placed to its RIGHT
    /*17 Bear buy(shop)     */ { 14, 13, 12,  7 },   // out-of-combat value
};
// Pets whose value accrues in the SHOP (gold, or permanent stat growth from
// buy/EOT triggers writing back to the live team) — their L1-base-stat combat
// score here is an artifact, not a verdict. Pup(1)/Haflinger(8) grow off
// every fresh shop buy via FRIEND_SUMMONED; Hen(2) compounds at end of turn;
// Squirrel(0) frees food gold, Duck(11) banks gold — pure economy pieces.
static int is_shop_phase(uint8_t id) {
    return id == 0 || id == 1 || id == 2 || id == 5 || id == 8 ||
           id == 11 || id == 17;
}

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
    rng_seed(0xBA1A9CE5u ^ 0x303);   // independent of other reports
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

// ---------------------------------------------------------------------------
// Report 4: RUN MONTE CARLO — the headline metric.
//
// Pilots the AI shopping policy through ENTIRE runs (10 wins to victory,
// 5 lives, draws cost/award nothing — mirrors game.c) against opponents
// generated exactly the way the game does (per-turn seed + handicap).
// Humans outplay the greedy policy, so the human run-success rate sits
// well above this number. Gates: policy success 22-32% (G7 → human ~40%);
// mean battles <= 18, mean turns <= 17, draws <= 25% (G8); every combat
// pet present in >= 2% of winning final teams (G9).
// ---------------------------------------------------------------------------
static void balance_runs(void) {
    rng_seed(0xBA1A9CE5u ^ 0x404);   // independent of other reports
    const int RUNS = 2000;
    int success = 0;
    long totBattles = 0, totTurns = 0, totDraws = 0;
    int winsAtDeath[10] = {0};            // wins held when the run died
    long pickWin[PET_COUNT] = {0}, pickLoss[PET_COUNT] = {0};
    long winRuns = 0, lossRuns = 0;

    for (int n = 0; n < RUNS; n++) {
        uint32_t playerSeed = rng_next();
        uint32_t runSeed    = rng_next();
        int wins = 0, lives = 5;
        int turn, battles = 0, draws = 0;
        Team P;
        clear_team(&P);
        for (turn = 1; wins < 10 && lives > 0 && turn <= 25; turn++) {
            ai_sim_team(&P, (uint8_t)turn, playerSeed);
            Team E;
            ai_sim_team(&E, (uint8_t)turn,
                        runSeed ^ (uint32_t)turn * 0x9E3779B9u);
            ai_apply_handicap(&E, (uint8_t)wins, (uint8_t)lives,
                              (uint8_t)turn);
            int r = sim_match(&P, &E);
            battles++;
            if (r == 0) wins++;
            else if (r == 1) lives--;
            else draws++;
        }
        totBattles += battles;
        totTurns   += turn - 1;
        totDraws   += draws;
        int won = (wins >= 10);
        if (won) { success++; winRuns++; }
        else     { lossRuns++; winsAtDeath[wins < 10 ? wins : 9]++; }
        for (int i = 0; i < TEAM_SIZE; i++) {
            uint8_t id = P.slots[i].id;
            if (id < PET_COUNT) {
                if (won) pickWin[id]++; else pickLoss[id]++;
            }
        }
    }

    printf("\n== RUN MONTE CARLO (%d full runs, policy player) ==\n", RUNS);
    printf("Greedy-policy pilot; humans outplay it, so human success >> this.\n");
    printf("Gate G7: success 22-32%%.  G8: battles<=18, turns<=17, draws<=25%%.\n\n");
    printf("run success   : %5.1f%%  (%d / %d)\n",
           100.0 * success / RUNS, success, RUNS);
    printf("mean battles  : %5.1f\n", (double)totBattles / RUNS);
    printf("mean turns    : %5.1f\n", (double)totTurns / RUNS);
    printf("draw rate     : %5.1f%% of battles\n",
           100.0 * totDraws / (totBattles > 0 ? totBattles : 1));
    printf("\nwins held when a failed run died:\n  ");
    for (int w = 0; w < 10; w++) printf("%d:%3d  ", w, winsAtDeath[w]);
    printf("\n\nfinal-team presence, winning vs losing runs (lift>1 = carried "
           "winners;\nread for dead pets and auto-includes, not fine ordering "
           "— reflects\nthe greedy policy's taste, not human meta):\n\n");
    printf("pet            win%%   loss%%   lift\n");
    printf("-----------   -----   -----   ----\n");
    for (int id = 0; id < PET_COUNT; id++) {
        double pw = winRuns  ? 100.0 * pickWin[id]  / winRuns  : 0.0;
        double pl = lossRuns ? 100.0 * pickLoss[id] / lossRuns : 0.0;
        double lift = (pl > 0.5) ? pw / pl : 0.0;
        printf("%-11s   %5.1f   %5.1f   %4.2f\n",
               pet_name((uint8_t)id), pw, pl, lift);
    }
}

int main(int argc, char** argv) {
    g_sys.logToConsole = stub_log;
    g_sys.error        = stub_log;
    g_file.open        = stub_open;
    g_api.system = &g_sys;
    g_api.file   = &g_file;
    pd = &g_api;

    // Each report re-seeds itself (fixed, distinct), so reports are
    // reproducible AND independent — adding a report never perturbs another.
    int doCurve = 1, doPets = 1, doSyn = 1, doRuns = 1;
    if (argc > 1) {
        char c = argv[1][0];
        doCurve = (c == 'c');
        doPets  = (c == 'p');
        doSyn   = (c == 's');
        doRuns  = (c == 'r');
        if (c == 'a') { doCurve = doPets = doSyn = doRuns = 1; }
    }
    if (doCurve) balance_curve();
    if (doPets)  balance_pets();
    if (doSyn)   balance_synergy();
    if (doRuns)  balance_runs();
    return 0;
}
