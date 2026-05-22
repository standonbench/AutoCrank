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
}

void ui_title_render(void) {
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
    pd->graphics->drawText("AutoCrank", 9, kASCIIEncoding, 130, 60);
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText("a pocket autobattler", 20, kASCIIEncoding, 110, 100);

    if ((game.titleBlinkTimer / 15) & 1) {
        if (game.fontBold) pd->graphics->setFont(game.fontBold);
        pd->graphics->drawText("Press A to play", 15, kASCIIEncoding, 130, 158);
    }
    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    pd->graphics->drawText("B: self-test", 12, kASCIIEncoding, 158, 182);

    if (game.fontSmall) pd->graphics->setFont(game.fontSmall);
    char buf[64];
    snprintf(buf, sizeof(buf), "Runs: %lu  Wins: %lu  Best streak: %lu",
             (unsigned long)game.totalRuns,
             (unsigned long)game.totalWins,
             (unsigned long)game.bestStreak);
    pd->graphics->drawText(buf, strlen(buf), kASCIIEncoding, 70, 210);
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
