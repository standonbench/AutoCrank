#include "game.h"

// ===========================================================================
// Procedural farm-creature sprites — 1-bit line-art, drawn into a 32x32 box.
// Swiss Alpine theme. Solid-silhouette style: bold black shapes with white
// detail cut in. Placeholder art — readable and distinct, swap-able later.
// ===========================================================================

#define B kColorBlack
#define W kColorWhite

static void ell(int x, int y, int w, int h, LCDColor c) {
    pd->graphics->fillEllipse(x, y, w, h, 0, 360, c);
}
static void tri(int x1,int y1,int x2,int y2,int x3,int y3,LCDColor c) {
    pd->graphics->fillTriangle(x1,y1,x2,y2,x3,y3,c);
}

// --- horn styles ---
enum { HORN_NONE, HORN_COW, HORN_GOAT, HORN_IBEX, HORN_BULL };
// --- ear styles ---
enum { EAR_ROUND, EAR_POINTY, EAR_FLOPPY, EAR_MANE };
// --- coat styles ---
enum { COAT_PLAIN, COAT_SPOTS, COAT_WOOLLY, COAT_SHAGGY };

// ---------------------------------------------------------------------------
// Quadruped — the workhorse. Side profile facing right.
//   size  : 0 small, 1 medium, 2 large
//   horns : HORN_*    ears: EAR_*    coat: COAT_*
//   snoutLong: longer muzzle (dog/horse)   tusks: boar tusks
//   dark  : solid-black body (dark breeds)
// ---------------------------------------------------------------------------
static void quadruped(int bx, int by, int size, int horns, int ears,
                      int coat, int snoutLong, int tusks, int dark)
{
    (void)dark;
    int bw = (size == 0) ? 17 : (size == 1) ? 21 : 24;   // body width
    int bh = (size == 0) ? 11 : (size == 1) ? 13 : 15;   // body height
    int legL = (size == 0) ? 8 : (size == 1) ? 10 : 11;  // leg length
    int hd  = (size == 0) ? 10 : (size == 1) ? 12 : 13;  // head diameter

    int bodyX = bx + 2;
    int bodyY = by + (size == 2 ? 6 : 8);
    int bellyY = bodyY + bh;
    int legTop = bellyY - 2;

    // --- Tail ---
    GFX_LINE(bodyX + 2, bodyY + 2, bodyX, bellyY + 2, 2, B);
    ell(bodyX - 2, bellyY + 1, 4, 5, B);

    // --- Legs: two clear pairs ---
    int lw = (size == 0) ? 2 : 3;
    GFX_FILL(bodyX + 3,        legTop, lw, legL, B);
    GFX_FILL(bodyX + 8,        legTop, lw, legL, B);
    GFX_FILL(bodyX + bw - 9,   legTop, lw, legL, B);
    GFX_FILL(bodyX + bw - 5,   legTop, lw, legL, B);

    int headX = bodyX + bw - 6;
    int headY = bodyY + (size == 2 ? 5 : 4);

    // --- Horns (drawn behind the head) ---
    int hx = headX + hd / 2;
    int hy = headY;
    if (horns == HORN_COW) {
        GFX_LINE(hx - 4, hy + 2, hx - 6, hy - 4, 3, B);
        GFX_LINE(hx - 6, hy - 4, hx - 3, hy - 7, 3, B);
        GFX_LINE(hx + 4, hy + 1, hx + 6, hy - 4, 3, B);
        GFX_LINE(hx + 6, hy - 4, hx + 3, hy - 7, 3, B);
    } else if (horns == HORN_BULL) {
        GFX_LINE(hx - 4, hy + 2, hx - 10, hy - 1, 3, B);
        GFX_LINE(hx - 10, hy - 1, hx - 11, hy - 5, 3, B);
        GFX_LINE(hx + 4, hy + 1, hx + 10, hy - 2, 3, B);
        GFX_LINE(hx + 10, hy - 2, hx + 11, hy - 6, 3, B);
    } else if (horns == HORN_GOAT) {
        GFX_LINE(hx - 2, hy, hx - 4, hy - 7, 2, B);
        GFX_LINE(hx + 2, hy, hx + 1, hy - 8, 2, B);
    } else if (horns == HORN_IBEX) {
        // long ribbed horns sweeping back over the body
        for (int i = 0; i < 12; i++) {
            int t = i;
            GFX_LINE(hx - 1 - t, hy - 1 - t, hx - 3 - t, hy - t, 2, B);
        }
        GFX_LINE(hx + 2, hy, hx + 3, hy - 6, 2, B);
    }

    // --- Ears ---
    if (ears == EAR_ROUND) {
        ell(headX - 1, headY + 1, 5, 4, B);
    } else if (ears == EAR_POINTY) {
        tri(headX, headY + 4, headX - 2, headY - 4, headX + 4, headY + 3, B);
    } else if (ears == EAR_FLOPPY) {
        ell(headX - 2, headY + 3, 5, 9, B);
    } else if (ears == EAR_MANE) {
        // a crest running down the neck
        for (int i = 0; i < 7; i++)
            GFX_LINE(headX - 2 + i, headY - 2, headX - 5 + i, headY + 7, 1, B);
    }

    // --- Body ---
    ell(bodyX, bodyY, bw, bh, B);
    if (size == 2) GFX_FILL(bodyX + 4, bodyY - 1, bw - 9, 8, B);  // flat back

    // --- Head + snout ---
    ell(headX, headY, hd, hd, B);
    int snoutW = snoutLong ? 9 : 7;
    int snoutX = headX + hd - 4;
    int snoutY = headY + hd / 2 - 1;
    ell(snoutX, snoutY, snoutW, 7, B);

    // --- White detail: eye + muzzle ---
    ell(headX + hd - 8, headY + 3, 4, 4, W);
    GFX_FILL(headX + hd - 7, headY + 4, 2, 2, B);
    ell(snoutX + 2, snoutY + 2, snoutW - 4, 4, W);
    GFX_FILL(snoutX + 3, snoutY + 3, 1, 2, B);
    GFX_FILL(snoutX + snoutW - 3, snoutY + 3, 1, 2, B);

    // --- Tusks (boar) ---
    if (tusks) {
        GFX_LINE(snoutX + 2, snoutY + 5, snoutX, snoutY + 1, 2, W);
        GFX_LINE(snoutX + snoutW - 3, snoutY + 5, snoutX + snoutW - 1, snoutY + 1, 2, W);
    }

    // --- Coat markings ---
    if (coat == COAT_SPOTS) {
        ell(bodyX + 4, bodyY + 2, 7, 6, W);
        ell(bodyX + bw - 12, bodyY + 5, 6, 5, W);
    } else if (coat == COAT_WOOLLY) {
        for (int i = 0; i < 6; i++)
            pd->graphics->drawEllipse(bodyX + 2 + i * 3, bodyY + 1, 6, 6,
                                      1, 0, 360, W);
    } else if (coat == COAT_SHAGGY) {
        for (int i = 0; i < bw - 4; i += 3)
            GFX_LINE(bodyX + 2 + i, bellyY - 1, bodyX + i, bellyY + 4, 1, B);
    }
}

