#include "game.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
Game game;
uint32_t _rng_state = 1;
bool g_debugOverlay = false;

// ---------------------------------------------------------------------------
// Forward decls (debug harness lives below init)
// ---------------------------------------------------------------------------
static void register_debug_menu(void);
static void debug_force_team(int presetIdx);

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void game_init(void)
{
    memset(&game, 0, sizeof(Game));
    game.state = STATE_TITLE;
    game.selectedTeamSlot = 0xFF;

    images_init();
    sound_init();
    save_load();

    const char* fontErr = NULL;
    game.fontBold = pd->graphics->loadFont("Asheville-Sans-14-Bold", &fontErr);
    if (!game.fontBold) DLOG("font bold: %s", fontErr ? fontErr : "?");
    game.fontLarge = pd->graphics->loadFont("Roobert-20-Medium", &fontErr);
    if (!game.fontLarge) DLOG("font large: %s", fontErr ? fontErr : "?");
    game.fontSmall = pd->graphics->loadFont("Roobert-11-Medium", &fontErr);
    if (!game.fontSmall) DLOG("font small: %s", fontErr ? fontErr : "?");

    register_debug_menu();
    DLOG("AutoCrank initialized.");
}

// ---------------------------------------------------------------------------
// Debug harness — accessible via Playdate system menu (hold MENU)
// ---------------------------------------------------------------------------
static const char* DBG_PRESETS[] = { "none", "Death", "Summon", "Buff", "Damage", "Copy" };
#define DBG_PRESET_COUNT 6

static void menu_toggle_debug(void* userdata) {
    (void)userdata;
    g_debugOverlay = !g_debugOverlay;
    DLOG("debug overlay = %d", (int)g_debugOverlay);
}

static PDMenuItem* g_mPreset = NULL;
static PDMenuItem* g_mCheat  = NULL;

static void menu_team_preset(void* userdata) {
    (void)userdata;
    if (!g_mPreset) return;
    int idx = pd->system->getMenuItemValue(g_mPreset);
    debug_force_team(idx);
}

static void menu_cheat_battle(void* userdata) {
    (void)userdata;
    if (!g_mCheat) return;
    int idx = pd->system->getMenuItemValue(g_mCheat);
    if (!game.inRun) { pd->system->setMenuItemValue(g_mCheat, 0); return; }
    switch (idx) {
        case 1: // +5 gold
            game.shop.gold += 5;
            DLOG("cheat: +5 gold (=%d)", game.shop.gold);
            break;
        case 2: // skip to turn 5
            game.turn = 5;
            game.shop.gold = STARTING_GOLD;
            game.shop.frozenPets = 0;
            game.shop.frozenFoods = 0;
            shop_refill();
            DLOG("cheat: jumped to turn 5");
            break;
        case 3: // instant win current battle (if in battle/result state)
            if (game.state == STATE_BATTLE || game.state == STATE_PRE_BATTLE ||
                game.state == STATE_RESULT) {
                game.battleResult = 0;
                game.state = STATE_RESULT;
                game.resultTimer = 61;
            }
            DLOG("cheat: win battle");
            break;
        case 4: // reset run
            game.inRun = false;
            game.state = STATE_TITLE;
            save_write();
            DLOG("cheat: reset to title");
            break;
    }
    pd->system->setMenuItemValue(g_mCheat, 0);
}

static void register_debug_menu(void) {
    pd->system->addCheckmarkMenuItem("Debug Overlay", 0, menu_toggle_debug, NULL);

    static const char* presetTitles[DBG_PRESET_COUNT];
    for (int i = 0; i < DBG_PRESET_COUNT; i++) presetTitles[i] = DBG_PRESETS[i];
    g_mPreset = pd->system->addOptionsMenuItem(
        "Force Team", presetTitles, DBG_PRESET_COUNT, menu_team_preset, NULL);

    static const char* cheatTitles[] = { "-", "+5g", "Turn 5", "Win", "Reset" };
    g_mCheat = pd->system->addOptionsMenuItem(
        "Cheat", cheatTitles, 5, menu_cheat_battle, NULL);
}

