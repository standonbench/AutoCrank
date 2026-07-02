#include "game.h"
#include "theme.h"
#include <stdio.h>

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
                                  (LCDColor)TH_PAT_50);
        if (selected)
            pd->graphics->drawEllipse(cx - 21, baseY - 10, 42, 14, 2, 0, 360,
                                      kColorBlack);
        return;
    }

    // ground shadow
    GFX_ELLIPSE(cx - half + 3, baseY - 5, spr - 6, 8, (LCDColor)TH_PAT_50);

    // selection / picked ring: a flat ground-perspective ellipse around the
    // feet. The sprite is drawn haloed on top so the ring reads as passing
    // BEHIND the pet (sprites blit white-transparent, so without the halo
    // the ring would show through the body).
    if (selected || picked) {
        pd->graphics->drawEllipse(cx - half - 3, baseY - 9, spr + 6, 14,
                                  picked ? 3 : 2, 0, 360, kColorBlack);
    }

    // sprite (feet on baseY) — faceLeft mirrors the right-facing art
    if (selected || picked)
        draw_creature_haloed(petId, cx, baseY - half, spriteScale, faceLeft);
    else
        draw_creature_xform(petId, cx, baseY - half, spriteScale, 0.0f,
                            faceLeft);

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
    // Sheaf of hay: domed bundle, two binding bands, loose straws poking
    // out the top, dither shading on the shaded flank.
    int yT = baseY - 22, yB = baseY - 4;
    int xbl = cx - 10, xbr = cx + 10;
    // loose straws first, so the bundle overlaps their roots
    GFX_LINE(cx - 4, yT + 2, cx - 8, yT - 4, 1, kColorBlack);
    GFX_LINE(cx,     yT + 2, cx + 1, yT - 5, 1, kColorBlack);
    GFX_LINE(cx + 4, yT + 2, cx + 8, yT - 3, 1, kColorBlack);
    // domed bundle: rounded crown over a flared body
    GFX_ELLIPSE(cx - 8, yT - 2, 16, 12, kColorWhite);
    pd->graphics->drawEllipse(cx - 8, yT - 2, 16, 12, 2, 90, 270, kColorBlack);
    pd->graphics->fillTriangle(cx - 8, yT + 3, cx + 8, yT + 3,
                               cx, yB, kColorWhite);
    GFX_FILL(xbl, yT + 8, 21, yB - yT - 8, kColorWhite);
    GFX_LINE(cx - 8, yT + 3, xbl, yB, 2, kColorBlack);
    GFX_LINE(cx + 8, yT + 3, xbr, yB, 2, kColorBlack);
    GFX_LINE(xbl, yB, xbr, yB, 2, kColorBlack);
    // shaded flank
    pd->graphics->fillTriangle(cx + 7, yT + 5, xbr - 2, yB - 2,
                               cx + 2, yB - 2, (LCDColor)TH_PAT_25);
    // straw strokes + two binding bands
    GFX_LINE(cx - 6, yB - 2, cx - 1, yT + 6, 1, kColorBlack);
    GFX_LINE(cx,     yB - 2, cx + 4, yT + 6, 1, kColorBlack);
    GFX_FILL(xbl,     yT + 9,  20, 2, kColorBlack);
    GFX_FILL(xbl - 1, yT + 15, 22, 2, kColorBlack);
}

static void render_food_edelweiss(int cx, int baseY) {
    // Star of pointed white petals (the real flower's shape) around a
    // fuzzy dotted centre, on a stem with one small leaf.
    int cy = baseY - 15;
    GFX_LINE(cx, cy + 3, cx, baseY - 4, 2, kColorBlack);       // stem
    GFX_LINE(cx, baseY - 8, cx + 4, baseY - 11, 2, kColorBlack); // leaf
    for (int i = 0; i < 6; i++) {
        float a = -PI_F / 2.0f + (float)i * (PI_F / 3.0f);
        int tx = cx + (int)(cosf(a) * 10.0f);
        int ty = cy + (int)(sinf(a) * 10.0f);
        int lx = cx + (int)(cosf(a + 0.55f) * 4.0f);
        int ly = cy + (int)(sinf(a + 0.55f) * 4.0f);
        int rx = cx + (int)(cosf(a - 0.55f) * 4.0f);
        int ry = cy + (int)(sinf(a - 0.55f) * 4.0f);
        pd->graphics->fillTriangle(lx, ly, rx, ry, tx, ty, kColorWhite);
        GFX_LINE(lx, ly, tx, ty, 1, kColorBlack);
        GFX_LINE(rx, ry, tx, ty, 1, kColorBlack);
    }
    // fuzzy centre: black disc ringed by tiny floret dots
    GFX_ELLIPSE(cx - 3, cy - 3, 7, 7, kColorBlack);
    GFX_FILL(cx - 1, cy - 1, 2, 2, kColorWhite);
    GFX_FILL(cx - 4, cy - 5, 1, 1, kColorBlack);
    GFX_FILL(cx + 3, cy - 4, 1, 1, kColorBlack);
    GFX_FILL(cx + 4, cy + 2, 1, 1, kColorBlack);
    GFX_FILL(cx - 5, cy + 3, 1, 1, kColorBlack);
}

static void render_food_salt(int cx, int baseY) {
    // Salt-lick block drawn as a small iso cube: white front face, dithered
    // side face, top face catching the light, sparkle glints up front.
    int yT = baseY - 22, yB = baseY - 4;
    int fL = cx - 10, fR = cx + 4;          // front face
    int d = 5;                              // iso depth
    // top face
    pd->graphics->fillTriangle(fL, yT + d, fL + d, yT, fR + d, yT, kColorWhite);
    pd->graphics->fillTriangle(fL, yT + d, fR + d, yT, fR, yT + d, kColorWhite);
    // side face, shaded
    pd->graphics->fillTriangle(fR, yT + d, fR + d, yT, fR + d, yB - d,
                               (LCDColor)TH_PAT_25);
    pd->graphics->fillTriangle(fR, yT + d, fR + d, yB - d, fR, yB,
                               (LCDColor)TH_PAT_25);
    // front face
    GFX_FILL(fL, yT + d, fR - fL, yB - yT - d, kColorWhite);
    // outline the silhouette + the two visible inner edges
    GFX_LINE(fL, yT + d, fL + d, yT, 1, kColorBlack);
    GFX_LINE(fL + d, yT, fR + d, yT, 2, kColorBlack);
    GFX_LINE(fR + d, yT, fR + d, yB - d, 2, kColorBlack);
    GFX_LINE(fR + d, yB - d, fR, yB, 1, kColorBlack);
    GFX_RECT(fL, yT + d, fR - fL, yB - yT - d, kColorBlack);
    GFX_LINE(fL + 1, yT + d + 1, fR - 1, yT + d + 1, 1, kColorBlack);
    // sparkle glints: tiny plus signs on the front face
    GFX_FILL(cx - 6, yT + 10, 1, 3, kColorBlack);
    GFX_FILL(cx - 7, yT + 11, 3, 1, kColorBlack);
    GFX_FILL(cx,     yB - 8,  1, 3, kColorBlack);
    GFX_FILL(cx - 1, yB - 7,  3, 1, kColorBlack);
}

static void render_food_cowbell(int cx, int baseY) {
    // Swiss bell: leather strap with buckle, flared body, dark mouth band,
    // dither shading on one cheek and a tiny engraved cross.
    int yT = baseY - 21, yB = baseY - 4;
    // strap + buckle
    GFX_FILL(cx - 3, yT - 6, 6, 7, kColorBlack);
    GFX_FILL(cx - 1, yT - 4, 2, 2, kColorWhite);
    // body: straight shoulders flaring out near the mouth
    int sL = cx - 5, sR = cx + 5;            // shoulder
    int fL = cx - 8, fR = cx + 8;            // waist where the flare begins
    int mL = cx - 11, mR = cx + 11;          // mouth
    int yW = yB - 7;
    GFX_FILL(sL, yT, sR - sL, yW - yT, kColorWhite);
    pd->graphics->fillTriangle(sL, yT, fL, yW, sL, yW, kColorWhite);
    pd->graphics->fillTriangle(sR, yT, sR, yW, fR, yW, kColorWhite);
    pd->graphics->fillTriangle(fL, yW, mL, yB - 2, fR, yW, kColorWhite);
    pd->graphics->fillTriangle(fR, yW, mL, yB - 2, mR, yB - 2, kColorWhite);
    GFX_LINE(sL, yT, fL, yW, 2, kColorBlack);
    GFX_LINE(sR, yT, fR, yW, 2, kColorBlack);
    GFX_LINE(fL, yW, mL, yB - 2, 2, kColorBlack);
    GFX_LINE(fR, yW, mR, yB - 2, 2, kColorBlack);
    GFX_LINE(sL, yT, sR, yT, 2, kColorBlack);
    // shaded cheek + engraved cross
    pd->graphics->fillTriangle(sR - 2, yT + 3, fR - 1, yW, cx + 1, yW,
                               (LCDColor)TH_PAT_25);
    GFX_FILL(cx - 1, yT + 5, 2, 6, kColorBlack);
    GFX_FILL(cx - 3, yT + 7, 6, 2, kColorBlack);
    // mouth band + clapper
    GFX_FILL(mL - 1, yB - 3, mR - mL + 2, 3, kColorBlack);
    GFX_FILL(cx - 2, yB - 1, 4, 5, kColorBlack);
}

