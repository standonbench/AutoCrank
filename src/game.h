#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pd_api.h"
#include "debug.h"
#include "mathutil.h"

// ---------------------------------------------------------------------------
// Global Playdate API pointer (set once in eventHandler)
// ---------------------------------------------------------------------------
extern PlaydateAPI* pd;

// ---------------------------------------------------------------------------
// GFX shorthand — color is always the LAST parameter to draw functions.
// ---------------------------------------------------------------------------
#define GFX_CLEAR(c)              pd->graphics->clear((c))
#define GFX_FILL(x,y,w,h,c)       pd->graphics->fillRect((x),(y),(w),(h),(c))
#define GFX_RECT(x,y,w,h,c)       pd->graphics->drawRect((x),(y),(w),(h),(c))
#define GFX_ELLIPSE(x,y,w,h,c)    pd->graphics->fillEllipse((x),(y),(w),(h),0,360,(c))
#define GFX_CIRCLE(x,y,w,h,lw,c)  pd->graphics->drawEllipse((x),(y),(w),(h),(lw),0,360,(c))
#define GFX_LINE(x1,y1,x2,y2,w,c) pd->graphics->drawLine((x1),(y1),(x2),(y2),(w),(c))
#define GFX_PUSH(bmp)             pd->graphics->pushContext((bmp))
#define GFX_POP()                 pd->graphics->popContext()

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
#define SCREEN_W 400
#define SCREEN_H 240

// ---------------------------------------------------------------------------
// Game balance / SAP-derived numbers
// ---------------------------------------------------------------------------
#define TEAM_SIZE        5
#define MAX_SHOP_PETS    5
#define MAX_SHOP_FOOD    2
#define STARTING_GOLD    10
#define STARTING_LIVES   5
#define WINS_TO_VICTORY  10
#define MAX_TIER         3
#define PET_COUNT        18
#define FOOD_COUNT       6
#define EMPTY_PET        0xFF
#define EMPTY_FOOD       0xFF

#define PET_COST         3
#define FOOD_COST        3
#define ROLL_COST        1
#define LEVEL_2_XP       2   // need 2 XP (i.e. 1 combine) to reach level 2
#define LEVEL_3_XP       3   // need 3 additional XP to reach level 3
#define MAX_PET_LEVEL    3
#define STAT_CAP         50

#define MAX_BATTLE_EVENTS 256
#define TRIG_QUEUE_MAX    64

// ---------------------------------------------------------------------------
// Game state machine
// ---------------------------------------------------------------------------
typedef enum {
    STATE_TITLE,
    STATE_SHOP,
    STATE_PRE_BATTLE,
    STATE_BATTLE,
    STATE_RESULT,
    STATE_GAMEOVER,
    STATE_VICTORY,
} GameState;

// ---------------------------------------------------------------------------
// Runtime structures
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t id;       // pet definition index, EMPTY_PET = empty
    uint8_t level;    // 1..3
    uint8_t xp;       // accumulated combine XP
    int16_t atk;      // current attack (base + permanent buffs)
    int16_t hp;       // current HP (base + permanent buffs - damage)
    uint8_t food;     // held food id, EMPTY_FOOD = none
    uint8_t flags;    // bit 0: garlic, bit 1: melon, etc. (battle-only)
} PetSlot;

typedef struct {
    PetSlot slots[TEAM_SIZE];
} Team;

typedef struct {
    uint8_t pets[MAX_SHOP_PETS];   // pet ids, EMPTY_PET = sold/empty
    uint8_t foods[MAX_SHOP_FOOD];  // food ids, EMPTY_FOOD = sold/empty
    uint8_t frozenPets;            // bitmask over MAX_SHOP_PETS
    uint8_t frozenFoods;           // bitmask over MAX_SHOP_FOOD
    int16_t gold;
} Shop;

// ---------------------------------------------------------------------------
// Trigger system (drives ability cascades)
// ---------------------------------------------------------------------------
typedef enum {
    TRIG_BUY,
    TRIG_SELL,
    TRIG_LEVEL_UP,
    TRIG_END_OF_TURN,
    TRIG_START_OF_BATTLE,
    TRIG_BEFORE_ATTACK,
    TRIG_AFTER_ATTACK,
    TRIG_FRIEND_SUMMONED,
    TRIG_FRIEND_FAINTS,
    TRIG_FRIEND_ATTACK,
    TRIG_AFTER_FRIEND_ABILITY,
    TRIG_FAINT,
    NUM_TRIGGER_KINDS,
} TriggerKind;

typedef struct {
    uint8_t kind;     // TriggerKind
    uint8_t side;     // 0 = player, 1 = enemy
    uint8_t srcSlot;  // pet slot that caused the trigger
    uint8_t dataSlot; // affected slot (e.g. who fainted)
    int16_t data;     // extra param
} TriggerEvent;

// ---------------------------------------------------------------------------
// Battle state (live during simulation + playback)
// ---------------------------------------------------------------------------
typedef struct {
    Team teams[2];     // [0] player, [1] enemy
    bool inBattle;
} BattleState;

