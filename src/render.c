#include "game.h"
#include <stdio.h>

#define TOPBAR_H     18

extern bool g_debugOverlay;

// ---------------------------------------------------------------------------
// Shop layout — exposed via game.h so shop.c can anchor popups to slots.
// ---------------------------------------------------------------------------
const int SHOP_COL_X[3] = { 36, 92, 148 };
const int TEAM_COL_X[5] = { 36, 92, 148, 204, 260 };
const int FOOD_COL_X[2] = { 322, 366 };

// ---------------------------------------------------------------------------
// Shop popup buffer — small ring of floating text + sparks. Pushed by
// shop.c on every gold/level/sell action, drained at the end of
// render_shop_screen and aged per draw call.
// ---------------------------------------------------------------------------
#define SHOP_POPUP_COUNT 8
#define SHOP_POPUP_LIFE  36

typedef struct {
    int16_t cx, cy;
    int16_t age;            // -1 = free slot
    uint8_t kind;           // 0 = text, 1 = burst
    char    text[14];
} ShopPopup;

static ShopPopup g_pops[SHOP_POPUP_COUNT];

static ShopPopup* shop_popup_alloc(void) {
    // Prefer a free slot; else overwrite the oldest.
    int oldestIdx = 0, oldestAge = -1;
    for (int i = 0; i < SHOP_POPUP_COUNT; i++) {
        if (g_pops[i].age < 0) return &g_pops[i];
        if (g_pops[i].age > oldestAge) { oldestAge = g_pops[i].age; oldestIdx = i; }
    }
    return &g_pops[oldestIdx];
}

void shop_popup_text(int cx, int cy, const char* s) {
    ShopPopup* p = shop_popup_alloc();
    p->cx = (int16_t)cx;
    p->cy = (int16_t)cy;
    p->age = 0;
    p->kind = 0;
    int n = 0;
    while (n + 1 < (int)sizeof(p->text) && s && s[n]) { p->text[n] = s[n]; n++; }
    p->text[n] = 0;
}

void shop_popup_burst(int cx, int cy) {
    ShopPopup* p = shop_popup_alloc();
    p->cx = (int16_t)cx;
    p->cy = (int16_t)cy;
    p->age = 0;
    p->kind = 1;
    p->text[0] = 0;
}

void shop_popup_clear(void) {
    for (int i = 0; i < SHOP_POPUP_COUNT; i++) g_pops[i].age = -1;
}