static void render_food_cheese(int cx, int baseY) {
    // Alpine cheese wedge lying point-toward-the-viewer: pale cut face on
    // top, dithered rind on the side, holes of mixed sizes (one bitten by
    // the edge so the silhouette reads as cheese instantly).
    int yB = baseY - 4;
    int apexX = cx - 11, apexY = yB - 9;     // wedge point, at left
    int bR = cx + 11;                        // back (rind) edge, at right
    int rindT = yB - 16;                     // top of the rind face
    // cut face: the sloping top plane
    pd->graphics->fillTriangle(apexX, apexY, bR, rindT, bR, rindT + 5,
                               kColorWhite);
    // body below the face
    pd->graphics->fillTriangle(apexX, apexY, bR, rindT + 5, bR, yB,
                               kColorWhite);
    pd->graphics->fillTriangle(apexX, apexY, bR, yB, apexX + 4, yB,
                               kColorWhite);
    // rind: dithered band down the right side
    pd->graphics->fillTriangle(bR - 4, rindT + 2, bR, rindT, bR, yB,
                               (LCDColor)TH_PAT_50);
    pd->graphics->fillTriangle(bR - 4, rindT + 2, bR, yB, bR - 4, yB,
                               (LCDColor)TH_PAT_50);
    // outline
    GFX_LINE(apexX, apexY, bR, rindT, 2, kColorBlack);
    GFX_LINE(bR, rindT, bR, yB, 2, kColorBlack);
    GFX_LINE(apexX, apexY, apexX + 4, yB, 2, kColorBlack);
    GFX_LINE(apexX + 4, yB, bR, yB, 2, kColorBlack);
    // holes: two round, one bitten by the top edge
    GFX_ELLIPSE(cx - 2, yB - 8, 5, 5, kColorBlack);
    GFX_ELLIPSE(cx + 4, yB - 12, 3, 3, kColorBlack);
    GFX_ELLIPSE(cx - 5, apexY + 1, 4, 3, kColorBlack);
}

static void render_food_milk(int cx, int baseY) {
    // Alpine milk churn: domed lid with a knob, two side handles, belly
    // bands, dither shading down one side.
    int yT = baseY - 26, yB = baseY - 4;
    int xL = cx - 8, xR = cx + 8;
    int lidY = yT + 4;
    // knob + domed lid
    GFX_FILL(cx - 2, yT, 4, 4, kColorBlack);
    GFX_ELLIPSE(cx - 9, lidY - 3, 18, 8, kColorWhite);
    pd->graphics->drawEllipse(cx - 9, lidY - 3, 18, 8, 2, 0, 360, kColorBlack);
    // handles poke out from under the lid
    GFX_LINE(xL - 3, lidY + 4, xL - 3, lidY + 9, 2, kColorBlack);
    GFX_LINE(xR + 2, lidY + 4, xR + 2, lidY + 9, 2, kColorBlack);
    // body, slightly tapered toward the top
    pd->graphics->fillTriangle(xL - 1, yB, xL + 1, lidY + 2, xR - 1, lidY + 2,
                               kColorWhite);
    pd->graphics->fillTriangle(xL - 1, yB, xR - 1, lidY + 2, xR + 1, yB,
                               kColorWhite);
    GFX_LINE(xL + 1, lidY + 2, xL - 1, yB, 2, kColorBlack);
    GFX_LINE(xR - 1, lidY + 2, xR + 1, yB, 2, kColorBlack);
    GFX_LINE(xL - 1, yB, xR + 1, yB, 2, kColorBlack);
    // shaded side + belly bands
    pd->graphics->fillTriangle(cx + 3, lidY + 4, xR - 2, lidY + 4, xR - 1,
                               yB - 2, (LCDColor)TH_PAT_25);
    GFX_LINE(xL, yB - 6, xR, yB - 6, 1, kColorBlack);
    GFX_LINE(xL + 1, lidY + 8, xR - 1, lidY + 8, 1, kColorBlack);
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
                                  (LCDColor)TH_PAT_50);
        return;
    }
    GFX_ELLIPSE(cx - 11, baseY - 5, 22, 7, (LCDColor)TH_PAT_50);   // shadow

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

