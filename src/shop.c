#include "game.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Shop tier schedule: T1@T1, T2@T3, T3@T5
// ---------------------------------------------------------------------------
uint8_t shop_current_tier(void) {
    if (game.turn >= 5) return 3;
    if (game.turn >= 3) return 2;
    return 1;
}

void shop_init_run(void) {
    memset(&game.shop, 0, sizeof(Shop));
    for (int i = 0; i < MAX_SHOP_PETS; i++) game.shop.pets[i] = EMPTY_PET;
    for (int i = 0; i < MAX_SHOP_FOOD; i++) game.shop.foods[i] = EMPTY_FOOD;
    game.shop.gold = STARTING_GOLD;
}

void shop_refill(void) {
    uint8_t tier = mini(shop_current_tier(), MAX_TIER);
    for (int i = 0; i < MAX_SHOP_PETS; i++) {
        if (game.shop.frozenPets & (1 << i)) continue;
        game.shop.pets[i] = pet_random_in_tier(tier);
    }
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        if (game.shop.frozenFoods & (1 << i)) continue;
        game.shop.foods[i] = (uint8_t)rng_range(0, FOOD_COUNT - 1);
    }
}

// ---------------------------------------------------------------------------
// Buy / sell / roll / freeze
// ---------------------------------------------------------------------------
bool shop_try_buy_pet(uint8_t shopSlot, uint8_t teamSlot) {
    if (shopSlot >= MAX_SHOP_PETS || teamSlot >= TEAM_SIZE) return false;
    uint8_t petId = game.shop.pets[shopSlot];
    if (petId == EMPTY_PET) return false;
    if (game.shop.gold < PET_COST) return false;

    PetSlot* dst = &game.player.slots[teamSlot];
    bool wasFresh = false;
    bool leveledUp = false;
    if (dst->id == EMPTY_PET) {
        const PetDef* def = pet_def(petId);
        dst->id = petId;
        dst->level = 1;
        dst->xp = 0;
        dst->atk = def->baseAtk;
        dst->hp = def->baseHp;
        dst->food = EMPTY_FOOD;
        dst->flags = 0;
        wasFresh = true;
    } else if (dst->id == petId && dst->level < MAX_PET_LEVEL) {
        dst->xp++;
        dst->atk = (int16_t)mini(dst->atk + 1, STAT_CAP);
        dst->hp  = (int16_t)mini(dst->hp + 1, STAT_CAP);
        int needed = pet_xp_for_level((uint8_t)(dst->level + 1));
        if (dst->xp >= needed) {
            dst->level++;
            dst->atk = (int16_t)mini(dst->atk + 1, STAT_CAP);
            dst->hp  = (int16_t)mini(dst->hp + 1, STAT_CAP);
            leveledUp = true;
        }
    } else {
        return false;
    }

    game.shop.pets[shopSlot] = EMPTY_PET;
    game.shop.gold -= PET_COST;
    sfx_buy();

    // Cost popup at the shop slot. Level-up burst + "Ln!" at the team slot.
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "-%dg", PET_COST);
        shop_popup_text(SHOP_COL_X[shopSlot], 80, buf);
        if (leveledUp) {
            snprintf(buf, sizeof(buf), "L%d!", (int)dst->level);
            shop_popup_text(TEAM_COL_X[teamSlot], 154, buf);
            shop_popup_burst(TEAM_COL_X[teamSlot], 158);
        }
    }

    // Fire BUY trigger (Cow, Dragon care about this). data = bought pet's tier.
    const PetDef* boughtDef = pet_def(petId);
    int boughtTier = boughtDef ? boughtDef->tier : 0;
    trig_push(TRIG_BUY, 0, teamSlot, teamSlot, (int16_t)boughtTier);
    if (wasFresh) {
        // FRIEND_SUMMONED for Dog/Horse buffs; srcSlot identifies the newly
        // summoned pet (matching battle-time convention).
        trig_push(TRIG_FRIEND_SUMMONED, 0, teamSlot, teamSlot, petId);
    }
    if (leveledUp) {
        trig_push(TRIG_LEVEL_UP, 0, teamSlot, teamSlot, 0);
    }
    trig_shop_drain();
    return true;
}

