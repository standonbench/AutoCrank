#include "game.h"
#include <stdio.h>

#define CARD_W 56
#define CARD_H 64
#define SPRITE_DRAW_SIZE 32

// ---------------------------------------------------------------------------
// Pet card: 56x64 box, sprite on top, "atk/hp" stat strip on bottom.
// ---------------------------------------------------------------------------
void render_pet_card(uint8_t petId, uint8_t level, int16_t atk, int16_t hp,
                     uint8_t heldFood, int x, int y, bool selected)
{
    if (petId == EMPTY_PET) {
        render_empty_slot(x, y, selected);
        return;
    }

    // Selection: thicker outline + invert background when selected.
    GFX_FILL(x, y, CARD_W, CARD_H, selected ? kColorBlack : kColorWhite);
    if (selected) {
        // Inverted card: draw subsequent text in white-on-black via FillWhite mode.
        pd->graphics->setDrawMode(kDrawModeFillWhite);
    } else {
        pd->graphics->setDrawMode(kDrawModeCopy);
    }
    GFX_RECT(x, y, CARD_W, CARD_H, kColorBlack);

    // Sprite area (top 40 px). Center 32x32 in 56x40 region.
    LCDBitmap* bmp = image_pet(petId);
    int sx = x + (CARD_W - SPRITE_DRAW_SIZE) / 2;
    int sy = y + 4;
    if (bmp) {
        pd->graphics->drawScaledBitmap(bmp, sx, sy, 2.0f, 2.0f);
    } else {
        // Stub: dotted square + first letter of name.
        GFX_RECT(sx, sy, SPRITE_DRAW_SIZE, SPRITE_DRAW_SIZE,
                 selected ? kColorWhite : kColorBlack);
        const char* nm = pet_name(petId);
        if (nm && nm[0] && game.fontLarge) {
            pd->graphics->setFont(game.fontLarge);
            char letter[2] = { nm[0], 0 };
            pd->graphics->drawText(letter, 1, kASCIIEncoding, sx + 11, sy + 6);
        }
    }

    // Level dots (top-left of card)
    for (int i = 0; i < level && i < MAX_PET_LEVEL; i++) {
        GFX_ELLIPSE(x + 2 + i * 5, y + 2, 4, 4, selected ? kColorWhite : kColorBlack);
    }

    // Held food indicator (top-right of card)
    if (heldFood != EMPTY_FOOD) {
        GFX_FILL(x + CARD_W - 8, y + 2, 6, 6, selected ? kColorWhite : kColorBlack);
    }

    // Stat strip (bottom 18 px)
    int statY = y + CARD_H - 16;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", atk, hp);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding,
                           x + 6, statY);

    // Restore default draw mode.
    pd->graphics->setDrawMode(kDrawModeCopy);
}

void render_empty_slot(int x, int y, bool selected)
{
    GFX_FILL(x, y, CARD_W, CARD_H, kColorWhite);
    // Dashed border (procedural)
    int color = kColorBlack;
    for (int i = 0; i < CARD_W; i += 4) {
        GFX_FILL(x + i, y, 2, 1, color);
        GFX_FILL(x + i, y + CARD_H - 1, 2, 1, color);
    }
    for (int i = 0; i < CARD_H; i += 4) {
        GFX_FILL(x, y + i, 1, 2, color);
        GFX_FILL(x + CARD_W - 1, y + i, 1, 2, color);
    }
    if (selected) {
        // Solid outline on top of dashed
        GFX_RECT(x - 1, y - 1, CARD_W + 2, CARD_H + 2, kColorBlack);
    }
}

// ---------------------------------------------------------------------------
// Top bar: turn / gold / lives / wins
// ---------------------------------------------------------------------------
void render_topbar(void)
{
    GFX_FILL(0, 0, SCREEN_W, 18, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);

    char buf[64];
    snprintf(buf, sizeof(buf),
             "T%d  G:%d  L:%d  W:%d/%d",
             game.turn, game.shop.gold, game.lives, game.wins, WINS_TO_VICTORY);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 6, 2);

    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Shop screen layout
