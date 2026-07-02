#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pd_api.h"
#include "debug.h"
#include "mathutil.h"
#include "names.h"

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
#define TOPBAR_H 18

// ---------------------------------------------------------------------------
// Game balance / SAP-derived numbers
// ---------------------------------------------------------------------------
#define TEAM_SIZE        5
#define MAX_SHOP_PETS    3
#define MAX_SHOP_FOOD    2
#define STARTING_GOLD    10
#define STARTING_LIVES   5
#define WINS_TO_VICTORY  10
#define MAX_TIER         3
#define PET_COUNT        18
#define FOOD_COUNT       6
#define EMPTY_PET        0xFF
#define EMPTY_FOOD       0xFF

// Sentinel spriteId — Kenney's 1-bit pack has no good fit for our bovid /
// horned pets (only one cow sprite for 7+ pets that want one). Setting a
// pet's spriteId to NO_SPRITE skips image loading entirely and forces the
// procedural creature fallback in draw_creature, which handles cows, goats,
// ibex horns, bells, etc. natively.
#define NO_SPRITE        0xFF

#define PET_COST         3
#define FOOD_COST        3
#define ROLL_COST        1
#define LEVEL_2_XP       2   // need 2 XP (i.e. 1 combine) to reach level 2
#define LEVEL_3_XP       3   // need 3 additional XP to reach level 3
#define MAX_PET_LEVEL    3
#define STAT_CAP         50

// Player settings — bitfield in game.settings, persisted in save (V5+).
#define SETTING_SFX_MUTE  0x01   // sound effects muted via the system menu
#define SETTING_HELP_SEEN 0x02   // how-to-play has been shown once

// Deep SOB chains (Reine|Ibex|Reine|Ibex|Bernard) can generate hundreds of
// cascading ABILITY/DAMAGE events. 256 was tight enough to truncate playback
// on stressed late-game boards; 1024 has 3 KB RAM cost and headroom to spare.
#define MAX_BATTLE_EVENTS 1024
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
    STATE_SELFTEST,
    STATE_GALLERY,
    STATE_TEAM_NAME,
    STATE_HALL_OF_FAME,
    STATE_HELP,
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

// Team name: indices into NAME_ADJECTIVES / NAME_NOUNS (names.h).
typedef struct {
    uint8_t adj;
    uint8_t noun;
} TeamName;

// One slot in the Hall of Fame: snapshot of a winning team + its name + the
// turn it crossed the 10-win line. Persisted in save data.
#define HOF_SIZE 16
typedef struct {
    uint8_t  pets[TEAM_SIZE];     // pet id per slot (0xFF empty)
    uint8_t  levels[TEAM_SIZE];   // pet level per slot
    uint8_t  turn;                // turn count when the run was won
    TeamName name;
    uint8_t  used;                // 1 if this slot holds a real record
} WinRecord;

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
    Team teams[2];        // [0] player, [1] enemy — used by simulator AND playback (mutated)
    Team initial[2];      // snapshot taken before sim, restored on playback
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
    BEV_CLASH,         // front-line exchange: slot=player front, data=enemy
                       // front, value low byte=player dmg, high byte=enemy dmg
} BattleEventKind;

typedef struct {
    uint8_t kind;
    uint8_t side;
    uint8_t slot;
    int16_t value;
    uint8_t data;
} BattleEvent;

// ---------------------------------------------------------------------------
// Projectile + debris pools for battle FX. Projectiles are spawned at the
// start of a BEV_ABILITY barrage (one per upcoming BEV_DAMAGE in the same
// barrage) and arc on a parabola to the target. On landing they spawn
// debris chunks and a damage popup. Goat L3 + a chained Hedgehog/Edelweiss
// can fill the pool, so size is generous.
// ---------------------------------------------------------------------------
#define MAX_PROJECTILES 16
#define MAX_DEBRIS      64

typedef struct {
    int16_t sx, sy;          // launch point (source pet centre)
    int16_t ex, ey;          // landing point (target pet centre)
    int16_t age;             // frames since spawn (scaled by battleSpeed)
    int16_t flightLife;      // frames to land; 0 = inactive slot
    int16_t popupLife;       // additional frames after landing for damage popup
    uint8_t side, slot;      // target identity
    uint8_t damage;          // value to pop on impact
    uint8_t debrisDone;      // 1 once impact debris has spawned
} Projectile;

typedef struct {
    float   x, y;
    float   vx, vy;
    uint8_t age;
    uint8_t life;            // 0 = inactive slot
    uint8_t size;            // 2..4 px
    uint8_t pad;
} Debris;

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
    // One description per level (1..3). Index with [level - 1] so the shop
    // tooltip shows the actual leveled effect, not the L1 baseline.
    const char* desc[MAX_PET_LEVEL];
    PetHandler handlers[NUM_TRIGGER_KINDS];
} PetDef;

