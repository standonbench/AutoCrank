#include "game.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout constants — pet card is 60w x 84h. Five fit in 5*60 + 4*4 = 316,
// leaving room for 2 food slots (32x40) on the right.
// ---------------------------------------------------------------------------
#define CARD_W       60
#define CARD_H       92
#define CARD_GAP      4
#define SPRITE_SIZE  32

#define TOPBAR_H     18

#define FOOD_W       32
#define FOOD_H       40

extern bool g_debugOverlay;

// ---------------------------------------------------------------------------
// Empty slot — dashed border
// ---------------------------------------------------------------------------
void render_empty_slot(int x, int y, bool selected)
{
    pd->graphics->setDrawMode(kDrawModeCopy);
    GFX_FILL(x, y, CARD_W, CARD_H, kColorWhite);
    // Dashed outline
    for (int i = 0; i < CARD_W; i += 4) {
        GFX_FILL(x + i, y,             2, 1, kColorBlack);
        GFX_FILL(x + i, y + CARD_H - 1, 2, 1, kColorBlack);
    }
    for (int i = 0; i < CARD_H; i += 4) {
        GFX_FILL(x,             y + i, 1, 2, kColorBlack);
        GFX_FILL(x + CARD_W - 1, y + i, 1, 2, kColorBlack);
    }
    if (selected) {
        GFX_RECT(x - 2, y - 2, CARD_W + 4, CARD_H + 4, kColorBlack);
        GFX_RECT(x - 1, y - 1, CARD_W + 2, CARD_H + 2, kColorBlack);
    }
}

