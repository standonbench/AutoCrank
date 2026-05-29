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
// Each pet carries one description per level — the shop card picks
// desc[level-1] so the tooltip matches what the handler actually does at
// that level. Handlers further down switch on level for qualitative jumps
// (extra targets, gained held food, expanded trigger pool, etc.).
const PetDef PETS[PET_COUNT] = {
    // Tier 1 — small alpine & young farm animals
    [0]  = { "Squirrel",  2, 2, 1,  0,
        { "Faint: 2 dmg to a random enemy.",
          "Faint: 4 dmg to a random enemy.",
          "Faint: 4 dmg to two random enemies." },
        { H_FAINT  pet_mouse_faint } },
    [1]  = { "Pup",       3, 2, 1,  1,
        { "Ally summoned: gain +1/+1.",
          "Ally summoned: gain +2/+2.",
          "Ally summoned: +2/+2 self, +1/+1 to it." },
        { H_SUMMON pet_dog_summon } },
    [2]  = { "Hen",       1, 3, 1,  2,   // chicken
        { "End of turn: +1/+1 to ally on left.",
          "End of turn: +2/+1 to ally on left.",
          "End of turn: +2/+1 to both neighbors." },
        { H_EOT    pet_frog_eot } },
    [3]  = { "Hedgehog",  2, 1, 1,  3,
        { "Battle start: +1 atk to all friends.",
          "Battle start: +2 atk to all friends.",
          "Battle start: +2/+1 to all friends." },
        { H_SOB    pet_bee_sob } },
    [4]  = { "Rabbit",    2, 2, 1,  4,
        { "Faint: spawn a 1/1 Rabbit here.",
          "Faint: spawn a 2/2 Rabbit here.",
          "Faint: spawn a 3/3 Rabbit w/ Edelweiss." },
        { H_FAINT  pet_cricket_faint } },
    [5]  = { "Calf",      3, 3, 1,  5,   // cow-0 (brown)
        { "On own buy: add Milk to the shop.",
          "+L1, and any T1 buy: gain +1g.",
          "+L1, and any buy: gain +2g." },
        { H_BUY    pet_cow_buy } },
    // Tier 2 — working farm animals
    [6]  = { "Sheep",     3, 2, 2,  6,   // sheep
        { "Faint: spawn two 2/2 Haflingers.",
          "Faint: spawn two 4/4 Haflingers.",
          "Faint: spawn two 6/6 Haflingers." },
        { H_FAINT  pet_sheep_faint } },
    [7]  = { "Goat",      3, 3, 2,  7,   // goat
        { "Faint: 2 dmg to all enemies.",
          "Faint: 3 dmg to all enemies.",
          "Faint: 4 dmg enemies, 2 dmg friends." },
        { H_FAINT  pet_hedgehog_faint } },
    [8]  = { "Haflinger", 3, 3, 2,  8,
        { "Ally summoned: give it +2 atk.",
          "Ally summoned: give it +3 atk.",
          "Ally summoned: give it +3/+2." },
        { H_SUMMON pet_horse_summon } },
    [9]  = { "Donkey",    2, 4, 2,  9,
        { "Ally faints: gain +2 atk.",
          "Ally faints: gain +3 atk.",
          "Ally faints: +3/+1 and gain Cowbell." },
        { H_FAINTFRD pet_ox_friendfaint } },
    [10] = { "Alligator", 3, 3, 2, 10,
        { "Ally attacks: 3 dmg to a random enemy.",
          "Ally attacks: 5 dmg to a random enemy.",
          "Ally attacks: 5 dmg to two enemies." },
        { H_FRDATK pet_snake_friendatk } },
    [11] = { "Duck",      3, 3, 2, 11,
        { "Faint: spawn a random T1 (3/3).",
          "Faint: spawn a random T1 (5/5).",
          "Faint: spawn a random T1/T2 (5/5)." },
        { H_FAINT  pet_spider_faint } },
    // Tier 3 — champions of the alp
    [12] = { "Alpaca",    5, 4, 3, 12,
        { "Battle start: 8 dmg to last enemy.",
          "Battle start: 12 dmg to last enemy.",
          "Battle start: 12 dmg to last two enemies." },
        { H_SOB    pet_croc_sob } },
    [13] = { "Highland",  4, 8, 3, 13,   // alpaca (shaggy alpine)
        { "Faint: 5 dmg to a random enemy.",
          "Faint: 8 dmg to a random enemy.",
          "Faint: 8 dmg to all enemies." },
        { H_FAINT  pet_whale_faint } },
    [14] = { "Boar",      6, 6, 3, 14,   // pig (wild pig stand-in)
        { "Before attack: gain +2/+2.",
          "Before attack: gain +3/+3.",
          "Before atk: +4/+4 self, +1 atk friends." },
        { H_BEFOREATK pet_boar_beforeatk } },
    [15] = { "Deer",      3, 4, 3, 15,
        { "Battle start: copy left ally's ability.",
          "Battle start: copy left at +1 power.",
          "Battle start: copy both neighbors." },
        { H_SOB    pet_parrot_sob } },
    [16] = { "Fox",       4, 5, 3, 16,
        { "Battle start: re-fire right ally.",
          "Battle start: re-fire right ally twice.",
          "Battle start: re-fire right ally x3." },
        { H_SOB    pet_tiger_sob } },
    [17] = { "Bear",      6, 8, 3, 17,
        { "Buy a T1: +1/+1 to all friends.",
          "Buy T1/T2: +1/+1 to all friends.",
          "Buy any ally: +2/+1 to all friends." },
        { H_BUY    pet_dragon_buy } },
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
// SAP-style food tiering: T1 foods (turn 1+) are always available; T2 unlocks
// at turn 3 alongside T2 pets; T3 unlocks at turn 5. shop_refill() filters by
// game.turn >= unlockTurn.
const FoodDef FOODS[FOOD_COUNT] = {
    // name        cost sprite target unlock  desc                                  apply
    [0] = { "Hay",       3, 0, true,  1, "+1 atk and +1 hp to a friend.",        food_apple_apply },
    [1] = { "Edelweiss", 3, 1, true,  3, "Held: on faint, spawn a 2/2 Hedgehog.", food_honey_apply },
    [2] = { "Salt Lick", 3, 2, true,  5, "+4 atk for the next battle.",          food_meatbone_apply },
    [3] = { "Cowbell",   3, 3, true,  5, "Held: takes 2 less damage.",           food_garlic_apply },
    [4] = { "Cheese",    1, 4, true,  1, "Faint a friend (synergies fire).",     food_sleepingpill_apply },
    [5] = { "Milk",      3, 5, false, 3, "Random friend gains +2/+2.",           food_milk_apply },
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

// "Alive" here means id != EMPTY AND hp > 0. A dying pet whose hp has been
// driven to 0 by a prior event in this drain still occupies its slot until
// the BEV_FAINT event applies — but treating it as a valid target wastes
// ability fires (Adder hitting a corpse, Bernard sniping a unit that's
// already going to faint, etc.) and logs spurious damage events the player
// can't make sense of. Battle.c's frontmost_alive uses the same gate.
static int last_alive_slot_side(BattleState* bs, uint8_t side) {
    for (int i = TEAM_SIZE - 1; i >= 0; i--) {
        PetSlot* p = &bs->teams[side].slots[i];
        if (p->id != EMPTY_PET && p->hp > 0) return i;
    }
    return -1;
}

static int random_alive_slot_side(BattleState* bs, uint8_t side) {
    int alive[TEAM_SIZE];
    int n = 0;
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &bs->teams[side].slots[i];
        if (p->id != EMPTY_PET && p->hp > 0) alive[n++] = i;
    }
    if (n == 0) return -1;
    return alive[rng_range(0, n - 1)];
}

static void deal_damage_to_slot(BattleState* bs, uint8_t side, int slot, int dmg) {
    if (slot < 0 || slot >= TEAM_SIZE) return;
    PetSlot* p = &bs->teams[side].slots[slot];
    // Skip empty AND dying pets — see comment on the alive-helpers above.
    if (p->id == EMPTY_PET || p->hp <= 0) return;
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
    // Fires only on our OWN faint. L1: 2 dmg / 1 target. L2: 4 dmg / 1 target.
    // L3: 4 dmg / 2 random targets (may repeat — random with replacement).
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg, targets;
    switch (lvl) {
        case 1:  dmg = 2; targets = 1; break;
        case 2:  dmg = 4; targets = 1; break;
        default: dmg = 4; targets = 2; break;
    }
    int targetSide = side ^ 1;
    for (int n = 0; n < targets; n++) {
        int t = random_alive_slot_side(bs, (uint8_t)targetSide);
        if (t < 0) return;
        // ABILITY logs the visual cue (glow at source) FIRST, then DAMAGE
        // applies — so cause-and-effect reads left-to-right at playback.
        battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
        deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    }
}

static void pet_dog_summon(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when any ally is summoned on our side, except us.
    // L1: +1/+1 self. L2: +2/+2 self. L3: +2/+2 self AND +1/+1 to the summon.
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int self_a, self_h, ally_a, ally_h;
    switch (lvl) {
        case 1:  self_a = 1; self_h = 1; ally_a = 0; ally_h = 0; break;
        case 2:  self_a = 2; self_h = 2; ally_a = 0; ally_h = 0; break;
        default: self_a = 2; self_h = 2; ally_a = 1; ally_h = 1; break;
    }
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(self_a, self_h), 0);
    if ((ally_a || ally_h) &&
        ev->dataSlot < TEAM_SIZE &&
        bs->teams[side].slots[ev->dataSlot].id != EMPTY_PET) {
        battle_emit(bs, BEV_BUFF, side, ev->dataSlot,
                    encode_pair(ally_a, ally_h), 0);
    }
}