// Pre-baked test teams for fast synergy iteration.
// Each preset is up to 5 pet IDs (EMPTY_PET = empty slot).
static const uint8_t DBG_TEAM_DEATH[5]  = {  4,  6,  9, 11,  7 }; // Cricket Sheep Ox Spider Hedgehog
static const uint8_t DBG_TEAM_SUMMON[5] = {  1,  8,  4,  6, 11 }; // Dog Horse Cricket Sheep Spider
static const uint8_t DBG_TEAM_BUFF[5]   = {  2,  3, 14, 16, 17 }; // Frog Bee Boar Tiger Dragon
static const uint8_t DBG_TEAM_DAMAGE[5] = {  0,  7, 10, 12, 17 }; // Mouse Hedgehog Snake Croc Dragon
static const uint8_t DBG_TEAM_COPY[5]   = { 15, 16,  4,  6,  9 }; // Parrot Tiger Cricket Sheep Ox

static void debug_force_team(int presetIdx) {
    if (presetIdx == 0) return;  // "none"
    if (!game.inRun) {
        // Auto-start a run so the preset has somewhere to go
        game_start_run();
    }
    const uint8_t* src = NULL;
    switch (presetIdx) {
        case 1: src = DBG_TEAM_DEATH;  break;
        case 2: src = DBG_TEAM_SUMMON; break;
        case 3: src = DBG_TEAM_BUFF;   break;
        case 4: src = DBG_TEAM_DAMAGE; break;
        case 5: src = DBG_TEAM_COPY;   break;
        default: return;
    }
    for (int i = 0; i < TEAM_SIZE; i++) {
        uint8_t pid = src[i];
        if (pid == EMPTY_PET) {
            game.player.slots[i].id = EMPTY_PET;
            game.player.slots[i].level = 1;
            game.player.slots[i].xp = 0;
            game.player.slots[i].atk = 0;
            game.player.slots[i].hp = 0;
        } else {
            const PetDef* def = pet_def(pid);
            game.player.slots[i].id = pid;
            game.player.slots[i].level = 1;
            game.player.slots[i].xp = 0;
            game.player.slots[i].atk = def->baseAtk;
            game.player.slots[i].hp = def->baseHp;
        }
        game.player.slots[i].food = EMPTY_FOOD;
        game.player.slots[i].flags = 0;
    }
    DLOG("forced team preset %d", presetIdx);
}

// ---------------------------------------------------------------------------
// Run lifecycle
// ---------------------------------------------------------------------------
void game_start_run(void)
{
    memset(&game.player, 0xFF, sizeof(Team));   // EMPTY_PET = 0xFF fills slots
    for (int i = 0; i < TEAM_SIZE; i++) {
        game.player.slots[i].id = EMPTY_PET;
        game.player.slots[i].level = 1;
        game.player.slots[i].xp = 0;
        game.player.slots[i].atk = 0;
        game.player.slots[i].hp = 0;
        game.player.slots[i].food = EMPTY_FOOD;
        game.player.slots[i].flags = 0;
    }
    game.turn = 1;
    game.wins = 0;
    game.lives = STARTING_LIVES;
    game.runSeed = rng_next();
    game.inRun = true;
    game.cursor = 0;
    game.selectedTeamSlot = 0xFF;
    game.crankShiftAccum = 0;
    game.totalRuns++;

    shop_init_run();
    shop_refill();
    game.state = STATE_SHOP;
    save_write();
}

void game_advance_to_battle(void)
{
    // Build enemy team
    ai_generate_opponent(&game.battle.teams[1], game.turn, game.runSeed ^ (game.turn * 0x9E3779B9));

    // Snapshot player into battle state
    game.battle.teams[0] = game.player;
    game.battle.inBattle = true;

    battle_simulate();              // populates battleLog
    game.battlePlayhead = 0;
    game.battleClock = 0.0f;
    game.battleSpeed = 1;

    game.state = STATE_PRE_BATTLE;
    game.preBattleTimer = 60;       // 2 seconds at 30fps
}

void game_after_battle(void)
{
    game.battle.inBattle = false;

    // Apply outcome
    if (game.battleResult == 0) {
        // Win
        game.wins++;
        if (game.wins > game.bestStreak) game.bestStreak = game.wins;
        game.totalWins++;
    } else if (game.battleResult == 1) {
        // Loss
        if (game.lives > 0) game.lives--;
    }
    // Draw: no change

    // Restore HP for surviving pets to base+permanent (battle damage doesn't persist)
    // The player team is the source of truth, not battle.teams[0].
    // (battle.teams[0] was a snapshot.)

    save_write();

    if (game.wins >= WINS_TO_VICTORY) {
        game.state = STATE_VICTORY;
        sfx_win();
    } else if (game.lives == 0) {
        game.state = STATE_GAMEOVER;
        sfx_lose();
    } else {
        shop_end_turn_advance();
        game.state = STATE_SHOP;
    }
}

