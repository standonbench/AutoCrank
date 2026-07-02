#include "game.h"
#include "theme.h"
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
#if DEBUG_BUILD
static void debug_force_team(int presetIdx);
#endif

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void game_init(void)
{
    memset(&game, 0, sizeof(Game));
    game.state = STATE_TITLE;
    game.selectedTeamSlot = 0xFF;
    game.pendingFoodSlot = 0xFF;

    images_init();
    sound_init();
    save_load();

    // Body text: Pedallica Fun — quirky-but-tight angular sans with
    // Playdate personality. Banner accent: Sasser Slab Bold — heavy chalet
    // serif for "ROUND 1" / "YOU WIN!" / titles.
    const char* fontErr = NULL;
    game.fontBold = pd->graphics->loadFont("fonts/font-pedallica-fun-14", &fontErr);
    if (!game.fontBold) DLOG("font bold: %s", fontErr ? fontErr : "?");
    game.fontLarge = pd->graphics->loadFont("fonts/Sasser-Slab-Bold", &fontErr);
    if (!game.fontLarge) DLOG("font large: %s", fontErr ? fontErr : "?");
    game.fontSmall = pd->graphics->loadFont("fonts/font-pedallica", &fontErr);
    if (!game.fontSmall) DLOG("font small: %s", fontErr ? fontErr : "?");

    register_debug_menu();
    DLOG("AutoCrank initialized.");
}

// ---------------------------------------------------------------------------
// System-menu items (hold MENU).
//
// The Playdate OS shows at most THREE game menu items, so we surface the
// player-facing "Sound" toggle (every build) plus — in dev builds only — a
// "Cheat" picker and a consolidated "Scene" picker (preview jumps + forced
// synergy teams + FPS overlay). Everything under Sound is compiled out of
// release builds (DEBUG_BUILD=0), so players never see debug controls.
// ---------------------------------------------------------------------------
static PDMenuItem* g_mSound = NULL;

// Player-facing setting: toggle SFX. Checked = sound on. Persists to save.
static void menu_sound_toggle(void* userdata) {
    (void)userdata;
    if (!g_mSound) return;
    if (pd->system->getMenuItemValue(g_mSound))
        game.settings &= (uint8_t)~SETTING_SFX_MUTE;   // checked -> sound on
    else
        game.settings |= SETTING_SFX_MUTE;             // unchecked -> muted
    save_write();
}

#if DEBUG_BUILD
static PDMenuItem* g_mCheat = NULL;
static PDMenuItem* g_mScene = NULL;

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
        case 3: // jump to the late game: hardest AI + biggest FX (turn 9 / 8 wins)
            game.turn = 9;
            game.wins = 8;
            game.shop.gold = STARTING_GOLD;
            game.shop.frozenPets = 0;
            game.shop.frozenFoods = 0;
            shop_refill();
            DLOG("cheat: jumped to turn 9 / 8 wins");
            break;
        case 4: // instant win current battle (if in battle/result state)
            if (game.state == STATE_BATTLE || game.state == STATE_PRE_BATTLE ||
                game.state == STATE_RESULT) {
                game.battleResult = 0;
                game.battle.inBattle = true;   // arm the one-shot apply guard
                game_apply_battle_result();     // tally the win now
                game.state = STATE_RESULT;
                game.resultTimer = 61;
            }
            DLOG("cheat: win battle");
            break;
        case 5: // reset run
            game.inRun = false;
            game.state = STATE_TITLE;
            save_write();
            DLOG("cheat: reset to title");
            break;
    }
    pd->system->setMenuItemValue(g_mCheat, 0);
}