extern const PetDef PETS[PET_COUNT];

typedef void (*FoodApply)(Team* team, uint8_t targetSlot);

typedef struct {
    const char* name;
    uint8_t cost;
    uint8_t spriteId;
    bool needsTarget;
    // Turn at which this food first appears in the shop pool (SAP-style food
    // tiering). 1 = always available; 3 = T2 unlock; 5 = T3 unlock.
    uint8_t unlockTurn;
    const char* description;
    FoodApply apply;
} FoodDef;

extern const FoodDef FOODS[FOOD_COUNT];

// ---------------------------------------------------------------------------
// Shop confirm modal — a lightweight sub-state (like pendingFoodSlot) that
// swallows input until the player answers with A (accept) or B (cancel).
// ---------------------------------------------------------------------------
typedef enum {
    CONFIRM_NONE = 0,
    CONFIRM_SELL,     // "sell this pet?" — confirmSlot = team slot
    CONFIRM_BATTLE,   // "gold left in the purse — battle anyway?"
} ConfirmKind;

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
    uint8_t  petMaxLevel[PET_COUNT];   // highest level ever reached, per pet
    uint8_t  petWinsAtMax[PET_COUNT];  // battles won while pet was at L3

    // current run
    bool     inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;
    TeamName playerName;
    TeamName enemyName;          // refreshed each round in ai_generate_opponent

    // Hall of Fame — ring of last HOF_SIZE 10-win runs. hofNext is the
    // write head; hofCount caps at HOF_SIZE.
    WinRecord hof[HOF_SIZE];
    uint8_t   hofCount;
    uint8_t   hofNext;

    // STATE_TEAM_NAME UI: 3 random adj/noun choices and the cursor / picks.
    uint8_t  nameAdjChoices[3];
    uint8_t  nameNounChoices[3];
    uint8_t  nameAdjPick;        // 0..2 — cursor column when in adj row
    uint8_t  nameNounPick;       // 0..2 — cursor column when in noun row
    uint8_t  nameRow;            // 0 = adj row, 1 = noun row
    uint8_t  nameAdjConfirmed;   // 1 if A was pressed on this row's pick
    uint8_t  nameNounConfirmed;

    // STATE_HALL_OF_FAME UI: which entry is highlighted.
    uint8_t  hofCursor;

    // shop UI
    uint8_t  cursor;             // index into shop+team flat selection
    uint8_t  selectedTeamSlot;   // 0xFF = none
    int16_t  crankShiftAccum;
    bool     cursorShown;        // hidden on shop entry; first d-pad reveals it
    uint8_t  pendingFoodSlot;    // shop-slot of a targeted food awaiting a
                                 // team-slot choice (0xFF = no pending pick)
    uint8_t  confirmKind;        // ConfirmKind — active shop confirm modal
    uint8_t  confirmSlot;        // team slot the CONFIRM_SELL modal is about


    // battle
    BattleState battle;
    BattleEvent battleLog[MAX_BATTLE_EVENTS];
    uint16_t    battleLogLen;
    uint16_t    battlePlayhead;
    float       battleClock;
    uint8_t     battleResult;
    uint8_t     battleSpeed;

    // battle FX pools — projectiles + debris from ability/clash impacts.
    // barrageSpawnedAt = playhead index we last spawned projectiles for
    // (so the spawn fires exactly once when an ABILITY enters playback);
    // 0xFFFF means no barrage is currently owned.
    Projectile  projectiles[MAX_PROJECTILES];
    Debris      debris[MAX_DEBRIS];
    uint16_t    barrageSpawnedAt;

    // result screen
    uint16_t resultTimer;

    // misc
    uint32_t frameCount;
    uint16_t titleBlinkTimer;
    uint16_t preBattleTimer;
    uint8_t  titleCursor;        // title-screen signpost selection (0..3)
    uint8_t  shopRollAnim;       // countdown timer for the roll-sweep visual
    int8_t   lastGoldDelta;      // signed gold change for the topbar flash
    uint8_t  goldFxFrames;       // countdown for the gold-counter flash
    uint8_t  settings;           // SETTING_* bitfield (persisted in save V5)
} Game;

extern Game game;

// ---------------------------------------------------------------------------
// Function declarations
// ---------------------------------------------------------------------------

// game.c
// Shop confirm modal (game.c) — open/answer the A/B confirm sub-state.
void shop_confirm_open(uint8_t kind, uint8_t teamSlot);
void shop_confirm_accept(void);
bool shop_should_confirm_battle(void);

void game_init(void);
int  update(void* userdata);
void game_start_run(void);
void game_advance_to_battle(void);
void game_apply_battle_result(void);   // tally win/life when the result opens
void game_after_battle(void);          // dismiss result -> next state