static void pet_frog_eot(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // L1: +1/+1 left. L2: +2/+1 left. L3: +2/+1 to BOTH neighbors.
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int a, h;
    bool buffRight;
    switch (lvl) {
        case 1:  a = 1; h = 1; buffRight = false; break;
        case 2:  a = 2; h = 1; buffRight = false; break;
        default: a = 2; h = 1; buffRight = true;  break;
    }
    if (slot > 0) {
        int l = (int)slot - 1;
        if (bs->teams[side].slots[l].id != EMPTY_PET)
            battle_emit(bs, BEV_BUFF, side, (uint8_t)l, encode_pair(a, h), 0);
    }
    if (buffRight && (int)slot + 1 < TEAM_SIZE) {
        int r = (int)slot + 1;
        if (bs->teams[side].slots[r].id != EMPTY_PET)
            battle_emit(bs, BEV_BUFF, side, (uint8_t)r, encode_pair(a, h), 0);
    }
}

static void pet_bee_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // L1: +1 atk all friends. L2: +2 atk all. L3: +2/+1 all (gains hp at L3).
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int a, h;
    switch (lvl) {
        case 1:  a = 1; h = 0; break;
        case 2:  a = 2; h = 0; break;
        default: a = 2; h = 1; break;
    }
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id == EMPTY_PET) continue;
        battle_emit(bs, BEV_BUFF, side, (uint8_t)i, encode_pair(a, h), 0);
    }
}