// ---------------------------------------------------------------------------
// Shop cursor layout — flat indexing across 4 rows:
//   0 .. MAX_SHOP_PETS-1                  : shop pets
//   MAX_SHOP_PETS .. MAX_SHOP_PETS+MAX_SHOP_FOOD-1 : shop foods
//   MAX_SHOP_PETS+MAX_SHOP_FOOD .. +TEAM_SIZE-1     : team slots
//   +TEAM_SIZE                          : ROLL button
//   +TEAM_SIZE+1                        : END TURN button
// ---------------------------------------------------------------------------
#define CUR_SHOP_PET_END   (MAX_SHOP_PETS)
#define CUR_SHOP_FOOD_END  (CUR_SHOP_PET_END + MAX_SHOP_FOOD)
#define CUR_TEAM_END       (CUR_SHOP_FOOD_END + TEAM_SIZE)
#define CUR_ROLL           (CUR_TEAM_END)
#define CUR_END_TURN       (CUR_TEAM_END + 1)
#define CURSOR_COUNT       (CUR_END_TURN + 1)

typedef enum { ROW_SHOP_PETS, ROW_SHOP_FOODS, ROW_TEAM, ROW_ACTIONS } CursorRow;

static CursorRow cursor_row(uint8_t c) {
    if (c < CUR_SHOP_PET_END)  return ROW_SHOP_PETS;
    if (c < CUR_SHOP_FOOD_END) return ROW_SHOP_FOODS;
    if (c < CUR_TEAM_END)      return ROW_TEAM;
    return ROW_ACTIONS;
}

// First empty team slot, or -1
static int first_empty_team_slot(void) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (game.player.slots[i].id == EMPTY_PET) return i;
    }
    return -1;
}

// First team slot holding `petId` and not yet maxed, or -1
static int first_combinable_slot(uint8_t petId) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (game.player.slots[i].id == petId &&
            game.player.slots[i].level < MAX_PET_LEVEL) return i;
    }
    return -1;
}

// First non-empty team slot, or -1
static int first_present_team_slot(void) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (game.player.slots[i].id != EMPTY_PET) return i;
    }
    return -1;
}

static void shop_action_a(void) {
    uint8_t c = game.cursor;
    CursorRow r = cursor_row(c);
    switch (r) {
    case ROW_SHOP_PETS: {
        uint8_t shopSlot = c;
        uint8_t petId = game.shop.pets[shopSlot];
        if (petId == EMPTY_PET) return;
        int dst = first_combinable_slot(petId);
        if (dst < 0) dst = first_empty_team_slot();
        if (dst < 0) return;  // team full and no combine target
        shop_try_buy_pet(shopSlot, (uint8_t)dst);
        break;
    }
    case ROW_SHOP_FOODS: {
        uint8_t shopSlot = (uint8_t)(c - CUR_SHOP_PET_END);
        uint8_t foodId = game.shop.foods[shopSlot];
        if (foodId == EMPTY_FOOD) return;
        int dst = first_present_team_slot();
        if (FOODS[foodId].needsTarget && dst < 0) return;
        shop_try_buy_food(shopSlot, (uint8_t)(dst < 0 ? 0 : dst));
        break;
    }
    case ROW_TEAM: {
        uint8_t teamSlot = (uint8_t)(c - CUR_SHOP_FOOD_END);
        if (game.player.slots[teamSlot].id == EMPTY_PET) break;
        // Toggle pickup for crank-reorder.
        game.selectedTeamSlot = (game.selectedTeamSlot == teamSlot) ? 0xFF : teamSlot;
        game.crankShiftAccum = 0;
        break;
    }
    case ROW_ACTIONS:
        if (c == CUR_ROLL) {
            shop_try_roll();
        } else if (c == CUR_END_TURN) {
            game_advance_to_battle();
        }
        break;
    }
}

static void shop_action_b(void) {
    uint8_t c = game.cursor;
    CursorRow r = cursor_row(c);
    if (r == ROW_SHOP_PETS) {
        shop_toggle_freeze_pet(c);
    } else if (r == ROW_SHOP_FOODS) {
        shop_toggle_freeze_food((uint8_t)(c - CUR_SHOP_PET_END));
    } else if (r == ROW_TEAM) {
        // B sells the team pet under the cursor.
        uint8_t teamSlot = (uint8_t)(game.cursor - CUR_SHOP_FOOD_END);
        shop_try_sell(teamSlot);
        if (game.selectedTeamSlot == teamSlot) {
            game.selectedTeamSlot = 0xFF;
            game.crankShiftAccum = 0;
        }
    }
}

