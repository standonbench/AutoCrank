#include "game.h"
#include <stdio.h>

// Forward declarations for the shared animation helpers (their definitions
// live below, near the pre-battle intro that first introduced them).
static int   ease_out(int from, int to, int t, int dur);
static float easef_out_back(float p, float overshoot);
static void  draw_banner_card(int x, int y, int w, int h);
static void  draw_banner_card_inverted(int x, int y, int w, int h);
static void  draw_scaled_text(const char* s, LCDFont* font, int cx, int cy,
                              float scale, bool invertColor);
static void  draw_flash_overlay(bool on);

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
void ui_title_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.titleBlinkTimer++;
    if (prs & kButtonUp)
        game.titleCursor = (uint8_t)((game.titleCursor + 3) % 4);
    if (prs & kButtonDown)
        game.titleCursor = (uint8_t)((game.titleCursor + 1) % 4);
    if (prs & kButtonA) {
        switch (game.titleCursor) {
        case 0: ui_team_name_enter(); break;     // PLAY -> name picker -> run
        case 1: ui_hof_enter();        break;     // HALL OF FAME
        case 2: game.state = STATE_GALLERY; break;
        case 3:
            selftest_run();
            game.state = STATE_SELFTEST;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Roster gallery — all 18 creatures in a grid + achievement badges.
// A pet earns the outline badge when first taken to L3; the badge fills
// in once a battle has been won with that pet on the team at L3.
// ---------------------------------------------------------------------------
static void draw_roster_badge(int cx, int cy, bool earned) {
    // White chip with a black ring; inner mark shows the progress tier.
    GFX_ELLIPSE(cx - 4, cy - 4, 9, 9, kColorWhite);
    pd->graphics->drawEllipse(cx - 4, cy - 4, 9, 9, 1, 0, 360, kColorBlack);
    if (earned) {
        // Filled diamond — L3 + at least one win
        pd->graphics->fillTriangle(cx, cy - 3, cx + 3, cy,
                                   cx, cy + 3, kColorBlack);
        pd->graphics->fillTriangle(cx, cy - 3, cx, cy + 3,
                                   cx - 3, cy, kColorBlack);
    } else {
        // Small dot — reached L3 but no win yet
        GFX_ELLIPSE(cx - 1, cy - 1, 3, 3, kColorBlack);
    }
}

void ui_gallery_render(void) {
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorWhite);
    GFX_FILL(0, 0, SCREEN_W, 18, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->drawText("ROSTER  -  18 alpine creatures", 30,
                           kASCIIEncoding, 8, 2);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // 6 columns x 3 rows, each cell 64 wide x 70 tall
    for (int i = 0; i < PET_COUNT; i++) {
        int col = i % 6, row = i / 6;
        int cx = 8 + col * 64;
        int cy = 24 + row * 70;
        GFX_FILL(cx, cy, 60, 50, kColorWhite);
        GFX_RECT(cx, cy, 60, 50, kColorBlack);
        draw_creature((uint8_t)i, cx + 14, cy + 9);
        const char* nm = pet_name((uint8_t)i);
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        int w = pd->graphics->getTextWidth(game.fontSmall, nm, strlen(nm),
                                           kASCIIEncoding, 0);
        pd->graphics->drawText(nm, strlen(nm), kASCIIEncoding,
                               cx + (60 - w) / 2, cy + 54);

        // Achievement badge top-right.
        if (game.petWinsAtMax[i] >= 1) {
            draw_roster_badge(cx + 51, cy + 6, true);
        } else if (game.petMaxLevel[i] >= MAX_PET_LEVEL) {
            draw_roster_badge(cx + 51, cy + 6, false);
        }
    }
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText("B: back", 7, kASCIIEncoding, 8, SCREEN_H - 13);
}

// Draw text horizontally centered on the screen at row y.
static void title_text_centered(const char* s, LCDFont* font, int y) {
    if (font) pd->graphics->setFont(font);
    int w = font ? pd->graphics->getTextWidth(font, s, strlen(s), kASCIIEncoding, 0)
                  : (int)strlen(s) * 6;
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, (SCREEN_W - w) / 2, y);
}

// A small five-petal edelweiss, white petals with a black centre.
static void draw_flower(int cx, int cy, int r) {
    for (int i = 0; i < 5; i++) {
        float a = i * (2.0f * PI_F / 5.0f) - PI_F / 2.0f;
        int px = cx + (int)(cosf(a) * r);
        int py = cy + (int)(sinf(a) * r);
        GFX_ELLIPSE(px - 3, py - 3, 7, 7, kColorWhite);
        pd->graphics->drawEllipse(px - 3, py - 3, 7, 7, 1, 0, 360, kColorBlack);
    }
    GFX_ELLIPSE(cx - 2, cy - 2, 5, 5, kColorBlack);
}

// 50% checker — a soft drop-shadow tone.
static LCDPattern UI_SHADE = {
    0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

// Text with a 1px white halo, drawn at (x, y). Stays legible over the photo.
static void draw_text_halo(const char* s, LCDFont* font, int x, int y) {
    if (font) pd->graphics->setFont(font);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    for (int ox = -1; ox <= 1; ox++)
        for (int oy = -1; oy <= 1; oy++)
            pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x + ox, y + oy);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x, y);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Centred text with a white halo — stays legible over the mountains.
static void draw_text_halo_centered(const char* s, LCDFont* font, int y) {
    if (font) pd->graphics->setFont(font);
    int w = pd->graphics->getTextWidth(font, s, strlen(s), kASCIIEncoding, 0);
    int x = (SCREEN_W - w) / 2;
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    for (int ox = -1; ox <= 1; ox++)
        for (int oy = -1; oy <= 1; oy++)
            pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x + ox, y + oy);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x, y);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// A triangular pennant on a thin pole, rising above the title banner.
static void draw_pennant(int x, int topY, int poleH, bool right) {
    GFX_LINE(x, topY, x, topY + poleH, 2, kColorBlack);
    int d = right ? 1 : -1;
    pd->graphics->fillTriangle(x, topY, x + d * 13, topY + 4,
                               x, topY + 10, kColorBlack);
}

// A black pine with a white halo so it reads over both pasture and forest.
static void draw_pine(int cx, int baseY, int h) {
    int w = (h * 9) / 20;
    int by[3], ay[3], tw[3];
    for (int t = 0; t < 3; t++) {
        tw[t] = (int)(w * (1.0f - 0.30f * t));
        by[t] = baseY - 5 - (int)(h * 0.34f * t);
        ay[t] = by[t] - (int)(h * 0.52f);
    }
    for (int t = 0; t < 3; t++)
        pd->graphics->fillTriangle(cx - tw[t] - 3, by[t] + 3, cx, ay[t] - 4,
                                   cx + tw[t] + 3, by[t] + 3, kColorWhite);
    for (int t = 0; t < 3; t++)
        pd->graphics->fillTriangle(cx - tw[t], by[t], cx, ay[t],
                                   cx + tw[t], by[t], kColorBlack);
    GFX_FILL(cx - 2, baseY - 7, 5, 9, kColorWhite);
    GFX_RECT(cx - 2, baseY - 7, 5, 9, kColorBlack);
}