// Jump straight to an animated screen — saves 5+ minutes per polish iteration.
// idx: 1=PreBattle 2=Win 3=Loss 4=Draw 5=GameOver 6=Victory
static void preview_jump(int idx) {
    if (idx == 0) return;

    // Common: ensure a plausible run/team is set up so the animations have
    // something to draw.
    if (!game.inRun) game_start_run();
    // give the player a recognisable team so we can eyeball it
    const uint8_t demo[5] = { 5, 13, 7, 17, 16 };  // Calf Highland Goat Bear Fox
    for (int i = 0; i < TEAM_SIZE; i++) {
        uint8_t pid = demo[i];
        const PetDef* def = pet_def(pid);
        game.player.slots[i].id = pid;
        game.player.slots[i].level = 2;
        game.player.slots[i].xp = 0;
        game.player.slots[i].atk = def ? def->baseAtk : 3;
        game.player.slots[i].hp  = def ? def->baseHp  : 3;
        game.player.slots[i].food = EMPTY_FOOD;
        game.player.slots[i].flags = 0;
    }
    // Player name "Sus Yodelers" so screens have a real string to show.
    game.playerName.adj  = 15;  // Sus
    game.playerName.noun = 3;   // Yodelers
    game.enemyName.adj   = 1;   // Horny
    game.enemyName.noun  = 21;  // Pretzels

    switch (idx) {
    case 1: // PreBattle
        ai_generate_opponent(&game.battle.teams[1], game.turn,
                             game.runSeed ^ (game.turn * 0x9E3779B9));
        game.battle.teams[0] = game.player;
        game.state = STATE_PRE_BATTLE;
        game.preBattleTimer = 90;
        break;
    case 2: // Result Win
        game.battleResult = 0;
        game.state = STATE_RESULT;
        game.resultTimer = 0;
        break;
    case 3: // Result Loss
        game.battleResult = 1;
        if (game.lives > 0) game.lives--;
        game.state = STATE_RESULT;
        game.resultTimer = 0;
        break;
    case 4: // Result Draw
        game.battleResult = 2;
        game.state = STATE_RESULT;
        game.resultTimer = 0;
        break;
    case 5: // GameOver
        game.lives = 0;
        game.state = STATE_GAMEOVER;
        game.resultTimer = 0;
        break;
    case 6: // Victory
        game.wins = WINS_TO_VICTORY;
        hof_record_win();
        game.state = STATE_VICTORY;
        game.resultTimer = 0;
        break;
    }
}

// Consolidated dev picker: preview jumps (1-6), forced synergy teams (7-9),
// and the FPS/debug overlay (10). Resets to rest after acting.
static void menu_scene(void* userdata) {
    (void)userdata;
    if (!g_mScene) return;
    int idx = pd->system->getMenuItemValue(g_mScene);
    if (idx >= 1 && idx <= 6) {
        preview_jump(idx);
    } else if (idx >= 7 && idx <= 9) {
        static const int preset[] = { 4, 5, 1 };  // F:Dmg, F:Copy, F:Death
        debug_force_team(preset[idx - 7]);
    } else if (idx == 10) {
        g_debugOverlay = !g_debugOverlay;
        DLOG("debug overlay = %d", (int)g_debugOverlay);
    }
    pd->system->setMenuItemValue(g_mScene, 0);
}
#endif // DEBUG_BUILD

static void register_debug_menu(void) {
    // Player-facing, every build: Sound on/off (checked = on), from saved state.
    g_mSound = pd->system->addCheckmarkMenuItem(
        "Sound", (game.settings & SETTING_SFX_MUTE) ? 0 : 1, menu_sound_toggle, NULL);

#if DEBUG_BUILD
    // Two dev tools fill the remaining slots (Playdate caps menu items at 3).
    static const char* cheatTitles[] = { "-", "+5g", "Turn 5", "Turn 9", "Win", "Reset" };
    g_mCheat = pd->system->addOptionsMenuItem(
        "Cheat", cheatTitles, 6, menu_cheat_battle, NULL);

    static const char* sceneTitles[] = {
        "-", "PreBattle", "Win", "Loss", "Draw", "GameOver", "Victory",
        "F:Dmg", "F:Copy", "F:Death", "FPS"
    };
    g_mScene = pd->system->addOptionsMenuItem(
        "Scene", sceneTitles, 11, menu_scene, NULL);
#endif
}