// ---------------------------------------------------------------------------
// Top bar — festival scoreboard. Left to right:
//   DAY chip · coin + gold count · hearts (lives) · ten summits (wins).
// Black band with a white pinline; the gold readout strobes on spend/gain.
// ---------------------------------------------------------------------------
void render_topbar(void)
{
    GFX_FILL(0, 0, SCREEN_W, TOPBAR_H, kColorBlack);
    GFX_LINE(0, TOPBAR_H - 1, SCREEN_W, TOPBAR_H - 1, 1, kColorWhite);

    char buf[24];
    LCDFont* f = game.fontBold;

    // DAY chip — white tab, black text, like a festival bib number.
    snprintf(buf, sizeof(buf), "DAY %d", (int)game.turn);
    int dayW = th_text_w(f, buf) + 10;
    GFX_FILL(0, 0, dayW, TOPBAR_H - 2, kColorWhite);
    pd->graphics->fillTriangle(dayW, 0, dayW + 6, 0, dayW, TOPBAR_H - 2,
                               kColorWhite);
    th_text(f, buf, 5, 1, false);

    // Gold — coin glyph + count.
    int gx = dayW + 18;
    th_coin(gx + 4, TOPBAR_H / 2 - 1);
    snprintf(buf, sizeof(buf), "%d", (int)game.shop.gold);
    th_text(f, buf, gx + 13, 1, true);
    int goldEnd = gx + 13 + th_text_w(f, buf);

    // Gold flash: strobe-invert the coin + number when gold just changed.
    if (game.goldFxFrames > 0 && (game.goldFxFrames & 2u) != 0u) {
        pd->graphics->setDrawMode(kDrawModeInverted);
        GFX_FILL(gx - 3, 0, goldEnd - gx + 7, TOPBAR_H - 1, kColorBlack);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // Lives — five small hearts, hollow when spent.
    int hx = goldEnd + 26;
    for (int i = 0; i < STARTING_LIVES; i++)
        th_heart(hx + i * 16, TOPBAR_H / 2 - 1, i < (int)game.lives, true);

    // Wins — ten summits to climb, right-aligned.
    th_peak_meter(SCREEN_W - 48, TOPBAR_H - 3, (int)game.wins,
                  WINS_TO_VICTORY, 8, true);
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

// ---------------------------------------------------------------------------
// Pet info card — overlay shown when the cursor is on a real pet. Anchored
// opposite the hovered row so the row being inspected stays visible.
// ---------------------------------------------------------------------------
// Wrapped 2-line description writer shared by both info cards.
static void card_desc_lines(const char* desc, int x, int topY, int maxW) {
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    int len = (int)strlen(desc);
    int from = 0, lineY = topY, lineH = 12, drawn = 0;
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
        pd->graphics->drawText(desc + from, to - from, kASCIIEncoding, x, lineY);
        from = to;
        while (from < len && desc[from] == ' ') from++;
        lineY += lineH;
        drawn++;
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

static void render_pet_info_card(uint8_t petId, int cursorColX, bool isShopHover,
                                 int atk, int hp, int level, const char* badge)
{
    const PetDef* def = pet_def(petId);
    if (!def) return;

    int cw = 260, ch = 72;
    int cy = isShopHover ? 124 : 22;
    int cx = cursorColX - cw / 2;
    if (cx < 4)                 cx = 4;
    if (cx + cw > SCREEN_W - 4) cx = SCREEN_W - 4 - cw;

    th_panel(cx, cy, cw, ch);

    int sx = cx + 9, sy = cy + 9;
    GFX_RECT(sx - 1, sy - 1, 34, 34, kColorBlack);
    draw_creature(petId, sx, sy);

    th_text(game.fontBold, def->name, cx + 48, cy + 7, false);

    // Stat strip: attack disc, heart, tier chip, level pips — icons over text.
    int ly = cy + 31;
    char num[8];
    GFX_ELLIPSE(cx + 48, ly - 5, 11, 11, kColorBlack);          // atk disc
    snprintf(num, sizeof(num), "%d", atk);
    th_text(game.fontSmall, num, cx + 62, ly - 7, false);
    int hx2 = cx + 62 + th_text_w(game.fontSmall, num) + 14;
    th_heart(hx2, ly, true, false);                              // hp heart
    snprintf(num, sizeof(num), "%d", hp);
    th_text(game.fontSmall, num, hx2 + 10, ly - 7, false);
    int tx2 = hx2 + 10 + th_text_w(game.fontSmall, num) + 14;
    snprintf(num, sizeof(num), "T%d", (int)def->tier);          // tier chip
    th_text(game.fontSmall, num, tx2, ly - 7, false);
    if (level > 1) {                                             // level pips
        int px2 = tx2 + th_text_w(game.fontSmall, num) + 12;
        for (int i = 0; i < level && i < MAX_PET_LEVEL; i++) {
            GFX_ELLIPSE(px2 + i * 8, ly - 4, 6, 6, kColorBlack);
        }
    }

    // Combine telegraph chip in the top-right corner (e.g. "-> L2", "+XP",
    // "FULL") — inverted so it pops off the white card. Caller passes NULL
    // when there's nothing to say (team-slot hovers, plain new buys).
    if (badge && badge[0]) {
        int bw = th_text_w(game.fontSmall, badge);
        int bx = cx + cw - bw - 14, bcy = cy + 8;
        GFX_FILL(bx - 4, bcy - 2, bw + 8, 15, kColorBlack);
        th_text(game.fontSmall, badge, bx, bcy, true);
    }

    // Ability description — pick the leveled string (level is 1..MAX_PET_LEVEL).
    int dlvl = level - 1;
    if (dlvl < 0) dlvl = 0;
    if (dlvl >= MAX_PET_LEVEL) dlvl = MAX_PET_LEVEL - 1;
    const char* desc = def->desc[dlvl] ? def->desc[dlvl] : "";
    card_desc_lines(desc, cx + 9, cy + 44, cw - 18);
}

// Same plate as the pet card, retooled for foods — mini-icon, name,
// coin-glyph cost, two-line description.
static void render_food_info_card(uint8_t foodId, int cursorColX)
{
    if (foodId >= FOOD_COUNT) return;
    const FoodDef* d = &FOODS[foodId];

    int cw = 260, ch = 72;
    int cy = 124;                                   // overlay the team row
    int cx = cursorColX - cw / 2;
    if (cx < 4)                 cx = 4;
    if (cx + cw > SCREEN_W - 4) cx = SCREEN_W - 4 - cw;

    th_panel(cx, cy, cw, ch);

    // Mini icon in the sprite cell.
    int sx = cx + 9, sy = cy + 9;
    GFX_RECT(sx - 1, sy - 1, 34, 34, kColorBlack);
    render_food(foodId, sx + 16, sy + 32, false, false);

    th_text(game.fontBold, d->name, cx + 48, cy + 7, false);

    // Cost: coin glyph + number.
    char costBuf[8];
    snprintf(costBuf, sizeof(costBuf), "%d", (int)d->cost);
    th_coin(cx + 53, cy + 31);
    th_text(game.fontSmall, costBuf, cx + 62, cy + 24, false);

    card_desc_lines(d->description ? d->description : "",
                    cx + 9, cy + 44, cw - 18);
}

// ---------------------------------------------------------------------------
// Shop screen — three pet slots on the left, two food slots on the right.
// ---------------------------------------------------------------------------
// Market-stall awning sheltering the shop pets — striped canopy with a
// scalloped valance and two posts. Pure scene dressing, drawn behind units.
static void shop_awning(int x0, int x1, int yTop, int yBot, int postFootY) {
    // posts
    GFX_FILL(x0 + 2, yBot, 3, postFootY - yBot, kColorBlack);
    GFX_FILL(x1 - 5, yBot, 3, postFootY - yBot, kColorBlack);
    // striped canopy
    int stripe = 16, k = 0;
    for (int x = x0; x < x1; x += stripe, k++) {
        int w = mini(stripe, x1 - x);
        if (k & 1) GFX_FILL(x, yTop, w, yBot - yTop, kColorBlack);
        else       GFX_FILL(x, yTop, w, yBot - yTop, kColorWhite);
        // scalloped valance, colour-matched per stripe
        GFX_ELLIPSE(x, yBot - 5, w, 10, (k & 1) ? kColorBlack : kColorWhite);
        pd->graphics->drawEllipse(x, yBot - 5, w, 10, 1, 90.0f, 270.0f,
                                  kColorBlack);
    }
    GFX_LINE(x0, yTop, x1, yTop, 2, kColorBlack);
    GFX_LINE(x0, yTop + 1, x0, yBot, 1, kColorBlack);
    GFX_LINE(x1, yTop + 1, x1, yBot, 1, kColorBlack);
}

// Low wooden fence behind the herd row.
static void shop_fence(int x0, int x1, int baseY) {
    GFX_LINE(x0, baseY - 24, x1, baseY - 24, 2, kColorBlack);   // rails
    GFX_LINE(x0, baseY - 14, x1, baseY - 14, 2, kColorBlack);
    for (int x = x0; x <= x1; x += 56) {                        // posts
        GFX_FILL(x - 1, baseY - 29, 4, 26, kColorBlack);
        pd->graphics->fillTriangle(x - 2, baseY - 29, x + 3, baseY - 29,
                                   x + 1, baseY - 32, kColorBlack);
    }
}

// A little signpost planted by the pasture, its arrow plank pointing at the
// herd row — same carved-wood language as the title menu's signposts.
static void shop_herd_sign(void) {
    GFX_FILL(376, 150, 6, 44, kColorBlack);                      // post
    th_sign_plank(286, 396, 158, "YOUR HERD", false, true);
}

// Food-stall counter — a plank table the two shop foods stand on.
static void shop_table(int x0, int x1, int topY) {
    GFX_FILL(x0 + 6, topY + 5, 4, 19, kColorBlack);             // legs
    GFX_FILL(x1 - 10, topY + 5, 4, 19, kColorBlack);
    GFX_FILL(x0, topY, x1 - x0, 6, kColorWhite);                // plank
    GFX_RECT(x0, topY, x1 - x0, 6, kColorBlack);
    GFX_LINE(x0 + 8, topY + 3, x1 - 8, topY + 3, 1, kColorBlack);
}

void render_shop_screen(void)
{
    th_backdrop(58, TH_BG_CLOUDS);
    render_topbar();

    const float SC = 1.5f;
    const int* shopColX = SHOP_COL_X;       // aliases for compactness
    const int* colX     = TEAM_COL_X;
    const int* foodX    = FOOD_COL_X;
    int shopFeet = 110;
    int teamFeet = 182;

    // --- Scene dressing: market awning over the pets for sale, a counter
    //     under the foods, and a fence behind your herd. The split tells the
    //     story at a glance — stall row above, your pasture below.
    shop_awning(shopColX[0] - 30, shopColX[2] + 30, 22, 40, shopFeet);
    shop_table(foodX[0] - 26, foodX[1] + 26, shopFeet - 3);
    shop_fence(colX[0] - 26, colX[TEAM_SIZE - 1] + 30, teamFeet);
    shop_herd_sign();

    // --- Pasture turf strips under the live rows.
    th_fill(shopColX[0] - 26, shopFeet, shopColX[2] - shopColX[0] + 52, 11, TH_PAT_12);
    th_fill(colX[0] - 26, teamFeet, colX[TEAM_SIZE-1] - colX[0] + 52, 11, TH_PAT_12);

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

        render_unit(pid, 0, a, h, EMPTY_FOOD, shopColX[i], shopFeet, petSC,
                    false, false, false);

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
        bool frz = (game.shop.frozenFoods & (1 << i)) != 0;
        render_food(game.shop.foods[i], foodX[i], shopFeet, false, frz);
        if (game.shop.foods[i] != EMPTY_FOOD) {
            char cb[8];
            snprintf(cb, sizeof(cb), "%dg", FOODS[game.shop.foods[i]].cost);
            if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
            pd->graphics->setDrawMode(kDrawModeFillBlack);
            int w = pd->graphics->getTextWidth(game.fontSmall, cb, strlen(cb),
                                               kASCIIEncoding, 0);
            pd->graphics->drawText(cb, strlen(cb), kASCIIEncoding,
                                   foodX[i] - w / 2, shopFeet + 6);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // --- Team row ---
    for (int i = 0; i < TEAM_SIZE; i++) {
        bool picked = (game.selectedTeamSlot == i);
        PetSlot* p = &game.player.slots[i];
        render_unit(p->id, p->level, p->atk, p->hp, p->food,
                    colX[i], teamFeet, SC, false, picked, false);
    }

    // --- Gliding cursor ring: one pulsing ellipse that eases between slot
    //     positions instead of teleporting — the eye follows it for free.
    //     Hidden on the action row (the planks fill in as selection there).
    {
        static float gx = -1.0f, gy = -1.0f, gw = 50.0f;
        uint8_t cc = game.cursor;
        float tx, ty, tw;
        if (cc < MAX_SHOP_PETS) {
            tx = (float)shopColX[cc]; ty = (float)shopFeet; tw = 52.0f;
        } else if (cc < MAX_SHOP_PETS + MAX_SHOP_FOOD) {
            tx = (float)foodX[cc - MAX_SHOP_PETS]; ty = (float)shopFeet;
            tw = 38.0f;
        } else if (cc < MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE) {
            tx = (float)colX[cc - MAX_SHOP_PETS - MAX_SHOP_FOOD];
            ty = (float)teamFeet; tw = 52.0f;
        } else {
            // actions row: park under the matching plank, ring not drawn
            tx = (cc == MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE)
                 ? 56.0f : 344.0f;
            ty = 212.0f; tw = 50.0f;
        }
        if (gx < 0.0f || !game.cursorShown) { gx = tx; gy = ty; gw = tw; }
        gx += (tx - gx) * 0.45f;
        gy += (ty - gy) * 0.45f;
        gw += (tw - gw) * 0.45f;
        bool onActions = cc >= MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
        if (game.cursorShown && !onActions) {
            // Bobbing pointer above the slot — hovers clear of the sprite,
            // level pips and stat badges instead of ringing through them.
            // gw doubles as a height proxy (pets are taller than foods).
            int bob = (int)(sinf((float)game.frameCount * 0.22f) * 3.0f);
            int cxp = (int)gx;
            int tip = (int)gy - (int)gw - 14 + bob;
            pd->graphics->fillTriangle(cxp - 9, tip - 14, cxp + 9, tip - 14,
                                       cxp, tip + 2, kColorWhite);
            pd->graphics->fillTriangle(cxp - 7, tip - 12, cxp + 7, tip - 12,
                                       cxp, tip, kColorBlack);
        }
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

    // --- Action bar: carved plank buttons ---
    int rollIdx = MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE;
    int endIdx  = rollIdx + 1;
    int ay = 196;
    th_plank_button(8,   ay, 96, 24, "ROLL 1g",
                    game.cursorShown && game.cursor == rollIdx);
    th_plank_button(296, ay, 96, 24, "BATTLE!",
                    game.cursorShown && game.cursor == endIdx);

    // --- Hint bar + combine telegraph ---
    // For a hovered shop pet, work out what A would do (combine into an
    // existing copy, fill an empty slot, or nothing when the team is full)
    // so the hint and the hover card can both say it plainly.
    const char* hintA = NULL, *hintB = NULL, *hintX = NULL;
    uint8_t c = game.cursor;
    char badgeBuf[12]; badgeBuf[0] = '\0';     // combine chip for the hover card
    CombinePreview cp = { -1, false, false, 1, 0, 0 };
    bool hoverShopPet = game.cursorShown && c < MAX_SHOP_PETS &&
                        game.pendingFoodSlot == 0xFF &&
                        game.shop.pets[c] != EMPTY_PET;
    if (hoverShopPet) {
        cp = shop_combine_preview(game.shop.pets[c]);
        if (cp.dstSlot < 0) {
            hintB = "freeze";
            hintX = "team full - sell to make room";
            snprintf(badgeBuf, sizeof badgeBuf, "FULL");
        } else if (cp.isCombine && cp.levelsUp) {
            hintA = (cp.resultLevel >= 3) ? "combine to Level 3!"
                                          : "combine to Level 2!";
            hintB = "freeze";
            snprintf(badgeBuf, sizeof badgeBuf, "-> L%d", (int)cp.resultLevel);
        } else if (cp.isCombine) {
            hintA = "combine (+1 xp)";
            hintB = "freeze";
            snprintf(badgeBuf, sizeof badgeBuf, "+XP");
        } else {
            hintA = "buy";
            hintB = "freeze";
        }
    }

    if (!game.cursorShown) { hintX = "press the d-pad to browse the market"; }
    else if (game.pendingFoodSlot != 0xFF) { hintA = "feed"; hintB = "cancel"; }
    else if (c < MAX_SHOP_PETS) { /* set above (empty slot -> nothing) */ }
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD) { hintA = "buy"; hintB = "freeze"; }
    else if (c < MAX_SHOP_PETS + MAX_SHOP_FOOD + TEAM_SIZE) {
        hintA = "grab + crank to move"; hintB = "sell";
    }
    else if (c == rollIdx) { hintA = "reroll the market"; }
    else { hintA = "march to battle"; }
    // Confirm modal owns the hint bar while it's up.
    if (game.confirmKind == CONFIRM_SELL) {
        hintA = "sell"; hintB = "keep"; hintX = NULL;
    } else if (game.confirmKind == CONFIRM_BATTLE) {
        hintA = "battle anyway"; hintB = "back to the stalls"; hintX = NULL;
    }
    th_hint_bar(hintA, hintB, hintX);

    // --- Destination pointer: a down-chevron over the team column this buy
    //     lands in, sitting in the gap just above the hover card (which would
    //     otherwise hide the team row). Level-ups also stamp the new level. ---
    if (hoverShopPet && cp.dstSlot >= 0) {
        int px = colX[cp.dstSlot], py = 116;
        bool lvl = cp.isCombine && cp.levelsUp;
        pd->graphics->fillTriangle(px - 6, py - 6, px + 6, py - 6,
                                   px, py, kColorBlack);
        if (lvl) {
            char lb[6];
            snprintf(lb, sizeof lb, "L%d", (int)cp.resultLevel);
            if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
            int lw = pd->graphics->getTextWidth(game.fontSmall, lb, strlen(lb),
                                                kASCIIEncoding, 0);
            GFX_FILL(px - lw / 2 - 3, py - 21, lw + 6, 14, kColorBlack);
            pd->graphics->setDrawMode(kDrawModeFillWhite);
            pd->graphics->drawText(lb, strlen(lb), kASCIIEncoding,
                                   px - lw / 2, py - 20);
            pd->graphics->setDrawMode(kDrawModeCopy);
        }
    }

    // --- Hover info card (only when the cursor is awake; the confirm modal
    //     suppresses it so the two panels don't stack) ---
    if (game.cursorShown && game.confirmKind == CONFIRM_NONE) {
        if (c < MAX_SHOP_PETS) {
            uint8_t pid = game.shop.pets[c];
            if (pid != EMPTY_PET) {
                const PetDef* d = pet_def(pid);
                render_pet_info_card(pid, shopColX[c], true,
                                     d ? d->baseAtk : 0, d ? d->baseHp : 0, 1,
                                     badgeBuf[0] ? badgeBuf : NULL);
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
                                     (int)p->atk, (int)p->hp, (int)p->level, NULL);
            }
        }
    }

    // --- Floating shop popups (gold deltas, level-up sparkles) ---
    shop_popup_drain();

    // --- Confirm modal (sell / battle-with-gold) — drawn over everything ---
    if (game.confirmKind != CONFIRM_NONE) {
        int mx = 100, my = 88, mw = 200, mh = 64;
        th_panel_dark(mx, my, mw, mh);
        char l1[48], l2[48];
        l1[0] = l2[0] = '\0';
        if (game.confirmKind == CONFIRM_SELL) {
            PetSlot* p = &game.player.slots[game.confirmSlot];
            if (p->id != EMPTY_PET) {
                snprintf(l1, sizeof l1, "Sell %s (L%d)?",
                         pet_name(p->id), (int)p->level);
                snprintf(l2, sizeof l2, "returns %dg to the purse",
                         shop_sell_refund(game.confirmSlot));
            }
        } else {
            snprintf(l1, sizeof l1, "%dg still in the purse!",
                     (int)game.shop.gold);
            snprintf(l2, sizeof l2, "the stalls stay open...");
        }
        th_text_center(game.fontBold, l1, mx + mw / 2, my + 13, true);
        th_text_center(game.fontSmall, l2, mx + mw / 2, my + 35, true);
    }

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

#define BATTLE_FEET   178
#define BATTLE_CX     (SCREEN_W / 2)

// Depth-staggered rest layout: the queue recedes up the pasture and shrinks
// with distance, so all five ranks per side stay on the 400px screen — a
// snipe at the back rank lands on a pet you can actually see. Rank 0 is the
// frontline; it keeps the clear centre gap so the clash lunge reads.
static const int   RANK_XOFF[TEAM_SIZE]  = { 46, 92, 130, 158, 180 };
static const int   RANK_FEET[TEAM_SIZE]  = { 178, 175, 169, 163, 158 };
static const float RANK_SCALE[TEAM_SIZE] = { 1.40f, 1.28f, 1.15f, 1.03f,
                                             0.92f };

// Eased on-screen X / feet-Y / scale per unit, so a survivor slides forward
// AND grows as it advances toward the frontline. Sentinel -1 means "snap on
// next frame" — set on battle start and after a FAINT so re-occupied slots
// don't drift from a stale off-screen flyoff x.
static float g_unitX[2][TEAM_SIZE];
static float g_unitY[2][TEAM_SIZE];
static float g_unitS[2][TEAM_SIZE];

void render_battle_reset_positions(void) {
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < TEAM_SIZE; i++) {
            g_unitX[s][i] = -1.0f;
            g_unitY[s][i] = -1.0f;
            g_unitS[s][i] = -1.0f;
        }
}

void render_battle_reset_slot_position(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE) return;
    g_unitX[side][slot] = -1.0f;
    g_unitY[side][slot] = -1.0f;
    g_unitS[side][slot] = -1.0f;
}

// Rest position for a unit at a given rank (0 = frontline).
static int unit_target_x(int side, int rank) {
    if (rank >= TEAM_SIZE) rank = TEAM_SIZE - 1;
    return (side == 0) ? BATTLE_CX - RANK_XOFF[rank]
                       : BATTLE_CX + RANK_XOFF[rank];
}

// Rank a slot currently holds: count the live pets in front of it.
static int battle_slot_rank(int side, int slot) {
    int rank = 0;
    for (int i = TEAM_SIZE - 1; i > slot; i--)
        if (game.battle.teams[side].slots[i].id != EMPTY_PET) rank++;
    if (rank >= TEAM_SIZE) rank = TEAM_SIZE - 1;
    return rank;
}

// On-screen X for a unit, robust against the -1 "needs snap" sentinel that
// g_unitX holds before a slot has first eased in. The easing loop in
// render_battle_screen normally positions every drawn unit before the
// projectile spawner runs, but battle-start abilities (Alpaca/Bernard SOB
// snipes) fire at playhead 0, and the render pipeline could be reordered —
// so the spawner reads through here and never launches from / aims at x=-1.
// Fallback derives the slot's rank (front = higher index) the same way the
// easing loop does and returns its rest position.
static int battle_slot_x(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE) return BATTLE_CX;
    float x = g_unitX[side][slot];
    if (x >= 0.0f && x < 2000.0f) return (int)x;
    return unit_target_x(side, battle_slot_rank(side, slot));
}

// Eased feet-line Y for a unit, same sentinel fallback as battle_slot_x.
static int battle_slot_feet(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE)
        return BATTLE_FEET;
    float y = g_unitY[side][slot];
    if (y >= 0.0f) return (int)y;
    return RANK_FEET[battle_slot_rank(side, slot)];
}

