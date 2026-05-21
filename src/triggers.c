#include "game.h"

static TriggerEvent g_q[TRIG_QUEUE_MAX];
static uint8_t g_head, g_tail;

// Wrap game.player in a BattleState so shop-context triggers can use the
// same handler dispatch as battle. The opponent side is left empty so
// "all enemies" effects safely no-op in the shop.
void trig_shop_drain(void) {
    static BattleState tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.teams[0] = game.player;
    for (int i = 0; i < TEAM_SIZE; i++) tmp.teams[1].slots[i].id = EMPTY_PET;
    trig_drain(&tmp);
    game.player = tmp.teams[0];
}

void trig_reset(void) {
    g_head = g_tail = 0;
}

void trig_push(TriggerKind kind, uint8_t side, uint8_t srcSlot,
               uint8_t dataSlot, int16_t data)
{
    uint8_t next = (uint8_t)((g_tail + 1) % TRIG_QUEUE_MAX);
    if (next == g_head) {
        DLOG("trig queue full, dropping kind=%d", kind);
        return;
    }
    g_q[g_tail].kind = (uint8_t)kind;
    g_q[g_tail].side = side;
    g_q[g_tail].srcSlot = srcSlot;
    g_q[g_tail].dataSlot = dataSlot;
    g_q[g_tail].data = data;
    g_tail = next;
}

// Drain the queue, dispatching each event to every pet's handler for that kind.
// Pets fire in descending attack order (SAP rule).
void trig_drain(BattleState* bs)
{
    while (g_head != g_tail) {
        TriggerEvent ev = g_q[g_head];
        g_head = (uint8_t)((g_head + 1) % TRIG_QUEUE_MAX);

        // Build attack-sorted index list across BOTH sides for this trigger.
        typedef struct { uint8_t side, slot; int16_t atk; } OrderEntry;
        OrderEntry order[TEAM_SIZE * 2];
        int n = 0;
        for (int side = 0; side < 2; side++) {
            for (int s = 0; s < TEAM_SIZE; s++) {
                PetSlot* p = &bs->teams[side].slots[s];
                if (p->id == EMPTY_PET) continue;
                order[n].side = (uint8_t)side;
                order[n].slot = (uint8_t)s;
                order[n].atk  = p->atk;
                n++;
            }
        }
        // Insertion sort by descending attack
        for (int i = 1; i < n; i++) {
            for (int j = i; j > 0 && order[j].atk > order[j-1].atk; j--) {
                OrderEntry tmp = order[j];
                order[j] = order[j-1];
                order[j-1] = tmp;
            }
        }

        for (int i = 0; i < n; i++) {
            PetSlot* p = &bs->teams[order[i].side].slots[order[i].slot];
            if (p->id == EMPTY_PET) continue;
            const PetDef* def = pet_def(p->id);
            if (!def) continue;
            PetHandler h = def->handlers[ev.kind];
            if (!h) continue;
            h(bs, order[i].side, order[i].slot, &ev);
        }
    }
}