static void pet_cricket_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // L1: spawn 1/1 Hopper. L2: 2/2. L3: 3/3 holding Edelweiss (Bee on faint).
    if (ev->side != side || ev->srcSlot != slot) return;
    // Spawned Zombie Hoppers carry the same faint handler; bail if this is
    // already a spawn so the cascade stops at one zombie per original.
    if (bs->teams[side].slots[slot].flags & 0x04) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int atk, hp;
    bool withEdelweiss;
    switch (lvl) {
        case 1:  atk = 1; hp = 1; withEdelweiss = false; break;
        case 2:  atk = 2; hp = 2; withEdelweiss = false; break;
        default: atk = 3; hp = 3; withEdelweiss = true;  break;
    }
    summon_pet_in_slot(bs, side, slot, 4 /*Hopper*/, (int16_t)atk, (int16_t)hp);
    if (withEdelweiss) {
        // BEV_SPAWN reset food to EMPTY_FOOD inside apply_event; set it again
        // so the Bee-on-faint hatch (food id 1) fires for the spawned Hopper.
        bs->teams[side].slots[slot].food = 1;
    }
}

static void pet_cow_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Calf fires on any buy on our side. Two effects stack by level:
    //   L1+: on Calf's OWN buy (or combine into self), drop a Milk in shop.
    //   L2+: on any qualifying-tier buy, rebate gold:
    //        L2 = any T1 buy → +1g.   L3 = any buy → +2g.
    // The any-buy rebate at L3 is what keeps Calf alive after max-level
    // (without it the ability would never fire again post-combine).
    if (ev->side != side) return;
    int lvl = (int)bs->teams[side].slots[slot].level;

    // L1+ — Milk delivery on own buy. Prefer empty, then unfrozen, else slot 0.
    if (ev->srcSlot == slot) {
        bool placed = false;
        for (int i = 0; i < MAX_SHOP_FOOD; i++) {
            if (game.shop.foods[i] == EMPTY_FOOD) {
                game.shop.foods[i] = 5;
                placed = true;
                break;
            }
        }
        if (!placed) {
            for (int i = 0; i < MAX_SHOP_FOOD; i++) {
                if (!(game.shop.frozenFoods & (1 << i))) {
                    game.shop.foods[i] = 5;
                    placed = true;
                    break;
                }
            }
            if (!placed) game.shop.foods[0] = 5;
        }
    }

    // L2+ — gold rebate on qualifying buys.
    if (lvl >= 2) {
        int boughtTier = (int)ev->data;
        int maxTier, rebate;
        if (lvl >= 3) { maxTier = 3; rebate = 2; }
        else          { maxTier = 1; rebate = 1; }
        if (boughtTier <= maxTier) {
            game.shop.gold = (int16_t)mini(game.shop.gold + rebate, 99);
            game.lastGoldDelta = (int8_t)rebate;
            game.goldFxFrames  = 12;
        }
    }
}