// Eased sprite scale for a unit, same sentinel fallback as battle_slot_x.
static float battle_slot_scale(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE)
        return RANK_SCALE[0];
    float s = g_unitS[side][slot];
    if (s > 0.0f) return s;
    return RANK_SCALE[battle_slot_rank(side, slot)];
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
// A BEV_SPAWN at the same slot also counts as evidence the original died —
// for Sheep/Hopper/Goose-style spawners, battle_resolve_faints replaces the
// slot before it can emit BEV_FAINT, so without this the sprite would just
// vanish mid-frame and a new pet would shimmer in with no death animation.
static bool clash_kills(int clashIdx, int side, int slot) {
    for (int i = clashIdx + 1; i < game.battleLogLen; i++) {
        BattleEvent* e = &game.battleLog[i];
        if (e->kind == BEV_CLASH || e->kind == BEV_ATTACK) break;
        if (e->kind == BEV_FAINT && e->side == side && e->slot == slot)
            return true;
        if (e->kind == BEV_SPAWN && e->side == side && e->slot == slot)
            return true;
    }
    return false;
}

// Tighter variant: clash kills AND the dying pet has no parting actions
// queued before its FAINT. Only then is it safe to fly the loser off
// during the clash window. If parting actions exist (Goat AoE, Mouse
// death-shot, Edelweiss Bee hatch, an Ox friend-faint buff, etc.) we
// keep the unit visible as a ghost and defer the fly-off to BEV_FAINT,
// so the source of the parting effect stays anchored on-screen.
static bool clash_kills_with_flyoff(int clashIdx, int side, int slot) {
    if (!clash_kills(clashIdx, side, slot)) return false;
    for (int i = clashIdx + 1; i < game.battleLogLen; i++) {
        BattleEvent* e = &game.battleLog[i];
        if (e->kind == BEV_CLASH || e->kind == BEV_ATTACK) return true;
        if (e->kind == BEV_FAINT && e->side == side && e->slot == slot)
            return true;
        if (e->kind == BEV_SPAWN && e->side == side && e->slot == slot)
            return true;
        // Any animatable event between the clash and our FAINT means
        // there's a parting action to keep us on-screen for.
        if (e->kind == BEV_ABILITY || e->kind == BEV_DAMAGE ||
            e->kind == BEV_BUFF) return false;
    }
    return true;
}