#if DEBUG_BUILD
// Pre-baked test teams for fast synergy iteration.
// Each preset is up to 5 pet IDs (EMPTY_PET = empty slot).
static const uint8_t DBG_TEAM_DEATH[5]  = {  4,  6,  9, 13,  7 }; // Cricket Sheep Ox Whale Hedgehog
static const uint8_t DBG_TEAM_SUMMON[5] = {  1,  8,  4,  6,  9 }; // Dog Horse Cricket Sheep Ox
static const uint8_t DBG_TEAM_BUFF[5]   = {  2,  3, 14, 16, 17 }; // Frog Bee Boar Tiger Dragon
static const uint8_t DBG_TEAM_DAMAGE[5] = { 13,  7, 10, 12, 17 }; // Whale Hedgehog Snake Croc Dragon
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
#endif // DEBUG_BUILD

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
    game.pendingFoodSlot = 0xFF;
    game.confirmKind = CONFIRM_NONE;
    game.bankedGold = 0;
    game.freeFoodCharges = 0;
    game.crankShiftAccum = 0;
    game.cursorShown = false;
    game.shopRollAnim = 0;
    shop_popup_clear();
    game.totalRuns++;

    shop_init_run();
    shop_refill();
    th_wipe_start();
    game.state = STATE_SHOP;
    save_write();
}

void game_advance_to_battle(void)
{
    // Build enemy team
    ai_generate_opponent(&game.battle.teams[1], game.turn, game.runSeed ^ (game.turn * 0x9E3779B9));

    // Pick an enemy team name from the same word pool the player uses. Seeded
    // off the turn so each round's nemesis has a fresh identity.
    {
        uint32_t s = game.runSeed ^ (game.turn * 0xDEADBEEFu);
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        game.enemyName.adj  = (uint8_t)(s % NAME_ADJ_COUNT);
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        game.enemyName.noun = (uint8_t)(s % NAME_NOUN_COUNT);
        // Avoid an exact-self collision — a tiny indignity, but real.
        if (game.enemyName.adj  == game.playerName.adj &&
            game.enemyName.noun == game.playerName.noun) {
            game.enemyName.noun = (uint8_t)((game.enemyName.noun + 1) % NAME_NOUN_COUNT);
        }
    }

    // Snapshot player into battle state
    game.battle.teams[0] = game.player;
    game.battle.inBattle = true;

    // Salt Lick: +5 atk for the battle copy only (player team unchanged).
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &game.battle.teams[0].slots[i];
        if (p->id != EMPTY_PET && (p->flags & 0x02))
            p->atk = (int16_t)mini(p->atk + 5, STAT_CAP);
    }

    battle_simulate();              // populates battleLog (mutates teams[])
    game.battlePlayhead = 0;
    game.battleClock = 0.0f;
    game.battleSpeed = 1;

    // battle_simulate() left teams[] in their end state — restore the starting
    // line-up so the PRE_BATTLE screen shows the actual match-up.
    game.battle.teams[0] = game.battle.initial[0];
    game.battle.teams[1] = game.battle.initial[1];

    th_wipe_start();
    game.state = STATE_PRE_BATTLE;
    game.preBattleTimer = 90;       // 3.0s: 30f intro + 60f hold/exit
}

// Apply the battle outcome to the run — win/life tally, roster achievement
// ticks, one-shot food cleanup — and persist it. Called the moment the result
// screen opens (from battle_playback_step) so the screen shows the new win
// count / lost life and can animate the change. The inBattle flag guards it to
// exactly once per battle.
void game_apply_battle_result(void)
{
    if (!game.battle.inBattle) return;
    game.battle.inBattle = false;

    // Salt Lick is one-battle-only — clear the flag and consume the food
    // slot so the +4 atk doesn't carry over. Cowbell (garlic, bit 0) stays.
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &game.player.slots[i];
        if (p->id == EMPTY_PET) continue;
        if (p->flags & 0x02) {
            p->flags &= (uint8_t)~0x02;
            if (p->food == 2) p->food = EMPTY_FOOD;
        }
    }

    // Apply outcome
    if (game.battleResult == 0) {
        // Win
        game.wins++;
        if (game.wins > game.bestStreak) game.bestStreak = game.wins;
        game.totalWins++;
        // Roster achievement: tick "wins at L3" for each max-level pet that
        // was on the team for this victory. Saturate at 255.
        for (int i = 0; i < TEAM_SIZE; i++) {
            PetSlot* p = &game.player.slots[i];
            if (p->id < PET_COUNT && p->level >= MAX_PET_LEVEL &&
                game.petWinsAtMax[p->id] < 255) {
                game.petWinsAtMax[p->id]++;
            }
        }
    } else if (game.battleResult == 1) {
        // Loss
        if (game.lives > 0) game.lives--;
    }
    // Draw: no change