// ---------------------------------------------------------------------------
// Tier 2 pet abilities
// ---------------------------------------------------------------------------
static void pet_sheep_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // L1: 2 x 2/2 Haflingers. L2: 2 x 4/4. L3: 2 x 6/6.
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int stat;
    switch (lvl) {
        case 1:  stat = 2; break;
        case 2:  stat = 4; break;
        default: stat = 6; break;
    }
    int16_t a = (int16_t)stat, h = (int16_t)stat;
    summon_pet_in_slot(bs, side, slot, 8 /*Haflinger*/, a, h);
    int t = first_empty_slot_side(bs, side);
    if (t >= 0) summon_pet_in_slot(bs, side, (uint8_t)t, 8, a, h);
}

static void pet_hedgehog_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Goat war-horn AoE. L1: 2 dmg enemies. L2: 3 dmg enemies. L3: 4 dmg
    // enemies AND 2 dmg friends (the L3 "sacrifice" payoff).
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int enemyDmg, friendDmg;
    switch (lvl) {
        case 1:  enemyDmg = 2; friendDmg = 0; break;
        case 2:  enemyDmg = 3; friendDmg = 0; break;
        default: enemyDmg = 4; friendDmg = 2; break;
    }
    // ABILITY first so the player sees the war-horn glow at Goat's slot
    // BEFORE the AoE damage numbers start landing on the enemies.
    battle_log_push(BEV_ABILITY, side, slot, (int16_t)enemyDmg, 0);
    int enemySide = side ^ 1;
    for (int i = 0; i < TEAM_SIZE; i++) {
        deal_damage_to_slot(bs, (uint8_t)enemySide, i, enemyDmg);
    }
    if (friendDmg > 0) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            if (i == slot) continue;  // Goat is already fainting; skip self
            deal_damage_to_slot(bs, side, i, friendDmg);
        }
    }
}

static void pet_horse_summon(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Haflinger buff to the just-summoned friend (ev->dataSlot), unless it's us.
    // L1: +2 atk. L2: +3 atk. L3: +3/+2 (gains hp at L3).
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    if (ev->dataSlot >= TEAM_SIZE) return;
    if (bs->teams[side].slots[ev->dataSlot].id == EMPTY_PET) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int a, h;
    switch (lvl) {
        case 1:  a = 2; h = 0; break;
        case 2:  a = 3; h = 0; break;
        default: a = 3; h = 2; break;
    }
    battle_emit(bs, BEV_BUFF, side, ev->dataSlot, encode_pair(a, h), 0);
}