// True when this slot's pet took lethal damage (hp <= 0) but its BEV_FAINT
// is still pending further in the log. During this "ghost phase" we keep
// rendering the dying pet at its rank position — so on-faint rays/
// projectiles/buffs read as coming from the actual source, and the sprite
// never blinks out ahead of its fly-off (it used to whenever the only
// events before its FAINT were other pets' faints, e.g. plain AoE wipes
// or a summon shimmering in elsewhere). Clash-folded faints are excluded:
// that pet already flew off inside the clash window and must stay gone.
static bool slot_in_ghost_phase(int side, int slot) {
    if (side < 0 || side >= 2 || slot < 0 || slot >= TEAM_SIZE) return false;
    PetSlot* p = &game.battle.teams[side].slots[slot];
    if (p->id == EMPTY_PET || p->hp > 0) return false;
    for (int j = (int)game.battlePlayhead; j < game.battleLogLen; j++) {
        BattleEvent* e = &game.battleLog[j];
        if (e->kind == BEV_FAINT && (int)e->side == side &&
            (int)e->slot == slot) {
            return !battle_faint_folded(j);
        }
        // SPAWN replacing this slot ends ghost phase — the new pet
        // shimmers in via the BEV_SPAWN branch and takes over the slot.
        if (e->kind == BEV_SPAWN && (int)e->side == side &&
            (int)e->slot == slot) return false;
        if (e->kind == BEV_CLASH || e->kind == BEV_ATTACK) return false;
    }
    return false;
}

// Forward decl — the debris pool lives further down (with the projectiles),
// but the ability-KO fly-off below sheds a directional puff on launch.
static void spawn_debris(int cx, int cy, int dirSign);

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
        if (clash_kills_with_flyoff(game.battlePlayhead, side, slot)) {
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
        // Subtle flinch — the real "you got hit" feel now comes from the
        // debris chunks the projectile spits off the victim on impact.
        // Divide frequency by playback speed so it reads at every crank rate.
        int spd = (int)game.battleSpeed; if (spd < 1) spd = 1;
        return (int)(sinf(t * 38.0f / (float)spd) * 1.0f);
    }
    return 0;
}

// KO launch — fling the unit off-screen with a hard spin and a scale punch.
// Ease-out so it bolts at the moment of impact instead of easing in.
static void draw_unit_flyoff(PetSlot* p, int side, int cx0, int feetY,
                             int spr, float baseSc, float flyT) {
    if (flyT < 0.0f) flyT = 0.0f;
    if (flyT > 1.0f) flyT = 1.0f;
    float eo = 1.0f - (1.0f - flyT) * (1.0f - flyT);
    int backDir = (side == 0) ? -1 : 1;
    int cxK = cx0 + backDir * (int)(eo * 540.0f);
    int cyK = (feetY - spr / 2) - (int)(sinf(flyT * PI_F) * 150.0f)
                                + (int)(eo * 60.0f);
    float deg = eo * 820.0f * (float)backDir;
    float sc  = baseSc *
                (1.0f + 0.45f * (1.0f - flyT) * (1.0f - flyT) - 0.32f * flyT);
    draw_creature_xform(p->id, cxK, cyK, sc, deg, side == 1);
}

