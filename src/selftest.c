#include "game.h"
#include <stdio.h>

// ===========================================================================
// Mechanics self-test suite.
// Each test crafts a battle where a single synergy flips the outcome, so a
// correct PASS/FAIL is unambiguous. Results render on STATE_SELFTEST.
// ===========================================================================

#define MAX_TEST_LINES 28
static char  g_lines[MAX_TEST_LINES][40];
static uint8_t g_ok[MAX_TEST_LINES];
static int   g_count = 0;
static int   g_pass = 0, g_fail = 0;

static void check(const char* name, bool ok) {
    if (g_count < MAX_TEST_LINES) {
        snprintf(g_lines[g_count], sizeof(g_lines[0]), "%s", name);
        g_ok[g_count] = ok ? 1 : 0;
        g_count++;
    }
    if (ok) g_pass++; else g_fail++;
    DLOG("%s %s", ok ? "PASS" : "FAIL", name);
}

// --- helpers --------------------------------------------------------------
static PetSlot mk(uint8_t id, int atk, int hp) {
    PetSlot p;
    memset(&p, 0, sizeof(p));
    p.id = id; p.level = 1; p.xp = 0;
    p.atk = (int16_t)atk; p.hp = (int16_t)hp;
    p.food = EMPTY_FOOD; p.flags = 0;
    return p;
}

static void clear_team(Team* t) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        t->slots[i].id = EMPTY_PET;
        t->slots[i].level = 1; t->slots[i].xp = 0;
        t->slots[i].atk = 0; t->slots[i].hp = 0;
        t->slots[i].food = EMPTY_FOOD; t->slots[i].flags = 0;
    }
}

// Run a battle; returns result (0 win, 1 loss, 2 draw).
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

static bool log_has(uint8_t kind) {
    for (int i = 0; i < game.battleLogLen; i++)
        if (game.battleLog[i].kind == kind) return true;
    return false;
}