static void pet_ox_friendfaint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires when an ally OTHER THAN US faints.
    // L1: +2 atk. L2: +3 atk. L3: +3/+1 AND gain Cowbell (bit 0 = -2 dmg taken).
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int a, h;
    bool gainCowbell;
    switch (lvl) {
        case 1:  a = 2; h = 0; gainCowbell = false; break;
        case 2:  a = 3; h = 0; gainCowbell = false; break;
        default: a = 3; h = 1; gainCowbell = true;  break;
    }
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(a, h), 0);
    if (gainCowbell) {
        // Set the garlic flag directly — battle_emit has no event kind for
        // flag-only changes, and apply_event reads p->flags on damage events.
        bs->teams[side].slots[slot].flags |= 0x01;
    }
}

static void pet_snake_friendatk(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Adder: fires when an ally other than us attacks.
    // L1: 3 dmg / 1 target. L2: 5 dmg / 1 target. L3: 5 dmg / 2 random targets.
    if (ev->side != side) return;
    if (ev->srcSlot == slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg, targets;
    switch (lvl) {
        case 1:  dmg = 3; targets = 1; break;
        case 2:  dmg = 5; targets = 1; break;
        default: dmg = 5; targets = 2; break;
    }
    int targetSide = side ^ 1;
    for (int n = 0; n < targets; n++) {
        int t = random_alive_slot_side(bs, (uint8_t)targetSide);
        if (t < 0) return;
        battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
        deal_damage_to_slot(bs, (uint8_t)targetSide, t, dmg);
    }
}

static void pet_spider_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Goose: summon a random pet from a level-gated pool.
    // L1: T1 at 3/3. L2: T1 at 5/5. L3: T1 or T2 at 5/5.
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int stat;
    uint8_t maxTier;
    switch (lvl) {
        case 1:  stat = 3; maxTier = 1; break;
        case 2:  stat = 5; maxTier = 1; break;
        default: stat = 5; maxTier = 2; break;
    }
    uint8_t pid = pet_random_in_tier(maxTier);
    summon_pet_in_slot(bs, side, slot, pid, (int16_t)stat, (int16_t)stat);
}

// ---------------------------------------------------------------------------
// Tier 3 pet abilities (simplified for v0)
// ---------------------------------------------------------------------------
static void pet_croc_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Bernard snipes the enemy backline at battle start.
    // L1: 8 dmg / last enemy. L2: 12 dmg / last enemy. L3: 12 dmg / last two.
    (void)ev;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg, targets;
    switch (lvl) {
        case 1:  dmg = 8;  targets = 1; break;
        case 2:  dmg = 12; targets = 1; break;
        default: dmg = 12; targets = 2; break;
    }
    int enemySide = side ^ 1;
    int hit = 0;
    for (int i = TEAM_SIZE - 1; i >= 0 && hit < targets; i--) {
        PetSlot* ep = &bs->teams[enemySide].slots[i];
        if (ep->id == EMPTY_PET || ep->hp <= 0) continue;
        battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)i);
        deal_damage_to_slot(bs, (uint8_t)enemySide, i, dmg);
        hit++;
    }
}

static void pet_boar_beforeatk(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Fires only when WE are the one attacking.
    // L1: +2/+2 self. L2: +3/+3 self. L3: +4/+4 self + +1 atk to all friends.
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int self, friendAtk;
    switch (lvl) {
        case 1:  self = 2; friendAtk = 0; break;
        case 2:  self = 3; friendAtk = 0; break;
        default: self = 4; friendAtk = 1; break;
    }
    battle_emit(bs, BEV_BUFF, side, slot, encode_pair(self, self), 0);
    if (friendAtk > 0) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            if (i == slot) continue;
            if (bs->teams[side].slots[i].id == EMPTY_PET) continue;
            battle_emit(bs, BEV_BUFF, side, (uint8_t)i,
                        encode_pair(friendAtk, 0), 0);
        }
    }
}

