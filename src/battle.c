#include "game.h"

// ---------------------------------------------------------------------------
// Apply a battle event to the live teams (mutator). Used during sim AND
// playback so they stay in sync.
// ---------------------------------------------------------------------------
void apply_event(BattleState* bs, BattleEvent* e) {
    PetSlot* p = (e->side < 2 && e->slot < TEAM_SIZE)
                    ? &bs->teams[e->side].slots[e->slot] : NULL;
    switch (e->kind) {
        case BEV_DAMAGE:
            if (p && p->id != EMPTY_PET) {
                int dmg = e->value;
                if (p->flags & 0x01) dmg = (dmg > 2) ? dmg - 2 : 0;
                p->hp -= (int16_t)dmg;
                if (p->hp < 0) p->hp = 0;
            }
            break;
        case BEV_FAINT:
            if (p) {
                p->id = EMPTY_PET;
                p->atk = 0;
                p->hp = 0;
                p->level = 1;
                p->xp = 0;
                p->food = EMPTY_FOOD;
                p->flags = 0;
                // Mark the slot's eased x as stale; if a SPAWN re-occupies
                // this slot, the new unit should snap into the rank, not
                // drift from wherever the flyoff left the position.
                render_battle_reset_slot_position((int)e->side, (int)e->slot);
            }
            break;
        case BEV_SPAWN:
            if (p) {
                p->id = e->data;
                p->level = 1;
                p->xp = 0;
                // SPAWN's value field encodes atk in low byte, hp in high byte
                int a = e->value & 0xFF;
                int h = (e->value >> 8) & 0xFF;
                p->atk = (int16_t)a;
                p->hp  = (int16_t)h;
                p->food = EMPTY_FOOD;
                p->flags = 0;
            }
            break;
        case BEV_BUFF: {
            if (!p || p->id == EMPTY_PET) break;
            int da = e->value & 0xFF;
            int dh = (e->value >> 8) & 0xFF;
            p->atk = (int16_t)mini(p->atk + da, STAT_CAP);
            p->hp  = (int16_t)mini(p->hp + dh, STAT_CAP);
            break;
        }
        case BEV_CLASH: {
            // Front-line exchange: slot = player front, data = enemy front.
            int pDmg = e->value & 0xFF;          // player deals -> enemy
            int eDmg = (e->value >> 8) & 0xFF;   // enemy deals  -> player
            PetSlot* pp = &bs->teams[0].slots[e->slot];
            PetSlot* ee = &bs->teams[1].slots[e->data];
            if (pp->id != EMPTY_PET) {
                int d = eDmg;
                if (pp->flags & 0x01) d = (d > 2) ? d - 2 : 0;
                pp->hp -= (int16_t)d;
                if (pp->hp < 0) pp->hp = 0;
            }
            if (ee->id != EMPTY_PET) {
                int d = pDmg;
                if (ee->flags & 0x01) d = (d > 2) ? d - 2 : 0;
                ee->hp -= (int16_t)d;
                if (ee->hp < 0) ee->hp = 0;
            }
            break;
        }
        case BEV_ATTACK:
        case BEV_ABILITY:
        case BEV_RESULT:
            break;
    }
}

// ---------------------------------------------------------------------------
// Push an event to the log WITHOUT applying it. Used for ATTACK/ABILITY/RESULT
// (visual-only) and for FAINT (apply must be deferred until after the dying
// pet's handler runs).
// ---------------------------------------------------------------------------
void battle_log_push(BattleEventKind kind, uint8_t side, uint8_t slot,
                     int16_t value, uint8_t data)
{
    // Only record into the log during a real battle (shop-context triggers
    // run the same handlers but must not pollute the playback log).
    if (!game.battle.inBattle) return;
    if (game.battleLogLen >= MAX_BATTLE_EVENTS) return;
    BattleEvent* e = &game.battleLog[game.battleLogLen++];
    e->kind = (uint8_t)kind;
    e->side = side;
    e->slot = slot;
    e->value = value;
    e->data = data;
}