// ---------------------------------------------------------------------------
// Cow — kept as a thin wrapper so callers/selftests still resolve.
// variant: 0 Holstein, 1 Herens (dark), 2 Highland.
// ---------------------------------------------------------------------------
void draw_cow(int bx, int by, int variant)
{
    if (variant == 2)
        quadruped(bx, by, 2, HORN_IBEX, EAR_ROUND, COAT_SHAGGY, 0, 0, 1);
    else
        quadruped(bx, by, 2, HORN_COW, EAR_ROUND,
                  variant == 0 ? COAT_SPOTS : COAT_PLAIN, 0, 0, variant == 1);
}

// ---------------------------------------------------------------------------
// Bird — hen / goose. longNeck distinguishes the goose.
// ---------------------------------------------------------------------------
static void draw_bird(int bx, int by, int longNeck)
{
    int bodyX = bx + 4, bodyY = by + 13;
    // Legs
    GFX_FILL(bodyX + 6,  bodyY + 9, 2, 7, B);
    GFX_FILL(bodyX + 11, bodyY + 9, 2, 7, B);
    GFX_LINE(bodyX + 4,  bodyY + 16, bodyX + 9,  bodyY + 16, 1, B);
    GFX_LINE(bodyX + 9,  bodyY + 16, bodyX + 14, bodyY + 16, 1, B);
    // Tail
    tri(bodyX, bodyY, bodyX - 4, bodyY - 4, bodyX + 2, bodyY + 8, B);
    // Body
    ell(bodyX, bodyY, 17, 13, B);
    // Neck + head
    int neckTop = longNeck ? by + 1 : by + 5;
    GFX_LINE(bodyX + 13, bodyY + 2, bodyX + 16, neckTop + 4, 4, B);
    ell(bodyX + 12, neckTop, 9, 9, B);
    // Beak
    tri(bodyX + 20, neckTop + 3, bodyX + 27, neckTop + 5,
        bodyX + 20, neckTop + 7, longNeck ? B : B);
    // Comb (hen only)
    if (!longNeck) {
        ell(bodyX + 14, neckTop - 2, 6, 4, B);
    }
    // Eye
    ell(bodyX + 16, neckTop + 2, 3, 3, W);
    GFX_FILL(bodyX + 17, neckTop + 3, 1, 1, B);
    // Wing
    ell(bodyX + 3, bodyY + 1, 9, 7, W);
    pd->graphics->drawEllipse(bodyX + 3, bodyY + 1, 9, 7, 1, 0, 360, B);
}