// A little Alpine chalet — white walls, a snow-capped roof, door & windows.
static void draw_chalet(int x, int baseY) {
    int bw = 48, bh = 30, bx = x, by = baseY - bh;
    GFX_FILL(bx, by, bw, bh, kColorWhite);
    GFX_RECT(bx, by, bw, bh, kColorBlack);
    GFX_RECT(bx + 1, by + 1, bw - 2, bh - 2, kColorBlack);
    // roof — wide overhanging triangle
    int apexY = by - 20, rcx = bx + bw / 2;
    pd->graphics->fillTriangle(bx - 9, by + 2, rcx, apexY,
                               bx + bw + 9, by + 2, kColorBlack);
    GFX_LINE(bx - 9, by + 2, bx + bw + 9, by + 2, 3, kColorBlack);
    // ridge snowcap
    pd->graphics->fillTriangle(rcx, apexY, rcx - 8, apexY + 11,
                               rcx + 8, apexY + 11, kColorWhite);
    GFX_LINE(rcx, apexY, rcx - 8, apexY + 11, 1, kColorBlack);
    GFX_LINE(rcx, apexY, rcx + 8, apexY + 11, 1, kColorBlack);
    // door
    int dw = 12, dh = 14, dx = bx + bw / 2 - dw / 2;
    GFX_FILL(dx, baseY - dh, dw, dh, kColorBlack);
    GFX_ELLIPSE(dx + dw - 4, baseY - dh / 2 - 1, 3, 3, kColorWhite);
    // two windows with cross mullions
    for (int i = 0; i < 2; i++) {
        int wx = bx + 6 + i * (bw - 24), wy = by + 6;
        GFX_FILL(wx, wy, 11, 11, kColorWhite);
        GFX_RECT(wx, wy, 11, 11, kColorBlack);
        GFX_LINE(wx + 5, wy, wx + 5, wy + 10, 1, kColorBlack);
        GFX_LINE(wx, wy + 5, wx + 10, wy + 5, 1, kColorBlack);
    }
}

// A distant gull — a simple double-arc silhouette.
static void draw_bird(int cx, int cy) {
    GFX_LINE(cx - 7, cy,     cx - 3, cy - 5, 2, kColorBlack);
    GFX_LINE(cx - 3, cy - 5, cx,     cy - 1, 2, kColorBlack);
    GFX_LINE(cx,     cy - 1, cx + 3, cy - 5, 2, kColorBlack);
    GFX_LINE(cx + 3, cy - 5, cx + 7, cy,     2, kColorBlack);
}

// A spotted toadstool nestled in the grass.
static void draw_mushroom(int cx, int baseY) {
    GFX_FILL(cx - 3, baseY - 9, 7, 9, kColorWhite);
    GFX_RECT(cx - 3, baseY - 9, 7, 9, kColorBlack);
    GFX_ELLIPSE(cx - 9, baseY - 16, 19, 13, kColorBlack);
    GFX_ELLIPSE(cx - 5, baseY - 13, 4, 4, kColorWhite);
    GFX_ELLIPSE(cx + 2, baseY - 14, 3, 3, kColorWhite);
}

// One signpost plank — a left-pointing wooden arrow sign.
static void draw_sign_plank(int cy, const char* label, bool selected) {
    int bodyL = 214, bodyR = 320, top = cy - 15, bot = cy + 15;
    int tipX = bodyL - 15;
    LCDColor fill = selected ? kColorBlack : kColorWhite;
    // drop shadow
    pd->graphics->fillTriangle(tipX + 3, cy + 4, bodyL + 3, top + 4,
                               bodyL + 3, bot + 4, (LCDColor)UI_SHADE);
    GFX_FILL(bodyL + 3, top + 4, bodyR - bodyL, bot - top, (LCDColor)UI_SHADE);
    // body
    pd->graphics->fillTriangle(tipX, cy, bodyL, top, bodyL, bot, fill);
    GFX_FILL(bodyL, top, bodyR - bodyL, bot - top, fill);
    // outline — top / right / bottom / two point edges
    GFX_LINE(bodyL, top, bodyR, top, 2, kColorBlack);
    GFX_LINE(bodyL, bot, bodyR, bot, 2, kColorBlack);
    GFX_LINE(bodyR, top, bodyR, bot, 2, kColorBlack);
    GFX_LINE(bodyL, top, tipX, cy, 2, kColorBlack);
    GFX_LINE(bodyL, bot, tipX, cy, 2, kColorBlack);
    // woodgrain + nail heads on the light planks
    if (!selected) {
        GFX_LINE(bodyL + 8, top + 8, bodyR - 8, top + 8, 1, kColorBlack);
        GFX_LINE(bodyL + 8, bot - 8, bodyR - 8, bot - 8, 1, kColorBlack);
        pd->graphics->drawEllipse(bodyR - 14, top + 6, 5, 5, 1, 0, 360,
                                  kColorBlack);
        pd->graphics->drawEllipse(bodyR - 14, bot - 11, 5, 5, 1, 0, 360,
                                  kColorBlack);
    }
    // label
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label),
                                        kASCIIEncoding, 0);
    int tx = (bodyL + bodyR) / 2 - tw / 2 - 3;
    pd->graphics->setDrawMode(selected ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding, tx, cy - 9);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Draw a pet sprite with a 1px white halo so it stays legible over the
// mountain photo. Title-screen-only — every other place uses
// draw_creature_xform directly to keep the perf cost off the hot path.
static void draw_creature_haloed(uint8_t petId, int cx, int cy,
                                 float scale, bool flip) {
    LCDBitmap* sprite = image_pet(petId);
    if (sprite) {
        float s = flip ? -scale : scale;
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        for (int ox = -1; ox <= 1; ox++)
            for (int oy = -1; oy <= 1; oy++)
                if (ox || oy)
                    pd->graphics->drawRotatedBitmap(sprite, cx + ox, cy + oy,
                                                    0.0f, 0.5f, 0.5f, s, scale);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }
    draw_creature_xform(petId, cx, cy, scale, 0.0f, flip);
}

// Small floating-rectangle menu card. Centered on (cx, cy), ~70×20 px.
// Selected: filled black, white text. Unselected: white fill, black border,
// 2px drop shadow.
static void draw_menu_card(int cx, int cy, const char* label, bool selected) {
    const int w = 70, h = 20;
    int x = cx - w / 2, y = cy - h / 2;
    if (!selected) {
        GFX_FILL(x + 2, y + 2, w, h, kColorBlack);                       // shadow
        GFX_FILL(x, y, w, h, kColorWhite);
        GFX_RECT(x, y, w, h, kColorBlack);
    } else {
        GFX_FILL(x, y, w, h, kColorBlack);
    }
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label),
                                        kASCIIEncoding, 0);
    int tx = cx - tw / 2 - 2;
    pd->graphics->setDrawMode(selected ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding, tx, cy - 8);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