#if RUNLOG_ENABLED
    // One CSV line per battle: run identity, progress, outcome (0 win /
    // 1 loss / 2 draw), unspent gold, and the team as id:level pairs.
    {
        SDFile* f = pd->file->open("runlog.csv", kFileAppend);
        if (f) {
            char line[160];
            int n = snprintf(line, sizeof line, "%lu,%d,%d,%d,%d,%d",
                             (unsigned long)game.runSeed, (int)game.turn,
                             (int)game.wins, (int)game.lives,
                             (int)game.battleResult, (int)game.shop.gold);
            for (int i = 0; i < TEAM_SIZE && n < (int)sizeof line; i++) {
                PetSlot* p = &game.player.slots[i];
                n += snprintf(line + n, sizeof line - (size_t)n, ",%d:%d",
                              (p->id == EMPTY_PET) ? -1 : (int)p->id,
                              (int)p->level);
            }
            if (n < (int)sizeof line - 1) line[n++] = '\n';
            pd->file->write(f, line, (unsigned int)n);
            pd->file->close(f);
        }
    }
#endif

    save_write();
}

// Dismiss the result screen and advance to the next state. The outcome was
// already applied (and saved) by game_apply_battle_result when the screen
// opened, so this only routes to victory / game over / the next shop.
void game_after_battle(void)
{
    if (game.wins >= WINS_TO_VICTORY) {
        hof_record_win();
        game.state = STATE_VICTORY;
        game.resultTimer = 0;
        sfx_win();
    } else if (game.lives == 0) {
        game.state = STATE_GAMEOVER;
        game.resultTimer = 0;
        sfx_lose();
    } else {
        shop_end_turn_advance();
        game.cursorShown = false;
        game.shopRollAnim = 0;
        game.pendingFoodSlot = 0xFF;
        game.confirmKind = CONFIRM_NONE;
        shop_popup_clear();
        th_wipe_start();
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

// Preview what buying `petId` would do — mirrors the dst logic in
// shop_action_a (combine first, else fill an empty slot) plus the XP/level
// math from shop_try_buy_pet, so the shop can telegraph it before the buy.
CombinePreview shop_combine_preview(uint8_t petId) {
    CombinePreview p = { -1, false, false, 1, 0, 0 };
    if (petId == EMPTY_PET) return p;
    int dst = first_combinable_slot(petId);
    if (dst >= 0) {
        PetSlot* s = &game.player.slots[dst];
        uint8_t needed = (uint8_t)pet_xp_for_level((uint8_t)(s->level + 1));
        p.dstSlot   = (int8_t)dst;
        p.isCombine = true;
        p.xpAfter   = (uint8_t)(s->xp + 1);
        p.xpNeeded  = needed;
        if (p.xpAfter >= needed) { p.levelsUp = true;  p.resultLevel = (uint8_t)(s->level + 1); }
        else                     { p.levelsUp = false; p.resultLevel = s->level; }
        return p;
    }
    dst = first_empty_team_slot();
    if (dst >= 0) { p.dstSlot = (int8_t)dst; }   // fresh buy into an empty slot
    return p;                                     // else dstSlot stays -1: team full
}

// --- Shop confirm modal -----------------------------------------------------
// A tiny A/B sub-state (pendingFoodSlot pattern): while confirmKind is set,
// shop_input swallows everything except A (accept) and B (cancel).
void shop_confirm_open(uint8_t kind, uint8_t teamSlot) {
    game.confirmKind = kind;
    game.confirmSlot = teamSlot;
}

// Prompt before marching with spendable gold: roll costs 1 and gold doesn't
// carry over, so any leftover coin can always buy *something*.
bool shop_should_confirm_battle(void) {
    return game.shop.gold >= ROLL_COST;
}

void shop_confirm_accept(void) {
    uint8_t kind = game.confirmKind;
    uint8_t slot = game.confirmSlot;
    game.confirmKind = CONFIRM_NONE;
    if (kind == CONFIRM_SELL) {
        shop_try_sell(slot);
        if (game.selectedTeamSlot == slot) {
            game.selectedTeamSlot = 0xFF;
            game.crankShiftAccum = 0;
        }
    } else if (kind == CONFIRM_BATTLE) {
        game_advance_to_battle();
    }
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
        if (FOODS[foodId].needsTarget) {
            int dst = first_present_team_slot();
            if (dst < 0) return;                          // nothing to feed
            if (game.shop.gold < FOODS[foodId].cost) return;
            // Enter target-selection: snap cursor onto a team pet and let
            // the player confirm with A (or cancel with B).
            game.pendingFoodSlot = shopSlot;
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END + dst);
            game.selectedTeamSlot = 0xFF;
            game.crankShiftAccum = 0;
        } else {
            shop_try_buy_food(shopSlot, 0);               // e.g. Milk
        }
        break;
    }
    case ROW_TEAM: {
        uint8_t teamSlot = (uint8_t)(c - CUR_SHOP_FOOD_END);
        if (game.pendingFoodSlot != 0xFF) {
            if (game.player.slots[teamSlot].id == EMPTY_PET) break;
            shop_try_buy_food(game.pendingFoodSlot, teamSlot);
            game.pendingFoodSlot = 0xFF;
            break;
        }
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
            if (shop_should_confirm_battle())
                shop_confirm_open(CONFIRM_BATTLE, 0);
            else
                game_advance_to_battle();
        }
        break;
    }
}

