#include "game.h"

// ===========================================================================
// Procedural farm-creature sprites — 1-bit line-art, drawn into a 32x32 box.
// Swiss Alpine theme: cows of several breeds + farm animals.
// Style: black linework on white, dot eyes, no eyebrows (house style).
// ===========================================================================

// fill / outline ellipse helpers
static void ell_fill(int x, int y, int w, int h, LCDColor c) {
    pd->graphics->fillEllipse(x, y, w, h, 0, 360, c);
}
static void ell_line(int x, int y, int w, int h, LCDColor c) {
    pd->graphics->drawEllipse(x, y, w, h, 1, 0, 360, c);
}

// ---------------------------------------------------------------------------
// Cow — side profile facing right. Solid black silhouette (always readable
// in 1-bit) with white details cut in.
// variant: 0 Holstein (white patches), 1 Hérens (dark fighting breed),
//          2 Highland (shaggy + sweeping horns).
// ---------------------------------------------------------------------------
void draw_cow(int bx, int by, int variant)
{
    int B = kColorBlack, W = kColorWhite;

    // --- Tail: hangs down the back, tuft at the end ---
    GFX_LINE(bx + 5, by + 8, bx + 3, by + 21, 2, B);
    ell_fill(bx + 1, by + 20, 4, 5, B);

    // --- Legs: two clear pairs, thin, gap between ---
    GFX_FILL(bx + 6,  by + 18, 3, 12, B);    // back pair
    GFX_FILL(bx + 11, by + 18, 3, 12, B);
    GFX_FILL(bx + 18, by + 18, 3, 12, B);    // front pair
    GFX_FILL(bx + 23, by + 18, 3, 12, B);

    // --- Horns: lyre-shaped — sweep up then curl toward each other ---
    if (variant == 2) {
        // Highland: long horns sweeping wide and level
        GFX_LINE(bx + 22, by + 12, bx + 13, by + 10, 3, B);
        GFX_LINE(bx + 13, by + 10, bx + 10, by + 5,  3, B);
        GFX_LINE(bx + 28, by + 11, bx + 31, by + 9,  3, B);
        GFX_LINE(bx + 31, by + 9,  bx + 31, by + 3,  3, B);
    } else {
        // Left horn — up, then curl inward
        GFX_LINE(bx + 22, by + 12, bx + 20, by + 6, 3, B);
        GFX_LINE(bx + 20, by + 6,  bx + 23, by + 3, 3, B);
        // Right horn — up, then curl inward
        GFX_LINE(bx + 29, by + 11, bx + 31, by + 6, 3, B);
        GFX_LINE(bx + 31, by + 6,  bx + 28, by + 3, 3, B);
    }

    // --- Ear ---
    pd->graphics->fillTriangle(bx + 19, by + 14, bx + 13, by + 12,
                               bx + 20, by + 18, B);

    // --- Body: flat-backed mass (ellipse + rect to straighten the topline) ---
    ell_fill(bx + 3, by + 7, 21, 14, B);
    GFX_FILL(bx + 7, by + 6, 13, 9, B);

    // --- Udder under the rear belly ---
    ell_fill(bx + 8, by + 17, 7, 5, B);

    // --- Head: forward, distinct, slightly low ---
    ell_fill(bx + 20, by + 11, 12, 13, B);
    // --- Muzzle: broad, protruding ---
    ell_fill(bx + 25, by + 16, 7, 8, B);

    // --- White details ---
    ell_fill(bx + 24, by + 14, 4, 4, W);     // eye
    GFX_FILL(bx + 25, by + 15, 2, 2, B);     // pupil
    ell_fill(bx + 26, by + 18, 6, 5, W);     // muzzle highlight
    GFX_FILL(bx + 27, by + 20, 1, 2, B);     // nostrils
    GFX_FILL(bx + 30, by + 20, 1, 2, B);

    if (variant == 0) {
        // Holstein white patches, safely inside the body
        ell_fill(bx + 8,  by + 9,  7, 6, W);
        ell_fill(bx + 14, by + 13, 6, 5, W);
    } else if (variant == 2) {
        // Highland: a white forelock tuft between the horns
        ell_fill(bx + 23, by + 9, 5, 4, W);
        GFX_FILL(bx + 24, by + 8, 1, 3, B);
        GFX_FILL(bx + 26, by + 8, 1, 3, B);
    }
}

// ---------------------------------------------------------------------------
// Dispatcher — maps a pet id to its procedural drawing.
// (Proof-of-concept: every pet draws a cow variant for now.)
// ---------------------------------------------------------------------------
void draw_creature(uint8_t petId, int bx, int by)
{
    draw_cow(bx, by, petId % 3);
}

// Render a creature into an offscreen 32x32 bitmap and blit it scaled.
// Used for large previews while iterating on art.
void draw_creature_scaled(uint8_t petId, int x, int y, int scale)
{
    LCDBitmap* bmp = pd->graphics->newBitmap(32, 32, kColorWhite);
    pd->graphics->pushContext(bmp);
    draw_creature(petId, 0, 0);
    pd->graphics->popContext();
    pd->graphics->drawScaledBitmap(bmp, x, y, (float)scale, (float)scale);
    pd->graphics->freeBitmap(bmp);
}
