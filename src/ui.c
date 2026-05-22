#include "game.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
void ui_title_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.titleBlinkTimer++;
    if (prs & kButtonUp)
        game.titleCursor = (uint8_t)((game.titleCursor + 2) % 3);
    if (prs & kButtonDown)
        game.titleCursor = (uint8_t)((game.titleCursor + 1) % 3);
    if (prs & kButtonA) {
        if (game.titleCursor == 0) {
            game_start_run();
        } else if (game.titleCursor == 1) {
            game.state = STATE_GALLERY;
        } else {
            selftest_run();
            game.state = STATE_SELFTEST;
        }
    }
}

// ---------------------------------------------------------------------------
// Roster gallery — all 18 creatures in a grid (debug / art review)
// ---------------------------------------------------------------------------
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

void ui_title_render(void) {
    // Alpine pasture backdrop.
    render_alpine_bg(150);

    // --- gulls drifting over the ridge ---
    draw_bird(46, 72);
    draw_bird(150, 60);
    draw_bird(96, 88);

    // --- scene: pines behind a chalet, hero cow on the pasture ---
    draw_pine(22, 170, 26);
    draw_pine(70, 190, 44);
    draw_chalet(36, 182);
    draw_creature_scaled(16, 92, 142, 2.0f);

    // --- foreground flora ---
    draw_flower(52, 210, 5);
    draw_flower(178, 206, 4);
    draw_flower(248, 198, 5);
    draw_flower(292, 212, 4);
    draw_mushroom(212, 220);
    draw_mushroom(266, 214);

    // --- wooden signpost menu ---
    int postCX = 322, postW = 24, postT = 72, postB = 206;
    int postL = postCX - postW / 2;
    GFX_ELLIPSE(postCX - 20, postB - 7, 56, 14, (LCDColor)UI_SHADE);
    GFX_FILL(postL + 4, postT + 4, postW, postB - postT, (LCDColor)UI_SHADE);
    GFX_FILL(postL, postT, postW, postB - postT, kColorWhite);
    GFX_RECT(postL, postT, postW, postB - postT, kColorBlack);
    GFX_RECT(postL + 1, postT + 1, postW - 2, postB - postT - 2, kColorBlack);
    GFX_FILL(postL - 3, postT - 5, postW + 6, 5, kColorBlack);          // cap
    GFX_LINE(postCX + 2, postT + 8, postCX + 2, postB - 8, 1, kColorBlack);
    GFX_LINE(postCX + 8, postT + 8, postCX + 8, postB - 8, 1, kColorBlack);
    pd->graphics->drawEllipse(postCX + 1, 188, 9, 11, 1, 0, 360, kColorBlack);
    draw_sign_plank(98,  "PLAY",   game.titleCursor == 0);
    draw_sign_plank(134, "ROSTER", game.titleCursor == 1);
    draw_sign_plank(170, "TEST",   game.titleCursor == 2);

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
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 8, 224);
    const char* hint = "up/down + A";
    int hw = pd->graphics->getTextWidth(game.fontSmall, hint, strlen(hint),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(hint, strlen(hint), kASCIIEncoding,
                           SCREEN_W - hw - 8, 224);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// ---------------------------------------------------------------------------
// Game over
// ---------------------------------------------------------------------------
void ui_gameover_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (prs & kButtonA) {
        game.inRun = false;
        game.state = STATE_TITLE;
        save_write();
    }
}

void ui_gameover_render(void) {
    render_alpine_bg(154);

    // Full-width black title bar
    GFX_FILL(0, 0, SCREEN_W, 44, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    const char* goLabel = "GAME OVER";
    int glw = pd->graphics->getTextWidth(game.fontLarge, goLabel, strlen(goLabel),
                                         kASCIIEncoding, 0);
    pd->graphics->drawText(goLabel, strlen(goLabel), kASCIIEncoding,
                           (SCREEN_W - glw) / 2, 11);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // A lone cow grazing on the empty pasture.
    draw_creature_scaled(16, SCREEN_W / 2 - 32, 86, 2);

    // Dashed pasture line.
    for (int x = 30; x < SCREEN_W - 30; x += 6)
        GFX_FILL(x, 152, 3, 2, kColorBlack);

    char buf[64];
    snprintf(buf, sizeof(buf), "the herd made it to %d wins", game.wins);
    title_text_centered(buf, game.fontSmall, 158);

    if ((game.frameCount / 15) & 1)
        title_text_centered("PRESS A", game.fontBold, 192);
    title_text_centered("back to the valley", game.fontSmall, 216);
}

// ---------------------------------------------------------------------------
// Victory
// ---------------------------------------------------------------------------
void ui_victory_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (prs & kButtonA) {
        game.inRun = false;
        game.state = STATE_TITLE;
        save_write();
    }
}