void ui_title_render(void) {
    // --- backdrop: mountain photo (with procedural fallback if asset missing) ---
    LCDBitmap* bg = image_title_bg();
    if (bg) {
        pd->graphics->drawBitmap(bg, 0, 0, kBitmapUnflipped);
    } else {
        render_alpine_bg(150);
    }

    // --- gulls drifting over the ridge ---
    draw_bird(46, 72);
    draw_bird(150, 60);
    draw_bird(96, 88);

    // --- three cows spread across the foreground (haloed for legibility) ---
    const float S = 1.4f;
    draw_creature_haloed(13,  55, 175, S, false);   // Highland, left
    draw_creature_haloed( 5, 155, 195, S, true);    // Calf, middle, facing left
    draw_creature_haloed(13, 250, 170, S, false);   // Highland, right

    // --- floating menu cards (right-side stack) ---
    draw_menu_card(332,  92, "PLAY",   game.titleCursor == 0);
    draw_menu_card(332, 118, "HALL",   game.titleCursor == 1);
    draw_menu_card(332, 144, "ROSTER", game.titleCursor == 2);
    draw_menu_card(332, 170, "TEST",   game.titleCursor == 3);

    // --- title banner with pennants ---
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    const char* T = "AUTOCRANK";
    int tW = pd->graphics->getTextWidth(game.fontLarge, T, strlen(T),
                                        kASCIIEncoding, 0);
    int bw = tW + 52, bh = 34, bx = (SCREEN_W - bw) / 2, by = 20;
    GFX_FILL(bx + 5, by + 6, bw, bh, (LCDColor)UI_SHADE);          // shadow
    draw_pennant(bx + 26,     by - 13, 16, false);                 // flags
    draw_pennant(bx + bw / 2, by - 16, 18, true);
    draw_pennant(bx + bw - 26, by - 13, 16, true);
    pd->graphics->fillTriangle(bx, by + bh - 12, bx, by + bh,      // ribbon tails
                               bx - 14, by + bh + 5, kColorBlack);
    pd->graphics->fillTriangle(bx + bw, by + bh - 12, bx + bw, by + bh,
                               bx + bw + 14, by + bh + 5, kColorBlack);
    GFX_FILL(bx, by, bw, bh, kColorBlack);                         // banner
    GFX_RECT(bx + 3, by + 3, bw - 6, bh - 6, kColorWhite);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText(T, strlen(T), kASCIIEncoding,
                           (SCREEN_W - tW) / 2, by + 7);
    pd->graphics->setDrawMode(kDrawModeCopy);
    draw_text_halo_centered("- combat des reines -", game.fontSmall,
                            by + bh + 2);

    // --- lifetime stats + control hint along the bottom ---
    char buf[64];
    snprintf(buf, sizeof(buf), "RUNS %lu   WINS %lu   BEST %lu",
             (unsigned long)game.totalRuns,
             (unsigned long)game.totalWins,
             (unsigned long)game.bestStreak);
    draw_text_halo(buf, game.fontSmall, 8, 224);
    const char* hint = "up/down + A";
    int hw = pd->graphics->getTextWidth(game.fontSmall, hint, strlen(hint),
                                        kASCIIEncoding, 0);
    draw_text_halo(hint, game.fontSmall, SCREEN_W - hw - 8, 224);
}

// ---------------------------------------------------------------------------
// Game over
// ---------------------------------------------------------------------------
void ui_gameover_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.resultTimer++;
    if (game.resultTimer > 30 && (prs & kButtonA)) {
        game.inRun = false;
        game.state = STATE_TITLE;
        save_write();
    }
}

