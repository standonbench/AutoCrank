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
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    pd->graphics->drawText("GAME OVER", 9, kASCIIEncoding, 130, 70);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    char buf[64];
    snprintf(buf, sizeof(buf), "Made it to %d wins", game.wins);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 130, 120);
    pd->graphics->drawText("Press A", 7, kASCIIEncoding, 160, 180);
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
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    pd->graphics->drawText("VICTORY!", 8, kASCIIEncoding, 140, 70);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    pd->graphics->drawText("10 wins. Beautiful.", 19, kASCIIEncoding, 110, 130);
    pd->graphics->drawText("Press A", 7, kASCIIEncoding, 160, 180);
    pd->graphics->setDrawMode(kDrawModeCopy);
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
    // "Round N" banner — a centered black bar so it never overlaps pets.
    char buf[32];
    snprintf(buf, sizeof(buf), "ROUND %d", game.turn);
    int bw = 160, bh = 32;
    int bx = (SCREEN_W - bw) / 2, by = SCREEN_H / 2 - bh / 2;
    GFX_FILL(bx, by, bw, bh, kColorBlack);
    GFX_RECT(bx - 2, by - 2, bw + 4, bh + 4, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    int tw = pd->graphics->getTextWidth(game.fontLarge, buf, strlen(buf),
                                        kASCIIEncoding, 0);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding,
                           bx + (bw - tw) / 2, by + 6);
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
    int boxW = 220, boxH = 80;
    int bx = (SCREEN_W - boxW) / 2;
    int by = (SCREEN_H - boxH) / 2;
    GFX_FILL(bx, by, boxW, boxH, kColorBlack);
    GFX_RECT(bx, by, boxW, boxH, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    const char* label =
        (game.battleResult == 0) ? "WIN!" :
        (game.battleResult == 1) ? "LOSS" : "DRAW";
    pd->graphics->drawText(label, strlen(label), kASCIIEncoding, bx + 70, by + 12);
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d wins  %d lives",
             game.wins + (game.battleResult == 0 ? 1 : 0),
             WINS_TO_VICTORY,
             game.lives - (game.battleResult == 1 ? 1 : 0));
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, bx + 30, by + 50);
    pd->graphics->setDrawMode(kDrawModeCopy);
}
