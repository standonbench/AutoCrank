#include "game.h"

// ---------------------------------------------------------------------------
// AI opponent generation by player-action simulation.
//
// Instead of stamping out a stat-bumped team for each turn, this models what
// a competent human would have done with the same gold budget: starting from
// an empty team on turn 1, the AI plays through every preceding turn —
// buying pets, combining duplicates for XP/level-ups, rolling when nothing
// useful sits in the shop — and emerges with whatever team that journey
// produced. Cumulative spending naturally scales: by turn 5 the AI has had
// ~50g of decisions, by turn 10 it's ~100g, mirroring the player's curve.
//
// Deterministic given seed (same seed + turn → same team), so battles remain
// reproducible. Streak bonus is the only "ghost team" tilt, applied post-sim.
// ---------------------------------------------------------------------------

// Hardcoded synergy categories — kept in sync with PETS[] in pets.c.
// Summoners benefit from a buffer behind them and vice versa, so the AI's
// shop evaluator gives that pairing a small value bump.
//   Summoners (faint-spawn):  Hopper (4), Sheep (6)
//   Buffers   (on-summon/atk/faint): Pup (1), Haflinger (8), Ox (9), Adder (10)
static bool is_summoner(uint8_t id) {
    return id == 4 || id == 6;
}
static bool is_buffer(uint8_t id) {
    return id == 1 || id == 8 || id == 9 || id == 10;
}

// Seed-derived shopping taste. Without it every late-game ghost converges on
// the same greedy stat-pile; with it, one opponent hoards summoners, the next
// drafts the snipe package, the next goes wide on buffs — late opponents read
// as archetypes you can scout and answer. BALANCED keeps pure-greedy in the
// rotation (and is forced before turn 4: tiny teams shouldn't go off-meta).
//   SWARM : Pup, Rabbit, Sheep, Haflinger, Donkey
//   SNIPER: Goat, Alligator, Alpaca, Highland, Deer, Fox
//   BUFFER: Hen, Hedgehog, Calf, Duck, Boar, Bear (the economy/shop-value set)
enum { AI_BALANCED = 0, AI_SWARM = 1, AI_SNIPER = 2, AI_BUFFER = 3 };

static bool persona_loves(uint8_t persona, uint8_t id) {
    switch (persona) {
        case AI_SWARM:
            return id == 1 || id == 4 || id == 6 || id == 8 || id == 9;
        case AI_SNIPER:
            return id == 7 || id == 10 || id == 12 || id == 13 ||
                   id == 15 || id == 16;
        case AI_BUFFER:
            return id == 2 || id == 3 || id == 5 || id == 11 || id == 14 ||
                   id == 17;
        default:
            return false;
    }
}

typedef struct {
    Team    team;
    int16_t gold;
    uint8_t persona;
    uint8_t bank;     // Duck economy: gold banked at end of the previous turn
} AIState;

static int ai_find_combine(const AIState* s, uint8_t petId) {
    for (int i = 0; i < TEAM_SIZE; i++)
        if (s->team.slots[i].id == petId &&
            s->team.slots[i].level < MAX_PET_LEVEL) return i;
    return -1;
}

static int ai_find_empty(const AIState* s) {
    for (int i = 0; i < TEAM_SIZE; i++)
        if (s->team.slots[i].id == EMPTY_PET) return i;
    return -1;
}

// Find a sell candidate: an unleveled team pet weak enough that swapping it
// for `newPetId` is worth the 2g net cost (sell refunds 1g, buy costs 3g).
// Leveled pets are protected — they're earned investments.
static int ai_find_replacement_target(const AIState* s, uint8_t newPetId) {
    const PetDef* nd = pet_def(newPetId);
    if (!nd) return -1;
    int newV = nd->baseAtk + nd->baseHp + nd->tier * 3;
    int worstSlot = -1, worstV = 9999;
    for (int i = 0; i < TEAM_SIZE; i++) {
        const PetSlot* p = &s->team.slots[i];
        if (p->id == EMPTY_PET || p->level > 1) continue;
        const PetDef* d = pet_def(p->id);
        int v = d ? (d->baseAtk + d->baseHp + d->tier * 3) : 0;
        if (v < worstV) { worstV = v; worstSlot = i; }
    }
    return (worstSlot >= 0 && newV >= worstV + 4) ? worstSlot : -1;
}