typedef enum {
    BEV_ATTACK,
    BEV_DAMAGE,
    BEV_FAINT,
    BEV_SPAWN,
    BEV_BUFF,
    BEV_ABILITY,
    BEV_RESULT,        // value: 0=player win, 1=loss, 2=draw
} BattleEventKind;

typedef struct {
    uint8_t kind;
    uint8_t side;
    uint8_t slot;
    int16_t value;
    uint8_t data;
} BattleEvent;

// ---------------------------------------------------------------------------
// Pet & food definitions
// ---------------------------------------------------------------------------
typedef void (*PetHandler)(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

typedef struct {
    const char* name;
    uint8_t baseAtk;
    uint8_t baseHp;
    uint8_t tier;
    uint8_t spriteId;
    PetHandler handlers[NUM_TRIGGER_KINDS];
} PetDef;

extern const PetDef PETS[PET_COUNT];

typedef void (*FoodApply)(Team* team, uint8_t targetSlot);

typedef struct {
    const char* name;
    uint8_t cost;
    uint8_t spriteId;
    bool needsTarget;
    FoodApply apply;
} FoodDef;

extern const FoodDef FOODS[FOOD_COUNT];

// ---------------------------------------------------------------------------
// One Game struct holds everything (CrankSurvivor pattern)
// ---------------------------------------------------------------------------
typedef struct {
    GameState state;
    LCDFont*  fontBold;
    LCDFont*  fontLarge;
    LCDFont*  fontSmall;

    // lifetime stats
    uint32_t totalRuns;
    uint32_t totalWins;
    uint32_t bestStreak;
    uint8_t  unlockedPets[4];

    // current run
    bool     inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;

    // shop UI
    uint8_t  cursor;             // index into shop+team flat selection
    uint8_t  selectedTeamSlot;   // 0xFF = none
    int16_t  crankShiftAccum;

    // battle
    BattleState battle;
    BattleEvent battleLog[MAX_BATTLE_EVENTS];
    uint16_t    battleLogLen;
    uint16_t    battlePlayhead;
    float       battleClock;
    uint8_t     battleResult;
    uint8_t     battleSpeed;

    // result screen
    uint16_t resultTimer;

    // misc
    uint32_t frameCount;
    uint16_t titleBlinkTimer;
    uint16_t preBattleTimer;
} Game;

extern Game game;

// ---------------------------------------------------------------------------
// Function declarations
// ---------------------------------------------------------------------------

// game.c
void game_init(void);
int  update(void* userdata);
void game_start_run(void);
void game_advance_to_battle(void);
void game_after_battle(void);

// images.c
void images_init(void);
LCDBitmap* image_pet(uint8_t id);
LCDBitmap* image_food(uint8_t id);

// render.c
void render_pet_card(uint8_t petId, uint8_t level, int16_t atk, int16_t hp,
                     uint8_t heldFood, int x, int y, bool selected);
void render_empty_slot(int x, int y, bool selected);
void render_topbar(void);
void render_shop_screen(void);
void render_battle_screen(void);

// shop.c
void shop_init_run(void);
void shop_refill(void);
bool shop_try_buy_pet(uint8_t shopSlot, uint8_t teamSlot);
bool shop_try_buy_food(uint8_t shopSlot, uint8_t teamSlot);
bool shop_try_sell(uint8_t teamSlot);
bool shop_try_roll(void);
void shop_toggle_freeze_pet(uint8_t shopSlot);
void shop_toggle_freeze_food(uint8_t shopSlot);
void shop_end_turn_advance(void);
uint8_t shop_current_tier(void);
void team_shift(uint8_t fromSlot, int delta);

// triggers.c
void trig_reset(void);
void trig_push(TriggerKind kind, uint8_t side, uint8_t srcSlot, uint8_t dataSlot, int16_t data);
void trig_drain(BattleState* bs);
void trig_shop_drain(void);

// pets.c
const PetDef* pet_def(uint8_t id);
uint8_t pet_random_in_tier(uint8_t maxTier);
const char* pet_name(uint8_t id);
int  pet_xp_for_level(uint8_t level);

// battle.c
void battle_begin(void);
void battle_simulate(void);
void battle_playback_step(void);
void battle_log_push(BattleEventKind kind, uint8_t side, uint8_t slot, int16_t value, uint8_t data);

// ai.c
void ai_generate_opponent(Team* out, uint8_t turn, uint32_t seed);

// ui.c
void ui_title_update(void);
void ui_title_render(void);
void ui_gameover_update(void);
void ui_gameover_render(void);
void ui_victory_update(void);
void ui_victory_render(void);
void ui_pre_battle_update(void);
void ui_pre_battle_render(void);
void ui_result_update(void);
void ui_result_render(void);

// save.c
void save_load(void);
void save_write(void);

// sound.c
void sound_init(void);
void sfx_buy(void);
void sfx_sell(void);
void sfx_roll(void);
void sfx_attack(void);
void sfx_faint(void);
void sfx_win(void);
void sfx_lose(void);

#endif // GAME_H
