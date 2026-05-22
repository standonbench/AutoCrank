#include "game.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
void ui_title_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.titleBlinkTimer++;
    if (prs & kButtonA) {
        game_start_run();
    }
    if (prs & kButtonB) {
        selftest_run();
        game.state = STATE_SELFTEST;
    }
    if (prs & kButtonUp) {
        game.state = STATE_GALLERY;
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

// A white plaque with a double black border — the game's panel style.
static void ui_plaque(int x, int y, int w, int h) {
    GFX_FILL(x, y, w, h, kColorWhite);
    GFX_RECT(x, y, w, h, kColorBlack);
    GFX_RECT(x + 3, y + 3, w - 6, h - 6, kColorBlack);
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

void ui_title_render(void) {
    // Light & cozy Alpine pasture.
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorWhite);
    pd->graphics->setDrawMode(kDrawModeCopy);

    // Mountain ridgeline behind the title
    render_alpine_bg(120);

    // Title — sits in a white plaque so it stays legible over the peaks
    {
        int pw = 200, ph = 30, px = (SCREEN_W - pw) / 2, py = 36;
        GFX_FILL(px, py, pw, ph, kColorWhite);
        GFX_RECT(px, py, pw, ph, kColorBlack);
        GFX_RECT(px + 2, py + 2, pw - 4, ph - 4, kColorBlack);
    }
    title_text_centered("AutoCrank", game.fontLarge, 42);
    title_text_centered("- combat des reines -", game.fontSmall, 80);

    // Hero cow grazing on the pasture
    int cowScale = 2, cowW = 32 * cowScale;
    draw_creature_scaled(16, (SCREEN_W - cowW) / 2, 104, cowScale);

    // Pasture line (dashed)
    for (int x = 20; x < SCREEN_W - 20; x += 6)
        GFX_FILL(x, 170, 3, 2, kColorBlack);

    // Prompts
    if ((game.titleBlinkTimer / 15) & 1)
        title_text_centered("PRESS A TO PLAY", game.fontBold, 182);
    title_text_centered("B: self-test    Up: roster", game.fontSmall, 204);

    char buf[64];
    snprintf(buf, sizeof(buf), "Runs %lu   Wins %lu   Best streak %lu",
             (unsigned long)game.totalRuns,
             (unsigned long)game.totalWins,
             (unsigned long)game.bestStreak);
    title_text_centered(buf, game.fontSmall, 222);
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
    render_alpine_bg(150);

    // Title plaque near the top.
    int pw = 250, ph = 58, px = (SCREEN_W - pw) / 2, py = 24;
    ui_plaque(px, py, pw, ph);
    title_text_centered("GAME OVER", game.fontLarge, py + 9);
    char buf[64];
    snprintf(buf, sizeof(buf), "the herd made it to %d wins", game.wins);
    title_text_centered(buf, game.fontSmall, py + 36);

    // A lone cow grazing on the empty pasture.
    draw_creature_scaled(16, SCREEN_W / 2 - 32, 110, 2);

    // Dashed pasture line.
    for (int x = 30; x < SCREEN_W - 30; x += 6)
        GFX_FILL(x, 178, 3, 2, kColorBlack);

    if ((game.frameCount / 15) & 1)
        title_text_centered("PRESS A", game.fontBold, 198);
    title_text_centered("back to the valley", game.fontSmall, 220);
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
    render_alpine_bg(150);

    // Title plaque.
    int pw = 280, ph = 56, px = (SCREEN_W - pw) / 2, py = 18;
    ui_plaque(px, py, pw, ph);
    title_text_centered("CHAMPION!", game.fontLarge, py + 7);
    title_text_centered("10 wins  -  queen of the alps", game.fontSmall,
                        py + 35);

    // The Desalpe cow, descending the mountain crowned with edelweiss.
    int cs = 3, cw = 32 * cs, cx = SCREEN_W / 2, cyTop = 92;
    draw_creature_scaled(16, cx - cw / 2, cyTop, cs);

    // Flower crown — a gentle arch of edelweiss over the cow's head.
    static const int arch[5] = { 6, 2, 0, 2, 6 };
    for (int i = 0; i < 5; i++)
        draw_flower(cx - 44 + i * 22, cyTop + 12 + arch[i], 4);

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
    render_battle_screen();

    // Central plaque — large enough to fully cover the line-up behind it.
    int pw = 268, ph = 116;
    int px = (SCREEN_W - pw) / 2, py = (SCREEN_H - ph) / 2 - 4;
    ui_plaque(px, py, pw, ph);

    const char* label = (game.battleResult == 0) ? "YOU WIN!"
                      : (game.battleResult == 1) ? "YOU LOSE"
                                                 : "A DRAW";
    title_text_centered(label, game.fontLarge, py + 14);

    int w = game.wins  + (game.battleResult == 0 ? 1 : 0);
    int l = game.lives - (game.battleResult == 1 ? 1 : 0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d / %d wins", w, WINS_TO_VICTORY);
    title_text_centered(buf, game.fontBold, py + 48);
    snprintf(buf, sizeof(buf), "%d lives remaining", l);
    title_text_centered(buf, game.fontBold, py + 70);

    if (game.resultTimer > 60 && ((game.frameCount / 15) & 1))
        title_text_centered("PRESS A", game.fontSmall, py + ph - 22);
}
