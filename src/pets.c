#include "game.h"

// ---------------------------------------------------------------------------
// Pet definitions — handlers wired in a later pass.
// Roster matches plan: 6 per tier, total 18.
// ---------------------------------------------------------------------------
const PetDef PETS[PET_COUNT] = {
    // Tier 1
    [0]  = { "Mouse",     1, 2, 1,  0, {0} },
    [1]  = { "Dog",       3, 2, 1,  1, {0} },
    [2]  = { "Frog",      1, 3, 1,  2, {0} },
    [3]  = { "Bee",       2, 1, 1,  3, {0} },
    [4]  = { "Cricket",   2, 2, 1,  4, {0} },
    [5]  = { "Cow",       3, 3, 1,  5, {0} },
    // Tier 2
    [6]  = { "Sheep",     2, 2, 2,  6, {0} },
    [7]  = { "Hedgehog",  3, 3, 2,  7, {0} },
    [8]  = { "Horse",     1, 1, 2,  8, {0} },
    [9]  = { "Ox",        1, 4, 2,  9, {0} },
    [10] = { "Snake",     3, 3, 2, 10, {0} },
    [11] = { "Spider",    2, 2, 2, 11, {0} },
    // Tier 3
    [12] = { "Crocodile", 5, 3, 3, 12, {0} },
    [13] = { "Whale",     3, 8, 3, 13, {0} },
    [14] = { "Boar",      8, 6, 3, 14, {0} },
    [15] = { "Parrot",    3, 3, 3, 15, {0} },
    [16] = { "Tiger",     4, 3, 3, 16, {0} },
    [17] = { "Dragon",    6, 8, 3, 17, {0} },
};

const PetDef* pet_def(uint8_t id) {
    if (id >= PET_COUNT) return NULL;
    return &PETS[id];
}

const char* pet_name(uint8_t id) {
    return id < PET_COUNT ? PETS[id].name : "?";
}

// XP needed cumulative to reach `level`. Level 1 = 0, level 2 = 2, level 3 = 5.
int pet_xp_for_level(uint8_t level) {
    if (level <= 1) return 0;
    if (level == 2) return LEVEL_2_XP;
    return LEVEL_2_XP + LEVEL_3_XP;
}

uint8_t pet_random_in_tier(uint8_t maxTier) {
    // Simple: pick uniformly over all pets at tier <= maxTier.
    uint8_t pool[PET_COUNT];
    int n = 0;
    for (uint8_t i = 0; i < PET_COUNT; i++) {
        if (PETS[i].tier <= maxTier) pool[n++] = i;
    }
    if (n == 0) return 0;
    return pool[rng_range(0, n - 1)];
}

// ---------------------------------------------------------------------------
// Foods (handlers wired later)
// ---------------------------------------------------------------------------
const FoodDef FOODS[FOOD_COUNT] = {
    [0] = { "Apple",         3, 0, true,  NULL },
    [1] = { "Honey",         3, 1, true,  NULL },
    [2] = { "Meat Bone",     3, 2, true,  NULL },
    [3] = { "Garlic",        3, 3, true,  NULL },
    [4] = { "Sleeping Pill", 1, 4, true,  NULL },
    [5] = { "Milk",          3, 5, false, NULL },
};