static void shop_action_b(void) {
    if (game.pendingFoodSlot != 0xFF) {
        // Cancel target-selection; the food stays in the shop.
        game.pendingFoodSlot = 0xFF;
        return;
    }
    uint8_t c = game.cursor;
    CursorRow r = cursor_row(c);
    if (r == ROW_SHOP_PETS) {
        shop_toggle_freeze_pet(c);
    } else if (r == ROW_SHOP_FOODS) {
        shop_toggle_freeze_food((uint8_t)(c - CUR_SHOP_PET_END));
    } else if (r == ROW_TEAM) {
        // B asks to sell the team pet under the cursor; the confirm modal
        // completes the sale on A (shop_confirm_accept).
        uint8_t teamSlot = (uint8_t)(game.cursor - CUR_SHOP_FOOD_END);
        if (game.player.slots[teamSlot].id != EMPTY_PET)
            shop_confirm_open(CONFIRM_SELL, teamSlot);
    }
}

// D-pad: up/down jumps rows; left/right moves within a row.
// Shop pets and shop foods render on a single visual row, so they navigate
// as one contiguous row (cursor 0 .. CUR_SHOP_FOOD_END-1).
static void shop_navigate(PDButtons pressed) {
    // Food-targeting mode: cursor is locked to the team row, and left/right
    // hop between *present* pets only (so the player can't land on a slot
    // with nothing to feed).
    if (game.pendingFoodSlot != 0xFF) {
        if (pressed & kButtonLeft) {
            for (int i = (int)game.cursor - 1; i >= CUR_SHOP_FOOD_END; i--) {
                if (game.player.slots[i - CUR_SHOP_FOOD_END].id != EMPTY_PET) {
                    game.cursor = (uint8_t)i; break;
                }
            }
        }
        if (pressed & kButtonRight) {
            for (int i = (int)game.cursor + 1; i < CUR_TEAM_END; i++) {
                if (game.player.slots[i - CUR_SHOP_FOOD_END].id != EMPTY_PET) {
                    game.cursor = (uint8_t)i; break;
                }
            }
        }
        return;
    }

    CursorRow r = cursor_row(game.cursor);
    bool inShop = (game.cursor < CUR_SHOP_FOOD_END);

    if (pressed & kButtonLeft) {
        if (inShop) {
            if (game.cursor > 0) game.cursor--;
        } else if (game.cursor > 0 &&
                   cursor_row((uint8_t)(game.cursor - 1)) == r) {
            game.cursor--;
        }
    }
    if (pressed & kButtonRight) {
        if (inShop) {
            if (game.cursor + 1 < CUR_SHOP_FOOD_END) game.cursor++;
        } else if (game.cursor + 1 < CURSOR_COUNT &&
                   cursor_row((uint8_t)(game.cursor + 1)) == r) {
            game.cursor++;
        }
    }
    if (pressed & kButtonDown) {
        // Move to row below at roughly the same column.
        if (inShop) {
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END +
                                    mini(game.cursor, TEAM_SIZE - 1));
        } else if (r == ROW_TEAM) {
            game.cursor = CUR_ROLL;
        }
    }
    if (pressed & kButtonUp) {
        if (r == ROW_ACTIONS) {
            game.cursor = (uint8_t)(CUR_SHOP_FOOD_END + 0);  // first team slot
        } else if (r == ROW_TEAM) {
            uint8_t col = (uint8_t)(game.cursor - CUR_SHOP_FOOD_END);
            game.cursor = (uint8_t)mini(col, CUR_SHOP_FOOD_END - 1);
        }
    }

    // Selection auto-clears if cursor leaves the team row
    if (cursor_row(game.cursor) != ROW_TEAM) {
        game.selectedTeamSlot = 0xFF;
        game.crankShiftAccum = 0;
    }
}