// Push AND apply immediately. Use for BUFF/DAMAGE/SPAWN/FAINT so the live
// state stays consistent across subsequent triggers. Playback re-applies the
// same events to the initial snapshot, so effects are never double-applied.
void battle_emit(BattleState* bs, BattleEventKind kind, uint8_t side, uint8_t slot,
                 int16_t value, uint8_t data)
{
    BattleEvent e;
    e.kind = (uint8_t)kind;
    e.side = side;
    e.slot = slot;
    e.value = value;
    e.data = data;
    apply_event(bs, &e);
    if (game.battle.inBattle && game.battleLogLen < MAX_BATTLE_EVENTS) {
        game.battleLog[game.battleLogLen++] = e;
    }
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
// → more triggers) all fire. The dying pet's slot stays "alive" while its
// FAINT handler runs (so Mouse/Cricket/Sheep can act on their own death).
// AFTER the trigger drain, we emit the BEV_FAINT events — which clear the
// slot via apply_event. If a handler replaced the pet in-slot (Cricket spawn),
// the slot is NOT cleared.
bool battle_resolve_faints(BattleState* bs)
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
        // Emit FAINT events (apply_event clears the slot) unless the handler
        // already replaced the pet. Detection: id-match alone isn't enough,
        // because Hopper→Hopper spawns reuse the same id; require hp <= 0 to
        // confirm the slot still holds the dying pet (a spawn would have set
        // hp > 0). If the dying pet held Edelweiss (food id 1), a 1/1 Bee
        // Buddy hatches into the freshly-vacated slot.
        for (int side = 0; side < 2; side++) {
            for (int s = 0; s < TEAM_SIZE; s++) {
                PetSlot* p = &bs->teams[side].slots[s];
                if (dead[side][s] != EMPTY_PET && p->id == dead[side][s] &&
                    p->hp <= 0) {
                    bool hadHoney = (p->food == 1);
                    battle_emit(bs, BEV_FAINT, (uint8_t)side, (uint8_t)s, 0, dead[side][s]);
                    if (hadHoney) {
                        // Edelweiss hatches a 2/2 Bee Buddy in the vacated slot.
                        int16_t bp = (int16_t)(2 | (2 << 8));
                        battle_emit(bs, BEV_SPAWN, (uint8_t)side, (uint8_t)s,
                                    bp, 3 /* Bee id */);
                    }
                }
            }
        }
    }
    return anyEver;
}

void battle_begin(void) {
    game.battleLogLen = 0;
    trig_reset();
    render_battle_reset_positions();
}