// D-pad: up/down jumps rows; left/right moves within a row.
static void shop_navigate(PDButtons pressed) {
    CursorRow r = cursor_row(game.cursor);

    if (pressed & kButtonLeft) {
        if (game.cursor > 0 && cursor_row((uint8_t)(game.cursor - 1)) == r) {
            game.cursor--;
        }
    }
    if (pressed & kButtonRight) {
        if (game.cursor + 1 < CURSOR_COUNT &&
            cursor_row((uint8_t)(game.cursor + 1)) == r) {
            game.cursor++;
        }
    }
    if (pressed & kButtonDown) {
        // Move to row below at roughly same column.
        if (r == ROW_SHOP_PETS || r == ROW_SHOP_FOODS) {
            uint8_t col = (r == ROW_SHOP_PETS) ? game.cursor
                                                : (uint8_t)(game.cursor - CUR_SHOP_PET_END);
            uint8_t target = (uint8_t)(CUR_SHOP_FOOD_END + mini(col, TEAM_SIZE - 1));
            game.cursor = target;
        } else if (r == ROW_TEAM) {
            game.cursor = CUR_ROLL;
        }
    }
    if (pressed & kButtonUp) {
        if (r == ROW_ACTIONS) {
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END + 0);  // first team slot
        } else if (r == ROW_TEAM) {
            uint8_t col = (uint8_t)(game.cursor - CUR_SHOP_FOOD_END);
            game.cursor = (uint8_t)mini(col, MAX_SHOP_PETS - 1);
        }
    }

    // Selection auto-clears if cursor leaves the team row
    if (cursor_row(game.cursor) != ROW_TEAM) {
        game.selectedTeamSlot = 0xFF;
        game.crankShiftAccum = 0;
    }
}

static void shop_input(PDButtons pressed) {
    shop_navigate(pressed);
    if (pressed & kButtonA) shop_action_a();
    if (pressed & kButtonB) shop_action_b();

    // Crank: when a team pet is selected, accumulate rotation and shift slots.
    if (game.selectedTeamSlot != 0xFF) {
        float dc = pd->system->getCrankChange();
        game.crankShiftAccum += (int16_t)dc;
        const int16_t threshold = 30;  // ~30 degrees per slot
        while (game.crankShiftAccum >= threshold) {
            team_shift(game.selectedTeamSlot, 1);
            if (game.selectedTeamSlot < TEAM_SIZE - 1) game.selectedTeamSlot++;
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END + game.selectedTeamSlot);
            game.crankShiftAccum -= threshold;
        }
        while (game.crankShiftAccum <= -threshold) {
            team_shift(game.selectedTeamSlot, -1);
            if (game.selectedTeamSlot > 0) game.selectedTeamSlot--;
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END + game.selectedTeamSlot);
            game.crankShiftAccum += threshold;
        }
    }
}

// ---------------------------------------------------------------------------
// Main update — frame router
// ---------------------------------------------------------------------------
int update(void* userdata)
{
    (void)userdata;
    game.frameCount++;

    PDButtons current, pressed, released;
    pd->system->getButtonState(&current, &pressed, &released);

    GFX_CLEAR(kColorWhite);

    switch (game.state) {
    case STATE_TITLE:
        ui_title_update();
        ui_title_render();
        break;
    case STATE_SHOP:
        shop_input(pressed);
        render_shop_screen();
        break;
    case STATE_PRE_BATTLE:
        ui_pre_battle_update();
        ui_pre_battle_render();
        break;
    case STATE_BATTLE:
        battle_playback_step();
        render_battle_screen();
        break;
    case STATE_RESULT:
        ui_result_update();
        ui_result_render();
        break;
    case STATE_GAMEOVER:
        ui_gameover_update();
        ui_gameover_render();
        break;
    case STATE_VICTORY:
        ui_victory_update();
        ui_victory_render();
        break;
    case STATE_SELFTEST: {
        PDButtons c2, p2, r2;
        pd->system->getButtonState(&c2, &p2, &r2);
        if (p2 & kButtonB) game.state = STATE_TITLE;
        selftest_render();
        break;
    }
    case STATE_GALLERY: {
        PDButtons c3, p3, r3;
        pd->system->getButtonState(&c3, &p3, &r3);
        if (p3 & kButtonB) game.state = STATE_TITLE;
        ui_gallery_render();
        break;
    }
    }

    if (g_debugOverlay) pd->system->drawFPS(SCREEN_W - 22, 2);
    return 1;
}