// ===========================================================================
void selftest_run(void) {
    g_count = 0; g_pass = 0; g_fail = 0;
    rng_seed(0x1234ABCD);

    // T1: Bee Start-of-Battle gives all allies +1 atk.
    // Bee(0/8) can only beat Mouse(1/3) if SOB raised its attack.
    {
        PetSlot p[] = { mk(3, 0, 8) };
        PetSlot e[] = { mk(0, 1, 3) };
        check("Bee start-of-battle", run_battle(p, 1, e, 1) == 0);
    }

    // T2: Hedgehog faint deals 2 to ALL — should kill the back-row enemy,
    // turning a loss into a draw.
    {
        PetSlot p[] = { mk(7, 1, 1) };
        PetSlot e[] = { mk(0, 1, 1), mk(0, 1, 1) };
        check("Hedgehog faint AoE", run_battle(p, 1, e, 2) == 2);
    }

    // T3: Cricket faint spawns a Zombie Cricket (BEV_SPAWN must appear).
    {
        PetSlot p[] = { mk(4, 1, 1) };
        PetSlot e[] = { mk(0, 50, 50) };
        run_battle(p, 1, e, 1);
        check("Cricket faint spawn", log_has(BEV_SPAWN));
    }

    // T4: Mouse faint deals 1 to a random enemy — kills the last enemy,
    // turning a loss into a draw.
    {
        PetSlot p[] = { mk(0, 1, 1) };
        PetSlot e[] = { mk(0, 1, 2) };
        check("Mouse faint damage", run_battle(p, 1, e, 1) == 2);
    }

    // T5: Crocodile start-of-battle deals 8 to the last enemy (kills it pre-combat).
    // Croc has enough HP to shrug off the dying Mouse's faint-revenge.
    {
        PetSlot p[] = { mk(12, 1, 5) };
        PetSlot e[] = { mk(0, 1, 8) };
        check("Crocodile start-of-battle", run_battle(p, 1, e, 1) == 0);
    }

    // T6: Sheep faint spawns 2 Rams (3/3) — they win a fight the lone Sheep loses.
    {
        PetSlot p[] = { mk(6, 1, 1) };
        PetSlot e[] = { mk(0, 1, 1), mk(0, 1, 1) };
        check("Sheep faint spawn x2", run_battle(p, 1, e, 2) == 0);
    }

    // T7: Ox gains +1 atk when a friend faints. Ox(0 atk) only wins if buffed.
    {
        PetSlot p[] = { mk(9, 0, 20), mk(0, 1, 1) };  // Ox slot0, Mouse slot1
        PetSlot e[] = { mk(0, 1, 5) };
        check("Ox friend-faint buff", run_battle(p, 2, e, 1) == 0);
    }

    // T8: Boar gains +2/+2 before it attacks. Boar(0 atk) only wins if buffed.
    {
        PetSlot p[] = { mk(14, 0, 4) };
        PetSlot e[] = { mk(0, 1, 3) };
        check("Boar before-attack buff", run_battle(p, 1, e, 1) == 0);
    }

    // T9: Snake deals 5 to a random enemy when a friend attacks.
    // The fragile ally only wins if Snake's burst finishes the enemy first.
    {
        PetSlot p[] = { mk(10, 0, 20), mk(0, 1, 2) }; // Snake slot0, ally slot1
        PetSlot e[] = { mk(0, 1, 6) };
        check("Snake friend-attack burst", run_battle(p, 2, e, 1) == 0);
    }

    // T10: Dog gains +1/+1 when a friend is summoned (shop buy path).
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(1, 3, 2);   // Dog
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 0;                // Mouse in shop
        game.shop.gold = 100;
        shop_try_buy_pet(0, 1);               // buy Mouse into slot 1
        check("Dog friend-summoned buff", game.player.slots[0].atk == 4);
    }

    // T11: Combine three of a kind to reach level 2.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(4, 2, 2);   // Cricket on team
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.gold = 100;
        game.shop.pets[0] = 4;
        shop_try_buy_pet(0, 0);               // combine #1
        game.shop.pets[1] = 4;
        shop_try_buy_pet(1, 0);               // combine #2 → level 2
        check("Combine to level 2", game.player.slots[0].level == 2);
    }

    // T12: Determinism — same opponent seed yields identical teams.
    {
        Team a, b;
        ai_generate_opponent(&a, 5, 0xCAFEBABE);
        ai_generate_opponent(&b, 5, 0xCAFEBABE);
        bool same = true;
        for (int i = 0; i < TEAM_SIZE; i++)
            if (a.slots[i].id != b.slots[i].id ||
                a.slots[i].atk != b.slots[i].atk ||
                a.slots[i].hp != b.slots[i].hp) same = false;
        check("AI opponent determinism", same);
    }

    // T13: An empty player team loses cleanly (no crash, result = loss).
    {
        PetSlot e[] = { mk(0, 1, 1) };
        check("Empty team -> loss", run_battle(NULL, 0, e, 1) == 1);
    }

    // T14: Two empty teams draw.
    {
        check("Empty vs empty -> draw", run_battle(NULL, 0, NULL, 0) == 2);
    }

    DLOG("SELFTEST: %d passed, %d failed", g_pass, g_fail);
}

// ===========================================================================
void selftest_render(void) {
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorWhite);

    // Header bar
    GFX_FILL(0, 0, SCREEN_W, 20, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "SELF TEST   %d/%d passed", g_pass, g_pass + g_fail);
    pd->graphics->drawText(hdr, strlen(hdr), kASCIIEncoding, 8, 3);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // Two columns of results
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    int perCol = (g_count + 1) / 2;
    for (int i = 0; i < g_count; i++) {
        int col = i / perCol;
        int row = i % perCol;
        int x = 8 + col * 196;
        int y = 26 + row * 15;
        const char* tag = g_ok[i] ? "OK " : "XX ";
        // Failed tests get an inverted chip so they jump out
        if (!g_ok[i]) {
            GFX_FILL(x - 2, y - 1, 190, 14, kColorBlack);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
        } else {
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        char line[64];
        snprintf(line, sizeof(line), "%s%.40s", tag, g_lines[i]);
        pd->graphics->drawText(line, strlen(line), kASCIIEncoding, x, y);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    pd->graphics->drawText("B: back to title", 16, kASCIIEncoding, 8, SCREEN_H - 14);
}