// Draw a single battle unit with clash / faint / hit animation.
static void battle_draw_unit(BattleEvent* e, float t, int side, int slot,
                             float fx, float fy, float fs) {
    PetSlot* p = &game.battle.teams[side].slots[slot];

    int spr   = (int)(32.0f * fs);
    int restX = (int)fx;
    int feetY = (int)fy;

    // Ability KO — faints not folded into a clash fly off on the group
    // head's window. Pets that died together are all members of the current
    // faint group, so mutual/AoE deaths launch simultaneously.
    if (e && e->kind == BEV_FAINT &&
        battle_faint_in_group((int)game.battlePlayhead, side, slot)) {
        if (p->id == EMPTY_PET) return;
        // One-time directional debris puff at the launch point, ejecting the
        // way the unit flies off (replaces the old symmetric spark-star).
        // Latched per slot — a shared group window launches several units,
        // each shedding its own puff. Key is playhead+1 so slot 0 at
        // playhead 0 can't collide with the zero-initialised latch.
        static int lastFaintKey[2][TEAM_SIZE];
        int key = (int)game.battlePlayhead + 1;
        if (lastFaintKey[side][slot] != key) {
            int dir = (side == 0) ? -1 : 1;   // matches draw_unit_flyoff backDir
            spawn_debris(restX, feetY - spr / 2, dir);
            lastFaintKey[side][slot] = key;
        }
        draw_unit_flyoff(p, side, restX, feetY, spr, fs, t);
        return;
    }

    // Phase 3.4: Spawn shimmer — render the NEW pet emerging using the
    // event's payload (slot may still hold the dying predecessor until the
    // SPAWN apply at end-of-window).
    if (e && e->kind == BEV_SPAWN && e->side == side && e->slot == slot) {
        uint8_t newPetId = e->data;
        int newAtk = e->value & 0xFF;
        int newHp  = (e->value >> 8) & 0xFF;
        float sc = fs * (0.55f + 0.45f * t);
        int hover = -(int)(sinf(t * PI_F) * 4.0f);
        render_unit(newPetId, 1, (int16_t)newAtk, (int16_t)newHp, EMPTY_FOOD,
                    restX, feetY + hover, sc, false, false, side == 1);
        // Rising "materialize" wisps instead of an expanding star-ring: short
        // vertical strokes climbing past the forming unit, staggered so they
        // ripple upward — reads as 'emerging from the ground', not a burst.
        for (int a = 0; a < 5; a++) {
            int wx = restX - 14 + a * 7;
            float ph = t * 1.25f - (float)a * 0.12f;     // staggered start
            if (ph <= 0.0f || ph >= 1.0f) continue;
            int wy = (feetY + hover) - (int)(ph * 40.0f);
            int len = 3 + (a & 1);
            GFX_LINE(wx, wy, wx, wy - len, 2, kColorBlack);
        }
        return;
    }

    // Ghost phase render — slot took lethal damage from a clash but its
    // parting actions (on-faint ability, AoE damage, Edelweiss hatch, an
    // Ox friend-faint buff) are still playing out. Keep the dying pet
    // visible at its rank position with a flickering silhouette and rising
    // wisps so the rays/projectiles below are clearly anchored to it
    // instead of looking like they emanate from the survivor behind. The
    // proper fly-off plays when BEV_FAINT eventually fires.
    bool selfFaintEv = e && e->kind == BEV_FAINT &&
                       battle_faint_in_group((int)game.battlePlayhead, side, slot);
    if (!selfFaintEv && slot_in_ghost_phase(side, slot)) {
        int bob = (int)(sinf((float)game.frameCount * 0.18f) * 2.0f);
        int gx = restX;
        int gy = feetY + bob;
        // Frame-driven so the flicker rate stays consistent at every
        // crank speed (1×..8×). 6 frames normal / 2 frames inverted ≈
        // 3.75 Hz strobe — readable, not seizure-inducing.
        bool invert = ((int)game.frameCount & 7) >= 6;
        if (invert) pd->graphics->setDrawMode(kDrawModeInverted);
        draw_creature_xform(p->id, gx, gy - spr / 2,
                            fs, 0.0f, side == 1);
        pd->graphics->setDrawMode(kDrawModeCopy);
        // Rising wisps above the head, counter-phase to the body flicker
        // so something is always moving — sells "passing spirit".
        if (!invert) {
            int wy = gy - spr - 4;
            for (int w = -1; w <= 1; w++) {
                int wx = gx + w * 7;
                int len = 4 + (w == 0 ? 2 : 0);
                GFX_LINE(wx, wy, wx, wy - len, 2, kColorBlack);
            }
        }
    }

    // (The ABILITY ray-fan + head sparks used to live here. They've been
    // replaced by the projectile system — the source pet's wind-up is the
    // BEV_ABILITY bob (dy below), and the visible attack is the pebble
    // arcing to each target via projectiles_step_and_draw.)

    // Don't render the unit sprite for empty or dying slots — the ghost
    // render above and the FAINT/SPAWN early-returns handle those visuals.
    if (p->id == EMPTY_PET || p->hp <= 0) return;

    // Lethal clash hit launches the sprite off-screen inside the clash
    // window itself (instead of waiting for a separate FAINT event).
    if (e && e->kind == BEV_CLASH &&
        ((side == 0 && slot == e->slot) || (side == 1 && slot == e->data))) {
        const float contactT = 0.42f;
        if (t >= contactT && clash_kills_with_flyoff(game.battlePlayhead, side, slot)) {
            int dir = (side == 0) ? 1 : -1;
            float flyT = (t - contactT) / (1.0f - contactT);
            draw_unit_flyoff(p, side, restX + dir * 26,
                             feetY, spr, fs, flyT);
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
                restX + dx, feetY + dy, fs,
                false, false, side == 1);

    // Phase 3.3: Stat-number flash on BUFF. Strobes an inverted disc over
    // whichever stat badge changed (low byte = atk delta, high byte = hp).
    if (e && e->kind == BEV_BUFF && e->side == side && e->slot == slot &&
        t < 0.55f) {
        int da = e->value & 0xFF;
        int dh = (e->value >> 8) & 0xFF;
        int half = spr / 2;
        int bo = half - 11;
        int by = feetY + dy - 7;
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

// ---------------------------------------------------------------------------
// Projectile + debris pools. Projectiles arc from the source pet to the
// target on BEV_ABILITY playback (one per upcoming BEV_DAMAGE in the same
// barrage), and on landing spawn debris chunks + a rising damage popup.
// Clash impact also drops chunks off both combatants. The whole pool ages
// at game.battleSpeed so flight and debris scale with crank fast-forward.
// ---------------------------------------------------------------------------
static int g_lastClashImpactPlayhead = -1;

// --- battle screen-shake ---------------------------------------------------
// A brief positional jitter applied (via setDrawOffset) to the battlefield
// layer ONLY — bg/topbar/HUD are drawn outside the offset, so there are no
// edge gaps and the readout stays rock-steady. Armed by weighty hits (lethal
// clash, ability impacts) and decays over a few frames. Suppressed above 3×
// crank so fast-forward stays clean instead of seasick.
static int g_battleShake = 0;

static void battle_shake(int frames) {
    if (game.battleSpeed > 3) return;
    if (frames > 6) frames = 6;
    if (frames > g_battleShake) g_battleShake = frames;
}

void projectiles_reset(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        game.projectiles[i].flightLife = 0;
        game.projectiles[i].popupLife = 0;
    }
    for (int i = 0; i < MAX_DEBRIS; i++) {
        game.debris[i].life = 0;
    }
    game.barrageSpawnedAt = 0xFFFF;
    g_lastClashImpactPlayhead = -1;
    g_battleShake = 0;
}

// Visual-only pseudo-random — never touch the simulation RNG from here, or
// playback would diverge from the simulated log. Frame-derived so the same
// barrage looks identical on every playthrough of a saved log.
static uint32_t fx_hash(uint32_t k) {
    k = (k ^ (k >> 16)) * 0x7feb352du;
    k = (k ^ (k >> 15)) * 0x846ca68bu;
    return k ^ (k >> 16);
}

static void spawn_debris(int cx, int cy, int dirSign) {
    int n = 4 + (int)(fx_hash((uint32_t)game.frameCount ^ (uint32_t)cx) & 1u);
    int placed = 0;
    for (int j = 0; j < MAX_DEBRIS && placed < n; j++) {
        Debris* d = &game.debris[j];
        if (d->life != 0) continue;
        uint32_t h = fx_hash(((uint32_t)game.frameCount << 8) ^
                             (uint32_t)(j * 31 + placed * 7));
        float spdFrac = ((float)((h >> 8) & 0xFFu) / 255.0f);
        float latFrac = ((float)(h & 0xFFu) / 255.0f);
        float vyMag = 2.0f + spdFrac * 1.6f;            // 2.0..3.6 up
        float vxMag = 0.6f + latFrac * 1.8f;            // 0.6..2.4 lateral
        d->x = (float)cx;
        d->y = (float)cy;
        d->vx = vxMag * (float)dirSign;
        d->vy = -vyMag;
        d->age = 0;
        d->life = (uint8_t)(14u + ((h >> 16) & 0xFu));  // 14..29
        d->size = (uint8_t)(2u + ((h >> 20) & 0x1u));   // 2..3
        d->pad = 0;
        placed++;
    }
}

static void spawn_projectile(int sx, int sy, int ex, int ey,
                              uint8_t tgtSide, uint8_t tgtSlot, uint8_t dmg) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* p = &game.projectiles[i];
        if (p->flightLife != 0 || p->popupLife != 0) continue;
        p->sx = (int16_t)sx; p->sy = (int16_t)sy;
        p->ex = (int16_t)ex; p->ey = (int16_t)ey;
        p->age = 0;
        p->flightLife = 18;
        p->popupLife = 22;
        p->side = tgtSide;
        p->slot = tgtSlot;
        p->damage = dmg;
        p->debrisDone = 0;
        return;
    }
}

