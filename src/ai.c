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

    int petCount = clampi(1 + turn / 2, 1, TEAM_SIZE);
    uint8_t maxTier = (turn >= 5) ? 3 : (turn >= 3 ? 2 : 1);

    for (int i = 0; i < petCount; i++) {
        uint8_t id = pet_random_in_tier(maxTier);
        const PetDef* def = pet_def(id);
        out->slots[i].id = id;
        // Stat bump: +1 atk/hp every 2 turns past turn 2
        int bump = (turn > 2) ? (turn - 2) / 2 : 0;
        out->slots[i].atk = (int16_t)mini(def->baseAtk + bump, STAT_CAP);
        out->slots[i].hp  = (int16_t)mini(def->baseHp + bump, STAT_CAP);
        // Occasional level 2 from turn 5+
        if (turn >= 5 && (rng_next() & 0x3) == 0) {
            out->slots[i].level = 2;
            out->slots[i].atk = (int16_t)mini(out->slots[i].atk + 1, STAT_CAP);
            out->slots[i].hp  = (int16_t)mini(out->slots[i].hp + 1, STAT_CAP);
        }
    }

    _rng_state = prevState;
}