// ---------------------------------------------------------------------------
// Bug — bee / hopper.
// ---------------------------------------------------------------------------
static void draw_bug(int bx, int by, int hopper)
{
    int cx = bx + 14, cy = by + 16;
    if (hopper) {
        // Grasshopper — long body, big bent hind leg
        ell(cx - 8, cy - 3, 20, 9, B);
        ell(cx + 9, cy - 5, 9, 8, B);                 // head
        // hind leg (bent)
        GFX_LINE(cx - 2, cy + 2, cx - 9, cy - 6, 3, B);
        GFX_LINE(cx - 9, cy - 6, cx - 5, cy + 10, 3, B);
        // front legs
        GFX_LINE(cx + 2, cy + 3, cx + 5, cy + 11, 1, B);
        GFX_LINE(cx + 6, cy + 3, cx + 10, cy + 11, 1, B);
        // antennae
        GFX_LINE(cx + 14, cy - 7, cx + 19, cy - 13, 1, B);
        // eye
        ell(cx + 13, cy - 4, 3, 3, W);
    } else {
        // Bee — round striped body + wings
        ell(cx - 9, cy - 7, 9, 9, W);                 // wing
        ell(cx - 4, cy - 9, 9, 9, W);                 // wing
        pd->graphics->drawEllipse(cx - 9, cy - 7, 9, 9, 1, 0, 360, B);
        pd->graphics->drawEllipse(cx - 4, cy - 9, 9, 9, 1, 0, 360, B);
        ell(cx - 6, cy - 2, 18, 13, B);               // body
        // stripes
        GFX_FILL(cx - 1, cy - 2, 2, 13, W);
        GFX_FILL(cx + 5, cy - 2, 2, 13, W);
        // head
        ell(cx + 9, cy - 1, 8, 8, B);
        ell(cx + 12, cy + 1, 3, 3, W);
        // stinger + antennae
        tri(cx - 6, cy + 4, cx - 11, cy + 5, cx - 6, cy + 8, B);
        GFX_LINE(cx + 14, cy - 2, cx + 18, cy - 7, 1, B);
    }
}

// ---------------------------------------------------------------------------
// Snake — adder. A coiled S-curve with a head.
// ---------------------------------------------------------------------------
static void draw_snake(int bx, int by)
{
    int y = by + 22;
    // lower coil
    pd->graphics->drawEllipse(bx + 3, y - 8, 20, 14, 4, 20, 320, B);
    // upper coil rising to the head
    pd->graphics->drawEllipse(bx + 9, y - 18, 16, 13, 4, 160, 360, B);
    // head
    ell(bx + 20, by + 4, 10, 8, B);
    // tongue
    GFX_LINE(bx + 29, by + 8, bx + 32, by + 7, 1, B);
    GFX_LINE(bx + 29, by + 8, bx + 32, by + 9, 1, B);
    // eye
    ell(bx + 24, by + 6, 3, 3, W);
    GFX_FILL(bx + 25, by + 7, 1, 1, B);
}