static int ai_synergy_bonus(const AIState* s, uint8_t petId) {
    // One pass over the team, then additive pairing bonuses. Magnitudes sit
    // near the base +6 so taste bends the greedy order, never overrules a
    // clearly better pet (a T-higher stat pile still wins by raw score).
    bool teamSummoner = false, teamBuffer = false, teamSob = false;
    bool teamGator = false, teamAmp = false, teamBigFront = false;
    int  teamFaintProne = 0;
    for (int i = 0; i < TEAM_SIZE; i++) {
        uint8_t id = s->team.slots[i].id;
        if (id == EMPTY_PET) continue;
        if (is_summoner(id)) teamSummoner = true;
        if (is_buffer(id))   teamBuffer = true;
        if (id == 3 || id == 12)  teamSob = true;      // Hedgehog / Alpaca SOB
        if (id == 10)             teamGator = true;    // Alligator
        if (id == 15 || id == 16) teamAmp = true;      // Deer / Fox amplifiers
        if (id == 4 || id == 6 || id == 7 || id == 13)
            teamFaintProne++;                          // faint-payoff bodies
        if (id == 13 || id == 14 || id == 17) teamBigFront = true;
    }
    int bonus = 0;
    if (is_buffer(petId) && teamSummoner)        bonus += 6;
    else if (is_summoner(petId) && teamBuffer)   bonus += 6;
    // Amplifiers want an SOB ability next door (Fox re-fires, Deer copies)…
    if ((petId == 15 || petId == 16) && teamSob) bonus += 8;
    // …Deer also treasures an Alligator to copy (ally-attack snipe engine).
    if (petId == 15 && teamGator)                bonus += 6;
    // The reverse draft: grab the Alpaca nuke when an amplifier is waiting.
    if (petId == 12 && teamAmp)                  bonus += 8;
    // Donkey feeds on friendly deaths; Alligator wants long fights.
    if (petId == 9 && teamFaintProne >= 2)       bonus += 6;
    if (petId == 10 && teamBigFront)             bonus += 4;
    return bonus;
}

// Apply a buy or combine. Stat math mirrors shop_try_buy_pet in shop.c so
// the AI ends up with the same numbers a player would after the same buys.
static void ai_apply_buy(AIState* s, uint8_t petId, uint8_t slot) {
    PetSlot* dst = &s->team.slots[slot];
    if (dst->id == EMPTY_PET) {
        const PetDef* def = pet_def(petId);
        dst->id = petId;
        dst->level = 1;
        dst->xp = 0;
        dst->atk = def ? def->baseAtk : 1;
        dst->hp  = def ? def->baseHp  : 1;
        dst->food = EMPTY_FOOD;
        dst->flags = 0;
    } else if (dst->id == petId && dst->level < MAX_PET_LEVEL) {
        dst->xp++;
        dst->atk = (int16_t)mini(dst->atk + 1, STAT_CAP);
        dst->hp  = (int16_t)mini(dst->hp + 1, STAT_CAP);
        int needed = pet_xp_for_level((uint8_t)(dst->level + 1));
        if (dst->xp >= needed) {
            dst->level++;
            dst->atk = (int16_t)mini(dst->atk + 1, STAT_CAP);
            dst->hp  = (int16_t)mini(dst->hp + 1, STAT_CAP);
        }
    }
    s->gold -= PET_COST;
}

// Refill the AI's shop with the same tier-weighted draw the player sees.
// Scaling comes from cumulative turns of spend, not from tilting the shop.
static void ai_refill_shop(uint8_t shop[3], uint8_t turn) {
    for (int i = 0; i < 3; i++) shop[i] = pet_roll_for_turn(turn);
}

// Score a shop pet against the current team. Combines always win; otherwise
// rank by base stats + tier + synergy bonus. Returns -1 if the pet can't be
// usefully bought (team full and no combine match).
static int ai_eval_shop_pet(const AIState* s, uint8_t petId, uint8_t turn) {
    if (petId == EMPTY_PET) return -1;
    if (ai_find_combine(s, petId) >= 0) return 1000;
    const PetDef* d = pet_def(petId);
    int base = d ? (d->baseAtk + d->baseHp + d->tier * 3) : 0;
    // Taste, not law — comparable to the +6 summoner/buffer pairing bonus,
    // so personas bend the greedy pick order without overruling a clearly
    // better pet. From turn 6 the taste hardens: late ghosts should commit
    // to their archetype instead of drifting back to the stat pile.
    if (persona_loves(s->persona, petId)) base += (turn >= 6) ? 9 : 5;
    if (ai_find_empty(s) >= 0) return base + ai_synergy_bonus(s, petId);
    // Team full — only attractive if a weak L1 can be cheaply swapped out.
    if (ai_find_replacement_target(s, petId) >= 0) return base - 5;
    return -1;
}