// Walk forward from the first-in-barrage ABILITY at `abilityIdx`, spawning
// one projectile per BEV_DAMAGE in the same barrage. All share the same
// flight life so they land simultaneously regardless of target count.
static void projectiles_spawn_barrage(int abilityIdx) {
    if (abilityIdx < 0 || abilityIdx >= game.battleLogLen) return;
    BattleEvent* a = &game.battleLog[abilityIdx];
    int srcSide = a->side;
    int srcSlot = a->slot;
    int sx = battle_slot_x(srcSide, srcSlot);
    int sy = battle_slot_feet(srcSide, srcSlot)
             - (int)(32.0f * battle_slot_scale(srcSide, srcSlot)) / 2;
    for (int i = abilityIdx + 1; i < game.battleLogLen; i++) {
        BattleEvent* e = &game.battleLog[i];
        if (e->kind == BEV_DAMAGE) {
            int tgtSide = e->side, tgtSlot = e->slot;
            if (tgtSlot >= TEAM_SIZE) continue;
            int ex = battle_slot_x(tgtSide, tgtSlot);
            int ey = battle_slot_feet(tgtSide, tgtSlot)
                     - (int)(32.0f * battle_slot_scale(tgtSide, tgtSlot)) / 2;
            spawn_projectile(sx, sy, ex, ey,
                             (uint8_t)tgtSide, (uint8_t)tgtSlot,
                             (uint8_t)(e->value & 0xFF));
            continue;
        }
        if (e->kind == BEV_ABILITY && e->side == srcSide && e->slot == srcSlot)
            continue;            // subsequent shot in same barrage
        break;                   // end of barrage
    }
}

static void projectiles_step_and_draw(void) {
    // Spawn exactly once when the playhead enters a first-in-barrage ABILITY.
    if (game.battlePlayhead < game.battleLogLen &&
        game.barrageSpawnedAt != game.battlePlayhead) {
        BattleEvent* e = &game.battleLog[game.battlePlayhead];
        if (e->kind == BEV_ABILITY &&
            ability_first_in_barrage((int)game.battlePlayhead)) {
            projectiles_spawn_barrage((int)game.battlePlayhead);
        }
        game.barrageSpawnedAt = game.battlePlayhead;
    }

    int speed = (int)game.battleSpeed; if (speed < 1) speed = 1;

    // One impact thud per render frame: a multi-target salvo lands on the
    // same frame, so latch by frameCount to avoid machine-gunning the SFX.
    static uint32_t impactSfxFrame = 0xFFFFFFFFu;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* p = &game.projectiles[i];
        if (p->flightLife == 0 && p->popupLife == 0) continue;
        p->age = (int16_t)(p->age + speed);

        if (p->age < p->flightLife) {
            float tt = (float)p->age / (float)p->flightLife;
            int dist = p->ex > p->sx ? p->ex - p->sx : p->sx - p->ex;
            int arcH = clampi(dist / 3, 22, 60);
            int x = p->sx + (int)((float)(p->ex - p->sx) * tt);
            int yL = p->sy + (int)((float)(p->ey - p->sy) * tt);
            int y = yL - (int)(sinf(tt * PI_F) * (float)arcH);
            // Bigger, white-haloed pebble so the shot reads clearly over both
            // the white pasture and the dark sprites it arcs across.
            GFX_ELLIPSE(x - 4, y - 4, 8, 8, kColorWhite);    // halo
            GFX_ELLIPSE(x - 3, y - 3, 6, 6, kColorBlack);    // pebble
            float tp = tt - 0.12f;
            if (tp > 0.0f) {
                int xp = p->sx + (int)((float)(p->ex - p->sx) * tp);
                int yLp = p->sy + (int)((float)(p->ey - p->sy) * tp);
                int yp = yLp - (int)(sinf(tp * PI_F) * (float)arcH);
                GFX_ELLIPSE(xp - 2, yp - 2, 4, 4, kColorBlack);  // trail
            }
        } else {
            if (!p->debrisDone) {
                // Impact debris removed — the shot just lands with a thud +
                // shake. `debrisDone` still latches that one-shot sfx/shake.
                p->debrisDone = 1;
                if (impactSfxFrame != game.frameCount) {
                    sfx_attack();
                    impactSfxFrame = game.frameCount;
                }
                battle_shake(2 + (int)p->damage / 4);  // weight scales w/ dmg
            }
            int landedAge = p->age - p->flightLife;
            if (landedAge >= p->popupLife) {
                p->flightLife = 0;
                p->popupLife = 0;
                continue;
            }
            float tt = (float)landedAge / (float)p->popupLife;
            char buf[8];
            snprintf(buf, sizeof(buf), "-%d", (int)p->damage);
            // Anchor above the target sprite's head so the rising number
            // doesn't overlap the badges; matches the CLASH-popup height.
            int popTop = p->ey - (int)(32.0f * battle_slot_scale(p->side,
                                                                 p->slot)) / 2
                         - 8;
            battle_popup(p->ex, popTop, buf, tt);
        }
    }
}

static void debris_step_and_draw(void) {
    int speed = (int)game.battleSpeed; if (speed < 1) speed = 1;
    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris* d = &game.debris[i];
        if (d->life == 0) continue;
        for (int s = 0; s < speed; s++) {
            d->x += d->vx;
            d->y += d->vy;
            d->vy += 0.35f;                // gravity
            d->vx *= 0.985f;
            d->age++;
            if (d->age >= d->life || d->y > (float)(SCREEN_H + 8)) {
                d->life = 0;
                break;
            }
        }
        if (d->life == 0) continue;
        int sz = d->size;
        if (d->age * 2 > d->life) sz = sz > 1 ? sz - 1 : 1;  // shrink near end
        GFX_ELLIPSE((int)d->x - sz / 2, (int)d->y - sz / 2,
                    sz, sz, kColorBlack);
    }
}

