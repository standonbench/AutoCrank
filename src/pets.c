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
static void pet_tiger_sob       (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_parrot_sob      (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);
static void pet_whale_faint     (BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev);

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

// Swiss Alpine roster — Combat des Reines. Abilities unchanged from the
// original prototype; names + sprites re-themed. spriteId selects the
// procedural drawer + variant (see creatures.c).
const PetDef PETS[PET_COUNT] = {
    // Tier 1 — small alpine & young farm animals
    [0]  = { "Marmot",    1, 2, 1,  0, "Faint: 1 dmg to a random enemy.",          { H_FAINT  pet_mouse_faint } },
    [1]  = { "Pup",       3, 2, 1,  1, "Ally summoned: gain +1/+1.",               { H_SUMMON pet_dog_summon } },
    [2]  = { "Hen",       1, 3, 1,  2, "End of turn: +1/+1 to ally on left.",      { H_EOT    pet_frog_eot } },
    [3]  = { "Bee",       2, 1, 1,  3, "Battle start: +1 atk to all friends.",     { H_SOB    pet_bee_sob } },
    [4]  = { "Hopper",    2, 2, 1,  4, "Faint: spawn a 1/1 Hopper here.",          { H_FAINT  pet_cricket_faint } },
    [5]  = { "Calf",      3, 3, 1,  5, "On buy: add a Milk to the shop.",          { H_BUY    pet_cow_buy } },
    // Tier 2 — working farm animals
    [6]  = { "Sheep",     2, 2, 2,  6, "Faint: spawn two 3/3 Haflingers.",         { H_FAINT  pet_sheep_faint } },
    [7]  = { "Goat",      3, 3, 2,  7, "Faint: 2 dmg to ALL units.",               { H_FAINT  pet_hedgehog_faint } },
    [8]  = { "Haflinger", 1, 1, 2,  8, "Ally summoned: give it +1 atk.",           { H_SUMMON pet_horse_summon } },
    [9]  = { "Ox",        1, 4, 2,  9, "Ally faints: gain +1 atk.",                { H_FAINTFRD pet_ox_friendfaint } },
    [10] = { "Adder",     3, 3, 2, 10, "Ally attacks: 5 dmg to random enemy.",     { H_FRDATK pet_snake_friendatk } },
    [11] = { "Goose",     2, 2, 2, 11, "Faint: spawn a random T1 (3/3).",          { H_FAINT  pet_spider_faint } },
    // Tier 3 — champions of the alp
    [12] = { "Bernard",   5, 3, 3, 12, "Battle start: 8 dmg to the last enemy.",   { H_SOB    pet_croc_sob } },
    [13] = { "Highland",  3, 8, 3, 13, "Faint: 5 dmg to a random enemy.",          { H_FAINT  pet_whale_faint } },
    [14] = { "Boar",      8, 6, 3, 14, "Before attack: gain +2/+2.",               { H_BEFOREATK pet_boar_beforeatk } },
    [15] = { "Ibex",      3, 3, 3, 15, "Battle start: copy left ally's ability.",  { H_SOB    pet_parrot_sob } },
    [16] = { "Reine",     4, 3, 3, 16, "Battle start: re-fire right ally x lvl.",  { H_SOB    pet_tiger_sob } },
    [17] = { "Bull",      6, 8, 3, 17, "Buy a T1 ally: +1/+1 to all friends.",     { H_BUY    pet_dragon_buy } },
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
    // Alpine reskin — same mechanics, Swiss-pasture flavour.
    [0] = { "Hay",       3, 0, true,  "+1 atk and +1 hp to a friend.",        food_apple_apply },
    [1] = { "Edelweiss", 3, 1, true,  "Held: on faint, spawn a 1/1 Bee.",     food_honey_apply },
    [2] = { "Salt Lick", 3, 2, true,  "+5 atk for the next battle.",          food_meatbone_apply },
    [3] = { "Cowbell",   3, 3, true,  "Held: takes 2 less damage.",           food_garlic_apply },
    [4] = { "Cheese",    1, 4, true,  "Faint a friend (synergies fire).",     food_sleepingpill_apply },
    [5] = { "Milk",      3, 5, false, "Random friend gains +2/+2.",           food_milk_apply },
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
    // apply_event handles garlic reduction; just emit the raw damage.
    battle_emit(bs, BEV_DAMAGE, side, (uint8_t)slot, (int16_t)dmg, 0);
}

// Encode atk/hp pair into a single int16_t for BEV_SPAWN / BEV_BUFF
static inline int16_t encode_pair(int atk, int hp) {
    int a = clampi(atk, 0, 255);
    int h = clampi(hp, 0, 255);
    return (int16_t)((a & 0xFF) | ((h & 0xFF) << 8));
}

static void summon_pet_in_slot(BattleState* bs, uint8_t side, uint8_t slot,
                               uint8_t petId, int16_t atk, int16_t hp)
{
    if (slot >= TEAM_SIZE) return;
    battle_emit(bs, BEV_SPAWN, side, slot, encode_pair(atk, hp), petId);
    // Mark as spawned-via-faint so chain-respawn handlers bail (e.g. Hopper
    // spawning Zombie Hoppers infinitely when each Zombie's own death fires
    // the same faint handler). BEV_SPAWN's apply zeroes flags, so set here.
    bs->teams[side].slots[slot].flags |= 0x04;
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
    int lvl = (int)bs->teams[side].slots[slot].level;
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(lvl, lvl), 0);
}