void ui_victory_render(void) {
    render_alpine_bg(154);

    // Full-width black title bar
    GFX_FILL(0, 0, SCREEN_W, 44, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    const char* champLabel = "CHAMPION!";
    int champW = pd->graphics->getTextWidth(game.fontLarge, champLabel,
                                            strlen(champLabel), kASCIIEncoding, 0);
    pd->graphics->drawText(champLabel, strlen(champLabel), kASCIIEncoding,
                           (SCREEN_W - champW) / 2, 11);

    char buf[64];
    snprintf(buf, sizeof(buf), "%d wins  -  queen of the alps", WINS_TO_VICTORY);
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    int subW = pd->graphics->getTextWidth(game.fontSmall, buf, strlen(buf),
                                          kASCIIEncoding, 0);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding,
                           (SCREEN_W - subW) / 2, 30);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // The Desalpe cow, descending the mountain crowned with edelweiss.
    int cs = 3, cowPx = 32 * cs, cowCx = SCREEN_W / 2, cyTop = 54;
    draw_creature_scaled(16, cowCx - cowPx / 2, cyTop, cs);

    // Flower crown — a gentle arch of edelweiss over the cow's head.
    static const int arch[5] = { 6, 2, 0, 2, 6 };
    for (int i = 0; i < 5; i++)
        draw_flower(cowCx - 44 + i * 22, cyTop + 12 + arch[i], 4);

    // Petals tossed across the meadow in celebration.
    draw_flower(44, 96, 5);
    draw_flower(360, 110, 5);
    draw_flower(74, 196, 4);
    draw_flower(330, 188, 4);

    if ((game.frameCount / 15) & 1)
        title_text_centered("PRESS A", game.fontBold, 214);
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

void ui_pre_battle_render(void) {
    render_battle_screen();
    // "Round N" banner — a centered black bar high on the ridgeline so it
    // never overlaps the two line-ups below.
    char buf[32];
    snprintf(buf, sizeof(buf), "ROUND %d", game.turn);
    int bw = 168, bh = 30;
    int bx = (SCREEN_W - bw) / 2, by = 26;
    GFX_FILL(bx, by, bw, bh, kColorBlack);
    GFX_RECT(bx - 2, by - 2, bw + 4, bh + 4, kColorWhite);
    GFX_RECT(bx - 3, by - 3, bw + 6, bh + 6, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    int tw = pd->graphics->getTextWidth(game.fontLarge, buf, strlen(buf),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding,
                           bx + (bw - tw) / 2, by + 5);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // "VS" mark on the centre line between the two teams.
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int vw = pd->graphics->getTextWidth(game.fontBold, "VS", 2,
                                        kASCIIEncoding, 0);
    int vx = (SCREEN_W - vw) / 2, vy = 118;
    GFX_ELLIPSE(SCREEN_W / 2 - 16, vy - 4, 32, 26, kColorWhite);
    pd->graphics->drawEllipse(SCREEN_W / 2 - 16, vy - 4, 32, 26, 2,
                              0, 360, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText("VS", 2, kASCIIEncoding, vx, vy + 1);
    pd->graphics->setDrawMode(kDrawModeCopy);
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

void ui_result_render(void) {
    render_alpine_bg(90);
    render_topbar();

    // Full-width black banner at vertical centre
    int by = 64, bh = 128;
    GFX_FILL(0, by, SCREEN_W, bh, kColorBlack);
    GFX_LINE(0, by,      SCREEN_W, by,      2, kColorWhite);
    GFX_LINE(0, by + bh, SCREEN_W, by + bh, 2, kColorWhite);

    // Result label
    const char* label = (game.battleResult == 0) ? "YOU WIN!"
                      : (game.battleResult == 1) ? "YOU LOSE"
                                                 : "A DRAW";
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    int lw = pd->graphics->getTextWidth(game.fontLarge, label, strlen(label),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding,
                           (SCREEN_W - lw) / 2, by + 8);

    // Win pips — one circle per win target, filled = achieved
    int wins = game.wins + (game.battleResult == 0 ? 1 : 0);
    int pipR = 6, pipStep = 16;
    int pipStartX = (SCREEN_W - ((WINS_TO_VICTORY - 1) * pipStep + pipR * 2)) / 2 + pipR;
    int pipY = by + 52;
    for (int i = 0; i < WINS_TO_VICTORY; i++) {
        int px = pipStartX + i * pipStep;
        if (i < wins)
            GFX_ELLIPSE(px - pipR, pipY - pipR, pipR * 2, pipR * 2, kColorWhite);
        else
            pd->graphics->drawEllipse(px - pipR, pipY - pipR, pipR * 2, pipR * 2,
                                      1, 0, 360, kColorWhite);
    }

    // Life hearts — filled white heart = remaining life
    int lives = game.lives - (game.battleResult == 1 ? 1 : 0);
    int hStep = 24;
    int hStartX = (SCREEN_W - ((STARTING_LIVES - 1) * hStep + 12)) / 2 + 6;
    int hY = by + 80;
    for (int i = 0; i < STARTING_LIVES; i++) {
        int hx = hStartX + i * hStep;
        if (i < lives) {
            GFX_ELLIPSE(hx - 5, hY - 4, 6, 6, kColorWhite);
            GFX_ELLIPSE(hx - 1, hY - 4, 6, 6, kColorWhite);
            pd->graphics->fillTriangle(hx - 5, hY, hx + 5, hY, hx, hY + 6, kColorWhite);
        } else {
            pd->graphics->drawEllipse(hx - 5, hY - 4, 6, 6, 1, 0, 360, kColorWhite);
            pd->graphics->drawEllipse(hx - 1, hY - 4, 6, 6, 1, 0, 360, kColorWhite);
        }
    }

    // PRESS A blink
    if (game.resultTimer > 60 && ((game.frameCount / 15) & 1)) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        const char* pa = "PRESS A";
        int pw = pd->graphics->getTextWidth(game.fontBold, pa, strlen(pa),
                                            kASCIIEncoding, 0);
        pd->graphics->drawText(pa, strlen(pa), kASCIIEncoding,
                               (SCREEN_W - pw) / 2, by + 106);
    }
    pd->graphics->setDrawMode(kDrawModeCopy);
}