// ---------------------------------------------------------------------------
// Dispatcher — maps each pet id to its procedural drawing.
// ---------------------------------------------------------------------------
void draw_creature(uint8_t petId, int bx, int by)
{
    switch (petId) {
        case 0:  quadruped(bx,by, 0, HORN_NONE, EAR_ROUND,  COAT_PLAIN,  0,0,0); break; // Marmot
        case 1:  quadruped(bx,by, 0, HORN_NONE, EAR_FLOPPY, COAT_PLAIN,  1,0,0); break; // Pup
        case 2:  draw_bird(bx,by, 0); break;                                            // Hen
        case 3:  draw_bug(bx,by, 0); break;                                             // Bee
        case 4:  draw_bug(bx,by, 1); break;                                             // Hopper
        case 5:  quadruped(bx,by, 0, HORN_NONE, EAR_ROUND,  COAT_SPOTS,  0,0,0); break; // Calf
        case 6:  quadruped(bx,by, 1, HORN_NONE, EAR_ROUND,  COAT_WOOLLY, 0,0,0); break; // Sheep
        case 7:  quadruped(bx,by, 1, HORN_GOAT, EAR_POINTY, COAT_PLAIN,  0,0,0); break; // Goat
        case 8:  quadruped(bx,by, 1, HORN_NONE, EAR_MANE,   COAT_PLAIN,  1,0,0); break; // Haflinger
        case 9:  quadruped(bx,by, 2, HORN_COW,  EAR_ROUND,  COAT_PLAIN,  0,0,1); break; // Ox
        case 10: draw_snake(bx,by); break;                                              // Adder
        case 11: draw_bird(bx,by, 1); break;                                            // Goose
        case 12: quadruped(bx,by, 2, HORN_NONE, EAR_FLOPPY, COAT_SPOTS,  1,0,0); break; // Bernard
        case 13: quadruped(bx,by, 2, HORN_IBEX, EAR_ROUND,  COAT_SHAGGY, 0,0,1); break; // Highland
        case 14: quadruped(bx,by, 1, HORN_NONE, EAR_POINTY, COAT_SHAGGY, 0,1,1); break; // Boar
        case 15: quadruped(bx,by, 1, HORN_IBEX, EAR_POINTY, COAT_PLAIN,  0,0,0); break; // Ibex
        case 16: quadruped(bx,by, 2, HORN_BULL, EAR_ROUND,  COAT_PLAIN,  0,0,1); break; // Reine
        case 17: quadruped(bx,by, 2, HORN_BULL, EAR_ROUND,  COAT_PLAIN,  0,0,0); break; // Bull
        default: quadruped(bx,by, 1, HORN_COW,  EAR_ROUND,  COAT_SPOTS,  0,0,0); break;
    }
}

// Render a creature into an offscreen 32x32 bitmap and blit it scaled,
// treating white as transparent so it drops cleanly onto any background.
void draw_creature_scaled(uint8_t petId, int x, int y, float scale)
{
    LCDBitmap* bmp = pd->graphics->newBitmap(32, 32, kColorWhite);
    pd->graphics->pushContext(bmp);
    draw_creature(petId, 0, 0);
    pd->graphics->popContext();
    pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
    pd->graphics->drawScaledBitmap(bmp, x, y, scale, scale);
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->freeBitmap(bmp);
}

// Draw a creature scaled AND rotated about its centre (cx, cy).
// Used for the faint tip-over.
void draw_creature_xform(uint8_t petId, int cx, int cy, float scale, float deg)
{
    LCDBitmap* bmp = pd->graphics->newBitmap(32, 32, kColorWhite);
    pd->graphics->pushContext(bmp);
    draw_creature(petId, 0, 0);
    pd->graphics->popContext();
    pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
    pd->graphics->drawRotatedBitmap(bmp, cx, cy, deg, 0.5f, 0.5f, scale, scale);
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->freeBitmap(bmp);
}