static void pet_frog_eot(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    (void)ev;
    if (slot == 0) return;  // no slot to the left
    int targetSlot = (int)slot - 1;
    if (bs->teams[side].slots[targetSlot].id == EMPTY_PET) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    battle_emit(bs, BEV_BUFF, side, (uint8_t)targetSlot, encode_pair(lvl, lvl), 0);
}

static void pet_bee_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id == EMPTY_PET) continue;
        battle_emit(bs, BEV_BUFF, side, (uint8_t)i, encode_pair(lvl, 0), 0);
    }
}

static void pet_cricket_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    if (ev->side != side || ev->srcSlot != slot) return;
    // Spawned Zombie Hoppers carry the same faint handler; bail if this is
    // already a spawn so the cascade stops at one zombie per original.
    if (bs->teams[side].slots[slot].flags & 0x04) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int16_t s = (int16_t)lvl;
    summon_pet_in_slot(bs, side, slot, 4 /*Cricket*/, s, s);
}

static void pet_cow_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires only when WE were bought
    if (ev->side != side || ev->srcSlot != slot) return;
    (void)bs;
    // The card promises Milk — deliver it. Prefer an empty slot, else
    // overwrite the leftmost unfrozen slot so the gift always lands.
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        if (game.shop.foods[i] == EMPTY_FOOD) {
            game.shop.foods[i] = 5;  // Milk
            return;
        }
    }
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        if (!(game.shop.frozenFoods & (1 << i))) {
            game.shop.foods[i] = 5;
            return;
        }
    }
    // All slots frozen — let Milk replace slot 0 anyway, this corner case is rare.
    game.shop.foods[0] = 5;
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
    if (ev->dataSlot >= TEAM_SIZE) return;
    if (bs->teams[side].slots[ev->dataSlot].id == EMPTY_PET) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    battle_emit(bs, BEV_BUFF, side, ev->dataSlot, encode_pair(lvl, 0), 0);
}

static void pet_ox_friendfaint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when an ally OTHER THAN US faints
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(lvl, 0), 0);
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
    int lvl = (int)bs->teams[side].slots[slot].level;
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(2 * lvl, 2 * lvl), 0);
}

static void pet_dragon_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires whenever ANY T1 pet is bought (ev->data holds the bought pet's tier)
    (void)slot;
    if (ev->side != side) return;
    int boughtTier = (int)ev->data;
    if (boughtTier != 1) return;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id == EMPTY_PET) continue;
        battle_emit(bs, BEV_BUFF, side, (uint8_t)i, encode_pair(1, 1), 0);
    }
}