// ---------------------------------------------------------------------------
// Pet card — solid outline, sprite/letter top, name middle, atk/hp bottom
// ---------------------------------------------------------------------------
void render_pet_card(uint8_t petId, uint8_t level, int16_t atk, int16_t hp,
                     uint8_t heldFood, int x, int y, bool selected)
{
    if (petId == EMPTY_PET) {
        render_empty_slot(x, y, selected);
        return;
    }

    pd->graphics->setDrawMode(kDrawModeCopy);
    GFX_FILL(x, y, CARD_W, CARD_H, kColorWhite);
    GFX_RECT(x, y, CARD_W, CARD_H, kColorBlack);

    // Sprite / fallback letter area (centered, 32x32 at top)
    int sx = x + (CARD_W - SPRITE_SIZE) / 2;
    int sy = y + 6;
    LCDBitmap* bmp = image_pet(petId);
    if (bmp) {
        pd->graphics->drawScaledBitmap(bmp, sx, sy, 2.0f, 2.0f);
    } else {
        // Stub: bordered square with a large centered letter
        GFX_FILL(sx, sy, SPRITE_SIZE, SPRITE_SIZE, kColorWhite);
        GFX_RECT(sx, sy, SPRITE_SIZE, SPRITE_SIZE, kColorBlack);
        const char* nm = pet_name(petId);
        if (nm && nm[0]) {
            char letter[2] = { nm[0], 0 };
            LCDFont* f = game.fontLarge ? game.fontLarge : game.fontBold;
            if (f) pd->graphics->setFont(f);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            // approximate centering (we don't have getTextWidth handy here)
            pd->graphics->drawText(letter, 1, kASCIIEncoding, sx + 10, sy + 7);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // Level dots (top-left, outside sprite area)
    for (int i = 0; i < level && i < MAX_PET_LEVEL; i++) {
        pd->graphics->fillEllipse(x + 2 + i * 5, y + 2, 3, 3, 0, 360, kColorBlack);
    }

    // Held food indicator (top-right, small filled square)
    if (heldFood != EMPTY_FOOD) {
        GFX_FILL(x + CARD_W - 8, y + 2, 6, 6, kColorBlack);
    }

    // Pet name (centered under sprite).
    const char* nm = pet_name(petId);
    if (nm) {
        LCDFont* nf = game.fontSmall ? game.fontSmall : game.fontBold;
        if (nf) pd->graphics->setFont(nf);
        pd->graphics->setDrawMode(kDrawModeCopy);
        int w = nf ? pd->graphics->getTextWidth(nf, nm, strlen(nm), kASCIIEncoding, 0)
                   : (int)strlen(nm) * 6;
        if (w <= 0) w = (int)strlen(nm) * 6;
        int tx = x + (CARD_W - w) / 2;
        if (tx < x) tx = x;
        pd->graphics->drawText(nm, strlen(nm), kASCIIEncoding, tx, y + 42);
    }

    // Stat strip: bold "atk/hp" centered near the bottom
    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", atk, hp);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->setDrawMode(kDrawModeCopy);
    int w = pd->graphics->getTextWidth(game.fontBold, buf, strlen(buf),
                                       kASCIIEncoding, 0);
    int tx = x + (CARD_W - w) / 2;
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, tx, y + CARD_H - 20);

    // Selection: thick double outline
    if (selected) {
        GFX_RECT(x - 2, y - 2, CARD_W + 4, CARD_H + 4, kColorBlack);
        GFX_RECT(x - 1, y - 1, CARD_W + 2, CARD_H + 2, kColorBlack);
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Food slot (small, 32w x 40h)
// ---------------------------------------------------------------------------
static void render_food_slot(uint8_t foodId, int x, int y, bool selected, bool frozen)
{
    pd->graphics->setDrawMode(kDrawModeCopy);
    GFX_FILL(x, y, FOOD_W, FOOD_H, kColorWhite);
    GFX_RECT(x, y, FOOD_W, FOOD_H, kColorBlack);

    if (foodId != EMPTY_FOOD) {
        const char* nm = FOODS[foodId].name;
        if (nm && nm[0]) {
            char letter[2] = { nm[0], 0 };
            if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText(letter, 1, kASCIIEncoding, x + 11, y + 6);
        }
        char cb[8];
        snprintf(cb, sizeof(cb), "%dg", FOODS[foodId].cost);
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(cb, strlen(cb), kASCIIEncoding, x + 6, y + 26);
    }
    if (frozen) {
        // Tiny black square top-right = frozen
        GFX_FILL(x + FOOD_W - 6, y + 2, 4, 4, kColorBlack);
    }
    if (selected) {
        GFX_RECT(x - 2, y - 2, FOOD_W + 4, FOOD_H + 4, kColorBlack);
        GFX_RECT(x - 1, y - 1, FOOD_W + 2, FOOD_H + 2, kColorBlack);
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Top bar (turn / gold / lives / wins)
// ---------------------------------------------------------------------------
void render_topbar(void)
{
    GFX_FILL(0, 0, SCREEN_W, TOPBAR_H, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);

    char buf[64];
    snprintf(buf, sizeof(buf),
             "TURN %d   GOLD %d   LIVES %d   WINS %d/%d",
             game.turn, game.shop.gold, game.lives, game.wins, WINS_TO_VICTORY);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 8, 2);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Debug overlay: shows cursor, selected slot, frame, state
// ---------------------------------------------------------------------------
static void render_debug_overlay(void)
{
    if (!g_debugOverlay) return;
    int by = SCREEN_H - 14;
    GFX_FILL(0, by, SCREEN_W, 14, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    char buf[96];
    snprintf(buf, sizeof(buf),
             "[dbg] state=%d cursor=%d sel=%d frame=%lu",
             (int)game.state, (int)game.cursor, (int)game.selectedTeamSlot,
             (unsigned long)game.frameCount);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 4, by);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Shop screen
// Layout (400x240):
//   Top bar       : y=0,   h=18
//   "SHOP" label  : y=20,  h=10  (small text)
//   Shop pets row : y=32,  h=84  (5 cards x 60w, 4px gap), x starts at 4
//   Foods (col)   : x=336, y=32, 2 stacked? or side-by-side. Use 2 side-by-side
//   "TEAM" label  : y=124, h=10
//   Team row      : y=136, h=84
//   Action bar    : y=224, h=14
// ---------------------------------------------------------------------------
void render_shop_screen(void)
{
    render_topbar();

    // Shop pet row (just below top bar, no label needed — layout is clear)
    int sx0 = 4;
    int sy  = TOPBAR_H + 6;
    for (int i = 0; i < MAX_SHOP_PETS; i++) {
        bool sel = (game.cursor == i);
        uint8_t pid = game.shop.pets[i];
        int16_t a = 0, h = 0;
        if (pid != EMPTY_PET) {
            const PetDef* def = pet_def(pid);
            if (def) { a = def->baseAtk; h = def->baseHp; }
        }
        int cx = sx0 + i * (CARD_W + CARD_GAP);
        render_pet_card(pid, 1, a, h, EMPTY_FOOD, cx, sy, sel);

        // Cost tag (top-center, below level dots)
        if (pid != EMPTY_PET) {
            if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText("3g", 2, kASCIIEncoding, cx + CARD_W - 14, sy + 2);
        }

        // Frozen indicator
        if (game.shop.frozenPets & (1 << i)) {
            // Black triangle in bottom-right corner
            for (int k = 0; k < 6; k++) {
                GFX_FILL(cx + CARD_W - 7 + k, sy + CARD_H - 8 + k, 7 - k, 1, kColorBlack);
            }
        }
    }

    // Food slots — side-by-side, right of shop
    int fx0 = 4 + MAX_SHOP_PETS * (CARD_W + CARD_GAP) + 4;
    int fy  = sy + (CARD_H - FOOD_H) / 2;
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + i);
        bool frz = (game.shop.frozenFoods & (1 << i)) != 0;
        render_food_slot(game.shop.foods[i], fx0 + i * (FOOD_W + 4), fy, sel, frz);
    }

    // Team row (below shop with small gap)
    int ty = sy + CARD_H + 6;
    for (int i = 0; i < TEAM_SIZE; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + MAX_SHOP_FOOD + i);
        PetSlot* p = &game.player.slots[i];
        // Override: if this slot is the "picked up for reorder" one, draw extra highlight
        bool picked = (game.selectedTeamSlot == i);
        int cx = sx0 + i * (CARD_W + CARD_GAP);
        render_pet_card(p->id, p->level, p->atk, p->hp, p->food, cx, ty, sel);
        if (picked) {
            // Inverse rectangle inside to indicate "held"
            pd->graphics->setDrawMode(kDrawModeCopy);
            for (int b = 0; b < 3; b++) {
                GFX_RECT(cx + b, ty + b, CARD_W - 2*b, CARD_H - 2*b, kColorBlack);
            }
        }
    }

    // Action bar (y=224, h=14)
    int ay = 222;
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int rollIdx = MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
    int endIdx  = rollIdx + 1;

    bool rollSel = (game.cursor == rollIdx);
    bool endSel  = (game.cursor == endIdx);

    GFX_FILL(4, ay, 110, 16, rollSel ? kColorBlack : kColorWhite);
    GFX_RECT(4, ay, 110, 16, kColorBlack);
    pd->graphics->setDrawMode(rollSel ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText("ROLL  1g", 8, kASCIIEncoding, 22, ay + 1);

    GFX_FILL(286, ay, 110, 16, endSel ? kColorBlack : kColorWhite);
    GFX_RECT(286, ay, 110, 16, kColorBlack);
    pd->graphics->setDrawMode(endSel ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText("END TURN", 8, kASCIIEncoding, 304, ay + 1);

    pd->graphics->setDrawMode(kDrawModeCopy);

    // Hint strip showing context-sensitive A/B (above action bar)
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    const char* hint = "";
    uint8_t c = game.cursor;
    if (c < MAX_SHOP_PETS) hint = "A: buy   B: freeze";
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD) hint = "A: buy food   B: freeze";
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE) hint = "A: pick up (crank to reorder)   B: sell";
    else if (c == rollIdx) hint = "A: roll shop";
    else hint = "A: start battle";
    pd->graphics->drawText(hint, strlen(hint), kASCIIEncoding, 120, ay + 2);

    pd->graphics->setDrawMode(kDrawModeCopy);
    render_debug_overlay();
}

// ---------------------------------------------------------------------------
// Battle screen
// Layout:
//   Top bar         : y=0
//   Player team     : right-aligned (closer to center is frontline)
//   Enemy team      : left-aligned of enemy half (closer to center is frontline)
// ---------------------------------------------------------------------------
void render_battle_screen(void)
{
    render_topbar();

    int yTeam = 70;

    // Center divider
    GFX_LINE(SCREEN_W/2, TOPBAR_H + 4, SCREEN_W/2, SCREEN_H - 24, 1, kColorBlack);

    // Player team: rightmost alive pet sits at the frontline (closest to center).
    // Earlier slots stack behind it to the left.
    int pFrontSlot = -1;
    for (int i = TEAM_SIZE - 1; i >= 0; i--) {
        if (game.battle.teams[0].slots[i].id != EMPTY_PET) { pFrontSlot = i; break; }
    }
    if (pFrontSlot >= 0) {
        for (int i = 0; i <= pFrontSlot; i++) {
            PetSlot* p = &game.battle.teams[0].slots[i];
            if (p->id == EMPTY_PET) continue;
            int offset = pFrontSlot - i;   // 0 = frontline, 1 = behind, ...
            int x = SCREEN_W/2 - 8 - (1 + offset) * (CARD_W + CARD_GAP);
            render_pet_card(p->id, p->level, p->atk, p->hp, p->food, x, yTeam, false);
        }
    }

    // Enemy team: leftmost alive pet (their slot 0) sits at the frontline.
    int eFrontSlot = -1;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (game.battle.teams[1].slots[i].id != EMPTY_PET) { eFrontSlot = i; break; }
    }
    // SAP rule: enemy's rightmost (highest slot) attacks first against player.
    // Mirror: highest-slot enemy at the divider, lower slots stacked behind.
    int eBackSlot = -1;
    for (int i = TEAM_SIZE - 1; i >= 0; i--) {
        if (game.battle.teams[1].slots[i].id != EMPTY_PET) { eBackSlot = i; break; }
    }
    if (eBackSlot >= 0) {
        for (int i = eBackSlot; i >= 0; i--) {
            PetSlot* p = &game.battle.teams[1].slots[i];
            if (p->id == EMPTY_PET) continue;
            int offset = eBackSlot - i;    // 0 = frontline (rightmost), 1 = behind
            int x = SCREEN_W/2 + 8 + offset * (CARD_W + CARD_GAP);
            render_pet_card(p->id, p->level, p->atk, p->hp, p->food, x, yTeam, false);
        }
    }
    (void)eFrontSlot;

    // Center status
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    char buf[32];
    snprintf(buf, sizeof(buf), "%u/%u  %dx",
             (unsigned)game.battlePlayhead,
             (unsigned)game.battleLogLen,
             (int)game.battleSpeed);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, SCREEN_W/2 - 30, 22);

    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText("crank: speed   A: skip",
                           22, kASCIIEncoding, 4, SCREEN_H - 14);
    render_debug_overlay();
}
