#include "game.h"

// Forward declarations of pet handlers (defined below). Naming pattern:
//   pet_<name>_<trigger>
// Each handler runs once per matching trigger event for the pet that owns it.

static void pet_mouse_faint     (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_dog_summon      (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_frog_eot        (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_bee_sob         (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_cricket_faint   (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_cow_buy         (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

static void pet_sheep_faint     (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_hedgehog_faint  (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_horse_summon    (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_ox_friendfaint  (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_snake_friendatk (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_spider_faint    (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

static void pet_croc_sob        (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_boar_beforeatk  (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_dragon_buy      (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

// Whale/Parrot/Tiger are deferred to v0.1 — handlers are NULL.

// Forward-declared so handlers/foods can spawn or apply
static void summon_pet_in_slot(BattleState* bs, uint8_t side, uint8_t slot,
                               uint8_t petId, int16_t atk, int16_t hp);
static int  first_empty_slot_side(BattleState* bs, uint8_t side);
static int  last_alive_slot_side(BattleState* bs, uint8_t side);
static int  random_alive_slot_side(BattleState* bs, uint8_t side);

// Food handlers
static void food_apple_apply        (Team* team, uint8_t targetSlot);
static void food_honey_apply        (Team* team, uint8_t targetSlot);
static void food_meatbone_apply     (Team* team, uint8_t targetSlot);
static void food_garlic_apply       (Team* team, uint8_t targetSlot);
static void food_sleepingpill_apply (Team* team, uint8_t targetSlot);
static void food_milk_apply         (Team* team, uint8_t targetSlot);

// ---------------------------------------------------------------------------
// Pet table — handlers wired by trigger kind index
// ---------------------------------------------------------------------------
#define H_FAINT     [TRIG_FAINT]               =
#define H_SUMMON    [TRIG_FRIEND_SUMMONED]     =
#define H_FAINTFRD  [TRIG_FRIEND_FAINTS]       =
#define H_FRDATK    [TRIG_FRIEND_ATTACK]       =
#define H_EOT       [TRIG_END_OF_TURN]         =
#define H_SOB       [TRIG_START_OF_BATTLE]     =
#define H_BUY       [TRIG_BUY]                 =
#define H_BEFOREATK [TRIG_BEFORE_ATTACK]       =

const PetDef PETS[PET_COUNT] = {
    // Tier 1
    [0]  = { "Mouse",     1, 2, 1,  0, { H_FAINT  pet_mouse_faint } },
    [1]  = { "Dog",       3, 2, 1,  1, { H_SUMMON pet_dog_summon } },
    [2]  = { "Frog",      1, 3, 1,  2, { H_EOT    pet_frog_eot } },
    [3]  = { "Bee",       2, 1, 1,  3, { H_SOB    pet_bee_sob } },
    [4]  = { "Cricket",   2, 2, 1,  4, { H_FAINT  pet_cricket_faint } },
    [5]  = { "Cow",       3, 3, 1,  5, { H_BUY    pet_cow_buy } },
    // Tier 2
    [6]  = { "Sheep",     2, 2, 2,  6, { H_FAINT  pet_sheep_faint } },
    [7]  = { "Hedgehog",  3, 3, 2,  7, { H_FAINT  pet_hedgehog_faint } },
    [8]  = { "Horse",     1, 1, 2,  8, { H_SUMMON pet_horse_summon } },
    [9]  = { "Ox",        1, 4, 2,  9, { H_FAINTFRD pet_ox_friendfaint } },
    [10] = { "Snake",     3, 3, 2, 10, { H_FRDATK pet_snake_friendatk } },
    [11] = { "Spider",    2, 2, 2, 11, { H_FAINT  pet_spider_faint } },
    // Tier 3
    [12] = { "Crocodile", 5, 3, 3, 12, { H_SOB    pet_croc_sob } },
    [13] = { "Whale",     3, 8, 3, 13, { 0 } },               // deferred
    [14] = { "Boar",      8, 6, 3, 14, { H_BEFOREATK pet_boar_beforeatk } },
    [15] = { "Parrot",    3, 3, 3, 15, { 0 } },               // deferred
    [16] = { "Tiger",     4, 3, 3, 16, { 0 } },               // deferred
    [17] = { "Dragon",    6, 8, 3, 17, { H_BUY    pet_dragon_buy } },
};

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------
const PetDef* pet_def(uint8_t id) {
    if (id >= PET_COUNT) return NULL;
    return &PETS[id];
}

const char* pet_name(uint8_t id) {
    return id < PET_COUNT ? PETS[id].name : "?";
}

int pet_xp_for_level(uint8_t level) {
    if (level <= 1) return 0;
    if (level == 2) return LEVEL_2_XP;
    return LEVEL_2_XP + LEVEL_3_XP;
}

uint8_t pet_random_in_tier(uint8_t maxTier) {
    uint8_t pool[PET_COUNT];
    int n = 0;
    for (uint8_t i = 0; i < PET_COUNT; i++) {
        if (PETS[i].tier <= maxTier) pool[n++] = i;
    }
    if (n == 0) return 0;
    return pool[rng_range(0, n - 1)];
}

// ---------------------------------------------------------------------------
// Foods (handlers for the simpler ones; others are stubs for now)
// ---------------------------------------------------------------------------
const FoodDef FOODS[FOOD_COUNT] = {
    [0] = { "Apple",         3, 0, true,  food_apple_apply },
    [1] = { "Honey",         3, 1, true,  food_honey_apply },
    [2] = { "Meat Bone",     3, 2, true,  food_meatbone_apply },
    [3] = { "Garlic",        3, 3, true,  food_garlic_apply },
    [4] = { "Sleeping Pill", 1, 4, true,  food_sleepingpill_apply },
    [5] = { "Milk",          3, 5, false, food_milk_apply },
};

// ---------------------------------------------------------------------------
// Battle helpers
// ---------------------------------------------------------------------------
static int first_empty_slot_side(BattleState* bs, uint8_t side) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id == EMPTY_PET) return i;
    }
    return -1;
}

static int last_alive_slot_side(BattleState* bs, uint8_t side) {
    for (int i = TEAM_SIZE - 1; i >= 0; i--) {
        if (bs->teams[side].slots[i].id != EMPTY_PET) return i;
    }
    return -1;
}

static int random_alive_slot_side(BattleState* bs, uint8_t side) {
    int alive[TEAM_SIZE];
    int n = 0;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id != EMPTY_PET) alive[n++] = i;
    }
    if (n == 0) return -1;
    return alive[rng_range(0, n - 1)];
}

static void deal_damage_to_slot(BattleState* bs, uint8_t side, int slot, int dmg) {
    if (slot < 0 || slot >= TEAM_SIZE) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    if (p->id == EMPTY_PET) return;
    // Garlic flag reduces damage taken by 2
    if (p->flags & 0x01) dmg = (dmg > 2) ? dmg - 2 : 0;
    p->hp -= (int16_t)dmg;
    battle_log_push(BEV_DAMAGE, side, (uint8_t)slot, (int16_t)dmg, 0);
}

static void summon_pet_in_slot(BattleState* bs, uint8_t side, uint8_t slot,
                               uint8_t petId, int16_t atk, int16_t hp)
{
    if (slot >= TEAM_SIZE) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    p->id = petId;
    p->level = 1;
    p->xp = 0;
    p->atk = atk;
    p->hp = hp;
    p->food = EMPTY_FOOD;
    p->flags = 0;
    battle_log_push(BEV_SPAWN, side, slot, 0, petId);
    trig_push(TRIG_FRIEND_SUMMONED, side, slot, slot, petId);
}

// ---------------------------------------------------------------------------
// Tier 1 pet abilities
// ---------------------------------------------------------------------------
static void pet_mouse_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires only on our OWN faint
    if (ev->side != side || ev->srcSlot != slot) return;
    int dmg = 1 * (int)bs->teams[side].slots[slot].level;
    int targetSide = side ^ 1;
    int t = random_alive_slot_side(bs, (uint8_t)targetSide);
    if (t < 0) return;
    deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    battle_log_push(BEV_ABILITY, side, slot, dmg, (uint8_t)t);
}

static void pet_dog_summon(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when any ally is summoned on our side, except us
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    int lvl = (int)p->level;
    p->atk = (int16_t)mini(p->atk + lvl, STAT_CAP);
    p->hp  = (int16_t)mini(p->hp + lvl, STAT_CAP);
    battle_log_push(BEV_BUFF, side, slot, (int16_t)(lvl + (lvl << 8)), 0);
    battle_log_push(BEV_ABILITY, side, slot, 0, 0);
}

static void pet_frog_eot(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    (void)ev;
    if (slot == 0) return;  // no slot to the left
    int targetSlot = (int)slot - 1;
    PetSlot* t = &bs->teams[side].slots[targetSlot];
    if (t->id == EMPTY_PET) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    t->atk = (int16_t)mini(t->atk + lvl, STAT_CAP);
    t->hp  = (int16_t)mini(t->hp + lvl, STAT_CAP);
    battle_log_push(BEV_BUFF, side, (uint8_t)targetSlot, (int16_t)(lvl + (lvl << 8)), 0);
}

static void pet_bee_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &bs->teams[side].slots[i];
        if (p->id == EMPTY_PET) continue;
        p->atk = (int16_t)mini(p->atk + lvl, STAT_CAP);
        battle_log_push(BEV_BUFF, side, (uint8_t)i, (int16_t)(lvl), 0);
    }
}

static void pet_cricket_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    // Spawn a "Zombie Cricket" in our slot (we reuse Cricket id with level=1, low stats)
    // Level scales the spawn's stats.
    int16_t s = (int16_t)lvl;
    summon_pet_in_slot(bs, side, slot, 4 /*Cricket*/, s, s);
}

static void pet_cow_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires only when WE were bought
    if (ev->side != side || ev->srcSlot != slot) return;
    (void)bs;
    // Add Milk to the first empty food slot (shop trigger)
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        if (game.shop.foods[i] == EMPTY_FOOD) {
            game.shop.foods[i] = 5;  // Milk
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Tier 2 pet abilities
// ---------------------------------------------------------------------------
static void pet_sheep_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int16_t ramAtk = (int16_t)(3 * lvl);
    int16_t ramHp  = (int16_t)(3 * lvl);
    // Spawn first Ram in our slot
    summon_pet_in_slot(bs, side, slot, 8 /*Horse acts as Ram for v0*/, ramAtk, ramHp);
    // Try to spawn the second in the next empty slot
    int t = first_empty_slot_side(bs, side);
    if (t >= 0) {
        summon_pet_in_slot(bs, side, (uint8_t)t, 8, ramAtk, ramHp);
    }
}

static void pet_hedgehog_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg = 2 * lvl;
    for (int sd = 0; sd < 2; sd++) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            deal_damage_to_slot(bs, (uint8_t)sd, i, dmg);
        }
    }
    battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, 0);
}

static void pet_horse_summon(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Buff the friend that was just summoned (in ev->dataSlot), unless it's us.
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    PetSlot* t = &bs->teams[side].slots[ev->dataSlot];
    if (t->id == EMPTY_PET) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    t->atk = (int16_t)mini(t->atk + lvl, STAT_CAP);
    battle_log_push(BEV_BUFF, side, ev->dataSlot, (int16_t)lvl, 0);
}

static void pet_ox_friendfaint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when an ally OTHER THAN US faints
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    int lvl = (int)p->level;
    p->atk = (int16_t)mini(p->atk + lvl, STAT_CAP);
    battle_log_push(BEV_BUFF, side, slot, (int16_t)lvl, 0);
}

static void pet_snake_friendatk(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when an ally attacks (not us). Deal 5*lvl to random enemy.
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg = 5 * lvl;
    int targetSide = side ^ 1;
    int t = random_alive_slot_side(bs, (uint8_t)targetSide);
    if (t < 0) return;
    deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
}

static void pet_spider_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    // Summon a random T1 pet at 3*lvl / 3*lvl in our slot
    uint8_t pid = pet_random_in_tier(1);
    summon_pet_in_slot(bs, side, slot, pid, (int16_t)(3 * lvl), (int16_t)(3 * lvl));
}