void render_battle_screen(void)
{
    th_backdrop(80, TH_BG_CLOUDS | TH_BG_BIRDS);
    render_topbar();

    // Festival dressing: a swaying pennant garland strung beneath the
    // scoreboard, and a trampled arena ring on the pasture where the two
    // herds meet. Drawn before units, outside the shake offset.
    th_pennant_garland(-8, SCREEN_W + 8, TOPBAR_H + 2, 7);
    pd->graphics->drawEllipse(BATTLE_CX - 185, BATTLE_FEET - 20, 370, 34,
                              2, 0, 360, (LCDColor)TH_PAT_50);
    th_fill(BATTLE_CX - 150, BATTLE_FEET - 12, 300, 18, TH_PAT_12);
    th_fill(BATTLE_CX - 70, BATTLE_FEET - 8, 140, 10, TH_PAT_25);

    // Pasture texture: deterministic hash-scattered grass tufts and dirt
    // specks so the field reads as turf instead of blank paper. Static
    // (position-hashed, not frame-hashed) so nothing shimmers.
    for (int i = 0; i < 10; i++) {
        int gx = (i * 83 + 21) % SCREEN_W;
        int gy = 96 + ((i * 47 + 9) % 96);
        if (gy > BATTLE_FEET + 24) gy = BATTLE_FEET + 24;
        GFX_LINE(gx - 3, gy, gx - 1, gy - 4, 1, kColorBlack);
        GFX_LINE(gx,     gy, gx,     gy - 5, 1, kColorBlack);
        GFX_LINE(gx + 3, gy, gx + 1, gy - 3, 1, kColorBlack);
    }
    for (int i = 0; i < 14; i++) {
        int dx = (i * 131 + 57) % SCREEN_W;
        int dy = 100 + ((i * 61 + 31) % 92);
        GFX_FILL(dx, dy, 2, 1, kColorBlack);
    }

    // Team name plates — player on white, challenger on black, anchored just
    // under the garland. Same draw in pre-battle and battle so nothing pops.
    {
        char nameBuf[40];
        LCDFont* f = game.fontBold;
        team_name_str(game.playerName.adj, game.playerName.noun,
                      nameBuf, sizeof(nameBuf));
        int pw = th_text_w(f, nameBuf) + 14;
        GFX_FILL(0, 38, pw, 18, kColorWhite);
        pd->graphics->fillTriangle(pw, 38, pw + 8, 38, pw, 56, kColorWhite);
        GFX_LINE(0, 56, pw + 8, 56, 1, kColorBlack);
        th_text(f, nameBuf, 6, 39, false);

        team_name_str(game.enemyName.adj, game.enemyName.noun,
                      nameBuf, sizeof(nameBuf));
        int ew = th_text_w(f, nameBuf) + 14;
        GFX_FILL(SCREEN_W - ew, 38, ew, 18, kColorBlack);
        pd->graphics->fillTriangle(SCREEN_W - ew, 38, SCREEN_W - ew - 8, 38,
                                   SCREEN_W - ew, 56, kColorBlack);
        th_text(f, nameBuf, SCREEN_W - ew + 8, 39, true);
    }

    float t = 0.0f;
    BattleEvent* e = battle_cur_event(&t);
    // Pre-battle just shows the static line-up — no event animation/popups.
    if (game.state == STATE_PRE_BATTLE) { e = NULL; t = 0.0f; }

    // --- ease unit positions toward their ranked rest spots ---
    // Dying-but-not-yet-fainted slots are excluded so survivors slide
    // forward immediately. EXCEPT: keep the slot in rank when (a) a
    // BEV_SPAWN will re-occupy it (Hopper → Zombie Hopper, Sheep →
    // Haflingers, Edelweiss → Bee) or (b) the slot is in ghost phase —
    // the dying pet is mid-parting-strike and must stay visible so its
    // on-faint rays/damage anchor to the actual source.
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
                if (!willRespawn && !slot_in_ghost_phase(side, i)) continue;
            }
            int r = (rank >= TEAM_SIZE) ? TEAM_SIZE - 1 : rank;
            float tgt  = (float)unit_target_x(side, r);
            float tgtY = (float)RANK_FEET[r];
            float tgtS = RANK_SCALE[r];
            float cur = g_unitX[side][i];
            if (cur < 1.0f || cur > 2000.0f || fabsf(tgt - cur) > 150.0f) {
                g_unitX[side][i] = tgt;                  // snap (battle start)
                g_unitY[side][i] = tgtY;
                g_unitS[side][i] = tgtS;
            } else {
                g_unitX[side][i] = cur + (tgt - cur) * 0.30f;
                g_unitY[side][i] += (tgtY - g_unitY[side][i]) * 0.30f;
                g_unitS[side][i] += (tgtS - g_unitS[side][i]) * 0.30f;
            }
            rank++;
        }
    }

    // --- battlefield screen-shake: jitter units + FX only (bg/topbar/HUD
    //     were drawn above at offset 0, so no edge gaps). Offset is derived
    //     from the decaying shake timer armed by the weighty hits below; the
    //     arm takes effect next frame, which reads as instant.
    int shx = 0, shy = 0;
    if (g_battleShake > 0) {
        int mag = g_battleShake; if (mag > 3) mag = 3;
        shx = (g_battleShake & 1) ? mag : -mag;
        shy = ((g_battleShake >> 1) & 1) ? (mag - 1) : -(mag - 1);
        g_battleShake--;
    }
    pd->graphics->setDrawOffset(shx, shy);

    // --- draw units, back rows first so the frontline overlaps ---
    // (low slot index = back rank, so ascending order is back-to-front)
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < TEAM_SIZE; i++)
            if (game.battle.teams[side].slots[i].id != EMPTY_PET)
                battle_draw_unit(e, t, side, i, g_unitX[side][i],
                                 battle_slot_feet(side, i),
                                 battle_slot_scale(side, i));

    // --- clash impact: spawn debris off both combatants ONCE at contact ---
    // The dying pet (if any) flies off via clash_kills_with_flyoff; the
    // survivor recoils. Either way, on the contact frame each combatant
    // sheds 4–5 chunks ejecting upward+outward away from the impact line.
    if (e && e->kind == BEV_CLASH && t >= 0.42f &&
        g_lastClashImpactPlayhead != (int)game.battlePlayhead) {
        int pSlot = (int)e->slot, eSlot = (int)e->data;
        if (pSlot < TEAM_SIZE && eSlot < TEAM_SIZE) {
            // Clash impact debris removed — the thud + recoil + (on a KO)
            // screen-shake carry the contact now.
            g_lastClashImpactPlayhead = (int)game.battlePlayhead;
            sfx_attack();                 // melee contact thud
            // A KO at the front line lands with weight; a survived trade
            // doesn't shake (the thud + debris already read the contact).
            if (clash_kills((int)game.battlePlayhead, 0, pSlot) ||
                clash_kills((int)game.battlePlayhead, 1, eSlot))
                battle_shake(5);
        }
    }

    // --- combat-text popups ---
    // CLASH numbers ride the sprite's per-event dx so they don't get left
    // at the rest position. ABILITY-driven damages are popped by their
    // projectile (see projectiles_step_and_draw). BUFF popups stay here.
    // CLASH popups always concern the two frontline pets (rank 0).
    int popTop = RANK_FEET[0] - (int)(32.0f * RANK_SCALE[0]) - 14;
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
    } else if (e && e->kind == BEV_BUFF && e->slot < TEAM_SIZE) {
        int da = e->value & 0xFF, dh = (e->value >> 8) & 0xFF;
        char s[12]; s[0] = 0;
        if (da && dh) snprintf(s, sizeof(s), "+%d/%d", da, dh);
        else if (da) snprintf(s, sizeof(s), "+%d", da);
        else if (dh) snprintf(s, sizeof(s), "+%d", dh);
        if (s[0]) {
            int px = (int)g_unitX[e->side][e->slot]
                   + event_unit_dx(e, t, (int)e->side, (int)e->slot);
            battle_popup(px, popTop, s, t);
        }
    }

    // --- projectile + debris pools ---
    // Stepped + drawn after units so they read as foreground. Projectiles
    // also pop their own damage numbers when they land.
    projectiles_step_and_draw();
    debris_step_and_draw();

    // Clear the shake offset so the HUD (speed readout, hint) and anything
    // drawn after render_battle_screen (the pre-battle intro overlay) sit
    // rock-steady, and battle never leaks an offset into other screens.
    pd->graphics->setDrawOffset(0, 0);

    // --- crank speed gauge: 8 ascending bars + "Nx" label, bottom-right.
    //     Only shown above 1× so a normal-speed battle stays clean.
    if (game.battleSpeed > 1) {
        int gx = SCREEN_W - 70, gy = SCREEN_H - 6;
        for (int i = 0; i < 8; i++) {
            int bh2 = 3 + i * 2;
            int bx2 = gx + i * 6;
            if (i < (int)game.battleSpeed)
                GFX_FILL(bx2, gy - bh2, 4, bh2, kColorBlack);
            else
                GFX_RECT(bx2, gy - bh2, 4, bh2, kColorBlack);
        }
        char sb[8];
        snprintf(sb, sizeof(sb), "%dx", (int)game.battleSpeed);
        th_text_halo(game.fontBold, sb, gx - 26, gy - 18);
    }

    if (game.state == STATE_BATTLE)
        th_text_halo_center(game.fontSmall, "crank or hold A to fast-forward",
                            SCREEN_W / 2, SCREEN_H - 15);
    render_debug_overlay();
}