static void pet_dragon_buy(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Bull buffs the whole team when a qualifying ally is bought.
    // L1: T1 buys only, +1/+1. L2: T1 or T2 buys, +1/+1. L3: any buy, +2/+1.
    if (ev->side != side) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int boughtTier = (int)ev->data;
    int maxTier, buffAtk, buffHp;
    switch (lvl) {
        case 1:  maxTier = 1; buffAtk = 1; buffHp = 1; break;
        case 2:  maxTier = 2; buffAtk = 1; buffHp = 1; break;
        default: maxTier = 3; buffAtk = 2; buffHp = 1; break;
    }
    if (boughtTier > maxTier) return;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (bs->teams[side].slots[i].id == EMPTY_PET) continue;
        battle_emit(bs, BEV_BUFF, side, (uint8_t)i, encode_pair(buffAtk, buffHp), 0);
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
    // Self-fire glyph so the player sees "Reine triggered something" — the
    // re-fired handlers log at the NEIGHBOUR's slot (especially for buffers
    // where there's no other on-source visual), so without this Reine looks
    // inert.
    battle_log_push(BEV_ABILITY, side, slot, 0, slot);
    g_abilityDepth++;
    for (int i = 0; i < times; i++) h(bs, side, (uint8_t)r, ev);
    g_abilityDepth--;
}

static void pet_parrot_sob(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Ibex copies a neighbour's start-of-battle ability and runs it AS itself
    // (using Ibex's own slot/level — so the copy already runs at Ibex's level,
    // i.e. "+1 power" naturally at L2 vs. a typical L1 neighbour).
    // L1: copy left. L2: copy left (at higher own-level power).
    // L3: copy both left AND right neighbours.
    if (g_abilityDepth >= ABILITY_MAX_DEPTH) return;
    int lvl = (int)bs->teams[side].slots[slot].level;

    int neighbours[2];
    int n = 0;
    if ((int)slot - 1 >= 0)            neighbours[n++] = (int)slot - 1;
    if (lvl >= 3 && (int)slot + 1 < TEAM_SIZE) neighbours[n++] = (int)slot + 1;

    // Self-fire glyph so the player sees Ibex doing something even when the
    // copied ability has no on-source visual (Bee-style buffers, etc.).
    bool fired = false;
    for (int k = 0; k < n; k++) {
        PetSlot* p = &bs->teams[side].slots[neighbours[k]];
        if (p->id == EMPTY_PET) continue;
        const PetDef* def = pet_def(p->id);
        if (!def) continue;
        PetHandler h = def->handlers[TRIG_START_OF_BATTLE];
        if (!h) continue;
        if (!fired) {
            battle_log_push(BEV_ABILITY, side, slot, 0, slot);
            fired = true;
        }
        g_abilityDepth++;
        h(bs, side, slot, ev);
        g_abilityDepth--;
        if (g_abilityDepth >= ABILITY_MAX_DEPTH) break;
    }
}

static void pet_whale_faint(BattleState* bs, uint8_t side, uint8_t slot, TriggerEvent* ev) {
    // Highland: death-bomb that grows in shape with level.
    // L1: 5 dmg / random enemy. L2: 8 dmg / random enemy. L3: 8 dmg / ALL enemies.
    if (ev->side != side || ev->srcSlot != slot) return;
    int lvl = (int)bs->teams[side].slots[slot].level;
    int dmg;
    bool aoe;
    switch (lvl) {
        case 1:  dmg = 5; aoe = false; break;
        case 2:  dmg = 8; aoe = false; break;
        default: dmg = 8; aoe = true;  break;
    }
    int enemySide = side ^ 1;
    if (aoe) {
        battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, 0);
        for (int i = 0; i < TEAM_SIZE; i++) {
            deal_damage_to_slot(bs, (uint8_t)enemySide, i, dmg);
        }
    } else {
        int t = random_alive_slot_side(bs, (uint8_t)enemySide);
        if (t < 0) return;
        battle_log_push(BEV_ABILITY, side, slot, (int16_t)dmg, (uint8_t)t);
        deal_damage_to_slot(bs, (uint8_t)enemySide, t, dmg);
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
