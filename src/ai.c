#include "game.h"

// Generate a plausible opponent for `turn`. Deterministic given seed.
void ai_generate_opponent(Team* out, uint8_t turn, uint32_t seed) {
    uint32_t prevState = _rng_state;
    rng_seed(seed);

    for (int i = 0; i < TEAM_SIZE; i++) {
        out->slots[i].id = EMPTY_PET;
        out->slots[i].level = 1;
        out->slots[i].xp = 0;
        out->slots[i].atk = 0;
        out->slots[i].hp = 0;
        out->slots[i].food = EMPTY_FOOD;
        out->slots[i].flags = 0;
    }

    // Difficulty curve: trivial T1-2, mild T3-4, real T5-6, hard T7-8, wall T9-10.
    // Stat bumps softened in the late game to compensate for the smaller
    // 3-slot shop (players see fewer pet options per roll).
    int petCount = (turn <= 2) ? 1 :
                   (turn <= 4) ? 2 :
                   (turn <= 6) ? 3 :
                   (turn <= 8) ? 4 : TEAM_SIZE;
    uint8_t maxTier = (turn >= 7) ? 3 : (turn >= 4) ? 2 : 1;
    int bump = (turn <= 4) ? 0 :
               (turn <= 6) ? 1 :
               (turn <= 8) ? 1 :
               (turn <= 9) ? 2 : 3;
    int lvl2Chance = (turn >= 10) ? 2 : (turn >= 8) ? 4 : 0;
    bool allowLvl3 = (turn >= 10);

    for (int i = 0; i < petCount; i++) {
        uint8_t id = pet_random_in_tier(maxTier);
        const PetDef* def = pet_def(id);
        out->slots[i].id  = id;
        out->slots[i].atk = (int16_t)mini(def->baseAtk + bump, STAT_CAP);
        out->slots[i].hp  = (int16_t)mini(def->baseHp  + bump, STAT_CAP);

        if (lvl2Chance && (rng_next() % (uint32_t)lvl2Chance) == 0) {
            uint8_t lvl = 2;
            int extra = 1;
            if (allowLvl3 && (rng_next() & 0x7) == 0) { lvl = 3; extra = 3; }
            out->slots[i].level = lvl;
            out->slots[i].atk = (int16_t)mini(out->slots[i].atk + extra, STAT_CAP);
            out->slots[i].hp  = (int16_t)mini(out->slots[i].hp  + extra, STAT_CAP);
        }
    }

    _rng_state = prevState;
}