// ---------------------------------------------------------------------------
void render_shop_screen(void)
{
    render_topbar();

    // Shop row: 5 pets + 2 foods at y=24
    int sx = 8;
    int sy = 24;
    for (int i = 0; i < MAX_SHOP_PETS; i++) {
        bool sel = (game.cursor == i);
        render_pet_card(game.shop.pets[i], 1, 0, 0, EMPTY_FOOD,
                        sx + i * (CARD_W + 4), sy, sel);
        if (game.shop.frozenPets & (1 << i)) {
            // Snowflake-ish indicator (outline triangle)
            int fx = sx + i * (CARD_W + 4) + CARD_W - 12;
            int fy = sy + CARD_H - 14;
            GFX_FILL(fx, fy, 8, 8, kColorBlack);
        }
    }
    int foodStartX = sx + MAX_SHOP_PETS * (CARD_W + 4) + 8;
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + i);
        int fx = foodStartX + i * 32;
        int fy = sy + 16;
        GFX_RECT(fx, fy, 28, 28, kColorBlack);
        if (sel) GFX_RECT(fx - 1, fy - 1, 30, 30, kColorBlack);
        if (game.shop.foods[i] != EMPTY_FOOD) {
            // Food letter stub
            const char* nm = FOODS[game.shop.foods[i]].name;
            if (nm && game.fontBold) {
                pd->graphics->setFont(game.fontBold);
                char letter[2] = { nm[0], 0 };
                pd->graphics->drawText(letter, 1, kASCIIEncoding, fx + 10, fy + 6);
            }
        }
    }

    // Team row at y=110
    int ty = 112;
    for (int i = 0; i < TEAM_SIZE; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + MAX_SHOP_FOOD + i);
        PetSlot* p = &game.player.slots[i];
        render_pet_card(p->id, p->level, p->atk, p->hp, p->food,
                        sx + i * (CARD_W + 4), ty, sel);
    }

    // Action bar at y=200
    int ay = 204;
    int rollIdx = MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
    int endIdx  = rollIdx + 1;
    bool rollSel = (game.cursor == rollIdx);
    bool endSel  = (game.cursor == endIdx);

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    GFX_FILL(8, ay, 100, 30, rollSel ? kColorBlack : kColorWhite);
    GFX_RECT(8, ay, 100, 30, kColorBlack);
    if (rollSel) pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText("ROLL (1g)", 9, kASCIIEncoding, 16, ay + 8);
    pd->graphics->setDrawMode(kDrawModeCopy);

    GFX_FILL(290, ay, 100, 30, endSel ? kColorBlack : kColorWhite);
    GFX_RECT(290, ay, 100, 30, kColorBlack);
    if (endSel) pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText("END TURN", 8, kASCIIEncoding, 305, ay + 8);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Battle screen (stub)
// ---------------------------------------------------------------------------
void render_battle_screen(void)
{
    render_topbar();

    // Player team on left side, enemy on right side.
    int yTeam = 70;
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &game.battle.teams[0].slots[i];
        if (p->id == EMPTY_PET) continue;
        int x = 8 + i * (CARD_W + 4);
        render_pet_card(p->id, p->level, p->atk, p->hp, p->food, x, yTeam, false);
    }
    for (int i = 0; i < TEAM_SIZE; i++) {
        PetSlot* p = &game.battle.teams[1].slots[i];
        if (p->id == EMPTY_PET) continue;
        // Enemy: mirror so slot 0 is rightmost (frontline), as in SAP.
        int x = SCREEN_W - 8 - (TEAM_SIZE - i) * (CARD_W + 4);
        render_pet_card(p->id, p->level, p->atk, p->hp, p->food, x, yTeam, false);
    }

    // Center status
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", game.battlePlayhead, game.battleLogLen);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 180, 22);
}