static void shop_input(PDButtons pressed) {
    if (game.shopRollAnim > 0) game.shopRollAnim--;

    // First-press reveal: shop opens with no visible selection so the
    // scene reads cleanly. Any d-pad press wakes the cursor at its current
    // position; A/B do nothing until then.
    PDButtons dpad = kButtonLeft | kButtonRight | kButtonUp | kButtonDown;
    if (!game.cursorShown) {
        if (pressed & dpad) game.cursorShown = true;
        return;
    }

    // Confirm modal swallows all input: A accepts, B cancels, the d-pad and
    // crank-reorder (below) are ignored until the player answers.
    if (game.confirmKind != CONFIRM_NONE) {
        if (pressed & kButtonA)      shop_confirm_accept();
        else if (pressed & kButtonB) game.confirmKind = CONFIRM_NONE;
        return;
    }

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
    if (game.goldFxFrames > 0) game.goldFxFrames--;

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
        if (p2 & kButtonB) { th_wipe_start(); game.state = STATE_TITLE; }
        selftest_render();
        break;
    }
    case STATE_GALLERY: {
        PDButtons c3, p3, r3;
        pd->system->getButtonState(&c3, &p3, &r3);
        if (p3 & kButtonB) { th_wipe_start(); game.state = STATE_TITLE; }
        ui_gallery_render();
        break;
    }
    case STATE_TEAM_NAME:
        ui_team_name_update();
        ui_team_name_render();
        break;
    case STATE_HALL_OF_FAME:
        ui_hof_update();
        ui_hof_render();
        break;
    case STATE_HELP:
        ui_help_update();
        ui_help_render();
        break;
    }

    // Scene-change curtain — always the very last draw of the frame.
    th_wipe_draw();

    if (g_debugOverlay) pd->system->drawFPS(SCREEN_W - 22, 2);
    return 1;
}

// ---------------------------------------------------------------------------
// Hall of Fame — snapshot the winning team into the ring buffer.
// ---------------------------------------------------------------------------
void hof_record_win(void) {
    WinRecord* r = &game.hof[game.hofNext];
    for (int i = 0; i < TEAM_SIZE; i++) {
        r->pets[i]   = game.player.slots[i].id;
        r->levels[i] = game.player.slots[i].level;
    }
    r->turn = game.turn;
    r->name = game.playerName;
    r->used = 1;
    game.hofNext = (uint8_t)((game.hofNext + 1) % HOF_SIZE);
    if (game.hofCount < HOF_SIZE) game.hofCount++;
    DLOG("HOF record: %s, turn %u", "(see name pool)", game.turn);
}