bool shop_try_buy_food(uint8_t shopSlot, uint8_t teamSlot) {
    if (shopSlot >= MAX_SHOP_FOOD) return false;
    uint8_t foodId = game.shop.foods[shopSlot];
    if (foodId == EMPTY_FOOD) return false;
    int cost = FOODS[foodId].cost;
    if (game.shop.gold < cost) return false;

    if (FOODS[foodId].needsTarget) {
        if (teamSlot >= TEAM_SIZE) return false;
        if (game.player.slots[teamSlot].id == EMPTY_PET) return false;
    }
    if (FOODS[foodId].apply) FOODS[foodId].apply(&game.player, teamSlot);
    game.shop.foods[shopSlot] = EMPTY_FOOD;
    game.shop.gold -= cost;
    sfx_buy();
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "-%dg", cost);
        shop_popup_text(FOOD_COL_X[shopSlot], 84, buf);
    }
    return true;
}

bool shop_try_sell(uint8_t teamSlot) {
    if (teamSlot >= TEAM_SIZE) return false;
    PetSlot* p = &game.player.slots[teamSlot];
    if (p->id == EMPTY_PET) return false;
    int refund = 1 + p->level - 1;
    if (refund < 1) refund = 1;
    game.shop.gold += refund;
    // Fire SELL trigger before clearing
    trig_push(TRIG_SELL, 0, teamSlot, teamSlot, p->id);
    p->id = EMPTY_PET;
    p->level = 1; p->xp = 0; p->atk = 0; p->hp = 0;
    p->food = EMPTY_FOOD; p->flags = 0;
    trig_shop_drain();
    sfx_sell();
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "+%dg", refund);
        shop_popup_text(TEAM_COL_X[teamSlot], 154, buf);
    }
    return true;
}

bool shop_try_roll(void) {
    if (game.shop.gold < ROLL_COST) return false;
    game.shop.gold -= ROLL_COST;
    shop_refill();
    sfx_roll();
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "-%dg", ROLL_COST);
        shop_popup_text(56, 180, buf);   // floats above the ROLL button
    }
    game.shopRollAnim = 12;
    return true;
}

void shop_toggle_freeze_pet(uint8_t shopSlot) {
    if (shopSlot >= MAX_SHOP_PETS) return;
    game.shop.frozenPets ^= (uint8_t)(1 << shopSlot);
}

void shop_toggle_freeze_food(uint8_t shopSlot) {
    if (shopSlot >= MAX_SHOP_FOOD) return;
    game.shop.frozenFoods ^= (uint8_t)(1 << shopSlot);
}

void shop_end_turn_advance(void) {
    // One global end-of-turn event — the dispatcher fans it out to every pet.
    trig_push(TRIG_END_OF_TURN, 0, 0, 0, 0);
    trig_shop_drain();

    game.turn++;
    game.shop.gold = STARTING_GOLD;
    game.shop.frozenPets = 0;
    game.shop.frozenFoods = 0;
    shop_refill();
}

// ---------------------------------------------------------------------------
// Team reorder helper (for crank-shift)
// ---------------------------------------------------------------------------
void team_shift(uint8_t fromSlot, int delta) {
    if (fromSlot >= TEAM_SIZE) return;
    int dest = (int)fromSlot + delta;
    if (dest < 0) dest = 0;
    if (dest >= TEAM_SIZE) dest = TEAM_SIZE - 1;
    if (dest == fromSlot) return;
    PetSlot moving = game.player.slots[fromSlot];
    if (delta > 0) {
        for (int i = fromSlot; i < dest; i++)
            game.player.slots[i] = game.player.slots[i + 1];
    } else {
        for (int i = fromSlot; i > dest; i--)
            game.player.slots[i] = game.player.slots[i - 1];
    }
    game.player.slots[dest] = moving;
}