// One simulated shop turn: fresh 10g, buy until nothing useful remains,
// then roll a few times to chase combines before ending.
static void ai_simulate_turn(AIState* s, uint8_t turn) {
    s->gold = (int16_t)(STARTING_GOLD + s->bank);
    s->bank = 0;
    uint8_t shop[3];
    ai_refill_shop(shop, turn);
    int rollsLeft = 3;

    int safety = 16;
    while (safety-- > 0 && s->gold >= PET_COST) {
        // Late persona-seeking reroll: a committed archetype shopper spends
        // a coin chasing pets it loves instead of settling for the greedy
        // pick — unless the shop holds a combine (never roll one away).
        if (turn >= 6 && s->persona != AI_BALANCED && rollsLeft > 0 &&
            s->gold >= ROLL_COST + PET_COST) {
            bool worthKeeping = false;
            for (int i = 0; i < 3; i++) {
                if (shop[i] == EMPTY_PET) continue;
                if (persona_loves(s->persona, shop[i]) ||
                    ai_find_combine(s, shop[i]) >= 0) { worthKeeping = true; break; }
            }
            if (!worthKeeping) {
                s->gold -= ROLL_COST;
                rollsLeft--;
                ai_refill_shop(shop, turn);
                continue;
            }
        }

        int bestI = -1, bestV = -1;
        for (int i = 0; i < 3; i++) {
            int v = ai_eval_shop_pet(s, shop[i], turn);
            if (v > bestV) { bestV = v; bestI = i; }
        }
        if (bestI >= 0 && bestV >= 0) {
            int slot = ai_find_combine(s, shop[bestI]);
            if (slot < 0) slot = ai_find_empty(s);
            if (slot < 0) {
                // Swap-out path: sell the weakest L1 then drop the new pet
                // into the freed slot. Sell refund = 1g for an L1.
                slot = ai_find_replacement_target(s, shop[bestI]);
                if (slot < 0) break;
                PetSlot* old = &s->team.slots[slot];
                s->gold += 1;
                old->id = EMPTY_PET; old->level = 1; old->xp = 0;
                old->atk = 0; old->hp = 0;
                old->food = EMPTY_FOOD; old->flags = 0;
            }
            ai_apply_buy(s, shop[bestI], (uint8_t)slot);
            shop[bestI] = EMPTY_PET;
        } else if (rollsLeft > 0 && s->gold >= ROLL_COST + PET_COST) {
            s->gold -= ROLL_COST;
            rollsLeft--;
            ai_refill_shop(shop, turn);
        } else {
            break;
        }
    }

    // End-of-turn banking (mirrors pet_goose_eot): each Duck lays away its
    // level in gold for next turn's shop, capped like the real handler.
    for (int i = 0; i < TEAM_SIZE; i++) {
        const PetSlot* p = &s->team.slots[i];
        if (p->id == 11)
            s->bank = (uint8_t)mini(s->bank + p->level, 6);
    }
}

// ---------------------------------------------------------------------------
// Positioning pass. The journey sim fills slots in buy order, which reads
// as a random pile; a competent player lines up: bodies and faint-engines
// up front (slot 4 fights first), position-agnostic pets mid, snipers and
// support in the back. 0 = back, 1 = mid, 2 = front, indexed by pet id.
// ---------------------------------------------------------------------------
static const uint8_t ROLE_POS[PET_COUNT] = {
    /* Squirrel */ 0, /* Pup      */ 0, /* Hen    */ 1, /* Hedgehog */ 1,
    /* Rabbit   */ 2, /* Calf     */ 2, /* Sheep  */ 2, /* Goat     */ 2,
    /* Haflinger*/ 0, /* Donkey   */ 1, /* Alligator*/ 0, /* Duck   */ 1,
    /* Alpaca   */ 0, /* Highland */ 2, /* Boar   */ 2, /* Deer     */ 0,
    /* Fox      */ 0, /* Bear     */ 2,
};

