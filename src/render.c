#include "game.h"
#include <stdio.h>

#define TOPBAR_H     18

extern bool g_debugOverlay;

// ---------------------------------------------------------------------------
// Dither patterns — 1-bit "greyscale". LCDPattern = 8 rows + 8 mask bytes.
// ---------------------------------------------------------------------------
static LCDPattern PAT_SKY = {        // ~5% black — faint grey sky
    0xFF,0xFF,0xFF,0xEF,0xFF,0xFF,0xFF,0xFE,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static LCDPattern PAT_STONE = {      // 50% checker — mountains
    0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static LCDPattern PAT_FOREST = {     // ~70% black — far forest
    0x22,0x88,0x22,0x88,0x22,0x88,0x22,0x88,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static LCDPattern PAT_GRASS = {      // ~20% black — pasture tint
    0xFF,0xDF,0xFF,0xFD,0xFF,0xDF,0xFF,0xFD,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

static void fill_pat(int x, int y, int w, int h, LCDPattern pat) {
    pd->graphics->fillRect(x, y, w, h, (LCDColor)pat);
}

// ---------------------------------------------------------------------------
// Stat badges — solid black shapes with white numbers (SAP-style). Attack is
// a disc, health is a heart; the shape alone tells them apart in 1-bit.
// ---------------------------------------------------------------------------
static void badge_number(int cx, int cy, int val) {
    char b[8];
    snprintf(b, sizeof(b), "%d", val);
    LCDFont* f = game.fontBold;
    if (f) pd->graphics->setFont(f);
    int w = f ? pd->graphics->getTextWidth(f, b, strlen(b), kASCIIEncoding, 0)
              : (int)strlen(b) * 7;
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText(b, strlen(b), kASCIIEncoding, cx - w / 2, cy - 9);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Attack — a bold black disc with a crisp white ring so it pops off any
// background and reads clearly even when units crowd together.
static void badge_atk(int cx, int cy, int val) {
    GFX_ELLIPSE(cx - 11, cy - 11, 22, 22, kColorWhite);
    GFX_ELLIPSE(cx - 9,  cy - 9,  18, 18, kColorBlack);
    badge_number(cx, cy, val);
}

// Health — a bold black heart with the same white ring treatment.
static void badge_hp(int cx, int cy, int val) {
    GFX_ELLIPSE(cx - 11, cy - 10, 12, 12, kColorWhite);
    GFX_ELLIPSE(cx - 1,  cy - 10, 12, 12, kColorWhite);
    pd->graphics->fillTriangle(cx - 11, cy - 3, cx + 11, cy - 3,
                               cx, cy + 11, kColorWhite);
    GFX_ELLIPSE(cx - 9, cy - 8, 10, 10, kColorBlack);
    GFX_ELLIPSE(cx - 1, cy - 8, 10, 10, kColorBlack);
    pd->graphics->fillTriangle(cx - 9, cy - 2, cx + 9, cy - 2,
                               cx, cy + 9, kColorBlack);
    badge_number(cx, cy, val);
}

// ---------------------------------------------------------------------------
// Card-free unit — the creature stands directly on the pasture with an
// attack disc bottom-left and a health heart bottom-right. No box, no name.
// cx = horizontal centre, baseY = ground line under its feet.
// ---------------------------------------------------------------------------
void render_unit(uint8_t petId, uint8_t level, int16_t atk, int16_t hp,
                 uint8_t heldFood, int cx, int baseY, float spriteScale,
                 bool selected, bool picked, bool faceLeft)
{
    int spr = (int)(32.0f * spriteScale);
    int half = spr / 2;

    if (petId == EMPTY_PET) {
        // empty slot: a faint footprint ring
        pd->graphics->drawEllipse(cx - 18, baseY - 8, 36, 12, 1, 0, 360,
                                  (LCDColor)PAT_STONE);
        if (selected)
            pd->graphics->drawEllipse(cx - 21, baseY - 11, 42, 18, 2, 0, 360,
                                      kColorBlack);
        return;
    }

    // ground shadow
    GFX_ELLIPSE(cx - half + 3, baseY - 5, spr - 6, 8, (LCDColor)PAT_STONE);

    // selection / picked ring under the feet
    if (selected || picked) {
        pd->graphics->drawEllipse(cx - half - 1, baseY - 9, spr + 2, 16,
                                  picked ? 3 : 2, 0, 360, kColorBlack);
    }

    // sprite (feet on baseY) — faceLeft mirrors the right-facing art
    draw_creature_xform(petId, cx, baseY - half, spriteScale, 0.0f, faceLeft);

    // level pips above the head
    int npip = level; if (npip > MAX_PET_LEVEL) npip = MAX_PET_LEVEL;
    for (int i = 0; i < npip; i++) {
        int px = cx - npip * 4 + i * 8;
        GFX_ELLIPSE(px, baseY - spr - 9, 5, 5, kColorWhite);
        GFX_ELLIPSE(px + 1, baseY - spr - 8, 3, 3, kColorBlack);
    }

    // held-food marker (small wedge, upper right)
    if (heldFood != EMPTY_FOOD) {
        GFX_ELLIPSE(cx + half - 7, baseY - spr + 1, 9, 9, kColorWhite);
        GFX_ELLIPSE(cx + half - 6, baseY - spr + 2, 7, 7, kColorBlack);
    }

    // picked-up arrows (reorder hint)
    if (picked) {
        pd->graphics->fillTriangle(cx - half - 9, baseY - half,
                                   cx - half - 2, baseY - half - 6,
                                   cx - half - 2, baseY - half + 6, kColorBlack);
        pd->graphics->fillTriangle(cx + half + 9, baseY - half,
                                   cx + half + 2, baseY - half - 6,
                                   cx + half + 2, baseY - half + 6, kColorBlack);
    }

    // stat badges — tucked inside the lower corners so they never collide
    // with the neighbouring unit's badges.
    int bo = half - 11;
    badge_atk(cx - bo, baseY - 7, atk);
    badge_hp (cx + bo, baseY - 7, hp);
}

// ---------------------------------------------------------------------------
// Card-free food item — a small jar on the ground.
// ---------------------------------------------------------------------------
static void render_food(uint8_t foodId, int cx, int baseY,
                        bool selected, bool frozen)
{
    if (selected) {
        pd->graphics->drawEllipse(cx - 17, baseY - 9, 34, 14, 2, 0, 360,
                                  kColorBlack);
    }
    if (foodId == EMPTY_FOOD) {
        pd->graphics->drawEllipse(cx - 12, baseY - 7, 24, 10, 1, 0, 360,
                                  (LCDColor)PAT_STONE);
        return;
    }
    // shadow
    GFX_ELLIPSE(cx - 11, baseY - 5, 22, 7, (LCDColor)PAT_STONE);
    // jar: white body + black rim
    GFX_ELLIPSE(cx - 11, baseY - 26, 22, 24, kColorWhite);
    pd->graphics->drawEllipse(cx - 11, baseY - 26, 22, 24, 2, 0, 360,
                              kColorBlack);
    GFX_FILL(cx - 8, baseY - 28, 16, 5, kColorBlack);     // lid
    // first letter as a placeholder label
    const char* nm = FOODS[foodId].name;
    if (nm && nm[0] && game.fontBold) {
        char letter[2] = { nm[0], 0 };
        pd->graphics->setFont(game.fontBold);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        int w = pd->graphics->getTextWidth(game.fontBold, letter, 1,
                                           kASCIIEncoding, 0);
        pd->graphics->drawText(letter, 1, kASCIIEncoding,
                               cx - w / 2, baseY - 22);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }
    if (frozen) {
        GFX_ELLIPSE(cx + 6, baseY - 30, 8, 8, kColorWhite);
        GFX_LINE(cx + 6, baseY - 30, cx + 12, baseY - 24, 1, kColorBlack);
        GFX_LINE(cx + 12, baseY - 30, cx + 6, baseY - 24, 1, kColorBlack);
    }
}

// One soft cloud — overlapping pure-white ellipses (pop against grey sky).
static void draw_cloud(int cx, int cy) {
    GFX_ELLIPSE(cx - 14, cy - 4, 18, 12, kColorWhite);
    GFX_ELLIPSE(cx - 4,  cy - 9, 20, 16, kColorWhite);
    GFX_ELLIPSE(cx + 8,  cy - 5, 17, 13, kColorWhite);
    GFX_ELLIPSE(cx - 8,  cy,     30, 10, kColorWhite);
}

// ---------------------------------------------------------------------------
// Layered Alpine backdrop. horizonY = where the forest meets the pasture.
// Depth ramp: white clouds < faint-grey sky < checker mountains < dark forest,
// then a bright pasture foreground so the black creature sprites read clearly.
// ---------------------------------------------------------------------------
void render_alpine_bg(int horizonY)
{
    pd->graphics->setDrawMode(kDrawModeCopy);

    // Sky
    fill_pat(0, 0, SCREEN_W, horizonY, PAT_SKY);
    // Clouds
    draw_cloud(70, 24);
    draw_cloud(220, 16);
    draw_cloud(330, 30);

    // Mountains — checker-filled triangles with white snowcaps
    static const int peakH[7] = { 34, 58, 26, 70, 40, 54, 30 };
    int seg = SCREEN_W / 7 + 1;
    int mtnBase = horizonY - 10;
    for (int i = 0; i < 7; i++) {
        int px = i * seg, peakX = px + seg / 2, nx = px + seg;
        int peakY = mtnBase - peakH[i];
        if (peakY < TOPBAR_H + 3) peakY = TOPBAR_H + 3;  // clamp so peaks fit
        pd->graphics->fillTriangle(px, mtnBase, peakX, peakY, nx, mtnBase,
                                   (LCDColor)PAT_STONE);
        GFX_LINE(px, mtnBase, peakX, peakY, 1, kColorBlack);
        GFX_LINE(peakX, peakY, nx, mtnBase, 1, kColorBlack);
        // white snowcap
        pd->graphics->fillTriangle(peakX, peakY,
                                   peakX - 7, peakY + 9,
                                   peakX + 7, peakY + 9, kColorWhite);
        GFX_LINE(peakX, peakY, peakX - 7, peakY + 9, 1, kColorBlack);
        GFX_LINE(peakX, peakY, peakX + 7, peakY + 9, 1, kColorBlack);
    }

    // Forest band — dark dither with a scalloped treetop edge
    int fTop = horizonY - 12;
    fill_pat(0, fTop, SCREEN_W, 12, PAT_FOREST);
    for (int x = -4; x < SCREEN_W + 4; x += 11) {
        GFX_ELLIPSE(x, fTop - 5, 13, 11, (LCDColor)PAT_FOREST);
    }

    // Pasture — bright foreground; faint grass tint band at the very bottom
    GFX_FILL(0, horizonY, SCREEN_W, SCREEN_H - horizonY, kColorWhite);
    GFX_LINE(0, horizonY, SCREEN_W, horizonY, 1, kColorBlack);
    fill_pat(0, SCREEN_H - 14, SCREEN_W, 14, PAT_GRASS);
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

// Small snowflake mark for frozen shop items.
static void draw_frozen_mark(int x, int y) {
    GFX_ELLIPSE(x - 5, y - 5, 11, 11, kColorWhite);
    pd->graphics->drawEllipse(x - 5, y - 5, 11, 11, 1, 0, 360, kColorBlack);
    GFX_LINE(x - 3, y, x + 3, y, 1, kColorBlack);
    GFX_LINE(x, y - 3, x, y + 3, 1, kColorBlack);
    GFX_LINE(x - 2, y - 2, x + 2, y + 2, 1, kColorBlack);
    GFX_LINE(x - 2, y + 2, x + 2, y - 2, 1, kColorBlack);
}

// A pill button for the action bar.
static void draw_button(int x, int y, int w, int h, const char* label, bool sel) {
    GFX_FILL(x, y, w, h, sel ? kColorBlack : kColorWhite);
    GFX_RECT(x, y, w, h, kColorBlack);
    GFX_RECT(x + 1, y + 1, w - 2, h - 2, kColorBlack);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label),
                                        kASCIIEncoding, 0);
    pd->graphics->setDrawMode(sel ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding,
                           x + (w - tw) / 2, y + (h - 16) / 2);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Shop screen — card-free, on the Alpine pasture.
// ---------------------------------------------------------------------------
void render_shop_screen(void)
{
    render_alpine_bg(58);
    render_topbar();

    const float SC = 1.5f;
    static const int colX[5]  = { 36, 92, 148, 204, 260 };
    static const int foodX[2] = { 322, 366 };
    int shopFeet = 110;
    int teamFeet = 182;

    // --- Shop pets ---
    for (int i = 0; i < MAX_SHOP_PETS; i++) {
        bool sel = (game.cursor == i);
        uint8_t pid = game.shop.pets[i];
        int16_t a = 0, h = 0;
        if (pid != EMPTY_PET) {
            const PetDef* d = pet_def(pid);
            if (d) { a = d->baseAtk; h = d->baseHp; }
        }
        render_unit(pid, 0, a, h, EMPTY_FOOD, colX[i], shopFeet, SC, sel,
                    false, false);
        if (pid != EMPTY_PET) {
            // cost tag above the creature
            int cy = shopFeet - (int)(32 * SC) - 8;
            GFX_ELLIPSE(colX[i] - 11, cy - 7, 22, 15, kColorBlack);
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            pd->graphics->drawText("3g", 2, kASCIIEncoding, colX[i] - 8, cy - 8);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        if (game.shop.frozenPets & (1 << i))
            draw_frozen_mark(colX[i] + 18, shopFeet - (int)(32 * SC) + 2);
    }

    // --- Shop foods ---
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + i);
        bool frz = (game.shop.frozenFoods & (1 << i)) != 0;
        render_food(game.shop.foods[i], foodX[i], shopFeet, sel, frz);
        if (game.shop.foods[i] != EMPTY_FOOD) {
            char cb[8];
            snprintf(cb, sizeof(cb), "%dg", FOODS[game.shop.foods[i]].cost);
            if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            int w = pd->graphics->getTextWidth(game.fontSmall, cb, strlen(cb),
                                               kASCIIEncoding, 0);
            pd->graphics->drawText(cb, strlen(cb), kASCIIEncoding,
                                   foodX[i] - w / 2, shopFeet + 2);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // --- Team row ---
    for (int i = 0; i < TEAM_SIZE; i++) {
        bool sel = (game.cursor == MAX_SHOP_PETS + MAX_SHOP_FOOD + i);
        bool picked = (game.selectedTeamSlot == i);
        PetSlot* p = &game.player.slots[i];
        render_unit(p->id, p->level, p->atk, p->hp, p->food,
                    colX[i], teamFeet, SC, sel, picked, false);
    }

    // --- Action bar ---
    int rollIdx = MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
    int endIdx  = rollIdx + 1;
    int ay = 196;
    draw_button(8,   ay, 96, 24, "ROLL 1g",  game.cursor == rollIdx);
    draw_button(296, ay, 96, 24, "END TURN", game.cursor == endIdx);

    // --- Hint strip ---
    const char* hint = "";
    uint8_t c = game.cursor;
    if (c < MAX_SHOP_PETS) hint = "A buy   B freeze";
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD) hint = "A buy food   B freeze";
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE)
        hint = "A grab (crank to move)   B sell";
    else if (c == rollIdx) hint = "A reroll the alp";
    else hint = "A march to battle";
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    int hw = pd->graphics->getTextWidth(game.fontSmall, hint, strlen(hint),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(hint, strlen(hint), kASCIIEncoding,
                           (SCREEN_W - hw) / 2, 220);
    pd->graphics->setDrawMode(kDrawModeCopy);
    render_debug_overlay();
}

// ---------------------------------------------------------------------------
// Battle screen — per-event animation: lunge, hit-shake, faint-flash,
// floating damage/buff numbers.
// ---------------------------------------------------------------------------
#define BATTLE_Y 74

// Current animating event + its 0..1 progress. NULL if playback finished.
static BattleEvent* battle_cur_event(float* tOut) {
    if (game.battlePlayhead >= game.battleLogLen) { *tOut = 0; return NULL; }
    BattleEvent* e = &game.battleLog[game.battlePlayhead];
    float dur = (float)battle_event_duration(e->kind);
    float t = dur > 0 ? game.battleClock / dur : 1.0f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    *tOut = t;
    return e;
}

#define BATTLE_FEET   162
#define BATTLE_STEP   50
#define BATTLE_SCALE  1.4f
#define BATTLE_CX     (SCREEN_W / 2)

// Eased on-screen X per unit, so the line slides forward as units fall.
static float g_unitX[2][TEAM_SIZE];

// Rest position for a unit at a given rank (0 = frontline). The frontline
// units rest with a clear gap so the clash lunge across it reads.
static int unit_target_x(int side, int rank) {
    return (side == 0) ? BATTLE_CX - 46 - rank * BATTLE_STEP
                       : BATTLE_CX + 46 + rank * BATTLE_STEP;
}

// A floating number with a white halo, rising as t advances.
static void battle_popup(int cx, int topY, const char* s, float t) {
    int ty = topY - (int)(t * 18.0f);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int w = pd->graphics->getTextWidth(game.fontBold, s, strlen(s),
                                       kASCIIEncoding, 0);
    int tx = cx - w / 2;
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    for (int ox = -1; ox <= 1; ox++)
        for (int oy = -1; oy <= 1; oy++)
            pd->graphics->drawText(s, strlen(s), kASCIIEncoding, tx + ox, ty + oy);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, tx, ty);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Draw a single battle unit with clash / faint / hit animation.
static void battle_draw_unit(BattleEvent* e, float t, int side, int slot, float fx) {
    PetSlot* p = &game.battle.teams[side].slots[slot];
    if (p->id == EMPTY_PET) return;

    int spr = (int)(32 * BATTLE_SCALE);
    int dx = 0, dy = 0;
    bool fainting = false;
    float fT = 0.0f;

    if (e) {
        if (e->kind == BEV_CLASH) {
            // Rush in fast, strike near centre at t~0.42, recoil, settle.
            if ((side == 0 && slot == e->slot) ||
                (side == 1 && slot == e->data)) {
                const float peak = 26.0f, contactT = 0.42f;
                float lunge;
                if (t < contactT) {
                    float a = t / contactT;
                    lunge = peak * a * a;                       // accelerate in
                } else {
                    float r = (t - contactT) / (1.0f - contactT);
                    lunge = peak * (1.0f - r)
                          - 7.0f * sinf(r * PI_F);              // recoil bounce
                }
                dx = (side == 0) ? (int)lunge : -(int)lunge;
            }
        } else if (e->kind == BEV_FAINT && e->side == side && e->slot == slot) {
            fainting = true; fT = t;
        } else if (e->kind == BEV_DAMAGE && e->side == side && e->slot == slot) {
            dx = (int)(sinf(t * 38.0f) * 3.0f);                // quick hurt-shake
        } else if ((e->kind == BEV_BUFF || e->kind == BEV_ABILITY) &&
                   e->side == side && e->slot == slot) {
            dy = -(int)(sinf(t * PI_F) * 5.0f);
        }
    }

    int ux = (int)fx + dx;
    int uy = BATTLE_FEET + dy;

    if (fainting) {
        // KO launch — flung off-screen away from the foe with a hard spin.
        int backDir = (side == 0) ? -1 : 1;     // player flies left, enemy right
        float k     = fT * fT;                  // accelerating travel
        int cxK = ux + backDir * (int)(k * 380.0f);
        int cyK = uy - spr / 2 + (int)(fT * -250.0f + k * 110.0f);
        float deg = fT * 540.0f * backDir;

        // impact starburst at the launch point for the first few frames
        if (fT < 0.32f) {
            int ix = ux, iy = uy - spr / 2;
            int r = (int)(9 + fT * 64.0f);
            for (int a = 0; a < 8; a++) {
                float ang = a * (PI_F / 4.0f) + 0.39f;
                GFX_LINE(ix + (int)(cosf(ang) * r * 0.40f),
                         iy + (int)(sinf(ang) * r * 0.40f),
                         ix + (int)(cosf(ang) * r),
                         iy + (int)(sinf(ang) * r), 3, kColorBlack);
            }
        }
        draw_creature_xform(p->id, cxK, cyK, BATTLE_SCALE, deg, side == 1);
        return;
    }

    render_unit(p->id, p->level, p->atk, p->hp, p->food, ux, uy,
                BATTLE_SCALE, false, false, side == 1);
}

void render_battle_screen(void)
{
    render_alpine_bg(80);
    render_topbar();

    float t = 0.0f;
    BattleEvent* e = battle_cur_event(&t);
    // Pre-battle just shows the static line-up — no event animation/popups.
    if (game.state == STATE_PRE_BATTLE) { e = NULL; t = 0.0f; }

    // --- ease unit positions toward their ranked rest spots ---
    for (int side = 0; side < 2; side++) {
        int rank = 0;
        for (int i = TEAM_SIZE - 1; i >= 0; i--) {
            if (game.battle.teams[side].slots[i].id == EMPTY_PET) continue;
            float tgt = (float)unit_target_x(side, rank);
            float cur = g_unitX[side][i];
            if (cur < 1.0f || cur > 2000.0f || fabsf(tgt - cur) > 150.0f)
                g_unitX[side][i] = tgt;                  // snap (battle start)
            else
                g_unitX[side][i] = cur + (tgt - cur) * 0.30f;
            rank++;
        }
    }

    // --- draw units, back rows first so the frontline overlaps ---
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < TEAM_SIZE; i++)
            if (game.battle.teams[side].slots[i].id != EMPTY_PET)
                battle_draw_unit(e, t, side, i, g_unitX[side][i]);

    // --- clash impact: spark spokes + an expanding shock-ring at contact ---
    if (e && e->kind == BEV_CLASH && t > 0.34f && t < 0.66f) {
        int mx = BATTLE_CX;
        int my = BATTLE_FEET - (int)(32 * BATTLE_SCALE) / 2 - 2;
        float k = (t - 0.34f) / 0.32f;               // 0..1 across the effect
        int r  = (int)(7 + k * 24);
        int lw = (k < 0.45f) ? 3 : 2;
        for (int a = 0; a < 6; a++) {
            float ang = a * (PI_F / 3.0f) + 0.30f;
            GFX_LINE(mx + (int)(cosf(ang) * r * 0.35f),
                     my + (int)(sinf(ang) * r * 0.35f),
                     mx + (int)(cosf(ang) * r),
                     my + (int)(sinf(ang) * r), lw, kColorBlack);
        }
        if (k < 0.70f) {                             // expanding shock-ring
            int rr = (int)(6 + k * 16);
            pd->graphics->drawEllipse(mx - rr, my - rr, rr * 2, rr * 2,
                                      2, 0, 360, kColorBlack);
        }
    }

    // --- combat-text popups ---
    int popTop = BATTLE_FEET - (int)(32 * BATTLE_SCALE) - 14;
    if (e && t > 0.42f && e->kind == BEV_CLASH) {
        int pDmg = e->value & 0xFF, eDmg = (e->value >> 8) & 0xFF;
        char s[8];
        if (eDmg > 0) {
            snprintf(s, sizeof(s), "-%d", eDmg);
            battle_popup((int)g_unitX[0][e->slot], popTop, s, t - 0.42f);
        }
        if (pDmg > 0) {
            snprintf(s, sizeof(s), "-%d", pDmg);
            battle_popup((int)g_unitX[1][e->data], popTop, s, t - 0.42f);
        }
    } else if (e && (e->kind == BEV_DAMAGE || e->kind == BEV_BUFF) &&
               e->slot < TEAM_SIZE) {
        char s[12]; s[0] = 0;
        if (e->kind == BEV_DAMAGE) snprintf(s, sizeof(s), "-%d", e->value);
        else {
            int da = e->value & 0xFF, dh = (e->value >> 8) & 0xFF;
            if (da && dh) snprintf(s, sizeof(s), "+%d/%d", da, dh);
            else if (da) snprintf(s, sizeof(s), "+%d", da);
            else if (dh) snprintf(s, sizeof(s), "+%d", dh);
        }
        if (s[0]) battle_popup((int)g_unitX[e->side][e->slot], popTop, s, t);
    }

    // --- speed indicator ---
    if (game.battleSpeed > 1) {
        char sb[12];
        snprintf(sb, sizeof(sb), "%dx", (int)game.battleSpeed);
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(sb, strlen(sb), kASCIIEncoding, BATTLE_CX - 8, 24);
    }

    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    const char* h = "crank: faster   A: skip";
    int hw = pd->graphics->getTextWidth(game.fontSmall, h, strlen(h),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(h, strlen(h), kASCIIEncoding,
                           (SCREEN_W - hw) / 2, SCREEN_H - 14);
    pd->graphics->setDrawMode(kDrawModeCopy);
    render_debug_overlay();
}