// ---------------------------------------------------------------------------
// Tier 3 pet abilities (simplified for v0)
// ---------------------------------------------------------------------------
static void pet_croc_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg = 8 * lvl;
    int targetSide = side ^ 1;
    int t = last_alive_slot_side(bs, (uint8_t)targetSide);
    if (t < 0) return;
    deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
}

static void pet_boar_beforeatk(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires only when WE are the one attacking
    if (ev->side != side || ev->srcSlot != slot) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    int lvl = (int)p->level;
    p->atk = (int16_t)mini(p->atk + 2 * lvl, STAT_CAP);
    p->hp  = (int16_t)mini(p->hp + 2 * lvl, STAT_CAP);
    battle_log_push(BEV_BUFF, side, slot, (int16_t)((2*lvl) + ((2*lvl) << 8)), 0);
}

static void pet_dragon_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires whenever ANY T1 pet is bought (ev->data holds the bought pet's tier)
    (void)slot;
    if (ev->side != side) return;
    int boughtTier = (int)ev->data;
    if (boughtTier != 1) return;
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &bs->teams[side].slots[i];
        if (p->id == EMPTY_PET) continue;
        p->atk = (int16_t)mini(p->atk + 1, STAT_CAP);
        p->hp  = (int16_t)mini(p->hp + 1, STAT_CAP);
    }
}