static void ai_swap_slots(Team* t, int a, int b) {
    if (a == b) return;
    PetSlot tmp = t->slots[a];
    t->slots[a] = t->slots[b];
    t->slots[b] = tmp;
}

void ai_position_team(Team* t) {
    // Compact live pets to slots 0..n-1, preserving buy order.
    PetSlot live[TEAM_SIZE];
    int n = 0;
    for (int i = 0; i < TEAM_SIZE; i++)
        if (t->slots[i].id != EMPTY_PET) live[n++] = t->slots[i];
    if (n == 0) return;
    // Stable insertion sort ascending by role — front roles end at the high
    // indices (battle.c frontmost_alive: rightmost slot fights first).
    for (int i = 1; i < n; i++) {
        PetSlot key = live[i];
        int j = i - 1;
        while (j >= 0 && ROLE_POS[live[j].id] > ROLE_POS[key.id]) {
            live[j + 1] = live[j];
            j--;
        }
        live[j + 1] = key;
    }
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (i < n) { t->slots[i] = live[i]; continue; }
        t->slots[i].id = EMPTY_PET;
        t->slots[i].level = 1; t->slots[i].xp = 0;
        t->slots[i].atk = 0; t->slots[i].hp = 0;
        t->slots[i].food = EMPTY_FOOD; t->slots[i].flags = 0;
    }

    // Deterministic adjacency fix-ups (one bounded pass each):
    // Fox re-fires its RIGHT neighbour's SOB — park Hedgehog/Alpaca there.
    int fox = -1;
    for (int i = 0; i < n; i++) if (t->slots[i].id == 16) { fox = i; break; }
    if (fox >= 0 && n >= 2) {
        if (fox == n - 1) { ai_swap_slots(t, fox, fox - 1); fox--; }
        if (t->slots[fox + 1].id != 3 && t->slots[fox + 1].id != 12) {
            for (int i = 0; i < n; i++) {
                if (i == fox) continue;
                if (t->slots[i].id == 3 || t->slots[i].id == 12) {
                    ai_swap_slots(t, i, fox + 1);
                    break;
                }
            }
        }
    }
    // Deer copies its LEFT neighbour — park an SOB pet or the Alligator
    // snipe engine there.
    int deer = -1;
    for (int i = 0; i < n; i++) if (t->slots[i].id == 15) { deer = i; break; }
    if (deer >= 0 && n >= 2) {
        if (deer == 0) { ai_swap_slots(t, deer, deer + 1); deer++; }
        uint8_t left = t->slots[deer - 1].id;
        if (left != 3 && left != 12 && left != 10) {
            for (int i = 0; i < n; i++) {
                if (i == deer || i == deer - 1) continue;
                uint8_t id = t->slots[i].id;
                if (id == 3 || id == 12 || id == 10) {
                    ai_swap_slots(t, i, deer - 1);
                    break;
                }
            }
        }
    }
}

