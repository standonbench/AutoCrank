#include "game.h"

// ---------------------------------------------------------------------------
// Battle log helper
// ---------------------------------------------------------------------------
void battle_log_push(BattleEventKind kind, uint8_t side, uint8_t slot,
                     int16_t value, uint8_t data)
{
    if (game.battleLogLen >= MAX_BATTLE_EVENTS) return;
    BattleEvent* e = &game.battleLog[game.battleLogLen++];
    e->kind = (uint8_t)kind;
    e->side = side;
    e->slot = slot;
    e->value = value;
    e->data = data;
}

// ---------------------------------------------------------------------------
// Battle simulation — populates game.battleLog.
// ---------------------------------------------------------------------------
static int frontmost_alive(Team* t) {
    // SAP attack order: rightmost slot attacks first. Frontmost = highest idx alive.
    for (int i = TEAM_SIZE - 1; i >= 0; i--) {
        if (t->slots[i].id != EMPTY_PET && t->slots[i].hp > 0) return i;
    }
    return -1;
}

static bool any_alive(Team* t) { return frontmost_alive(t) >= 0; }

// Resolve faints iteratively so cascading deaths (Hedgehog AoE → more deaths
// → more triggers) all fire. The dying pet's slot stays "alive" until AFTER
// its FAINT handler runs, so Mouse/Cricket/Sheep etc. can act on their own
// death. If the handler replaced the pet in-slot (Cricket→Zombie Cricket),
// the slot is NOT cleared.
static bool resolve_faints(BattleState* bs)
{
    bool anyEver = false;
    int safety = 20;
    while (safety-- > 0) {
        bool any = false;
        uint8_t dead[2][TEAM_SIZE];
        for (int sd = 0; sd < 2; sd++)
            for (int s = 0; s < TEAM_SIZE; s++) dead[sd][s] = EMPTY_PET;

        for (int side = 0; side < 2; side++) {
            for (int s = 0; s < TEAM_SIZE; s++) {
                PetSlot* p = &bs->teams[side].slots[s];
                if (p->id != EMPTY_PET && p->hp <= 0) {
                    battle_log_push(BEV_FAINT, (uint8_t)side, (uint8_t)s, 0, p->id);
                    trig_push(TRIG_FAINT, (uint8_t)side, (uint8_t)s, (uint8_t)s, p->id);
                    trig_push(TRIG_FRIEND_FAINTS, (uint8_t)side, (uint8_t)s, (uint8_t)s, p->id);
                    dead[side][s] = p->id;
                    any = true;
                    anyEver = true;
                }
            }
        }
        if (!any) break;
        trig_drain(bs);
        // Clear slots whose pet ID is unchanged (handler didn't replace it)
        for (int side = 0; side < 2; side++) {
            for (int s = 0; s < TEAM_SIZE; s++) {
                PetSlot* p = &bs->teams[side].slots[s];
                if (dead[side][s] != EMPTY_PET && p->id == dead[side][s]) {
                    p->id = EMPTY_PET;
                    p->atk = 0;
                    p->hp = 0;
                    p->level = 1;
                    p->xp = 0;
                    p->food = EMPTY_FOOD;
                    p->flags = 0;
                }
            }
        }
    }
    return anyEver;
}

void battle_begin(void) {
    game.battleLogLen = 0;
    trig_reset();
}

void battle_simulate(void) {
    BattleState* bs = &game.battle;
    battle_begin();

    // Start of battle: fire trigger (no handlers wired yet, but log abilities).
    trig_push(TRIG_START_OF_BATTLE, 0, 0, 0, 0);
    trig_push(TRIG_START_OF_BATTLE, 1, 0, 0, 0);
    trig_drain(bs);
    resolve_faints(bs);

    // Combat loop
    int safety = 100;
    while (safety-- > 0 && any_alive(&bs->teams[0]) && any_alive(&bs->teams[1])) {
        int p = frontmost_alive(&bs->teams[0]);
        int e = frontmost_alive(&bs->teams[1]);
        if (p < 0 || e < 0) break;

        PetSlot* pp = &bs->teams[0].slots[p];
        PetSlot* ee = &bs->teams[1].slots[e];

        // Before attack
        trig_push(TRIG_BEFORE_ATTACK, 0, (uint8_t)p, (uint8_t)e, 0);
        trig_push(TRIG_BEFORE_ATTACK, 1, (uint8_t)e, (uint8_t)p, 0);
        trig_drain(bs);

        // Re-fetch frontmost — abilities may have killed/spawned units
        p = frontmost_alive(&bs->teams[0]);
        e = frontmost_alive(&bs->teams[1]);
        if (p < 0 || e < 0) break;
        pp = &bs->teams[0].slots[p];
        ee = &bs->teams[1].slots[e];

        // Mutual attack
        int16_t pAtk = pp->atk;
        int16_t eAtk = ee->atk;
        battle_log_push(BEV_ATTACK, 0, (uint8_t)p, pAtk, (uint8_t)e);
        battle_log_push(BEV_ATTACK, 1, (uint8_t)e, eAtk, (uint8_t)p);
        ee->hp -= pAtk;
        pp->hp -= eAtk;
        battle_log_push(BEV_DAMAGE, 1, (uint8_t)e, pAtk, 0);
        battle_log_push(BEV_DAMAGE, 0, (uint8_t)p, eAtk, 0);

        // After attack triggers + faint resolution
        trig_push(TRIG_AFTER_ATTACK, 0, (uint8_t)p, (uint8_t)e, pAtk);
        trig_push(TRIG_AFTER_ATTACK, 1, (uint8_t)e, (uint8_t)p, eAtk);
        trig_push(TRIG_FRIEND_ATTACK, 0, (uint8_t)p, (uint8_t)e, pAtk);
        trig_push(TRIG_FRIEND_ATTACK, 1, (uint8_t)e, (uint8_t)p, eAtk);
        trig_drain(bs);
        resolve_faints(bs);
    }

    // Determine result
    bool pAlive = any_alive(&bs->teams[0]);
    bool eAlive = any_alive(&bs->teams[1]);
    if (pAlive && !eAlive)      game.battleResult = 0; // win
    else if (!pAlive && eAlive) game.battleResult = 1; // loss
    else                        game.battleResult = 2; // draw
    battle_log_push(BEV_RESULT, 0, 0, game.battleResult, 0);
}

// ---------------------------------------------------------------------------
// Playback — chew through events at a rate set by crank.
// ---------------------------------------------------------------------------
void battle_playback_step(void) {
    // Crank-driven speed: 1× by default, scales with rotation rate.
    float crankChange = pd->system->getCrankChange();
    if (crankChange < 0) crankChange = -crankChange;
    int speed = 1 + (int)(crankChange / 5.0f);
    if (speed < 1) speed = 1;
    if (speed > 8) speed = 8;
    game.battleSpeed = (uint8_t)speed;

    // Skip to end with A
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (prs & kButtonA) {
        game.battlePlayhead = game.battleLogLen;
    }

    // Each event consumes a chunk of game.battleClock
    const float baseEventDur = 6.0f;  // frames per event at speed 1
    game.battleClock += (float)speed;

    while (game.battleClock >= baseEventDur && game.battlePlayhead < game.battleLogLen) {
        game.battleClock -= baseEventDur;
        BattleEvent* e = &game.battleLog[game.battlePlayhead++];
        if (e->kind == BEV_ATTACK)  sfx_attack();
        if (e->kind == BEV_FAINT)   sfx_faint();
    }

    if (game.battlePlayhead >= game.battleLogLen) {
        game.state = STATE_RESULT;
        game.resultTimer = 0;
    }
}