// Draw + age all live popups. Called at the end of render_shop_screen.
static void shop_popup_drain(void) {
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    for (int i = 0; i < SHOP_POPUP_COUNT; i++) {
        ShopPopup* p = &g_pops[i];
        if (p->age < 0 || p->age >= SHOP_POPUP_LIFE) { p->age = -1; continue; }
        float t = (float)p->age / (float)SHOP_POPUP_LIFE;
        if (p->kind == 0) {
            int slen = (int)strlen(p->text);
            int w = pd->graphics->getTextWidth(game.fontBold, p->text, slen,
                                                kASCIIEncoding, 0);
            int x = p->cx - w / 2;
            int y = p->cy - (int)(t * 24.0f);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            for (int ox = -1; ox <= 1; ox++)
                for (int oy = -1; oy <= 1; oy++)
                    pd->graphics->drawText(p->text, slen, kASCIIEncoding,
                                           x + ox, y + oy);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            pd->graphics->drawText(p->text, slen, kASCIIEncoding, x, y);
        } else {
            // Burst — 8 spokes growing outward, thinning as they fade.
            int r  = (int)(6 + t * 34.0f);
            int lw = (t < 0.45f) ? 3 : 2;
            for (int a = 0; a < 8; a++) {
                float ang = a * (2.0f * PI_F / 8.0f) + 0.30f;
                int x1 = p->cx + (int)(cosf(ang) * r * 0.40f);
                int y1 = p->cy + (int)(sinf(ang) * r * 0.40f);
                int x2 = p->cx + (int)(cosf(ang) * r);
                int y2 = p->cy + (int)(sinf(ang) * r);
                GFX_LINE(x1, y1, x2, y2, lw, kColorBlack);
            }
        }
        p->age++;
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

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
// Each food gets its own 1-bit silhouette so the shop reads at a glance.
// Sprites sit in roughly a 22x26 box, anchored at (cx, baseY) = bottom-centre.
static void render_food_hay(int cx, int baseY) {
    // Trapezoidal bundle with diagonal straws and a binding band.
    int yT = baseY - 23, yB = baseY - 4;
    int xtl = cx - 6, xtr = cx + 6;
    int xbl = cx - 10, xbr = cx + 10;
    GFX_FILL(xbl, yT + 4, 21, yB - yT - 4, kColorWhite);
    GFX_LINE(xtl, yT, xbl, yT + 4, 2, kColorBlack);
    GFX_LINE(xtr, yT, xbr, yT + 4, 2, kColorBlack);
    GFX_LINE(xtl, yT, xtr, yT, 2, kColorBlack);
    GFX_LINE(xbl, yT + 4, xbl, yB, 2, kColorBlack);
    GFX_LINE(xbr, yT + 4, xbr, yB, 2, kColorBlack);
    GFX_LINE(xbl, yB, xbr, yB, 2, kColorBlack);
    GFX_LINE(cx - 7, yB - 1, cx,     yT + 5, 1, kColorBlack);
    GFX_LINE(cx - 2, yB - 1, cx + 5, yT + 5, 1, kColorBlack);
    GFX_LINE(cx + 4, yB - 1, cx - 3, yT + 5, 1, kColorBlack);
    GFX_FILL(xbl - 1, yT + 11, 22, 3, kColorBlack);          // binding band
}

static void render_food_edelweiss(int cx, int baseY) {
    // 5 white round petals around a black centre + short stem.
    int cy = baseY - 14;
    const int px[5] = { 0,  6,  4, -4, -6 };
    const int py[5] = {-7, -2,  5,  5, -2 };
    GFX_LINE(cx, cy + 3, cx, baseY - 4, 2, kColorBlack);     // stem
    for (int i = 0; i < 5; i++) {
        int ex = cx + px[i] - 4, ey = cy + py[i] - 4;
        GFX_ELLIPSE(ex, ey, 9, 9, kColorWhite);
        pd->graphics->drawEllipse(ex, ey, 9, 9, 2, 0, 360, kColorBlack);
    }
    GFX_ELLIPSE(cx - 3, cy - 3, 7, 7, kColorBlack);          // centre
}

static void render_food_salt(int cx, int baseY) {
    // Chunky block with dotted crystal texture.
    int yT = baseY - 22, yB = baseY - 4;
    int xL = cx - 10;
    GFX_FILL(xL, yT, 20, yB - yT, kColorWhite);
    GFX_RECT(xL, yT, 20, yB - yT, kColorBlack);
    GFX_RECT(xL + 1, yT + 1, 18, yB - yT - 2, kColorBlack);  // double border
    for (int dy = yT + 4; dy < yB - 2; dy += 5) {
        for (int dx = xL + 4; dx < xL + 20 - 2; dx += 5) {
            GFX_FILL(dx, dy, 2, 2, kColorBlack);
        }
    }
}

static void render_food_cowbell(int cx, int baseY) {
    // Trapezoidal bell with a hoop handle and a clapper.
    int yT = baseY - 23, yB = baseY - 4;
    int xtl = cx - 4, xtr = cx + 4;
    int xbl = cx - 10, xbr = cx + 10;
    pd->graphics->drawEllipse(cx - 3, yT - 5, 7, 7, 2, 0, 360, kColorBlack);
    GFX_FILL(xbl + 1, yT + 2, 19, yB - yT - 4, kColorWhite);
    GFX_LINE(xtl, yT, xbl, yB - 2, 2, kColorBlack);
    GFX_LINE(xtr, yT, xbr, yB - 2, 2, kColorBlack);
    GFX_LINE(xtl, yT, xtr, yT, 2, kColorBlack);
    GFX_FILL(xbl - 1, yB - 3, 22, 3, kColorBlack);           // lip
    GFX_FILL(cx - 2, yB - 1, 4, 5, kColorBlack);             // clapper
}

static void render_food_cheese(int cx, int baseY) {
    // Triangular wedge filled with scanlines, with a few holes.
    int yT = baseY - 22, yB = baseY - 4;
    int H = yB - yT;
    for (int y = yT + 1; y < yB; y++) {
        int half = ((y - yT) * 10) / H;
        if (half > 0)
            GFX_FILL(cx - half, y, half * 2, 1, kColorWhite);
    }
    GFX_LINE(cx, yT, cx - 10, yB, 2, kColorBlack);
    GFX_LINE(cx, yT, cx + 10, yB, 2, kColorBlack);
    GFX_LINE(cx - 10, yB, cx + 10, yB, 2, kColorBlack);
    GFX_ELLIPSE(cx - 4, yB - 7, 4, 4, kColorBlack);
    GFX_ELLIPSE(cx + 1, yB - 5, 3, 3, kColorBlack);
    GFX_ELLIPSE(cx - 1, yB - 13, 3, 3, kColorBlack);
}

static void render_food_milk(int cx, int baseY) {
    // Tall bottle with a narrow neck and a black label band.
    int yT = baseY - 26, yB = baseY - 4;
    int xL = cx - 7;
    // neck
    GFX_FILL(cx - 4, yT, 8, 5, kColorWhite);
    GFX_RECT(cx - 4, yT, 8, 5, kColorBlack);
    // body
    int bodyY = yT + 5;
    int bodyH = yB - bodyY;
    GFX_FILL(xL, bodyY, 14, bodyH, kColorWhite);
    GFX_RECT(xL, bodyY, 14, bodyH, kColorBlack);
    GFX_RECT(xL + 1, bodyY + 1, 12, bodyH - 2, kColorBlack);
    GFX_FILL(xL + 2, bodyY + 5, 10, 5, kColorBlack);          // label
}

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
    GFX_ELLIPSE(cx - 11, baseY - 5, 22, 7, (LCDColor)PAT_STONE);   // shadow

    switch (foodId) {
        case 0: render_food_hay(cx, baseY);       break;
        case 1: render_food_edelweiss(cx, baseY); break;
        case 2: render_food_salt(cx, baseY);      break;
        case 3: render_food_cowbell(cx, baseY);   break;
        case 4: render_food_cheese(cx, baseY);    break;
        case 5: render_food_milk(cx, baseY);      break;
        default: {
            // Fallback: jar + first letter (so a new food without art is
            // still visible until its sprite is wired).
            GFX_ELLIPSE(cx - 11, baseY - 26, 22, 24, kColorWhite);
            pd->graphics->drawEllipse(cx - 11, baseY - 26, 22, 24, 2, 0, 360,
                                      kColorBlack);
            GFX_FILL(cx - 8, baseY - 28, 16, 5, kColorBlack);
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
            break;
        }
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

    // Gold-counter flash: invert the pixel range of the "GOLD N" substring
    // when game.goldFxFrames > 0. Strobes every two frames for snap.
    if (game.goldFxFrames > 0 && game.fontBold) {
        char prefix[24], gold[24];
        int prefixLen = snprintf(prefix, sizeof(prefix), "TURN %d   ", game.turn);
        int goldLen   = snprintf(gold,   sizeof(gold),   "GOLD %d",   game.shop.gold);
        int xStart = 8 + pd->graphics->getTextWidth(game.fontBold, prefix,
                                                    prefixLen, kASCIIEncoding, 0);
        int gw     = pd->graphics->getTextWidth(game.fontBold, gold,
                                                goldLen, kASCIIEncoding, 0);
        if ((game.goldFxFrames & 2u) != 0u) {
            pd->graphics->setDrawMode(kDrawModeInverted);
            GFX_FILL(xStart - 2, 0, gw + 4, TOPBAR_H, kColorBlack);
        }
    }
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
// Pet info card — overlay shown when the cursor is on a real pet. Anchored
// opposite the hovered row so the row being inspected stays visible.
// ---------------------------------------------------------------------------
static void render_pet_info_card(uint8_t petId, int cursorColX, bool isShopHover,
                                 int atk, int hp, int level)
{
    const PetDef* def = pet_def(petId);
    if (!def) return;

    int cw = 260, ch = 68;
    int cy = isShopHover ? 126 : 22;
    int cx = cursorColX - cw / 2;
    if (cx < 4)                 cx = 4;
    if (cx + cw > SCREEN_W - 4) cx = SCREEN_W - 4 - cw;

    GFX_FILL(cx, cy, cw, ch, kColorWhite);
    GFX_RECT(cx, cy, cw, ch, kColorBlack);
    GFX_RECT(cx + 1, cy + 1, cw - 2, ch - 2, kColorBlack);

    int sx = cx + 6, sy = cy + 6;
    GFX_RECT(sx - 1, sy - 1, 34, 34, kColorBlack);
    draw_creature(petId, sx, sy);

    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(def->name, strlen(def->name), kASCIIEncoding,
                           cx + 44, cy + 4);

    char stats[40];
    if (level > 1)
        snprintf(stats, sizeof(stats), "T%d   ATK %d   HP %d   L%d",
                 (int)def->tier, atk, hp, level);
    else
        snprintf(stats, sizeof(stats), "T%d   ATK %d   HP %d",
                 (int)def->tier, atk, hp);
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText(stats, strlen(stats), kASCIIEncoding,
                           cx + 44, cy + 24);

    // Ability description — wrap to a second line if it doesn't fit.
    const char* desc = def->description ? def->description : "";
    int maxW = cw - 12, len = (int)strlen(desc);
    int from = 0, lineY = cy + 40, lineH = 12, drawn = 0;
    while (from < len && drawn < 2) {
        int to = len;
        while (to > from) {
            int w = pd->graphics->getTextWidth(game.fontSmall, desc + from,
                                               to - from, kASCIIEncoding, 0);
            if (w <= maxW) break;
            int sp = to - 1;
            while (sp > from && desc[sp] != ' ') sp--;
            if (sp <= from) { to--; continue; }
            to = sp;
        }
        pd->graphics->drawText(desc + from, to - from, kASCIIEncoding,
                               cx + 6, lineY);
        from = to;
        while (from < len && desc[from] == ' ') from++;
        lineY += lineH;
        drawn++;
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Same plate as the pet card, retooled for foods — jar mini-icon, name,
// cost, one-line description.
static void render_food_info_card(uint8_t foodId, int cursorColX)
{
    if (foodId >= FOOD_COUNT) return;
    const FoodDef* d = &FOODS[foodId];

    int cw = 260, ch = 68;
    int cy = 126;                                   // overlay the team row
    int cx = cursorColX - cw / 2;
    if (cx < 4)                 cx = 4;
    if (cx + cw > SCREEN_W - 4) cx = SCREEN_W - 4 - cw;

    GFX_FILL(cx, cy, cw, ch, kColorWhite);
    GFX_RECT(cx, cy, cw, ch, kColorBlack);
    GFX_RECT(cx + 1, cy + 1, cw - 2, ch - 2, kColorBlack);

    // Mini jar in the sprite cell.
    int sx = cx + 6, sy = cy + 6;
    GFX_RECT(sx - 1, sy - 1, 34, 34, kColorBlack);
    render_food(foodId, sx + 16, sy + 32, false, false);

    // Name (fontBold).
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(d->name, strlen(d->name), kASCIIEncoding,
                           cx + 44, cy + 4);

    // Cost line.
    char costBuf[16];
    snprintf(costBuf, sizeof(costBuf), "Cost: %dg", (int)d->cost);
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText(costBuf, strlen(costBuf), kASCIIEncoding,
                           cx + 44, cy + 24);

    // Description — wraps to a second line if needed (same logic as pets).
    const char* desc = d->description ? d->description : "";
    int maxW = cw - 12, len = (int)strlen(desc);
    int from = 0, lineY = cy + 40, lineH = 12, drawn = 0;
    while (from < len && drawn < 2) {
        int to = len;
        while (to > from) {
            int w = pd->graphics->getTextWidth(game.fontSmall, desc + from,
                                               to - from, kASCIIEncoding, 0);
            if (w <= maxW) break;
            int sp = to - 1;
            while (sp > from && desc[sp] != ' ') sp--;
            if (sp <= from) { to--; continue; }
            to = sp;
        }
        pd->graphics->drawText(desc + from, to - from, kASCIIEncoding,
                               cx + 6, lineY);
        from = to;
        while (from < len && desc[from] == ' ') from++;
        lineY += lineH;
        drawn++;
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Shop screen — three pet slots on the left, two food slots on the right.
// ---------------------------------------------------------------------------
void render_shop_screen(void)
{
    render_alpine_bg(58);
    render_topbar();

    const float SC = 1.5f;
    const int* shopColX = SHOP_COL_X;       // aliases for compactness
    const int* colX     = TEAM_COL_X;
    const int* foodX    = FOOD_COL_X;
    int shopFeet = 110;
    int teamFeet = 182;

    // --- Shop pets ---
    // On reroll, each unfrozen pet pops in with a 3-phase emergence,
    // staggered left-to-right so the three slots cascade:
    //   1) puff cloud blooms (pet invisible)
    //   2) pet scale-bounces up with overshoot
    //   3) sparkle ring contracts and fades
    // Frozen pets are skipped entirely.
    const int ROLL_TOTAL  = 15;
    const int ROLL_STAGGER = 3;        // frames between each pet's pop start
    const int ROLL_WINDOW  = 9;        // frames each pet's anim lasts
    int rollElapsed = 0;
    if (game.shopRollAnim > 0) {
        rollElapsed = ROLL_TOTAL - (int)game.shopRollAnim;
        if (rollElapsed < 0) rollElapsed = 0;
        if (rollElapsed > ROLL_TOTAL) rollElapsed = ROLL_TOTAL;
    }
    for (int i = 0; i < MAX_SHOP_PETS; i++) {
        bool sel = game.cursorShown && (game.cursor == i);
        bool frozen = (game.shop.frozenPets & (1 << i)) != 0;
        uint8_t pid = game.shop.pets[i];
        int16_t a = 0, h = 0;
        if (pid != EMPTY_PET) {
            const PetDef* d = pet_def(pid);
            if (d) { a = d->baseAtk; h = d->baseHp; }
        }

        float petSC = SC;
        int   puffR = 0;
        int   sparkR = 0;
        if (game.shopRollAnim > 0 && !frozen) {
            int petElapsed = rollElapsed - i * ROLL_STAGGER;
            float k = (float)petElapsed / (float)ROLL_WINDOW;
            if (k < 0.0f) k = 0.0f;
            if (k > 1.0f) k = 1.0f;
            if (k <= 0.4f) {
                petSC = 0.0f;
                puffR = 5 + (int)(k * 32.0f);                  // 5 → 18
            } else {
                float p = (k - 0.4f) / 0.6f;                   // 0..1
                petSC = SC * (p < 0.6f
                              ? (p / 0.6f) * 1.18f             // bounce up
                              : 1.18f - (p - 0.6f) / 0.4f * 0.18f); // settle
                sparkR = 16 - (int)(p * 12.0f);                // 16 → 4
            }
        }

        // Puff cloud — four overlapping white ellipses with black outlines.
        if (puffR > 0) {
            int cx = shopColX[i];
            int cy = shopFeet - (int)(32 * SC) / 2;
            for (int pi = 0; pi < 4; pi++) {
                float ang = pi * (PI_F / 2.0f) + 0.7f;
                int px = cx + (int)(cosf(ang) * (puffR * 0.7f));
                int py = cy + (int)(sinf(ang) * (puffR * 0.5f));
                GFX_ELLIPSE(px - puffR / 2, py - puffR / 2,
                            puffR, puffR, kColorWhite);
                pd->graphics->drawEllipse(px - puffR / 2, py - puffR / 2,
                                          puffR, puffR, 1, 0, 360, kColorBlack);
            }
        }

        render_unit(pid, 0, a, h, EMPTY_FOOD, shopColX[i], shopFeet, petSC, sel,
                    false, false);

        // Sparkle ring — 6 short radial spokes that contract as pet settles.
        if (sparkR > 0) {
            int cx = shopColX[i];
            int cy = shopFeet - (int)(32 * SC) / 2;
            for (int sp = 0; sp < 6; sp++) {
                float ang = sp * (PI_F / 3.0f) + 0.2f;
                int x1 = cx + (int)(cosf(ang) * sparkR);
                int y1 = cy + (int)(sinf(ang) * sparkR);
                int x2 = cx + (int)(cosf(ang) * (sparkR + 5));
                int y2 = cy + (int)(sinf(ang) * (sparkR + 5));
                GFX_LINE(x1, y1, x2, y2, 2, kColorBlack);
            }
        }

        // Cost badge + frozen mark only when stable.
        bool stable = (game.shopRollAnim == 0 || frozen);
        if (pid != EMPTY_PET && stable) {
            int cy = shopFeet - (int)(32 * SC) - 8;
            GFX_ELLIPSE(shopColX[i] - 11, cy - 7, 22, 15, kColorBlack);
            if (game.fontBold) pd->graphics->setFont(game.fontBold);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            pd->graphics->drawText("3g", 2, kASCIIEncoding,
                                   shopColX[i] - 8, cy - 8);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
        if (frozen)
            draw_frozen_mark(shopColX[i] + 18, shopFeet - (int)(32 * SC) + 2);
    }

    // --- Shop foods ---
    for (int i = 0; i < MAX_SHOP_FOOD; i++) {
        bool sel = game.cursorShown && (game.cursor == MAX_SHOP_PETS + i);
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
        bool sel = game.cursorShown &&
                   (game.cursor == MAX_SHOP_PETS + MAX_SHOP_FOOD + i);
        bool picked = (game.selectedTeamSlot == i);
        PetSlot* p = &game.player.slots[i];
        render_unit(p->id, p->level, p->atk, p->hp, p->food,
                    colX[i], teamFeet, SC, sel, picked, false);
    }

    // --- Food targeting overlay: pulsing corner-brackets + the held food
    //     floating above the candidate pet. Drawn after the team row so it
    //     sits on top of the sprite.
    if (game.pendingFoodSlot != 0xFF) {
        int colIdx = (int)game.cursor - (MAX_SHOP_PETS + MAX_SHOP_FOOD);
        if (colIdx >= 0 && colIdx < TEAM_SIZE) {
            int rx = colX[colIdx];
            int spr = (int)(32 * SC);
            int ry = teamFeet - spr / 2;
            int half = spr / 2 + 6;
            int off = (int)((game.frameCount / 4) % 4);  // 0..3 pulse
            int len = 9;
            int x0 = rx - half + off, x1 = rx + half - off;
            int y0 = ry - half + off, y1 = ry + half - off;
            // four L-brackets
            GFX_LINE(x0, y0, x0 + len, y0, 2, kColorBlack);
            GFX_LINE(x0, y0, x0, y0 + len, 2, kColorBlack);
            GFX_LINE(x1, y0, x1 - len, y0, 2, kColorBlack);
            GFX_LINE(x1, y0, x1, y0 + len, 2, kColorBlack);
            GFX_LINE(x0, y1, x0 + len, y1, 2, kColorBlack);
            GFX_LINE(x0, y1, x0, y1 - len, 2, kColorBlack);
            GFX_LINE(x1, y1, x1 - len, y1, 2, kColorBlack);
            GFX_LINE(x1, y1, x1, y1 - len, 2, kColorBlack);
            // floating food above the head, bobbing
            int bob = (int)(2.0f * sinf((float)game.frameCount * 0.18f));
            uint8_t fid = game.shop.foods[game.pendingFoodSlot];
            render_food(fid, rx, ry - spr / 2 - 4 + bob, false, false);
        }
    }

    // --- Action bar ---
    int rollIdx = MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
    int endIdx  = rollIdx + 1;
    int ay = 196;
    draw_button(8,   ay, 96, 24, "ROLL 1g",
                game.cursorShown && game.cursor == rollIdx);
    draw_button(296, ay, 96, 24, "END TURN",
                game.cursorShown && game.cursor == endIdx);

    // --- Hint strip ---
    const char* hint = "";
    uint8_t c = game.cursor;
    if (!game.cursorShown) hint = "press d-pad to begin";
    else if (game.pendingFoodSlot != 0xFF) hint = "A feed   B cancel";
    else if (c < MAX_SHOP_PETS) hint = "A buy   B freeze";
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

    // (Reroll sweep band removed — the staggered per-pet puff + bounce +
    //  sparkle pop carries the animation on its own.)

    // --- Hover info card (only when the cursor is awake) ---
    if (game.cursorShown) {
        if (c < MAX_SHOP_PETS) {
            uint8_t pid = game.shop.pets[c];
            if (pid != EMPTY_PET) {
                const PetDef* d = pet_def(pid);
                render_pet_info_card(pid, shopColX[c], true,
                                     d ? d->baseAtk : 0, d ? d->baseHp : 0, 1);
            }
        } else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD) {
            int fi = c - MAX_SHOP_PETS;
            uint8_t fid = game.shop.foods[fi];
            if (fid != EMPTY_FOOD)
                render_food_info_card(fid, foodX[fi]);
        } else if (c >= (MAX_SHOP_PETS + MAX_SHOP_FOOD) &&
                   c <  (MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE)) {
            int ti = c - MAX_SHOP_PETS - MAX_SHOP_FOOD;
            PetSlot* p = &game.player.slots[ti];
            if (p->id != EMPTY_PET) {
                render_pet_info_card(p->id, colX[ti], false,
                                     (int)p->atk, (int)p->hp, (int)p->level);
            }
        }
    }

    // --- Floating shop popups (gold deltas, level-up sparkles) ---
    shop_popup_drain();

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
    float dur = (float)battle_event_window(game.battlePlayhead);
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
// Sentinel -1 means "snap on next frame" — set on battle start and after a
// FAINT so re-occupied slots don't drift from a stale off-screen flyoff x.
static float g_unitX[2][TEAM_SIZE];

void render_battle_reset_positions(void) {
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < TEAM_SIZE; i++)
            g_unitX[s][i] = -1.0f;
}

void render_battle_reset_slot_position(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE) return;
    g_unitX[side][slot] = -1.0f;
}

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

// Will the unit at side/slot be KO'd by the clash logged at `clashIdx`?
// Used by the clash animation to fold the loser's fly-off into the same
// window — so a lethal hit launches them off-screen the instant of contact.
static bool clash_kills(int clashIdx, int side, int slot) {
    for (int i = clashIdx + 1; i < game.battleLogLen; i++) {
        BattleEvent* e = &game.battleLog[i];
        if (e->kind == BEV_CLASH || e->kind == BEV_ATTACK) break;
        if (e->kind == BEV_FAINT && e->side == side && e->slot == slot)
            return true;
    }
    return false;
}

// A radial spark-burst at (cx,cy); k = 0..1 progress, fades out by ~0.5.
static void battle_burst(int cx, int cy, float k) {
    if (k < 0.0f || k >= 0.5f) return;
    int r = (int)(9 + k * 80.0f);
    for (int a = 0; a < 9; a++) {
        float ang = a * (2.0f * PI_F / 9.0f) + 0.30f;
        GFX_LINE(cx + (int)(cosf(ang) * r * 0.34f),
                 cy + (int)(sinf(ang) * r * 0.34f),
                 cx + (int)(cosf(ang) * r),
                 cy + (int)(sinf(ang) * r), 3, kColorBlack);
    }
}

// Per-event horizontal offset that battle_draw_unit applies to a sprite
// (clash lunge, hurt-shake, fly-off in clash window). Extracted so combat-
// text popups can ride the same offset and stay glued to the sprite instead
// of floating at the eased rest position.
static int event_unit_dx(BattleEvent* e, float t, int side, int slot) {
    if (!e) return 0;
    if (e->kind == BEV_CLASH) {
        bool isClasher = (side == 0 && slot == e->slot) ||
                         (side == 1 && slot == e->data);
        if (!isClasher) return 0;
        const float peak = 26.0f, contactT = 0.42f, windT = 0.12f;
        int dir = (side == 0) ? 1 : -1;
        if (t < windT) {
            float w = t / windT;
            return -dir * (int)(w * w * 4.0f);
        }
        if (t < contactT) {
            float local = (t - windT) / (contactT - windT);
            float pos = -4.0f + (peak + 4.0f) * local * local;
            return (int)((float)dir * pos);
        }
        if (clash_kills(game.battlePlayhead, side, slot)) {
            // Sprite flies off — popup rides alongside so the number doesn't
            // get left behind at the rest position.
            float flyT = (t - contactT) / (1.0f - contactT);
            float eo = 1.0f - (1.0f - flyT) * (1.0f - flyT);
            return -dir * (int)(eo * 540.0f);
        }
        float r = (t - contactT) / (1.0f - contactT);
        return dir * (int)(peak * (1.0f - r) - 7.0f * sinf(r * PI_F));
    }
    if (e->kind == BEV_DAMAGE && e->side == side && e->slot == slot) {
        // P2: divide frequency by playback speed so the shake stays
        // perceptually consistent at all crank speeds (1×..8×).
        int spd = (int)game.battleSpeed; if (spd < 1) spd = 1;
        return (int)(sinf(t * 38.0f / (float)spd) * 3.0f);
    }
    return 0;
}

// KO launch — fling the unit off-screen with a hard spin and a scale punch.
// Ease-out so it bolts at the moment of impact instead of easing in.
static void draw_unit_flyoff(PetSlot* p, int side, int cx0, int feetY,
                             int spr, float flyT) {
    if (flyT < 0.0f) flyT = 0.0f;
    if (flyT > 1.0f) flyT = 1.0f;
    float eo = 1.0f - (1.0f - flyT) * (1.0f - flyT);
    int backDir = (side == 0) ? -1 : 1;
    int cxK = cx0 + backDir * (int)(eo * 540.0f);
    int cyK = (feetY - spr / 2) - (int)(sinf(flyT * PI_F) * 150.0f)
                                + (int)(eo * 60.0f);
    float deg = eo * 820.0f * (float)backDir;
    float sc  = BATTLE_SCALE *
                (1.0f + 0.45f * (1.0f - flyT) * (1.0f - flyT) - 0.32f * flyT);
    draw_creature_xform(p->id, cxK, cyK, sc, deg, side == 1);
}

// Draw a single battle unit with clash / faint / hit animation.
static void battle_draw_unit(BattleEvent* e, float t, int side, int slot, float fx) {
    PetSlot* p = &game.battle.teams[side].slots[slot];

    int spr   = (int)(32 * BATTLE_SCALE);
    int restX = (int)fx;

    // Ability KO — a faint not folded into a clash flies off on its own window.
    if (e && e->kind == BEV_FAINT && e->side == side && e->slot == slot) {
        if (p->id == EMPTY_PET) return;
        battle_burst(restX, BATTLE_FEET - spr / 2, t);
        draw_unit_flyoff(p, side, restX, BATTLE_FEET, spr, t);
        return;
    }

    // Phase 3.4: Spawn shimmer — render the NEW pet emerging using the
    // event's payload (slot may still hold the dying predecessor until the
    // SPAWN apply at end-of-window).
    if (e && e->kind == BEV_SPAWN && e->side == side && e->slot == slot) {
        uint8_t newPetId = e->data;
        int newAtk = e->value & 0xFF;
        int newHp  = (e->value >> 8) & 0xFF;
        float sc = BATTLE_SCALE * (0.55f + 0.45f * t);
        int hover = -(int)(sinf(t * PI_F) * 4.0f);
        render_unit(newPetId, 1, (int16_t)newAtk, (int16_t)newHp, EMPTY_FOOD,
                    restX, BATTLE_FEET + hover, sc, false, false, side == 1);
        int cy = BATTLE_FEET + hover - (int)(32 * sc) / 2;
        int r = (int)(8 + t * 14);
        for (int a = 0; a < 6; a++) {
            float ang = a * (PI_F / 3.0f) + t * 0.4f;
            int x1 = restX + (int)(cosf(ang) * r);
            int y1 = cy    + (int)(sinf(ang) * r);
            int x2 = restX + (int)(cosf(ang) * (r + 5));
            int y2 = cy    + (int)(sinf(ang) * (r + 5));
            GFX_LINE(x1, y1, x2, y2, 2, kColorBlack);
        }
        return;
    }

    // Ability glow — pulsing ring + cardinal spikes anchored at the slot's
    // rest position. Drawn BEFORE the hp gate so faint-triggered abilities
    // (Hopper, Goat) still show their origin after the pet has flown off.
    bool isMyAbility = e && e->kind == BEV_ABILITY &&
                       e->side == side && e->slot == slot;
    if (isMyAbility) {
        int gx = restX;
        int gy = BATTLE_FEET - spr / 2;
        float k = sinf(t * PI_F);                     // 0 → 1 → 0
        int r = spr / 2 + 4 + (int)(k * 5.0f);
        pd->graphics->drawEllipse(gx - r, gy - r, r * 2, r * 2,
                                  3, 0, 360, kColorBlack);
        int len = 4 + (int)(k * 4);
        GFX_LINE(gx, gy - r - 1, gx, gy - r - 1 - len, 2, kColorBlack);
        GFX_LINE(gx, gy + r + 1, gx, gy + r + 1 + len, 2, kColorBlack);
        GFX_LINE(gx - r - 1, gy, gx - r - 1 - len, gy, 2, kColorBlack);
        GFX_LINE(gx + r + 1, gy, gx + r + 1 + len, gy, 2, kColorBlack);
    }

    // Don't render the unit sprite for empty or dying slots — the glow above
    // and the FAINT/SPAWN early-returns handle their respective visuals.
    if (p->id == EMPTY_PET || p->hp <= 0) return;

    // Lethal clash hit launches the sprite off-screen inside the clash
    // window itself (instead of waiting for a separate FAINT event).
    if (e && e->kind == BEV_CLASH &&
        ((side == 0 && slot == e->slot) || (side == 1 && slot == e->data))) {
        const float contactT = 0.42f;
        if (t >= contactT && clash_kills(game.battlePlayhead, side, slot)) {
            int dir = (side == 0) ? 1 : -1;
            float flyT = (t - contactT) / (1.0f - contactT);
            draw_unit_flyoff(p, side, restX + dir * 26,
                             BATTLE_FEET, spr, flyT);
            return;
        }
    }

    int dx = event_unit_dx(e, t, side, slot);
    int dy = 0;
    if (e && (e->kind == BEV_BUFF || e->kind == BEV_ABILITY) &&
        e->side == side && e->slot == slot) {
        dy = -(int)(sinf(t * PI_F) * 5.0f);                    // ability bob
    }

    render_unit(p->id, p->level, p->atk, p->hp, p->food,
                restX + dx, BATTLE_FEET + dy, BATTLE_SCALE,
                false, false, side == 1);

    // Phase 3.3: Stat-number flash on BUFF. Strobes an inverted disc over
    // whichever stat badge changed (low byte = atk delta, high byte = hp).
    if (e && e->kind == BEV_BUFF && e->side == side && e->slot == slot &&
        t < 0.55f) {
        int da = e->value & 0xFF;
        int dh = (e->value >> 8) & 0xFF;
        int half = spr / 2;
        int bo = half - 11;
        int by = BATTLE_FEET + dy - 7;
        int bx = restX + dx;
        bool on = ((int)(t * 22.0f) & 1) == 0;
        if (on) {
            pd->graphics->setDrawMode(kDrawModeInverted);
            if (da > 0) GFX_ELLIPSE(bx - bo - 12, by - 12, 24, 24, kColorBlack);
            if (dh > 0) GFX_ELLIPSE(bx + bo - 12, by - 12, 24, 24, kColorBlack);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }
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
    // Dying-but-not-yet-fainted slots are excluded so survivors slide
    // forward immediately. EXCEPT: if the slot has a pending BEV_SPAWN in
    // the next few events (Hopper → Zombie Hopper, Sheep → Haflingers,
    // Edelweiss → Bee), keep it in the rank so the neighbour doesn't
    // shuffle forward and then snap back when the spawn materialises.
    for (int side = 0; side < 2; side++) {
        int rank = 0;
        for (int i = TEAM_SIZE - 1; i >= 0; i--) {
            PetSlot* p = &game.battle.teams[side].slots[i];
            if (p->id == EMPTY_PET) continue;
            if (p->hp <= 0) {
                bool willRespawn = false;
                int safety = 6;
                for (int j = (int)game.battlePlayhead;
                     j < game.battleLogLen && safety > 0; j++, safety--) {
                    BattleEvent* ev = &game.battleLog[j];
                    if (ev->kind == BEV_SPAWN && (int)ev->side == side &&
                        (int)ev->slot == i) { willRespawn = true; break; }
                    if (ev->kind == BEV_CLASH || ev->kind == BEV_ATTACK) break;
                }
                if (!willRespawn) continue;
            }
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
    // Anchor each popup at the sprite's CURRENT animated x (rest position
    // plus the same per-event dx the sprite uses), so the number rises from
    // above the bouncing/shaking pet rather than its eased rest spot.
    int popTop = BATTLE_FEET - (int)(32 * BATTLE_SCALE) - 14;
    if (e && t > 0.42f && e->kind == BEV_CLASH) {
        int pDmg = e->value & 0xFF, eDmg = (e->value >> 8) & 0xFF;
        char s[8];
        if (eDmg > 0) {
            snprintf(s, sizeof(s), "-%d", eDmg);
            int px = (int)g_unitX[0][e->slot] + event_unit_dx(e, t, 0, e->slot);
            battle_popup(px, popTop, s, t - 0.42f);
        }
        if (pDmg > 0) {
            snprintf(s, sizeof(s), "-%d", pDmg);
            int px = (int)g_unitX[1][e->data] + event_unit_dx(e, t, 1, e->data);
            battle_popup(px, popTop, s, t - 0.42f);
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
        if (s[0]) {
            int px = (int)g_unitX[e->side][e->slot]
                   + event_unit_dx(e, t, (int)e->side, (int)e->slot);
            battle_popup(px, popTop, s, t);
        }
    }

    // --- speed indicator ---
    if (game.battleSpeed > 1) {
        char sb[12];
        int sblen = snprintf(sb, sizeof(sb), "%dx", (int)game.battleSpeed);
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        int tx = BATTLE_CX - 8, ty = 24;
        // 8-offset white halo for legibility against varied backgrounds,
        // then the black glyph on top — same trick battle_popup uses.
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        for (int ox = -1; ox <= 1; ox++)
            for (int oy = -1; oy <= 1; oy++)
                pd->graphics->drawText(sb, sblen, kASCIIEncoding, tx + ox, ty + oy);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(sb, sblen, kASCIIEncoding, tx, ty);
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