void battle_simulate(void) {
    BattleState* bs = &game.battle;
    battle_begin();

    // Snapshot initial state so playback can rewind for visual replay.
    bs->initial[0] = bs->teams[0];
    bs->initial[1] = bs->teams[1];

    // Start of battle: fire trigger (no handlers wired yet, but log abilities).
    // One global start-of-battle event — the dispatcher fans it out to every
    // pet on both sides, each firing its own handler exactly once.
    trig_push(TRIG_START_OF_BATTLE, 0, 0, 0, 0);
    trig_drain(bs);
    battle_resolve_faints(bs);

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

        // Front-line clash — one combined exchange event
        int pAtk = pp->atk; if (pAtk > 255) pAtk = 255;
        int eAtk = ee->atk; if (eAtk > 255) eAtk = 255;
        int16_t packed = (int16_t)((pAtk & 0xFF) | ((eAtk & 0xFF) << 8));
        battle_emit(bs, BEV_CLASH, 0, (uint8_t)p, packed, (uint8_t)e);

        // After attack triggers + faint resolution
        trig_push(TRIG_AFTER_ATTACK, 0, (uint8_t)p, (uint8_t)e, pAtk);
        trig_push(TRIG_AFTER_ATTACK, 1, (uint8_t)e, (uint8_t)p, eAtk);
        trig_push(TRIG_FRIEND_ATTACK, 0, (uint8_t)p, (uint8_t)e, pAtk);
        trig_push(TRIG_FRIEND_ATTACK, 1, (uint8_t)e, (uint8_t)p, eAtk);
        trig_drain(bs);
        battle_resolve_faints(bs);
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
// Per-event animation window (in frames at 1x speed)
// ---------------------------------------------------------------------------
int battle_event_duration(uint8_t kind) {
    switch (kind) {
        case BEV_CLASH:   return 34;   // rush in, collide, recoil — give it weight
        case BEV_ATTACK:  return 16;
        case BEV_DAMAGE:  return 13;
        case BEV_FAINT:   return 16;   // quick KO fly-off (ability kills)
        case BEV_BUFF:    return 15;
        case BEV_SPAWN:   return 11;   // shimmer + emerge
        case BEV_ABILITY: return 14;
        case BEV_RESULT:  return 3;
    }
    return 9;
}

// Did the faint at `faintIdx` happen as the direct result of a front-line
// clash? If so its fly-off plays during the clash window — the FAINT event
// itself needs no window of its own.
//
// We scan back to the most recent CLASH the slot participated in. If between
// that clash and this FAINT there's been NO further BEV_DAMAGE or BEV_SPAWN
// for this slot, the unit is the same one that already flew off in the clash
// window and a second flyoff would look like it "respawned and died again."
// If there WAS damage/spawn for this slot in between, the killing blow was
// not the clash (it was an AoE, a faint-counter, etc.) — keep the FAINT
// animation so the death reads clearly.
static bool faint_from_clash(int faintIdx) {
    if (faintIdx < 0 || faintIdx >= game.battleLogLen) return false;
    BattleEvent* f = &game.battleLog[faintIdx];
    if (f->kind != BEV_FAINT) return false;
    int clashIdx = -1;
    for (int i = faintIdx - 1; i >= 0; i--) {
        BattleEvent* e = &game.battleLog[i];
        if (e->kind != BEV_CLASH) continue;
        bool isParticipant = (f->side == 0 && e->slot == f->slot) ||
                             (f->side == 1 && e->data == f->slot);
        if (isParticipant) { clashIdx = i; break; }
    }
    if (clashIdx < 0) return false;
    for (int i = clashIdx + 1; i < faintIdx; i++) {
        BattleEvent* e = &game.battleLog[i];
        if ((e->kind == BEV_DAMAGE || e->kind == BEV_SPAWN) &&
            e->side == f->side && e->slot == f->slot) {
            return false;
        }
    }
    return true;
}

// Animation window for the event at `playhead`. Clash-driven faints collapse
// to zero — the dying unit already flew off during the clash window itself.
int battle_event_window(int playhead) {
    if (playhead < 0 || playhead >= game.battleLogLen) return 0;
    BattleEvent* e = &game.battleLog[playhead];
    if (e->kind == BEV_FAINT && faint_from_clash(playhead)) return 0;
    return battle_event_duration(e->kind);
}

// ---------------------------------------------------------------------------
// Playback — animate one event at a time; crank scales speed.
// Each event animates over its window, then is applied at the window's end.
// ---------------------------------------------------------------------------
void battle_playback_step(void) {
    BattleState* bs = &game.battle;

    // Reset to initial snapshot on the very first frame of playback.
    if (game.battlePlayhead == 0 && game.battleClock == 0.0f) {
        bs->teams[0] = bs->initial[0];
        bs->teams[1] = bs->initial[1];
    }

    // Crank-driven speed: 1x baseline, scales with rotation rate.
    // A deadzone keeps idle crank jitter from quietly fast-forwarding.
    float crankChange = pd->system->getCrankChange();
    if (crankChange < 0) crankChange = -crankChange;
    if (crankChange < 6.0f) crankChange = 0.0f;
    int speed = 1 + (int)(crankChange / 5.0f);
    if (speed < 1) speed = 1;

    // A held: speed boost (no longer skips to end — too easy to fat-finger
    // and miss the whole fight). Floors the speed to 4× while held.
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if ((cur & kButtonA) && speed < 4) speed = 4;

    if (speed > 8) speed = 8;
    game.battleSpeed = (uint8_t)speed;

    game.battleClock += (float)speed;

    // Advance through events whose animation window has fully elapsed.
    while (game.battlePlayhead < game.battleLogLen) {
        BattleEvent* e = &game.battleLog[game.battlePlayhead];
        float dur = (float)battle_event_window(game.battlePlayhead);
        if (game.battleClock < dur) break;
        game.battleClock -= dur;
        apply_event(bs, e);
        if (e->kind == BEV_ATTACK)  sfx_attack();
        if (e->kind == BEV_FAINT)   sfx_faint();
        game.battlePlayhead++;
    }

    if (game.battlePlayhead >= game.battleLogLen) {
        game.state = STATE_RESULT;
        game.resultTimer = 0;
    }
}