// ---------------------------------------------------------------------------
// Food handlers
// ---------------------------------------------------------------------------
static void food_apple_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->atk = (int16_t)mini(p->atk + 1, STAT_CAP);
    p->hp  = (int16_t)mini(p->hp + 1, STAT_CAP);
}

static void food_honey_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->food = 1;  // Honey held-item flag: spawn Bee Buddy on faint (handled at faint time later)
}

static void food_meatbone_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->food = 2;  // Held: +5 atk in next battle (apply at start of battle later)
    p->atk = (int16_t)mini(p->atk + 5, STAT_CAP);  // crude v0: apply permanently
}

static void food_garlic_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->flags |= 0x01;  // Damage taken -2 in next battle
    p->food = 3;
}

static void food_sleepingpill_apply(Team* team, uint8_t targetSlot) {
    // Immediately faints the pet (synergy enabler — fires shop-context FAINT)
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    // For v0 simplicity: just clear the slot (no shop-faint dispatch yet)
    p->id = EMPTY_PET;
    p->level = 1;
    p->xp = 0;
    p->atk = 0;
    p->hp = 0;
    p->food = EMPTY_FOOD;
    p->flags = 0;
}

static void food_milk_apply(Team* team, uint8_t targetSlot) {
    (void)targetSlot;
    // Random ally on the team gets +2/+2
    int alive[TEAM_SIZE];
    int n = 0;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (team->slots[i].id != EMPTY_PET) alive[n++] = i;
    }
    if (n == 0) return;
    int idx = alive[rng_range(0, n - 1)];
    team->slots[idx].atk = (int16_t)mini(team->slots[idx].atk + 2, STAT_CAP);
    team->slots[idx].hp  = (int16_t)mini(team->slots[idx].hp + 2, STAT_CAP);
}