// --- "Exploding synergy" pets: Tiger / Parrot re-fire neighbour abilities ---
// Recursion guard so Tiger-next-to-Parrot-next-to-Tiger can't loop forever.
static int g_abilityDepth = 0;
#define ABILITY_MAX_DEPTH 6

static void pet_tiger_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Re-fire the start-of-battle ability of the pet on our RIGHT (slot+1),
    // once per Tiger level. Turns a Crocodile/Bee next to Tiger into a combo.
    if (g_abilityDepth >= ABILITY_MAX_DEPTH) return;
    int r = (int)slot + 1;
    if (r >= TEAM_SIZE) return;
    PetSlot* p = &bs->teams[side].slots[r];
    if (p->id == EMPTY_PET) return;
    const PetDef* def = pet_def(p->id);
    if (!def) return;
    PetHandler h = def->handlers[TRIG_START_OF_BATTLE];
    if (!h) return;
    int times = (int)bs->teams[side].slots[slot].level;  // L1=1, L2=2, L3=3
    g_abilityDepth++;
    for (int i = 0; i < times; i++) h(bs, side, (uint8_t)r, ev);
    g_abilityDepth--;
}

static void pet_parrot_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Copy the start-of-battle ability of the pet on our LEFT (slot-1) and
    // perform it AS the Parrot (using Parrot's own slot/level).
    if (g_abilityDepth >= ABILITY_MAX_DEPTH) return;
    int l = (int)slot - 1;
    if (l < 0) return;
    PetSlot* p = &bs->teams[side].slots[l];
    if (p->id == EMPTY_PET) return;
    const PetDef* def = pet_def(p->id);
    if (!def) return;
    PetHandler h = def->handlers[TRIG_START_OF_BATTLE];
    if (!h) return;
    g_abilityDepth++;
    h(bs, side, slot, ev);   // run the copied ability as ourselves
    g_abilityDepth--;
}

static void pet_whale_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Death-bomb: on faint, deal (5 x level) damage to a random enemy.
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg = 5 * lvl;
    int targetSide = side ^ 1;
    int t = random_alive_slot_side(bs, (uint8_t)targetSide);
    if (t < 0) return;
    deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
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
    p->food = 1;  // Edelweiss held-item flag: spawn Bee Buddy on faint
}

static void food_meatbone_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->food = 2;            // Salt Lick: +5 atk next battle (battle-only)
    p->flags |= 0x02;       // applied at battle start, cleared after
}

static void food_garlic_apply(Team* team, uint8_t targetSlot) {
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;
    p->flags |= 0x01;  // Damage taken -2 in next battle
    p->food = 3;
}

static void food_sleepingpill_apply(Team* team, uint8_t targetSlot) {
    // Immediately faints the pet, routing through the same cascade pipeline
    // the battle sim uses — so Sheep spawns its rams, Hopper its zombie,
    // Goose its random T1, Ox/Adder get their friend-faints triggers, etc.
    if (targetSlot >= TEAM_SIZE) return;
    PetSlot* p = &team->slots[targetSlot];
    if (p->id == EMPTY_PET) return;

    BattleState* bs = &game.battle;
    bool wasInBattle = bs->inBattle;
    Team savedP = bs->teams[0], savedE = bs->teams[1];

    // Stage the player team on side 0, leave the enemy side empty so
    // outward-targeting handlers (Marmot, Highland, Snake) find no targets
    // and silently no-op. battle_emit applies state but skips logging while
    // inBattle == false.
    bs->inBattle = false;
    bs->teams[0] = *team;
    memset(&bs->teams[1], 0, sizeof(Team));
    for (int i = 0; i < TEAM_SIZE; i++) {
        bs->teams[1].slots[i].id = EMPTY_PET;
        bs->teams[1].slots[i].food = EMPTY_FOOD;
    }

    // Knock the target's HP to 0 so battle_resolve_faints catches it.
    bs->teams[0].slots[targetSlot].hp = 0;

    trig_reset();
    battle_resolve_faints(bs);

    *team = bs->teams[0];

    bs->teams[0] = savedP;
    bs->teams[1] = savedE;
    bs->inBattle = wasInBattle;
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