void ui_gameover_render(void) {
    render_alpine_bg(154);
    int elapsed = (int)game.resultTimer;

    // Vertical curtain wipe: a stippled black mat falls from the top during
    // frames 0..15 and rests as a permanent backdrop scrim for the title bar.
    int wipeH = 44;
    int wipeY = (elapsed < 15) ? -wipeH + (wipeH + 4) * elapsed / 15 : 0;
    GFX_FILL(0, wipeY, SCREEN_W, wipeH, kColorBlack);
    // Soft stippled trailing edge so the wipe doesn't look like a hard cut.
    if (elapsed < 18) {
        int trailY = wipeY + wipeH;
        for (int y = 0; y < 6; y++) {
            int yy = trailY + y;
            if (yy < 0 || yy >= SCREEN_H) continue;
            for (int x = (y & 1); x < SCREEN_W; x += 2)
                GFX_FILL(x, yy, 1, 1, kColorBlack);
        }
    }

    // "GAME OVER" title — stamps in over frames 0..20 with overshoot.
    if (elapsed >= 2) {
        float p = (elapsed < 22) ? ((float)(elapsed - 2) / 20.0f) : 1.0f;
        float scale = easef_out_back(p, 1.5f);
        draw_scaled_text("GAME OVER", game.fontLarge,
                         SCREEN_W / 2, 22, scale, true /* white */);
    }

    // Lone cow drops in from above starting at frame 15.
    if (elapsed >= 15) {
        int targetY = 86;
        int y = ease_out(-50, targetY, elapsed - 15, 18);
        draw_creature_scaled(16, SCREEN_W / 2 - 32, y, 2);
    }

    // Dashed pasture line — fades in via dither density frames 25..40.
    int pastureFade = elapsed - 25;
    if (pastureFade > 0) {
        int density = (pastureFade > 15) ? 6 : (6 + (15 - pastureFade));
        for (int x = 30; x < SCREEN_W - 30; x += density)
            GFX_FILL(x, 152, 3, 2, kColorBlack);
    }

    // Team-name epitaph card — slides up from below starting at frame 35.
    if (elapsed >= 35) {
        const int cardW = 220, cardH = 38;
        int targetY = 162;
        int startY  = SCREEN_H + 4;
        int y = ease_out(startY, targetY, elapsed - 35, 20);
        int x = SCREEN_W / 2 - cardW / 2;
        draw_banner_card(x, y, cardW, cardH);
        const char* tag = "R.I.P.";
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        int tw = pd->graphics->getTextWidth(game.fontSmall, tag, strlen(tag),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(tag, strlen(tag), kASCIIEncoding,
                               x + cardW / 2 - tw / 2, y + 3);
        pd->graphics->setDrawMode(kDrawModeCopy);
        char nameBuf[40];
        team_name_str(game.playerName.adj, game.playerName.noun,
                      nameBuf, sizeof(nameBuf));
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        int nw = pd->graphics->getTextWidth(game.fontBold, nameBuf, strlen(nameBuf),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(nameBuf, strlen(nameBuf), kASCIIEncoding,
                               x + cardW / 2 - nw / 2, y + 16);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // Subtitle and prompt come last.
    if (elapsed >= 55) {
        char buf[64];
        snprintf(buf, sizeof(buf), "made it to %d wins", game.wins);
        draw_text_halo_centered(buf, game.fontSmall, 207);
    }
    if (elapsed >= 60 && ((game.frameCount / 15) & 1))
        draw_text_halo_centered("PRESS A", game.fontBold, 224);
}

// ---------------------------------------------------------------------------
// Victory
// ---------------------------------------------------------------------------
void ui_victory_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.resultTimer++;
    if (game.resultTimer > 30 && (prs & kButtonA)) {
        game.inRun = false;
        game.state = STATE_TITLE;
        save_write();
    }
}

// Deterministic sparkles around the victory cow — a handful of single pixels
// that wink on/off independently. No state needed.
static void draw_sparkles(int cx, int cy, int rOuter, int t) {
    const int N = 12;
    for (int i = 0; i < N; i++) {
        // angle around the cow + radial variance
        float a = (float)i * (2.0f * PI_F / (float)N);
        int r = rOuter - ((i * 13) % 18);
        int x = cx + (int)(cosf(a) * r);
        int y = cy + (int)(sinf(a) * (r * 6 / 10));
        // wink: each sparkle on for ~8 frames every 24
        int phase = (i * 7) % 24;
        if (((t + phase) % 24) < 8)
            GFX_FILL(x, y, 1, 1, kColorBlack);
    }
}

void ui_victory_render(void) {
    render_alpine_bg(154);
    int elapsed = (int)game.resultTimer;

    // ---- frame 0-10: diagonal banner wipe from top-left ----
    // The wipe leaves a black title bar across the top of the screen.
    int wipeW = SCREEN_W + SCREEN_H;
    int wipeProgress = (elapsed < 10) ? (wipeW * elapsed / 10) : wipeW;
    // Draw the title bar progressively from the wipe
    for (int y = 0; y < 44; y++) {
        int xEnd = wipeProgress - y;     // diagonal: bigger y = shorter so far
        if (xEnd > SCREEN_W) xEnd = SCREEN_W;
        if (xEnd > 0) GFX_FILL(0, y, xEnd, 1, kColorBlack);
    }

    // ---- frame 5-20: CHAMPION! stamps in ----
    if (elapsed >= 5) {
        float p = (elapsed < 20) ? ((float)(elapsed - 5) / 15.0f) : 1.0f;
        float scale = easef_out_back(p, 1.8f);
        if (scale > 0.05f)
            draw_scaled_text("CHAMPION!", game.fontLarge,
                             SCREEN_W / 2, 22, scale, true /* white */);
    }

    // ---- frame 15-35: cow rises from below into position ----
    int cs = 3, cowPx = 32 * cs, cowCx = SCREEN_W / 2;
    int cyTarget = 54;
    int cyStart  = SCREEN_H + 10;
    int cyTop = (elapsed >= 15) ? ease_out(cyStart, cyTarget, elapsed - 15, 20)
                                : cyStart;
    draw_creature_scaled(16, cowCx - cowPx / 2, cyTop, cs);

    // ---- frame 25-45: edelweiss arch materialises one-by-one ----
    static const int arch[5] = { 6, 2, 0, 2, 6 };
    for (int i = 0; i < 5; i++) {
        int appearAt = 25 + i * 3;
        if (elapsed >= appearAt) {
            draw_flower(cowCx - 44 + i * 22, cyTarget + 12 + arch[i], 4);
        }
    }

    // ---- frame 35-55: team-name banner slides up from the bottom ----
    if (elapsed >= 35) {
        const int cardW = 240, cardH = 40;
        int targetY = 158;
        int startY  = SCREEN_H + 4;
        int y = ease_out(startY, targetY, elapsed - 35, 20);
        int x = SCREEN_W / 2 - cardW / 2;
        draw_banner_card(x, y, cardW, cardH);
        const char* tag = "CHAMPIONS";
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        int tw = pd->graphics->getTextWidth(game.fontSmall, tag, strlen(tag),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(tag, strlen(tag), kASCIIEncoding,
                               x + cardW / 2 - tw / 2, y + 4);
        pd->graphics->setDrawMode(kDrawModeCopy);
        char nameBuf[40];
        team_name_str(game.playerName.adj, game.playerName.noun,
                      nameBuf, sizeof(nameBuf));
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        int nw = pd->graphics->getTextWidth(game.fontBold, nameBuf, strlen(nameBuf),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(nameBuf, strlen(nameBuf), kASCIIEncoding,
                               x + cardW / 2 - nw / 2, y + 18);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // ---- frame 60+: idle sparkles around the cow ----
    if (elapsed >= 60) {
        draw_sparkles(cowCx, cyTarget + cowPx / 2 - 4, 70, elapsed);
    }

    // ---- frame 70+: PRESS A blink ----
    if (elapsed >= 70 && ((game.frameCount / 15) & 1))
        draw_text_halo_centered("PRESS A", game.fontBold, 210);
}

// ---------------------------------------------------------------------------
// Pre-battle
// ---------------------------------------------------------------------------
void ui_pre_battle_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (game.preBattleTimer > 0) game.preBattleTimer--;
    if (game.preBattleTimer == 0 || (prs & kButtonA)) {
        game.state = STATE_BATTLE;
    }
}

// ---------------------------------------------------------------------------
// Shared animation helpers — used by pre-battle, result, gameover, victory.
// ---------------------------------------------------------------------------

// Quadratic ease-out: float 0..1 in, 0..1 out, fast at the start.
static float easef_out(float p) {
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return 1.0f - (1.0f - p) * (1.0f - p);
}

// Quadratic ease-out used on integer ranges.
static int ease_out(int from, int to, int t, int dur) {
    if (t <= 0) return from;
    if (t >= dur) return to;
    float p = easef_out((float)t / (float)dur);
    return from + (int)((to - from) * p);
}

// Overshoot ease (a.k.a. "back" ease): rises past 1.0 then settles. Used for
// stamp-in scale animations so the title hits like a stamp.
static float easef_out_back(float p, float overshoot) {
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    float c1 = overshoot;
    float c3 = c1 + 1.0f;
    float q = p - 1.0f;
    return 1.0f + c3 * q * q * q + c1 * q * q;
}

// Draw a "card" — drop shadow, white fill, double border. Same shape used by
// the title menu, name picker, intro banners, etc.
static void draw_banner_card(int x, int y, int w, int h) {
    GFX_FILL(x + 3, y + 3, w, h, kColorBlack);          // drop shadow
    GFX_FILL(x, y, w, h, kColorWhite);
    GFX_RECT(x, y, w, h, kColorBlack);
    GFX_RECT(x + 1, y + 1, w - 2, h - 2, kColorBlack);
}

// Inverted variant — solid black card, white text on it.
static void draw_banner_card_inverted(int x, int y, int w, int h) {
    GFX_FILL(x + 3, y + 3, w, h, (LCDColor)UI_SHADE);   // soft shadow
    GFX_FILL(x, y, w, h, kColorBlack);
    GFX_RECT(x, y, w, h, kColorBlack);
}

// Scaled bold text — draws `s` at scale (1.0 = native). Uses a temporary
// bitmap for the source font glyphs so we can apply Playdate's bitmap scale.
// For 1.0 falls through to drawText (cheap). For other scales we render to
// a tight bitmap and scale-blit it.
static void draw_scaled_text(const char* s, LCDFont* font, int cx, int cy,
                             float scale, bool invertColor) {
    if (font) pd->graphics->setFont(font);
    int w = pd->graphics->getTextWidth(font, s, strlen(s), kASCIIEncoding, 0);
    int h = pd->graphics->getFontHeight(font);
    if (scale > 0.98f && scale < 1.02f) {
        pd->graphics->setDrawMode(invertColor ? kDrawModeFillWhite
                                              : kDrawModeFillBlack);
        pd->graphics->drawText(s, strlen(s), kASCIIEncoding,
                               cx - w / 2, cy - h / 2);
        pd->graphics->setDrawMode(kDrawModeCopy);
        return;
    }
    LCDBitmap* tmp = pd->graphics->newBitmap(w + 4, h + 4, kColorClear);
    pd->graphics->pushContext(tmp);
    pd->graphics->setDrawMode(invertColor ? kDrawModeFillWhite
                                          : kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, 2, 2);
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->popContext();
    pd->graphics->drawRotatedBitmap(tmp, cx, cy, 0.0f, 0.5f, 0.5f,
                                    scale, scale);
    pd->graphics->freeBitmap(tmp);
}

// One-frame screen-flash. Pass true to invert the whole screen.
static void draw_flash_overlay(bool on) {
    if (!on) return;
    pd->graphics->setDrawMode(kDrawModeInverted);
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Pre-battle intro animation: overlay on top of the live battle line-up.
//
//   0–18  ACT 1  player card slides in from off-left, lands left of centre
//  12–30  ACT 2  enemy card slides in from off-right, lands right of centre
//  24–36  ACT 3  big VS stamps in centre with overshoot, 1-frame impact flash
//  36–75  HOLD   both cards + VS sit steady (1.3 s reading time)
//  75–90  EXIT   cards slide back off-screen; VS shrinks to its small dot
// ---------------------------------------------------------------------------
static void draw_intro_animation(int elapsed) {
    char playerBuf[40], enemyBuf[40];
    team_name_str(game.playerName.adj, game.playerName.noun,
                  playerBuf, sizeof(playerBuf));
    team_name_str(game.enemyName.adj, game.enemyName.noun,
                  enemyBuf, sizeof(enemyBuf));

    // No full-screen scrim — the battle line-up shows through behind us.
    // A short black title bar across the top corners would clash with the
    // existing topbar, so the intro cards live just under it.

    const int bannerW = 178, bannerH = 44;
    const int leftTargetX  = 8;
    const int rightTargetX = SCREEN_W - bannerW - 8;
    const int bannerY = 56;

    // ---- ACT 1: player card from off-left ----
    {
        int startX = -bannerW - 6;
        int x = ease_out(startX, leftTargetX, elapsed, 18);
        // Exit slide: starts at frame 75, completes by 90
        if (elapsed >= 75) {
            x = ease_out(leftTargetX, startX, elapsed - 75, 15);
        }
        draw_banner_card(x, bannerY, bannerW, bannerH);
        const char* tag = "YOUR HERD";
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        int tw = pd->graphics->getTextWidth(game.fontSmall, tag, strlen(tag),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(tag, strlen(tag), kASCIIEncoding,
                               x + bannerW / 2 - tw / 2, bannerY + 4);
        pd->graphics->setDrawMode(kDrawModeCopy);
        if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
        int nw = pd->graphics->getTextWidth(game.fontLarge, playerBuf, strlen(playerBuf),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(playerBuf, strlen(playerBuf), kASCIIEncoding,
                               x + bannerW / 2 - nw / 2, bannerY + 18);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // ---- ACT 2: enemy card from off-right ----
    if (elapsed >= 12) {
        int startX = SCREEN_W + 6;
        int x = ease_out(startX, rightTargetX, elapsed - 12, 18);
        if (elapsed >= 75) {
            x = ease_out(rightTargetX, startX, elapsed - 75, 15);
        }
        draw_banner_card_inverted(x, bannerY, bannerW, bannerH);
        const char* tag = "THE CHALLENGER";
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        int tw = pd->graphics->getTextWidth(game.fontSmall, tag, strlen(tag),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        pd->graphics->drawText(tag, strlen(tag), kASCIIEncoding,
                               x + bannerW / 2 - tw / 2, bannerY + 4);
        pd->graphics->setDrawMode(kDrawModeCopy);
        if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
        int nw = pd->graphics->getTextWidth(game.fontLarge, enemyBuf, strlen(enemyBuf),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        pd->graphics->drawText(enemyBuf, strlen(enemyBuf), kASCIIEncoding,
                               x + bannerW / 2 - nw / 2, bannerY + 18);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    // ---- ACT 3: VS stamp centred between the two cards ----
    if (elapsed >= 24) {
        int t = elapsed - 24;
        float p = (t > 12) ? 1.0f : ((float)t / 12.0f);
        float scale = easef_out_back(p, 1.7f);
        // Exit: shrink down during 75..90
        if (elapsed >= 75) {
            float q = (float)(elapsed - 75) / 15.0f;
            if (q > 1.0f) q = 1.0f;
            scale = 1.0f - q;
        }
        int rr = 22;
        int cx = SCREEN_W / 2;
        int cy = bannerY + bannerH / 2;
        if (scale > 0.05f) {
            int rs = (int)(rr * scale);
            GFX_ELLIPSE(cx - rs, cy - rs, rs * 2, rs * 2, kColorBlack);
            GFX_ELLIPSE(cx - rs + 2, cy - rs + 2, rs * 2 - 4, rs * 2 - 4, kColorWhite);
            pd->graphics->drawEllipse(cx - rs, cy - rs, rs * 2, rs * 2, 2, 0, 360,
                                      kColorBlack);
            draw_scaled_text("VS", game.fontLarge, cx, cy, scale, false);
        }
        if (t == 12) draw_flash_overlay(true);
    }
}

void ui_pre_battle_render(void) {
    // Draw the live battle scene behind everything — the intro is purely
    // an overlay.
    render_battle_screen();

    int elapsed = 90 - (int)game.preBattleTimer;
    draw_intro_animation(elapsed);
}

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------
void ui_result_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.resultTimer++;
    if (game.resultTimer > 60 && (prs & kButtonA)) {
        game_after_battle();
    }
}

// 16 × 14 white heart, anchored at the centre (cx, cy). Two top lobes and a
// chevron-style point at the bottom; filled or outline. Reads cleanly on the
// black banner background.
static void draw_heart(int cx, int cy, bool filled) {
    if (filled) {
        // Top lobes: two filled ellipses slightly overlapping.
        GFX_ELLIPSE(cx - 7, cy - 5, 8, 7, kColorWhite);
        GFX_ELLIPSE(cx - 1, cy - 5, 8, 7, kColorWhite);
        // Body block joins the lobes so the silhouette reads as one shape.
        GFX_FILL(cx - 6, cy - 2, 13, 4, kColorWhite);
        // Bottom V — a filled triangle ending in a single-pixel point.
        pd->graphics->fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2,
                                   cx, cy + 8, kColorWhite);
    } else {
        // Outline lobes
        pd->graphics->drawEllipse(cx - 7, cy - 5, 8, 7, 1, 0, 360, kColorWhite);
        pd->graphics->drawEllipse(cx - 1, cy - 5, 8, 7, 1, 0, 360, kColorWhite);
        // Connector arcs along the bottom of the lobes are hidden by drawing
        // the two slanting V edges from the lobe tips down to a single point.
        GFX_LINE(cx - 6, cy + 1, cx, cy + 8, 1, kColorWhite);
        GFX_LINE(cx + 6, cy + 1, cx, cy + 8, 1, kColorWhite);
    }
}

// Horizontal progress bar across the result banner — clearer than dots for
// "X of 10 wins". Reads left-to-right like a fuel gauge.
static void draw_win_progress(int cx, int cy, int wins, int total) {
    int segW = 22, segH = 10, gap = 3;
    int totalW = total * segW + (total - 1) * gap;
    int x0 = cx - totalW / 2;
    for (int i = 0; i < total; i++) {
        int sx = x0 + i * (segW + gap);
        if (i < wins) {
            GFX_FILL(sx, cy - segH / 2, segW, segH, kColorWhite);
        } else {
            pd->graphics->drawRect(sx, cy - segH / 2, segW, segH, kColorWhite);
            // single pixel hole in the middle so it reads as "empty cup"
            // rather than just an empty rect.
            GFX_FILL(sx + segW / 2 - 1, cy - 1, 2, 2, kColorWhite);
        }
    }
}

// Draw confetti dots falling from the top of the screen. Deterministic by
// dot index + frame so it loops cleanly without state.
static void draw_confetti(int elapsed) {
    const int N = 36;
    for (int i = 0; i < N; i++) {
        // each dot has a fixed x and a phase offset
        int x = ((i * 1657) % SCREEN_W);
        int speed = 2 + ((i * 97) % 4);            // 2..5 px/frame
        int phase = (i * 213) % 90;                // staggered start
        int t = (elapsed + phase) % 120;
        int y = (t * speed) % (SCREEN_H + 8) - 4;
        // alternate dot vs short streak
        if ((i & 1) == 0)
            GFX_FILL(x, y, 2, 2, kColorBlack);
        else
            GFX_FILL(x, y, 1, 3, kColorBlack);
    }
}

// Spray 4 small black diamonds outward from (cx, cy) over `t` frames.
// Used as the "heart shatter" on loss.
static void draw_shatter(int cx, int cy, int t) {
    if (t < 0 || t > 14) return;
    int r = (t * 14) / 14 + 4;
    const int dirs[4][2] = {{-1,-1},{1,-1},{-1,1},{1,1}};
    for (int i = 0; i < 4; i++) {
        int x = cx + dirs[i][0] * r;
        int y = cy + dirs[i][1] * r;
        pd->graphics->fillTriangle(x, y - 3, x + 3, y, x, y + 3, kColorBlack);
        pd->graphics->fillTriangle(x, y - 3, x, y + 3, x - 3, y, kColorBlack);
    }
}

void ui_result_render(void) {
    render_alpine_bg(90);
    render_topbar();

    int elapsed = (int)game.resultTimer;
    int by = 38, bh = 168;

    // Outcome-specific FX behind the banner.
    if (game.battleResult == 0) {
        // WIN: confetti rains for the whole screen lifetime.
        draw_confetti(elapsed);
    } else if (game.battleResult == 1) {
        // LOSS: a 50% stipple darken-pass over the alpine bg.
        pd->graphics->setDrawMode(kDrawModeCopy);
        for (int y = 18; y < SCREEN_H; y += 2)        // 18 = TOPBAR_H
            for (int x = (y & 2) ? 0 : 1; x < SCREEN_W; x += 2)
                GFX_FILL(x, y, 1, 1, kColorBlack);
    }

    // Banner with double border. Slides down 10 px during stamp-in for a
    // settling effect.
    int yoff = (elapsed < 8) ? (-10 + (elapsed * 10) / 8) : 0;
    GFX_FILL(0, by + yoff, SCREEN_W, bh, kColorBlack);
    GFX_LINE(0, by + yoff,      SCREEN_W, by + yoff,      2, kColorWhite);
    GFX_LINE(0, by + bh + yoff, SCREEN_W, by + bh + yoff, 2, kColorWhite);

    // Title — punchy single word. Stamp-in over 15 frames with overshoot.
    const char* label = (game.battleResult == 0) ? "VICTORY"
                      : (game.battleResult == 1) ? "DEFEAT"
                                                 : "STALEMATE";
    float p = (elapsed < 15) ? ((float)elapsed / 15.0f) : 1.0f;
    float scale = easef_out_back(p, 1.6f);
    draw_scaled_text(label, game.fontLarge,
                     SCREEN_W / 2, by + 28 + yoff, scale, true /* white */);

    // One-frame impact flash on stamp landing.
    if (elapsed == 6) draw_flash_overlay(true);

    // ---- WINS section: "WINS  N / 10" label + segmented progress bar ----
    if (elapsed >= 15) {
        int wins = game.wins;   // already incremented in game_after_battle
        char winsBuf[24];
        snprintf(winsBuf, sizeof(winsBuf), "WINS  %d / %d", wins,
                 WINS_TO_VICTORY);
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        int wtw = pd->graphics->getTextWidth(game.fontBold, winsBuf,
                                             strlen(winsBuf),
                                             kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        pd->graphics->drawText(winsBuf, strlen(winsBuf), kASCIIEncoding,
                               (SCREEN_W - wtw) / 2, by + 52);
        pd->graphics->setDrawMode(kDrawModeCopy);
        draw_win_progress(SCREEN_W / 2, by + 78, wins, WINS_TO_VICTORY);
        // Pulse the just-earned segment on win.
        if (game.battleResult == 0 && wins > 0) {
            int pulseT = elapsed - 15;
            if (pulseT < 30 && (pulseT / 5) % 2 == 0) {
                // overlay a 2px outline ring around the freshly-filled segment
                int segW = 22, gap = 3;
                int totalW = WINS_TO_VICTORY * segW + (WINS_TO_VICTORY - 1) * gap;
                int x0 = SCREEN_W / 2 - totalW / 2;
                int sx = x0 + (wins - 1) * (segW + gap);
                int sy = by + 78 - 5;
                pd->graphics->drawRect(sx - 2, sy - 2, segW + 4, 14, kColorWhite);
            }
        }
    }

    // ---- LIVES section: "LIVES" label + bigger pixel-art hearts ----
    if (elapsed >= 15) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        const char* livesLbl = "LIVES";
        int llw = pd->graphics->getTextWidth(game.fontBold, livesLbl,
                                             strlen(livesLbl),
                                             kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        pd->graphics->drawText(livesLbl, strlen(livesLbl), kASCIIEncoding,
                               (SCREEN_W - llw) / 2, by + 95);
        pd->graphics->setDrawMode(kDrawModeCopy);

        int lives = game.lives;
        int hStep = 28;
        int hStartX = (SCREEN_W - (STARTING_LIVES - 1) * hStep) / 2;
        int hY = by + 122;
        for (int i = 0; i < STARTING_LIVES; i++) {
            int hx = hStartX + i * hStep;
            draw_heart(hx, hY, i < lives);
        }
        if (game.battleResult == 1 && lives < STARTING_LIVES) {
            int shatterX = hStartX + lives * hStep;
            draw_shatter(shatterX, hY, elapsed - 15);
        }
    }

    // PRESS A blink — bottom of the banner.
    if (elapsed > 45 && ((game.frameCount / 15) & 1)) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        const char* pa = "PRESS A";
        int pw = pd->graphics->getTextWidth(game.fontBold, pa, strlen(pa),
                                            kASCIIEncoding, 0);
        pd->graphics->setDrawMode(kDrawModeFillWhite);
        pd->graphics->drawText(pa, strlen(pa), kASCIIEncoding,
                               (SCREEN_W - pw) / 2, by + 144);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Team-name picker — SAP-style 3-of-each, adj + noun.
// Player presses A on PLAY → here. Pick a row with up/down, an option with
// left/right, A to confirm and start the run, B to re-roll the 9 options.
// ---------------------------------------------------------------------------
static void name_reroll(void) {
    uint8_t adj_picked[3] = {0xFF, 0xFF, 0xFF};
    uint8_t noun_picked[3] = {0xFF, 0xFF, 0xFF};
    for (int i = 0; i < 3; i++) {
        uint8_t v;
        do { v = (uint8_t)(rng_next() % NAME_ADJ_COUNT); }
        while (v == adj_picked[0] || v == adj_picked[1]);
        adj_picked[i] = v;
        game.nameAdjChoices[i] = v;
    }
    for (int i = 0; i < 3; i++) {
        uint8_t v;
        do { v = (uint8_t)(rng_next() % NAME_NOUN_COUNT); }
        while (v == noun_picked[0] || v == noun_picked[1]);
        noun_picked[i] = v;
        game.nameNounChoices[i] = v;
    }
    game.nameAdjPick  = 0;
    game.nameNounPick = 0;
}

void ui_team_name_enter(void) {
    name_reroll();
    game.nameRow = 0;
    game.nameAdjConfirmed  = 0;
    game.nameNounConfirmed = 0;
    game.state = STATE_TEAM_NAME;
}

void ui_team_name_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);

    bool curConfirmed = (game.nameRow == 0) ? game.nameAdjConfirmed
                                            : game.nameNounConfirmed;

    // Grid navigation: row changes inherit the cursor column from the row
    // being left. Left/Right within a confirmed row is locked (B first to
    // unselect).
    if ((prs & kButtonUp) && game.nameRow == 1) {
        game.nameRow = 0;
        game.nameAdjPick = game.nameNounPick;
    }
    if ((prs & kButtonDown) && game.nameRow == 0) {
        game.nameRow = 1;
        game.nameNounPick = game.nameAdjPick;
    }
    if ((prs & kButtonLeft) && !curConfirmed) {
        if (game.nameRow == 0)
            game.nameAdjPick = (uint8_t)((game.nameAdjPick + 2) % 3);
        else
            game.nameNounPick = (uint8_t)((game.nameNounPick + 2) % 3);
    }
    if ((prs & kButtonRight) && !curConfirmed) {
        if (game.nameRow == 0)
            game.nameAdjPick = (uint8_t)((game.nameAdjPick + 1) % 3);
        else
            game.nameNounPick = (uint8_t)((game.nameNounPick + 1) % 3);
    }

    if (prs & kButtonA) {
        if (game.nameRow == 0 && !game.nameAdjConfirmed) {
            game.nameAdjConfirmed = 1;
            // jump cursor down so the next A confirms the noun row.
            if (!game.nameNounConfirmed) {
                game.nameRow = 1;
                game.nameNounPick = game.nameAdjPick;
            }
        } else if (game.nameRow == 1 && !game.nameNounConfirmed) {
            game.nameNounConfirmed = 1;
            if (!game.nameAdjConfirmed) {
                game.nameRow = 0;
                game.nameAdjPick = game.nameNounPick;
            }
        }
        if (game.nameAdjConfirmed && game.nameNounConfirmed) {
            game.playerName.adj  = game.nameAdjChoices[game.nameAdjPick];
            game.playerName.noun = game.nameNounChoices[game.nameNounPick];
            game_start_run();
        }
    }

    if (prs & kButtonB) {
        // Unselect: clear the current row's confirmation if it has one;
        // otherwise the most recent one; otherwise back out to title.
        if (curConfirmed) {
            if (game.nameRow == 0) game.nameAdjConfirmed  = 0;
            else                   game.nameNounConfirmed = 0;
        } else if (game.nameAdjConfirmed || game.nameNounConfirmed) {
            game.nameAdjConfirmed  = 0;
            game.nameNounConfirmed = 0;
        } else {
            game.state = STATE_TITLE;
        }
    }
}

// Pill states for the name picker:
//   hovered    — the cursor is on this pill; draws a 2px outer ring
//   confirmed  — the player A'd this pill; fills black with white text
//   neither    — default white card with drop shadow
// Confirmed + hovered get both (filled card + ring).
static void draw_pill(int cx, int cy, int w, int h, const char* label,
                      bool hovered, bool confirmed) {
    int x = cx - w / 2, y = cy - h / 2;
    if (confirmed) {
        GFX_FILL(x, y, w, h, kColorBlack);
    } else {
        GFX_FILL(x + 2, y + 2, w, h, (LCDColor)UI_SHADE);
        GFX_FILL(x, y, w, h, kColorWhite);
        GFX_RECT(x, y, w, h, kColorBlack);
    }
    if (hovered) {
        // 2px outer ring extending beyond the pill edge.
        GFX_RECT(x - 3, y - 3, w + 6, h + 6, kColorBlack);
        GFX_RECT(x - 2, y - 2, w + 4, h + 4, kColorBlack);
    }
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int tw = pd->graphics->getTextWidth(game.fontBold, label, strlen(label),
                                        kASCIIEncoding, 0);
    pd->graphics->setDrawMode(confirmed ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding,
                           cx - tw / 2 - 1, cy - 8);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

void ui_team_name_render(void) {
    LCDBitmap* bg = image_name_bg();
    if (!bg) bg = image_title_bg();
    if (bg) pd->graphics->drawBitmap(bg, 0, 0, kBitmapUnflipped);
    else    GFX_CLEAR(kColorWhite);

    GFX_FILL(0, 0, SCREEN_W, 30, kColorBlack);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    const char* T = "NAME YOUR HERD";
    int tw = pd->graphics->getTextWidth(game.fontBold, T, strlen(T),
                                        kASCIIEncoding, 0);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText(T, strlen(T), kASCIIEncoding,
                           (SCREEN_W - tw) / 2, 8);
    pd->graphics->setDrawMode(kDrawModeCopy);

    char buf[40];
    team_name_str(game.nameAdjChoices[game.nameAdjPick],
                  game.nameNounChoices[game.nameNounPick],
                  buf, sizeof(buf));
    int cw = 280, ch = 28, cx = SCREEN_W / 2, cy = 54;
    GFX_FILL(cx - cw/2 + 3, cy - ch/2 + 3, cw, ch, (LCDColor)UI_SHADE);
    GFX_FILL(cx - cw/2, cy - ch/2, cw, ch, kColorWhite);
    GFX_RECT(cx - cw/2, cy - ch/2, cw, ch, kColorBlack);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    int bw = pd->graphics->getTextWidth(game.fontLarge, buf, strlen(buf),
                                        kASCIIEncoding, 0);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding,
                           cx - bw / 2, cy - 10);
    pd->graphics->setDrawMode(kDrawModeCopy);

    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    draw_text_halo("adjective", game.fontSmall, 12, 96);
    draw_text_halo("noun",      game.fontSmall, 12, 146);

    int rowAdjY = 110, rowNounY = 160;
    int pillW = 100, pillH = 22, pillGap = 8;
    int totalW = pillW * 3 + pillGap * 2;
    int leftX  = (SCREEN_W - totalW) / 2 + pillW / 2;
    for (int i = 0; i < 3; i++) {
        int x = leftX + i * (pillW + pillGap);

        bool adjHovered = (game.nameRow == 0) && (game.nameAdjPick == i);
        bool adjConfirmed = game.nameAdjConfirmed && (game.nameAdjPick == i);
        draw_pill(x, rowAdjY, pillW, pillH,
                  NAME_ADJECTIVES[game.nameAdjChoices[i]],
                  adjHovered, adjConfirmed);

        bool nounHovered = (game.nameRow == 1) && (game.nameNounPick == i);
        bool nounConfirmed = game.nameNounConfirmed && (game.nameNounPick == i);
        draw_pill(x, rowNounY, pillW, pillH,
                  NAME_NOUNS[game.nameNounChoices[i]],
                  nounHovered, nounConfirmed);
    }

    const char* hint = (!game.nameAdjConfirmed && !game.nameNounConfirmed)
        ? "A select   B back"
        : (game.nameAdjConfirmed && game.nameNounConfirmed)
        ? "A start    B unselect"
        : "A confirm  B unselect";
    draw_text_halo(hint, game.fontSmall, 24, 222);
}

// ---------------------------------------------------------------------------
// Hall of Fame — scrollable list of past 10-win runs.
// ---------------------------------------------------------------------------
void ui_hof_enter(void) {
    game.hofCursor = 0;
    game.state = STATE_HALL_OF_FAME;
}

static int hof_index_for_n(int n) {
    if (n < 0 || n >= game.hofCount) return -1;
    int newest = (game.hofNext - 1 + HOF_SIZE) % HOF_SIZE;
    return (newest - n + HOF_SIZE) % HOF_SIZE;
}

void ui_hof_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (game.hofCount == 0) {
        if (prs & (kButtonA | kButtonB)) game.state = STATE_TITLE;
        return;
    }
    if (prs & kButtonUp) {
        if (game.hofCursor > 0) game.hofCursor--;
    }
    if (prs & kButtonDown) {
        if (game.hofCursor + 1 < game.hofCount)
            game.hofCursor++;
    }
    if (prs & kButtonB) game.state = STATE_TITLE;
}

void ui_hof_render(void) {
    GFX_CLEAR(kColorWhite);

    GFX_FILL(0, 0, SCREEN_W, 30, kColorBlack);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    const char* T = "HALL OF FAME";
    int tw = pd->graphics->getTextWidth(game.fontBold, T, strlen(T),
                                        kASCIIEncoding, 0);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    pd->graphics->drawText(T, strlen(T), kASCIIEncoding,
                           (SCREEN_W - tw) / 2, 8);
    pd->graphics->setDrawMode(kDrawModeCopy);

    if (game.hofCount == 0) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        const char* m = "no winning runs yet";
        int mw = pd->graphics->getTextWidth(game.fontBold, m, strlen(m),
                                            kASCIIEncoding, 0);
        pd->graphics->drawText(m, strlen(m), kASCIIEncoding,
                               (SCREEN_W - mw) / 2, 110);
        const char* hint = "B to go back";
        draw_text_halo(hint, game.fontSmall, 24, 222);
        return;
    }

    int firstShown = game.hofCursor - 1;
    if (firstShown < 0) firstShown = 0;
    if (firstShown + 3 > game.hofCount) firstShown = game.hofCount - 3;
    if (firstShown < 0) firstShown = 0;

    int rowH = 56, rowTop = 38;
    for (int i = 0; i < 3 && firstShown + i < game.hofCount; i++) {
        int n = firstShown + i;
        int idx = hof_index_for_n(n);
        if (idx < 0) continue;
        WinRecord* r = &game.hof[idx];
        int y = rowTop + i * (rowH + 4);
        bool sel = (n == game.hofCursor);

        GFX_FILL(6, y, SCREEN_W - 12, rowH, kColorWhite);
        GFX_RECT(6, y, SCREEN_W - 12, rowH, kColorBlack);
        if (sel) GFX_RECT(7, y + 1, SCREEN_W - 14, rowH - 2, kColorBlack);

        for (int s = 0; s < TEAM_SIZE; s++) {
            int sx = 14 + s * 36;
            int sy = y + 8;
            if (r->pets[s] < PET_COUNT) {
                draw_creature_xform(r->pets[s], sx + 16, sy + 16, 1.0f,
                                    0.0f, false);
            }
        }

        char nameBuf[40];
        team_name_str(r->name.adj, r->name.noun, nameBuf, sizeof(nameBuf));
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        pd->graphics->setDrawMode(kDrawModeFillBlack);
        pd->graphics->drawText(nameBuf, strlen(nameBuf), kASCIIEncoding,
                               200, y + 8);
        char meta[24];
        snprintf(meta, sizeof(meta), "turn %u", r->turn);
        if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
        pd->graphics->drawText(meta, strlen(meta), kASCIIEncoding,
                               200, y + 28);
        pd->graphics->setDrawMode(kDrawModeCopy);
    }

    char nav[24];
    snprintf(nav, sizeof(nav), "%u / %u   B back",
             (unsigned)(game.hofCursor + 1), (unsigned)game.hofCount);
    draw_text_halo(nav, game.fontSmall, 8, 222);
}
