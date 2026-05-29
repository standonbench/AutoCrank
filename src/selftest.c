#include "game.h"
#include <stdio.h>

// ===========================================================================
// Mechanics self-test suite.
// Each test crafts a battle where a single synergy flips the outcome, so a
// correct PASS/FAIL is unambiguous. Results render on STATE_SELFTEST.
// ===========================================================================

#define MAX_TEST_LINES 64
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

// Same as mk() but with an explicit level — used to verify L2/L3 ability jumps.
static PetSlot mkL(uint8_t id, int atk, int hp, uint8_t level) {
    PetSlot p = mk(id, atk, hp);
    p.level = level;
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

    // T6: Sheep faint spawns 2 Haflingers (2/2 at L1) — they win a fight the
    // lone Sheep loses. With new L1 spawns (2/2) the cross-summon buffs lift
    // each to 4/2; one survives Mouse's faint counter-snipe.
    {
        PetSlot p[] = { mk(6, 1, 1) };
        PetSlot e[] = { mk(0, 1, 4) };
        check("Sheep faint spawn x2", run_battle(p, 1, e, 1) == 0);
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

    // T12: Frog end-of-turn buffs the ally on its left (+1/+1).
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(0, 1, 2);   // Mouse
        game.player.slots[1] = mk(2, 1, 3);   // Frog
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.turn = 1;
        shop_end_turn_advance();
        check("Frog end-of-turn buff", game.player.slots[0].atk == 2);
    }

    // T13: Haflinger L1 buffs a newly summoned friend (+2 atk). Mouse base
    // atk is 2 after rebalance, so post-buff = 4.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(8, 1, 1);   // Haflinger
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 0;                // Marmot (T1)
        game.shop.gold = 100;
        shop_try_buy_pet(0, 1);               // Marmot summoned into slot 1
        check("Haflinger summon buff +2", game.player.slots[1].atk == 4);
    }

    // T14: Spider faint summons a Tier-1 pet (BEV_SPAWN of a T1 id).
    {
        PetSlot p[] = { mk(11, 1, 1) };
        PetSlot e[] = { mk(0, 50, 50) };
        run_battle(p, 1, e, 1);
        bool t1spawn = false;
        for (int i = 0; i < game.battleLogLen; i++) {
            if (game.battleLog[i].kind == BEV_SPAWN) {
                const PetDef* d = pet_def(game.battleLog[i].data);
                if (d && d->tier == 1) t1spawn = true;
            }
        }
        check("Spider faint summons T1", t1spawn);
    }

    // T15: Dragon — buying a T1 pet gives every ally +1/+1.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(17, 6, 8);  // Dragon
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 0;                // Mouse (T1)
        game.shop.gold = 100;
        shop_try_buy_pet(0, 1);
        check("Dragon buy-T1 team buff", game.player.slots[0].atk == 7);
    }

    // T16: Tiger re-fires the right neighbour's start-of-battle ability.
    // Tiger+Croc deal 16 to the last enemy, killing a 12-HP Mouse pre-combat.
    {
        PetSlot p[] = { mk(16, 4, 1), mk(12, 1, 1) };  // Tiger, Crocodile
        PetSlot e[] = { mk(0, 1, 12) };
        check("Tiger re-fires neighbour", run_battle(p, 2, e, 1) == 0);
    }

    // T17: Parrot copies the left neighbour's start-of-battle ability.
    {
        PetSlot p[] = { mk(12, 1, 1), mk(15, 3, 1) };  // Crocodile, Parrot
        PetSlot e[] = { mk(0, 1, 12) };
        check("Parrot copies neighbour", run_battle(p, 2, e, 1) == 0);
    }

    // T18: Whale faint death-bomb (5 dmg) kills the enemy back -> draw.
    {
        PetSlot p[] = { mk(13, 1, 1) };
        PetSlot e[] = { mk(0, 1, 5) };
        check("Whale faint death-bomb", run_battle(p, 1, e, 1) == 2);
    }

    // T19: Determinism — same opponent seed yields identical teams.
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

    // T22: Hay food gives the target +1/+1.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(0, 1, 2);
        if (FOODS[0].apply) FOODS[0].apply(&game.player, 0);
        check("Hay food +1/+1",
              game.player.slots[0].atk == 2 && game.player.slots[0].hp == 3);
    }

    // T23: Milk food gives an ally +2/+2 (single ally = deterministic target).
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(0, 1, 2);
        if (FOODS[5].apply) FOODS[5].apply(&game.player, 0);
        check("Milk food +2/+2",
              game.player.slots[0].atk == 3 && game.player.slots[0].hp == 4);
    }

    // T24: Winning the 10th battle reaches the victory state.
    {
        clear_team(&game.player);
        game.inRun = true;
        game.wins = WINS_TO_VICTORY - 1;
        game.lives = 5;
        game.battleResult = 0;
        game_after_battle();
        check("10th win -> victory", game.state == STATE_VICTORY);
    }

    // T25: Losing the last life reaches the game-over state.
    {
        clear_team(&game.player);
        game.inRun = true;
        game.wins = 3;
        game.lives = 1;
        game.battleResult = 1;
        game_after_battle();
        check("Last life lost -> game over", game.state == STATE_GAMEOVER);
    }

    // =======================================================================
    // L2 / L3 ability scaling — verifies every qualitative jump the rebalance
    // introduces. Tests are intentionally tight: each one constructs a
    // scenario where the new behavior is the *only* explanation.
    // =======================================================================

    // T26: Marmot L3 fires 2 ability events at 4 dmg each.
    {
        PetSlot p[] = { mkL(0, 0, 1, 3) };           // L3 Marmot, no atk
        PetSlot e[] = { mk(0, 10, 10), mk(0, 10, 10) };
        run_battle(p, 1, e, 2);
        int n = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 4) n++;
        check("Marmot L3: 2 x 4 dmg events", n == 2);
    }

    // T27: Pup L3 buffs SELF +2/+2 AND the just-summoned ally +1/+1.
    {
        clear_team(&game.player);
        game.player.slots[0] = mkL(1, 3, 2, 3);      // L3 Pup
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 0;                       // Marmot (base 2/2)
        game.shop.gold = 100;
        shop_try_buy_pet(0, 1);
        bool pupOk = game.player.slots[0].atk == 5 && game.player.slots[0].hp == 4;
        bool allyOk = game.player.slots[1].atk == 3 && game.player.slots[1].hp == 3;
        check("Pup L3: self +2/+2 and ally +1/+1", pupOk && allyOk);
    }

    // T28: Hen L3 buffs BOTH neighbours +2/+1 on end of turn.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(0, 1, 2);          // Marmot left
        game.player.slots[1] = mkL(2, 1, 3, 3);      // L3 Hen
        game.player.slots[2] = mk(0, 1, 2);          // Marmot right
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.turn = 1;
        shop_end_turn_advance();
        bool leftOk  = game.player.slots[0].atk == 3 && game.player.slots[0].hp == 3;
        bool rightOk = game.player.slots[2].atk == 3 && game.player.slots[2].hp == 3;
        check("Hen L3: both neighbours +2/+1", leftOk && rightOk);
    }

    // T29: Bee L3 buffs all friends +2/+1 (the hp component is the L3 jump).
    {
        clear_team(&game.battle.teams[0]);
        clear_team(&game.battle.teams[1]);
        game.battle.teams[0].slots[0] = mkL(3, 0, 1, 3);  // L3 Bee
        game.battle.teams[0].slots[1] = mk(0, 1, 1);       // Marmot ally
        game.battle.inBattle = false;
        game.battleLogLen = 0;
        trig_reset();
        TriggerEvent ev = { TRIG_START_OF_BATTLE, 0, 0, 0, 0 };
        PETS[3].handlers[TRIG_START_OF_BATTLE](&game.battle, 0, 0, &ev);
        check("Bee L3: ally +2/+1",
              game.battle.teams[0].slots[1].atk == 3 &&
              game.battle.teams[0].slots[1].hp == 2);
    }

    // T30: Hopper L3 spawns a 3/3 Hopper carrying Edelweiss (food id 1).
    // Enemy dies on clash so the spawned Hopper survives the simulation; we
    // can then read its food field after run_battle returns.
    {
        PetSlot p[] = { mkL(4, 5, 1, 3) };           // L3 Hopper, dies on clash
        PetSlot e[] = { mk(0, 1, 1) };               // Mouse dies on clash too
        run_battle(p, 1, e, 1);
        check("Hopper L3: spawn holds Edelweiss",
              game.battle.teams[0].slots[0].id == 4 &&
              game.battle.teams[0].slots[0].food == 1);
    }

    // T31: Calf L2 rebates +1g on the combine-buy that takes it to L2. Calf's
    // BUY handler only fires when Calf itself is the bought pet (srcSlot ==
    // slot), so triggering a rebate requires buying/combining a Calf.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(5, 3, 3);          // L1 Calf
        game.player.slots[0].xp = LEVEL_2_XP - 1;    // 1 xp; next combine -> L2
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 5;                       // Calf in shop
        game.shop.gold = 10;
        int before = game.shop.gold;
        shop_try_buy_pet(0, 0);                      // combine -> level 2
        check("Calf L2: +1g rebate on combine",
              game.shop.gold == before - PET_COST + 1 &&
              game.player.slots[0].level == 2);
    }

    // T32: Calf L3 rebates +2g on the combine that takes it from L2 to L3.
    {
        clear_team(&game.player);
        game.player.slots[0] = mkL(5, 5, 5, 2);      // L2 Calf 5/5
        // To reach L3: need cumulative xp = LEVEL_2_XP + LEVEL_3_XP = 5.
        // Calf already at L2 with xp set so next combine hits 5.
        game.player.slots[0].xp = LEVEL_2_XP + LEVEL_3_XP - 1;
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 5;
        game.shop.gold = 10;
        int before = game.shop.gold;
        shop_try_buy_pet(0, 0);                      // combine -> level 3
        check("Calf L3: +2g rebate on combine",
              game.shop.gold == before - PET_COST + 2 &&
              game.player.slots[0].level == 3);
    }

    // T33: Sheep L3 spawns two 6/6 Haflingers.
    {
        PetSlot p[] = { mkL(6, 0, 1, 3) };           // L3 Sheep, dies
        PetSlot e[] = { mk(0, 5, 5) };
        run_battle(p, 1, e, 1);
        int spawned6 = 0;
        for (int i = 0; i < game.battleLogLen; i++) {
            if (game.battleLog[i].kind == BEV_SPAWN) {
                int a = game.battleLog[i].value & 0xFF;
                int h = (game.battleLog[i].value >> 8) & 0xFF;
                if (a == 6 && h == 6) spawned6++;
            }
        }
        check("Sheep L3: two 6/6 spawns", spawned6 == 2);
    }

    // T34: Goat L3 deals 4 to enemies AND 2 to friends. Marmot in slot 0 takes
    // the friendly-fire (Goat sits frontmost in slot 1 and dies on clash).
    // Marmot hp 8 because both enemy Mice will also faint and each deal 2 dmg
    // to a random player (only Marmot alive), so 8 - 2 (Goat) - 2 - 2 = 2.
    {
        PetSlot p[] = { mk(0, 0, 8), mkL(7, 0, 1, 3) };  // Marmot + L3 Goat
        PetSlot e[] = { mk(0, 1, 4), mk(0, 1, 4) };
        run_battle(p, 2, e, 2);
        check("Goat L3: Marmot ends at 2 hp (friendly+faint)",
              game.battle.teams[0].slots[0].hp == 2);
    }

    // T35: Adder L3 fires 2 ability events per ally attack. Needs two alive
    // enemies so both Adder hits land — abilities now correctly skip
    // dying-but-not-yet-cleared corpses, so a single 1-hp dummy would only
    // get hit once before falling off the random_alive target pool.
    {
        PetSlot p[] = { mkL(10, 0, 100, 3), mk(0, 1, 100) };
        PetSlot e[] = { mk(0, 0, 100), mk(0, 0, 100) }; // 2 tanky dummies
        run_battle(p, 2, e, 2);
        int n = 0;
        for (int i = 0; i < game.battleLogLen; i++) {
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 5) n++;
        }
        // Round 1 fires exactly 2 (one per Adder target). Stop counting at 2
        // to keep the test independent of how many further safety rounds run.
        check("Adder L3: 2 hits per ally attack", n >= 2);
    }

    // T36: Bernard L3 hits the last TWO enemies for 12 dmg each at SOB.
    {
        PetSlot p[] = { mkL(12, 1, 1, 3) };          // L3 Bernard
        PetSlot e[] = { mk(0, 1, 50), mk(0, 1, 50) };
        run_battle(p, 1, e, 2);
        int n = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 12) n++;
        check("Bernard L3: 12 dmg x 2 targets", n == 2);
    }

    // T37: Highland L3 AoE — every enemy takes 8 on Highland's faint.
    {
        PetSlot p[] = { mkL(13, 0, 1, 3) };          // L3 Highland, dies
        PetSlot e[] = { mk(0, 1, 50), mk(0, 1, 50), mk(0, 1, 50) };
        run_battle(p, 1, e, 3);
        int n = 0;
        for (int i = 0; i < 3; i++) {
            if (game.battle.teams[1].slots[i].id != EMPTY_PET &&
                game.battle.teams[1].slots[i].hp <= 50 - 8) n++;
        }
        check("Highland L3: AoE hits all 3 enemies", n == 3);
    }

    // T38: Boar L3 grants its OTHER friends +1 atk on each of its attacks.
    {
        PetSlot p[] = { mk(0, 1, 100), mkL(14, 6, 6, 3) };  // Marmot + L3 Boar
        PetSlot e[] = { mk(0, 0, 100) };                      // soak
        run_battle(p, 2, e, 1);
        check("Boar L3: friend gains +1 atk per Boar atk",
              game.battle.teams[0].slots[0].atk >= 2);
    }

    // T39: Ibex L3 copies BOTH neighbours' SOB. Bernard's handler reads its
    // CALLER's slot/level — so Ibex-copied Bernards run at Ibex's level (L3 ->
    // 12 dmg, 2 targets). Two copies x two targets = 4 ABILITY events of 12.
    // The own Bernards still fire at L1 = 8 dmg each (irrelevant to this check).
    {
        PetSlot p[] = { mk(12, 1, 1), mkL(15, 3, 3, 3), mk(12, 1, 1) };
        PetSlot e[] = { mk(0, 1, 200), mk(0, 1, 200) };  // 2 targets needed
        run_battle(p, 3, e, 2);
        int n = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 12) n++;
        check("Ibex L3: 4 x 12 dmg from copies", n == 4);
    }

    // T40: Reine L3 re-fires the right ally x3 (so Bernard fires 4 total).
    {
        PetSlot p[] = { mkL(16, 4, 1, 3), mk(12, 1, 1) };
        PetSlot e[] = { mk(0, 1, 100) };
        run_battle(p, 2, e, 1);
        int n = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 8) n++;
        check("Reine L3: 4 total Bernard fires", n == 4);
    }

    // T41: Bull L3 triggers on ANY tier buy (T3 here) and grants +2/+1.
    {
        clear_team(&game.player);
        game.player.slots[0] = mkL(17, 6, 8, 3);     // L3 Bull
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 13;                      // Highland (T3)
        game.shop.gold = 100;
        int before = game.player.slots[0].atk;
        shop_try_buy_pet(0, 1);
        check("Bull L3: T3 buy fires +2/+1",
              game.player.slots[0].atk == before + 2);
    }

    // T42: Bull L1 does NOT trigger on a T3 buy.
    {
        clear_team(&game.player);
        game.player.slots[0] = mk(17, 6, 8);         // L1 Bull
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 13;                      // Highland (T3)
        game.shop.gold = 100;
        int before = game.player.slots[0].atk;
        shop_try_buy_pet(0, 1);
        check("Bull L1: T3 buy does NOT fire",
              game.player.slots[0].atk == before);
    }

    // T43: Ox L3 gains Cowbell flag (bit 0) when an ally faints.
    {
        PetSlot p[] = { mkL(9, 1, 20, 3), mk(0, 1, 1) };  // L3 Ox + Marmot
        PetSlot e[] = { mk(0, 1, 5) };
        run_battle(p, 2, e, 1);
        check("Ox L3: gains Cowbell on friend faint",
              (game.battle.teams[0].slots[0].flags & 0x01) != 0);
    }

    // T44: Food schedule — at turn 1, only T1 foods (Hay/Cheese) populate shop.
    {
        game.turn = 1;
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        bool onlyT1 = true;
        for (int trial = 0; trial < 100 && onlyT1; trial++) {
            shop_refill();
            for (int i = 0; i < MAX_SHOP_FOOD; i++) {
                uint8_t f = game.shop.foods[i];
                if (f < FOOD_COUNT && FOODS[f].unlockTurn > 1) {
                    onlyT1 = false; break;
                }
            }
        }
        check("Food schedule: turn 1 only T1 foods", onlyT1);
    }

    // T_Haflinger_L3: Haflinger L3 gives +3/+2 to the summoned ally.
    {
        clear_team(&game.player);
        game.player.slots[0] = mkL(8, 2, 2, 3);      // L3 Haflinger
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 0;                       // Marmot (base 2/2)
        game.shop.gold = 100;
        shop_try_buy_pet(0, 1);
        // Marmot base 2/2, +3/+2 = 5/4.
        check("Haflinger L3: summon +3/+2",
              game.player.slots[1].atk == 5 && game.player.slots[1].hp == 4);
    }

    // T_Adder_DeadTarget: Adder should NOT fire on an ally attack that
    // already killed the only enemy. Previously the random_alive helper
    // returned dying-but-still-in-slot pets, so Adder wasted hits on corpses.
    {
        PetSlot p[] = { mk(10, 0, 100), mk(0, 100, 100) }; // Adder + heavy ally
        PetSlot e[] = { mk(0, 0, 1) };                     // dies in 1 hit
        run_battle(p, 2, e, 1);
        int adderHits = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 3) adderHits++;
        // Marmot killed the dummy in clash, then FRIEND_ATTACK fires — Adder
        // should find no alive enemy and bail. Pre-fix: hit corpse for 3.
        check("Adder no-fires on corpse", adderHits == 0);
    }

    // T_Ibex_NoNeighbour: Ibex alone at slot 0 has no left neighbour to copy.
    // SOB should safely no-op (regression guard for the bounds-check path).
    {
        clear_team(&game.battle.teams[0]);
        clear_team(&game.battle.teams[1]);
        game.battle.teams[0].slots[0] = mkL(15, 3, 4, 1);
        game.battle.teams[1].slots[0] = mk(0, 1, 1);
        game.battle.inBattle = true;
        battle_simulate();
        game.battle.inBattle = false;
        check("Ibex alone: no-op safely", game.battleResult <= 2);
    }

    // T_Ibex_L1_LeftOnly: Ibex L1 with a Bernard to its LEFT copies Bernard's
    // SOB and fires 1 extra Bernard snipe (8 dmg). Without a right neighbour
    // copy at L1, total Bernard fires = 2 (own + Ibex's copy).
    {
        PetSlot p[] = { mk(12, 1, 1), mkL(15, 3, 4, 1) }; // Bernard, Ibex L1
        PetSlot e[] = { mk(0, 1, 100) };
        run_battle(p, 2, e, 1);
        int n8 = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 8) n8++;
        check("Ibex L1: copies left ally SOB", n8 == 2);
    }

    // T_Freeze_Empty: freezing an empty shop slot should be a no-op, not a
    // permanent skip. Without the fix, shop_refill would skip the slot every
    // turn and it would stay empty forever (until the player un-froze it).
    {
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        // Attempt to freeze empty pet slot 0 and empty food slot 0.
        shop_toggle_freeze_pet(0);
        shop_toggle_freeze_food(0);
        // Refill — both slots should populate (freeze bit was not set).
        shop_refill();
        bool petOk  = (game.shop.frozenPets  & 0x1) == 0 && game.shop.pets[0]  != EMPTY_PET;
        bool foodOk = (game.shop.frozenFoods & 0x1) == 0 && game.shop.foods[0] != EMPTY_FOOD;
        check("Freeze on empty slot is a no-op", petOk && foodOk);
    }

    // T_Freeze_Persists: freezing a real shop pet keeps it across refill.
    {
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
        game.shop.pets[0] = 12;          // Bernard
        shop_toggle_freeze_pet(0);
        shop_refill();
        check("Freeze on real pet persists across refill",
              game.shop.pets[0] == 12 && (game.shop.frozenPets & 0x1));
    }

    // T_Goose_L3: Goose L3 spawns at 5/5 (was 3/3 at L1) from T1-or-T2 pool.
    {
        PetSlot p[] = { mkL(11, 0, 1, 3) };          // L3 Goose, dies on clash
        PetSlot e[] = { mk(0, 5, 5) };
        run_battle(p, 1, e, 1);
        int spawned5 = 0;
        for (int i = 0; i < game.battleLogLen; i++) {
            if (game.battleLog[i].kind == BEV_SPAWN) {
                int a = game.battleLog[i].value & 0xFF;
                int h = (game.battleLog[i].value >> 8) & 0xFF;
                if (a == 5 && h == 5) spawned5++;
            }
        }
        check("Goose L3: spawn at 5/5", spawned5 >= 1);
    }

    // T45: Food schedule — at turn 5 the T3 foods can actually appear.
    {
        game.turn = 5;
        memset(&game.shop, 0, sizeof(Shop));
        for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
        bool sawT3 = false;
        for (int trial = 0; trial < 400 && !sawT3; trial++) {
            shop_refill();
            for (int i = 0; i < MAX_SHOP_FOOD; i++) {
                uint8_t f = game.shop.foods[i];
                if (f < FOOD_COUNT && FOODS[f].unlockTurn >= 5) sawT3 = true;
            }
        }
        check("Food schedule: turn 5 reveals T3 foods", sawT3);
    }

    // T46: Truncation safety — a deep copy/snipe cascade still terminates with
    // a valid result, never overflows the log array, and always ends on a
    // BEV_RESULT terminator (so playback can't run off the end of a truncated
    // log). Fox/Deer re-fire Alpaca snipes into 5 tanky Bulls.
    {
        PetSlot p[] = { mkL(16,4,4,3), mkL(12,4,4,3), mkL(15,4,4,3),
                        mkL(12,4,4,3), mkL(16,4,4,3) };
        PetSlot e[] = { mk(17,1,50), mk(17,1,50), mk(17,1,50),
                        mk(17,1,50), mk(17,1,50) };
        run_battle(p, 5, e, 5);
        bool bounded   = game.battleLogLen <= MAX_BATTLE_EVENTS;
        bool terminated = game.battleLogLen > 0 &&
            game.battleLog[game.battleLogLen - 1].kind == BEV_RESULT;
        check("Truncation: deep cascade bounded + RESULT terminator",
              bounded && terminated && game.battleResult <= 2);
    }

    // T47: Determinism — sound playback must NEVER touch the simulation RNG
    // (_rng_state), or the next battle/shop diverges from its log. Snapshot
    // the state, fire every SFX, assert it's untouched. (sfx_attack used to
    // call rng_float; this guards against that class of regression.)
    {
        uint32_t before = _rng_state;
        sfx_attack(); sfx_attack(); sfx_faint();
        sfx_buy(); sfx_sell(); sfx_roll(); sfx_win(); sfx_lose();
        check("Determinism: SFX leave sim RNG untouched", _rng_state == before);
    }

    // T48: Determinism — the same matchup + seed simulates byte-identically
    // (result, length, and every event), so a saved log replays the same fight.
    {
        PetSlot p[] = { mk(4,3,3), mk(6,3,3), mk(11,3,3) };   // summoner-heavy
        PetSlot e[] = { mk(0,3,3), mk(7,3,3), mk(10,3,3) };
        rng_seed(0xABCDEF01);
        run_battle(p, 3, e, 3);
        int r1 = game.battleResult, n1 = game.battleLogLen;
        static BattleEvent snap[MAX_BATTLE_EVENTS];
        for (int i = 0; i < n1; i++) snap[i] = game.battleLog[i];
        rng_seed(0xABCDEF01);
        run_battle(p, 3, e, 3);
        bool same = (r1 == game.battleResult) && (n1 == game.battleLogLen);
        for (int i = 0; same && i < n1; i++) {
            BattleEvent* a = &snap[i]; BattleEvent* b = &game.battleLog[i];
            if (a->kind != b->kind || a->side != b->side || a->slot != b->slot ||
                a->value != b->value || a->data != b->data) same = false;
        }
        check("Determinism: same seed -> identical battle log", same);
    }

    // T49: Deer/Fox re-fire is attack-order independent (audit guard). T40
    // covers Fox firing BEFORE its neighbor (Fox atk 4 > Bernard atk 1). Here
    // Bernard's atk is higher, so it takes its OWN start-of-battle turn first
    // and Fox re-fires it afterward — the total must still be 1 own + 3
    // re-fires = 4 snipes, proving the direct-call re-fire never double-counts
    // regardless of which pet the attack-ordered drain reaches first.
    {
        PetSlot p[] = { mkL(16,1,1,3), mk(12,10,80) };   // Fox L3 (low atk), Bernard (high atk)
        PetSlot e[] = { mk(0,1,200) };                   // tanky soak so every snipe lands
        run_battle(p, 2, e, 1);
        int n8 = 0;
        for (int i = 0; i < game.battleLogLen; i++)
            if (game.battleLog[i].kind == BEV_ABILITY &&
                game.battleLog[i].value == 8) n8++;
        check("Deer/Fox: re-fire count is attack-order independent", n8 == 4);
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