// Pure journey sim: the team a competent shopper would field on `turn`,
// given `seed`. No difficulty tilt — that lives in ai_apply_handicap so the
// balance harness can drive the two independently (run-level Monte Carlo
// pilots this same policy as the "player").
void ai_sim_team(Team* out, uint8_t turn, uint32_t seed) {
    uint32_t prevState = _rng_state;
    rng_seed(seed);

    AIState s;
    for (int i = 0; i < TEAM_SIZE; i++) {
        s.team.slots[i].id = EMPTY_PET;
        s.team.slots[i].level = 1;
        s.team.slots[i].xp = 0;
        s.team.slots[i].atk = 0;
        s.team.slots[i].hp = 0;
        s.team.slots[i].food = EMPTY_FOOD;
        s.team.slots[i].flags = 0;
    }
    s.gold = 0;
    s.bank = 0;
    // Persona bits sit above the low bits rng_range consumes first, and the
    // whole derivation is a pure function of (seed, turn) — deterministic,
    // resume-safe, no save-format change.
    s.persona = (turn >= 4) ? (uint8_t)((seed >> 9) & 3) : (uint8_t)AI_BALANCED;

    // Same tier-weighted shop schedule the player lives on (pets.c
    // pet_roll_for_turn drives both).
    for (uint8_t t = 1; t <= turn; t++)
        ai_simulate_turn(&s, t);

    // Late-game food pass: a competent player spends leftover gold on food
    // for the final-turn team. Approximated with direct stat/flag writes
    // (Cowbell = flag 0x01, Milk = +2/+2) — deliberately NOT p->food, since
    // the held-food battle application in game_advance_to_battle only runs
    // for the player side. Deterministic: fixed targets, no RNG draws.
    if (turn >= 6) {
        int front = -1;
        for (int i = TEAM_SIZE - 1; i >= 0; i--)
            if (s.team.slots[i].id != EMPTY_PET) { front = i; break; }
        int fed = 0;
        if (front >= 0 && s.persona == AI_SNIPER && s.gold >= 4) {
            s.team.slots[front].flags |= 0x01;   // Cowbell: takes 2 less dmg
            s.gold -= 4;
            fed++;
        }
        if (s.persona == AI_SWARM || s.persona == AI_BUFFER) {
            while (fed < 2 && s.gold >= 3) {     // Milk the carry (+2/+2)
                int best = -1, bestAtk = -1;
                for (int i = TEAM_SIZE - 1; i >= 0; i--) {
                    PetSlot* p = &s.team.slots[i];
                    if (p->id != EMPTY_PET && (int)p->atk > bestAtk) {
                        bestAtk = p->atk;
                        best = i;
                    }
                }
                if (best < 0) break;
                s.team.slots[best].atk = (int16_t)mini(s.team.slots[best].atk + 2, STAT_CAP);
                s.team.slots[best].hp  = (int16_t)mini(s.team.slots[best].hp + 2, STAT_CAP);
                s.gold -= 3;
                fed++;
            }
        }
    }

    ai_position_team(&s.team);

    *out = s.team;
    _rng_state = prevState;
}

// Difficulty tilt, applied after the sim. Design rules (balance harness):
// every ±1 total enemy stat moves the policy-player winrate ~∓2.5%, so all
// deltas here are SINGLE-DIGIT totals. No tilt at 0-7 wins — mid-run success
// is never punished (the old wins-scaled bump rubber-banded the run into a
// 70%-loss wall exactly when the player approached victory). Instead the
// climb has three named beats:
//   on-ramp  (turns 1-2):  enemies -1 hp — first battles tilt player-ward
//   foothill (at 8 wins):  frontmost enemy +1 hp — the climb stiffens
//   summit   (at 9 wins):  all enemies +1 hp — the epic, ~55-60% final fight
// Mercy is independent and stacks: <=2 lives softens, last life softens more.
void ai_apply_handicap(Team* t, uint8_t wins, uint8_t lives, uint8_t turn) {
    int hpDelta = 0, atkDelta = 0, frontHp = 0;
    if (turn <= 2)                hpDelta -= 1;   // on-ramp
    if (turn >= 6)                frontHp += 1;   // alpine hardening
    if (wins == 8)                frontHp += 1;   // foothill
    if (wins >= 9) { hpDelta += 1; frontHp -= 1; } // summit: the hardening
    if (frontHp < 0)              frontHp  = 0;    // folds into a full-team
                                                   // bump, it doesn't stack
    if (lives >= 1 && lives <= 2) hpDelta -= 1;   // mercy
    if (lives == 1)               atkDelta -= 1;  // deep mercy
    if (!hpDelta && !atkDelta && !frontHp) return;

    int front = -1;                               // battle.c: rightmost fights
    for (int i = TEAM_SIZE - 1; i >= 0; i--)
        if (t->slots[i].id != EMPTY_PET) { front = i; break; }

    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &t->slots[i];
        if (p->id == EMPTY_PET) continue;
        int hp  = p->hp + hpDelta + (i == front ? frontHp : 0);
        int atk = p->atk + atkDelta;
        p->hp  = (int16_t)mini(hp  < 1 ? 1 : hp,  STAT_CAP);
        p->atk = (int16_t)mini(atk < 0 ? 0 : atk, STAT_CAP);
    }
}

void ai_generate_opponent(Team* out, uint8_t turn, uint32_t seed) {
    ai_sim_team(out, turn, seed);
    ai_apply_handicap(out, game.wins, game.lives, turn);
}