// Shop combine telegraph: what pressing A on a given shop pet would do.
// dstSlot < 0 means the team is full with no combine target (A is inert).
typedef struct {
    int8_t  dstSlot;      // destination team slot, or -1 if none
    bool    isCombine;    // true = stacks onto an existing copy; false = new pet
    bool    levelsUp;     // a combine that crosses into a new level
    uint8_t resultLevel;  // level after the action (1 for a fresh buy)
    uint8_t xpAfter;      // combine XP after the action
    uint8_t xpNeeded;     // XP needed for the next level
} CombinePreview;
CombinePreview shop_combine_preview(uint8_t petId);

// images.c
void images_init(void);
LCDBitmap* image_pet(uint8_t id);
int image_pet_size(uint8_t id);    // native px of the loaded asset (32/64)
LCDBitmap* image_food(uint8_t id);
LCDBitmap* image_title_bg(void);
LCDBitmap* image_name_bg(void);

// render.c
void render_topbar(void);
void render_shop_screen(void);
void render_battle_screen(void);
void render_battle_reset_positions(void);
void render_battle_reset_slot_position(int side, int slot);
void projectiles_reset(void);

// Barrage detection — exposed so battle_event_window can collapse
// subsequent-in-barrage ABILITY/DAMAGE events to zero duration while the
// first ABILITY of the barrage drives the whole projectile animation.
bool ability_first_in_barrage(int idx);
bool damage_in_barrage(int idx);

// Shop layout — exposed so shop.c can anchor popups to the right slot.
extern const int SHOP_COL_X[3];
extern const int TEAM_COL_X[5];
extern const int FOOD_COL_X[2];

// Shop popup buffer — short floating text & sparks fired on shop actions.
// Pushed from shop.c, drained by render_shop_screen.
void shop_popup_text(int cx, int cy, const char* s);
// Gold returned if this team slot were sold now (level refund + Calf bonus).
int  shop_sell_refund(uint8_t teamSlot);
void shop_popup_burst(int cx, int cy);
void shop_popup_clear(void);
void render_unit(uint8_t petId, uint8_t level, int16_t atk, int16_t hp,
                 uint8_t heldFood, int cx, int baseY, float spriteScale,
                 bool selected, bool picked, bool faceLeft);

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
// Deer battle-long ability copies (see g_copiedPet in pets.c)
void pets_reset_copies(void);
void pets_clear_copy_slot(uint8_t side, uint8_t slot);
void pets_fire_copied(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

// battle.c
void battle_begin(void);
void battle_simulate(void);
void battle_playback_step(void);
void battle_log_push(BattleEventKind kind, uint8_t side, uint8_t slot, int16_t value, uint8_t data);
void battle_emit(BattleState* bs, BattleEventKind kind, uint8_t side, uint8_t slot, int16_t value, uint8_t data);
int  battle_event_duration(uint8_t kind);
int  battle_event_window(int playhead);
// Faint animation queries shared with the renderer: a faint whose fly-off
// already played inside its clash window, and membership of the current
// simultaneous-death group (see faint_group_head in battle.c).
bool battle_faint_folded(int faintIdx);
bool battle_faint_in_group(int playhead, int side, int slot);
// Drains all pending FAINTs through the trigger system, cascading. Returns
// true if any pet fainted. Used by battle sim AND by shop-context food
// triggers (Sleeping Pill) so synergies fire identically.
bool battle_resolve_faints(BattleState* bs);

// ai.c
uint8_t pet_roll_for_turn(uint8_t turn);
void ai_generate_opponent(Team* out, uint8_t turn, uint32_t seed);
void ai_sim_team(Team* out, uint8_t turn, uint32_t seed);
void ai_apply_handicap(Team* t, uint8_t wins, uint8_t lives, uint8_t turn);

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

// selftest.c
void selftest_run(void);
void selftest_render(void);

// ui.c gallery
void ui_gallery_render(void);

// ui.c team-name picker
void ui_team_name_enter(void);
void ui_team_name_update(void);
void ui_team_name_render(void);

// ui.c hall of fame
void ui_hof_enter(void);
void ui_hof_update(void);
void ui_hof_render(void);

// ui.c how-to-play
void ui_help_update(void);
void ui_help_render(void);

// game.c — record a 10-win run into the Hall of Fame ring buffer.
void hof_record_win(void);

// creatures.c
void draw_creature(uint8_t petId, int bx, int by);
void draw_cow(int bx, int by, int variant);
void draw_creature_scaled(uint8_t petId, int x, int y, float scale);
void draw_creature_xform(uint8_t petId, int cx, int cy, float scale,
                         float deg, bool flip);
void draw_creature_haloed(uint8_t petId, int cx, int cy, float scale,
                          bool flip);

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
