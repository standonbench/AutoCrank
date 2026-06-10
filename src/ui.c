#include "game.h"
#include "theme.h"
#include <stdio.h>

// ===========================================================================
// Menu / end-screen presentation — scherenschnitt festival styling.
// All chrome comes from theme.c; this file owns layout + choreography.
// ===========================================================================

// Onboarding flow: true when how-to-play was opened from PLAY on a first run,
// so dismissing it continues into the team-name picker rather than the title.
static bool g_helpThenPlay = false;

// Integer ease-out over a frame window (shared shorthand).
static int ease_out(int from, int to, int t, int dur) {
    if (t <= 0) return from;
    if (t >= dur) return to;
    return th_lerpi(from, to, th_ease_out((float)t / (float)dur));
}

// One-frame full-screen invert — the "impact flash".
static void draw_flash_overlay(bool on) {
    if (!on) return;
    pd->graphics->setDrawMode(kDrawModeInverted);
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorBlack);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

// Deterministic confetti (win celebrations). Stateless: piece index + frame
// count fully determine every position. A few chunky tumbling ribbons,
// diamonds and flakes that sway as they fall — festive, not rain.
static void draw_confetti(int elapsed) {
    const int N = 16;
    for (int i = 0; i < N; i++) {
        int x0 = ((i * 1657 + 83) % SCREEN_W);
        int speed = 2 + ((i * 97) % 3);
        int phase = (i * 213) % 110;
        int t = elapsed + phase;
        int y = ((t * speed) % (SCREEN_H + 16)) - 8;
        int x = x0 + (int)(sinf((float)t * 0.07f + (float)i * 1.7f) * 7.0f);
        float spin = (float)t * 0.12f + (float)i;
        int dx = (int)(cosf(spin) * 4.0f);
        int dy = (int)(sinf(spin) * 2.0f);
        switch (i % 3) {
            case 0:   // tumbling ribbon
                GFX_LINE(x - dx, y - dy, x + dx, y + dy, 2, kColorBlack);
                break;
            case 1: { // diamond
                pd->graphics->fillTriangle(x - 3, y, x + 3, y, x, y - 4,
                                           kColorBlack);
                pd->graphics->fillTriangle(x - 3, y, x + 3, y, x, y + 4,
                                           kColorBlack);
                break;
            }
            default: { // square flake, foreshortened as it tumbles
                int w = 2 + ((dx > 0) ? dx : -dx);
                GFX_RECT(x - w / 2, y - 2, w, 5, kColorBlack);
                break;
            }
        }
    }
}

// Deterministic snowfall (the long walk home). White flakes over a dimmed
// scene; slight sinusoidal drift so it reads as snow, not static.
static void draw_snow(int elapsed) {
    const int N = 28;
    for (int i = 0; i < N; i++) {
        int x0 = (i * 149) % SCREEN_W;
        int speed = 1 + (i % 2);
        int phase = (i * 331) % 160;
        int t = elapsed + phase;
        int y = ((t * speed) % (SCREEN_H + 10)) - 5;
        int x = x0 + (int)(sinf((float)t * 0.045f + (float)i) * 5.0f);
        GFX_FILL(x, y, 2, 2, kColorWhite);
        GFX_RECT(x - 1, y - 1, 4, 4, kColorBlack);
    }
}

// ---------------------------------------------------------------------------
// Title screen — a festival poster. Signpost menu on the right, garlanded
// slab banner up top, grazing herd below, one calf forever walking home.
// titleCursor 0..4 = PLAY / HALL / ROSTER / TEST / HELP.
// ---------------------------------------------------------------------------
void ui_title_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.titleBlinkTimer++;
    if (prs & kButtonUp)
        game.titleCursor = (uint8_t)((game.titleCursor + 4) % 5);
    if (prs & kButtonDown)
        game.titleCursor = (uint8_t)((game.titleCursor + 1) % 5);
    if (prs & kButtonA) {
        switch (game.titleCursor) {
        case 0:                                   // PLAY
            // First-ever PLAY shows the how-to-play once, then continues into
            // the run; afterwards it goes straight to the team-name picker.
            if (!(game.settings & SETTING_HELP_SEEN)) {
                g_helpThenPlay = true;
                th_wipe_start();
                game.state = STATE_HELP;
            } else {
                ui_team_name_enter();
            }
            break;
        case 1: ui_hof_enter();        break;     // HALL OF FAME
        case 2:
            th_wipe_start();
            game.state = STATE_GALLERY;
            break;
        case 3:
            selftest_run();
            game.state = STATE_SELFTEST;
            break;
        case 4:                                   // HELP (re-readable any time)
            g_helpThenPlay = false;
            th_wipe_start();
            game.state = STATE_HELP;
            break;
        }
    }
}

void ui_title_render(void) {
    // Entry clock: detects (re)entry to the title so the furniture can slide
    // in. A gap in frameCount means we were on another screen last frame.
    static uint32_t lastFrame = 0;
    static int entry = 1000;
    if (game.frameCount != lastFrame + 1) entry = 0;
    else if (entry < 1000) entry++;
    lastFrame = game.frameCount;

    // --- backdrop: mountain photo if shipped, else the rich paper-cut scene
    LCDBitmap* bg = image_title_bg();
    if (bg) {
        pd->graphics->drawBitmap(bg, 0, 0, kBitmapUnflipped);
    } else {
        th_backdrop(150, TH_BG_RICH);
        th_chalet(16, 196);
        th_pine(212, 178, 26);
        th_pine(110, 232, 30);
    }

    // --- grazing herd (haloed for legibility over either backdrop) ---
    const float S = 1.4f;
    draw_creature_haloed(13,  60, 175, S, false);   // Highland, left
    draw_creature_haloed( 5, 158, 196, S, true);    // Calf, middle, facing left
    draw_creature_haloed(13, 252, 172, S, false);   // Highland, right

    // --- wildlife on the mountain: a handful of animals bounding across
    //     the ridges in parabolic hops, each on its own long cycle with a
    //     dead phase so they appear only occasionally, from either side.
    //     Plus the original calf, forever walking home.
    {
        typedef struct {
            uint8_t petId;
            int16_t x0;          // where the run starts
            int16_t baseY;       // ridge line the run follows
            int8_t  dir;         // +1 = bounds rightward, -1 = leftward
            uint8_t spdQ4;       // px/frame in 1/16ths
            uint16_t runPx;      // length of the run before vanishing
            uint16_t period;     // full cycle length in frames (30fps)
            uint16_t phase;      // cycle offset so they desynchronise
            uint8_t hopLen;      // px per bound
            uint8_t hopH;        // bound height in px
            uint8_t scaleQ4;     // sprite scale in 1/16ths
        } TitleHopper;
        static const TitleHopper H[4] = {
            { 4, -18, 150, +1, 22, 150,  800,   0, 34, 9, 12 }, // Rabbit
            { 0, 265, 166, -1, 18, 130, 1050, 380, 26, 7, 11 }, // Squirrel
            { 1, 275, 188, -1, 26, 180,  900, 620, 40, 8, 13 }, // Pup
            { 4, -18, 176, +1, 24, 210, 1150, 250, 30, 8, 12 }, // Rabbit
        };
        for (int i = 0; i < 4; i++) {
            const TitleHopper* h = &H[i];
            int t = (int)((game.frameCount + h->phase) % h->period);
            int dist = t * h->spdQ4 / 16;
            if (dist > (int)h->runPx) continue;         // resting off-screen
            int wx = h->x0 + h->dir * dist;
            float hop = sinf(PI_F * (float)(dist % h->hopLen)
                             / (float)h->hopLen);
            int wy = h->baseY - (int)(hop * (float)h->hopH);
            draw_creature_haloed(h->petId, wx, wy,
                                 (float)h->scaleQ4 / 16.0f, h->dir < 0);
        }
        int cycle = (int)(game.frameCount % 2600u);
        int wx = SCREEN_W + 30 - (int)((float)cycle * 0.19f);
        int wy = 157 + (int)(sinf((float)game.frameCount * 0.30f) * 1.2f);
        draw_creature_haloed(5, wx, wy, 0.8f, true);
    }

    // --- signpost menu: a pole of arrow planks pointing into the valley ---
    {
        static const char* items[5] = { "PLAY", "HALL", "ROSTER", "TEST", "HELP" };
        int poleX = 372;
        GFX_FILL(poleX, 58, 5, 166, kColorBlack);                 // pole
        pd->graphics->fillTriangle(poleX - 2, 58, poleX + 7, 58,
                                   poleX + 2, 50, kColorBlack);   // cap
        for (int i = 0; i < 5; i++) {
            int cy = 78 + i * 30;
            // staggered slide-in from the right on entry
            int slide = 0;
            if (entry < 60) {
                float p = ((float)entry - (float)i * 3.0f) / 14.0f;
                slide = th_lerpi(130, 0, th_ease_out(p));
            }
            bool sel = (game.titleCursor == i);
            int nudge = sel ? -4 : 0;
            th_sign_plank(286 + slide + nudge, 380 + slide + nudge, cy,
                          items[i], sel, true);
        }
    }

    // --- title banner: slab type on black, garlands running to the edges ---
    {
        if (game.fontLarge) pd->graphics->setFont(game.fontLarge);
        const char* T = "AUTOCRANK";
        int tW = th_text_w(game.fontLarge, T);
        int bw = tW + 52, bh = 34, bx = (SCREEN_W - bw) / 2;
        int by = 16;
        if (entry < 60) by = ease_out(-50, 16, entry, 16);

        th_pennant_garland(-8, bx + 6, by + bh / 2, 8);
        th_pennant_garland(bx + bw - 6, SCREEN_W + 8, by + bh / 2, 8);

        th_fill(bx + 4, by + 5, bw, bh, TH_PAT_HATCH);             // shadow
        pd->graphics->fillTriangle(bx, by + bh - 12, bx, by + bh,  // ribbon tails
                                   bx - 14, by + bh + 5, kColorBlack);
        pd->graphics->fillTriangle(bx + bw, by + bh - 12, bx + bw, by + bh,
                                   bx + bw + 14, by + bh + 5, kColorBlack);
        GFX_FILL(bx, by, bw, bh, kColorBlack);                     // banner
        GFX_RECT(bx + 3, by + 3, bw - 6, bh - 6, kColorWhite);
        th_text(game.fontLarge, T, (SCREEN_W - tW) / 2, by + 7, true);
        th_chip(SCREEN_W / 2, by + bh + 9, "COMBAT DES REINES", true);
    }

    // --- lifetime stats + control hint along the bottom ---
    char buf[64];
    snprintf(buf, sizeof(buf), "RUNS %lu   WINS %lu   BEST %lu",
             (unsigned long)game.totalRuns,
             (unsigned long)game.totalWins,
             (unsigned long)game.bestStreak);
    th_text_halo(game.fontSmall, buf, 8, 224);
    th_button_glyph(330, 231, 'A', false);
    th_text_halo(game.fontSmall, "choose", 340, 224);
}

// ---------------------------------------------------------------------------
// Roster gallery — the eighteen, one tier per row.
// ---------------------------------------------------------------------------
static void draw_roster_badge(int cx, int cy, bool earned) {
    GFX_ELLIPSE(cx - 4, cy - 4, 9, 9, kColorWhite);
    pd->graphics->drawEllipse(cx - 4, cy - 4, 9, 9, 1, 0, 360, kColorBlack);
    if (earned) {
        pd->graphics->fillTriangle(cx, cy - 3, cx + 3, cy,
                                   cx, cy + 3, kColorBlack);
        pd->graphics->fillTriangle(cx, cy - 3, cx, cy + 3,
                                   cx - 3, cy, kColorBlack);
    } else {
        GFX_ELLIPSE(cx - 1, cy - 1, 3, 3, kColorBlack);
    }
}

void ui_gallery_render(void) {
    GFX_FILL(0, 0, SCREEN_W, SCREEN_H, kColorWhite);

    // header band with a white pinline
    GFX_FILL(0, 0, SCREEN_W, 22, kColorBlack);
    GFX_LINE(0, 21, SCREEN_W, 21, 1, kColorWhite);
    th_text_center(game.fontBold, "THE ROSTER", SCREEN_W / 2, 3, true);
    th_edelweiss(SCREEN_W / 2 - 78, 11, 3);
    th_edelweiss(SCREEN_W / 2 + 78, 11, 3);

    // 6 columns x 3 rows — one tier per row, tier tab on the left margin.
    for (int row = 0; row < 3; row++) {
        int y = 28 + row * 64;
        char tier[4];
        snprintf(tier, sizeof(tier), "T%d", row + 1);
        GFX_FILL(0, y + 10, 20, 20, kColorBlack);
        pd->graphics->fillTriangle(20, y + 10, 26, y + 20, 20, y + 30,
                                   kColorBlack);
        th_text(game.fontSmall, tier, 3, y + 12, true);
    }
    for (int i = 0; i < PET_COUNT; i++) {
        int col = i % 6, row = i / 6;
        int cx = 30 + col * 61;
        int cy = 28 + row * 64;
        th_fill(cx + 2, cy + 2, 56, 48, TH_PAT_HATCH);
        GFX_FILL(cx, cy, 56, 48, kColorWhite);
        GFX_RECT(cx, cy, 56, 48, kColorBlack);
        draw_creature((uint8_t)i, cx + 12, cy + 8);
        const char* nm = pet_name((uint8_t)i);
        int w = th_text_w(game.fontSmall, nm);
        th_text(game.fontSmall, nm, cx + (56 - w) / 2, cy + 49, false);

        if (game.petWinsAtMax[i] >= 1)
            draw_roster_badge(cx + 48, cy + 7, true);
        else if (game.petMaxLevel[i] >= MAX_PET_LEVEL)
            draw_roster_badge(cx + 48, cy + 7, false);
    }
    th_hint_bar(NULL, "back", "badges: reach L3, then win with it");
}

// ---------------------------------------------------------------------------
// Game over — snowfall, a lone cow, a carved memorial.
// ---------------------------------------------------------------------------
void ui_gameover_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.resultTimer++;
    if (game.resultTimer > 30 && (prs & kButtonA)) {
        game.inRun = false;
        th_wipe_start();
        game.state = STATE_TITLE;
        save_write();
    }
}

void ui_gameover_render(void) {
    th_backdrop(150, TH_BG_CLOUDS);                 // overcast: no sun
    th_fill(0, 0, SCREEN_W, SCREEN_H, TH_PAT_SCRIM);  // dusk falls
    int elapsed = (int)game.resultTimer;

    // Curtain band drops from the top over frames 0..15, then stays.
    int wipeH = 44;
    int wipeY = (elapsed < 15) ? -wipeH + (wipeH + 4) * elapsed / 15 : 0;
    GFX_FILL(0, wipeY, SCREEN_W, wipeH, kColorBlack);
    GFX_LINE(0, wipeY + wipeH, SCREEN_W, wipeY + wipeH, 1, kColorWhite);

    // "GAME OVER" stamps in with overshoot.
    if (elapsed >= 2) {
        float p = (elapsed < 22) ? ((float)(elapsed - 2) / 20.0f) : 1.0f;
        th_text_scaled(game.fontLarge, "GAME OVER",
                       SCREEN_W / 2, 22, th_ease_back(p, 1.5f), true);
    }

    // The last hen drops in from above at frame 15.
    if (elapsed >= 15) {
        int y = ease_out(-50, 78, elapsed - 15, 18);
        draw_creature_scaled(2, SCREEN_W / 2 - 32, y, 2);
    }

    // Memorial plank slides up from below at frame 35 — name, epitaph,
    // and the summits they did climb, all carved on one board.
    if (elapsed >= 35) {
        const int cardW = 260, cardH = 70;
        int y = ease_out(SCREEN_H + 4, 150, elapsed - 35, 20);
        int x = SCREEN_W / 2 - cardW / 2;
        th_panel(x, y, cardW, cardH);
        th_chip(SCREEN_W / 2, y, "R.I.P.", true);
        char nameBuf[40];
        team_name_str(game.playerName.adj, game.playerName.noun,
                      nameBuf, sizeof(nameBuf));
        th_text_center(game.fontBold, nameBuf, SCREEN_W / 2, y + 13, false);
        th_edelweiss(x + 18, y + 24, 4);
        th_edelweiss(x + cardW - 18, y + 24, 4);
        if (elapsed >= 55) {
            char buf[48];
            snprintf(buf, sizeof(buf), "fell %d summit%s short",
                     WINS_TO_VICTORY - (int)game.wins,
                     (WINS_TO_VICTORY - (int)game.wins) == 1 ? "" : "s");
            th_text_center(game.fontSmall, buf, SCREEN_W / 2, y + 31, false);
            th_peak_meter(SCREEN_W / 2, y + 60, (int)game.wins,
                          WINS_TO_VICTORY, 9, false);
        }
    }
    if (elapsed >= 60 && ((game.frameCount / 15) & 1))
        th_text_halo_center(game.fontBold, "PRESS A", SCREEN_W / 2, 226);

    draw_snow(elapsed);
}

// ---------------------------------------------------------------------------
// Victory — rosettes, garlands, confetti, and the queen herself.
// ---------------------------------------------------------------------------
void ui_victory_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    game.resultTimer++;
    if (game.resultTimer > 30 && (prs & kButtonA)) {
        game.inRun = false;
        th_wipe_start();
        game.state = STATE_TITLE;
        save_write();
    }
}

// Deterministic winking sparkles around the champion.
static void draw_sparkles(int cx, int cy, int rOuter, int t) {
    const int N = 12;
    for (int i = 0; i < N; i++) {
        float a = (float)i * (2.0f * PI_F / (float)N);
        int r = rOuter - ((i * 13) % 18);
        int x = cx + (int)(cosf(a) * (float)r);
        int y = cy + (int)(sinf(a) * (float)(r * 6 / 10));
        int phase = (i * 7) % 24;
        if (((t + phase) % 24) < 8)
            GFX_FILL(x, y, 2, 2, kColorBlack);
    }
}

void ui_victory_render(void) {
    th_backdrop(154, TH_BG_RICH);
    int elapsed = (int)game.resultTimer;

    int cowCx = SCREEN_W / 2;
    int cs = 3, cowPx = 32 * cs;
    int cyTarget = 60;                       // sprite top once seated
    int cowMidY = cyTarget + cowPx / 2;      // sprite centre

    // ---- frame 12+: festival medallion behind the queen — a white disc
    //      with slowly turning sunburst rays, so she's presented on a
    //      stage instead of pasted onto the pasture ----
    if (elapsed >= 12) {
        int rx = 100, ry = 54;
        GFX_ELLIPSE(cowCx - rx, cowMidY - ry, rx * 2, ry * 2, kColorWhite);
        float a0 = (float)game.frameCount * 0.005f;
        for (int i = 0; i < 9; i++) {
            float a = a0 + (float)i * (2.0f * PI_F / 9.0f);
            float b = a + (2.0f * PI_F / 22.0f);
            int x1 = cowCx + (int)(cosf(a) * (float)(rx - 4));
            int y1 = cowMidY + (int)(sinf(a) * (float)(ry - 3));
            int x2 = cowCx + (int)(cosf(b) * (float)(rx - 4));
            int y2 = cowMidY + (int)(sinf(b) * (float)(ry - 3));
            pd->graphics->fillTriangle(cowCx, cowMidY, x1, y1, x2, y2,
                                       (LCDColor)TH_PAT_25);
        }
        pd->graphics->drawEllipse(cowCx - rx, cowMidY - ry, rx * 2, ry * 2,
                                  2, 0, 360, kColorBlack);
    }

    // ---- frame 0-10: diagonal banner wipe leaves the black title band ----
    int wipeW = SCREEN_W + SCREEN_H;
    int wipeProgress = (elapsed < 10) ? (wipeW * elapsed / 10) : wipeW;
    for (int y = 0; y < 44; y++) {
        int xEnd = wipeProgress - y;
        if (xEnd > SCREEN_W) xEnd = SCREEN_W;
        if (xEnd > 0) GFX_FILL(0, y, xEnd, 1, kColorBlack);
    }
    if (elapsed >= 10) {
        GFX_LINE(0, 44, SCREEN_W, 44, 1, kColorWhite);
        th_pennant_garland(-8, SCREEN_W + 8, 46, 7);
    }

    // ---- frame 5-20: LA REINE! stamps in ----
    if (elapsed >= 5) {
        float p = (elapsed < 20) ? ((float)(elapsed - 5) / 15.0f) : 1.0f;
        th_text_scaled(game.fontLarge, "LA REINE!",
                       SCREEN_W / 2, 22, th_ease_back(p, 1.8f), true);
    }

    // ---- frame 15-35: podium rises, then the queen onto it ----
    if (elapsed >= 15) {
        int pw = 150, ph = 11;
        int py = cyTarget + cowPx - 6;
        th_fill(cowCx - pw / 2 + 3, py + 3, pw, ph, TH_PAT_HATCH);
        GFX_FILL(cowCx - pw / 2, py, pw, ph, kColorWhite);
        GFX_RECT(cowCx - pw / 2, py, pw, ph, kColorBlack);
        th_fill(cowCx - pw / 2 + 2, py + ph - 4, pw - 4, 3, TH_PAT_25);
    }
    int cyTop = (elapsed >= 15)
                ? ease_out(SCREEN_H + 10, cyTarget, elapsed - 15, 20)
                : SCREEN_H + 10;
    draw_creature_scaled(13, cowCx - cowPx / 2, cyTop, cs);

    // ---- frame 28-42: the winner's flower garland blooms around her
    //      chest, one edelweiss at a time (Combat des Reines style) ----
    if (elapsed >= 28) {
        int gcy = cowMidY + 8;
        for (int i = 0; i < 7; i++) {
            if (elapsed < 28 + i * 2) break;
            float a = PI_F * (float)i / 6.0f;       // lower half-ellipse
            int fx = cowCx + (int)(cosf(a) * 38.0f);
            int fy = gcy + (int)(sinf(a) * 20.0f);
            th_edelweiss(fx, fy, i == 3 ? 5 : 4);   // centre bloom largest
        }
    }

    // ---- frame 22-34: prize rosettes stamp in on the medallion rim ----
    if (elapsed >= 22) {
        float p = (elapsed < 34) ? ((float)(elapsed - 22) / 12.0f) : 1.0f;
        int rr = (int)(16.0f * th_ease_back(p, 1.6f));
        if (rr > 2) {
            th_rosette(96,  92, rr);
            th_rosette(304, 92, rr);
        }
    }

    // ---- frame 18+: QUEEN OF THE ALPS sub-banner under the title band ----
    if (elapsed >= 18)
        th_chip(SCREEN_W / 2, 52, "QUEEN OF THE ALPS", true);

    // ---- frame 35-55: champions plate slides up ----
    if (elapsed >= 35) {
        const int cardW = 250, cardH = 54;
        int y = ease_out(SCREEN_H + 4, 168, elapsed - 35, 20);
        int x = SCREEN_W / 2 - cardW / 2;
        th_panel(x, y, cardW, cardH);
        th_chip(SCREEN_W / 2, y, "CHAMPIONS", true);
        char nameBuf[40];
        team_name_str(game.playerName.adj, game.playerName.noun,
                      nameBuf, sizeof(nameBuf));
        th_text_center(game.fontBold, nameBuf, SCREEN_W / 2, y + 13, false);
        th_peak_meter(SCREEN_W / 2, y + 46, WINS_TO_VICTORY, WINS_TO_VICTORY,
                      11, false);
    }

    // ---- frame 45+: confetti, then sparkles winking on the medallion ----
    if (elapsed >= 45) draw_confetti(elapsed);
    if (elapsed >= 60) draw_sparkles(cowCx, cowMidY - 4, 70, elapsed);

    // ---- frame 70+: PRESS A blink ----
    if (elapsed >= 70 && ((game.frameCount / 15) & 1))
        th_text_halo_center(game.fontBold, "PRESS A", SCREEN_W / 2, 226);
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
// Pre-battle intro animation: overlay on top of the live battle line-up.
//
//   0–18  ACT 1  player plate slides in from off-left
//  12–30  ACT 2  challenger plate slides in from off-right
//  24–36  ACT 3  the VS rosette stamps in centre, 1-frame impact flash
//  36–75  HOLD   everything sits steady (reading time)
//  75–90  EXIT   plates slide back off; the rosette shrinks away
// ---------------------------------------------------------------------------
static void draw_intro_animation(int elapsed) {
    char playerBuf[40], enemyBuf[40];
    team_name_str(game.playerName.adj, game.playerName.noun,
                  playerBuf, sizeof(playerBuf));
    team_name_str(game.enemyName.adj, game.enemyName.noun,
                  enemyBuf, sizeof(enemyBuf));

    const int bannerW = 178, bannerH = 46;
    const int leftTargetX  = 8;
    const int rightTargetX = SCREEN_W - bannerW - 8;
    const int bannerY = 58;

    // ---- ACT 1: player plate from off-left ----
    {
        int startX = -bannerW - 8;
        int x = ease_out(startX, leftTargetX, elapsed, 18);
        if (elapsed >= 75) x = ease_out(leftTargetX, startX, elapsed - 75, 15);
        th_panel(x, bannerY, bannerW, bannerH);
        th_text_center(game.fontSmall, "YOUR HERD",
                       x + bannerW / 2, bannerY + 6, false);
        th_text_center(game.fontLarge, playerBuf,
                       x + bannerW / 2, bannerY + 20, false);
    }

    // ---- ACT 2: challenger plate from off-right ----
    if (elapsed >= 12) {
        int startX = SCREEN_W + 8;
        int x = ease_out(startX, rightTargetX, elapsed - 12, 18);
        if (elapsed >= 75) x = ease_out(rightTargetX, startX, elapsed - 75, 15);
        th_panel_dark(x, bannerY, bannerW, bannerH);
        th_text_center(game.fontSmall, "THE CHALLENGER",
                       x + bannerW / 2, bannerY + 6, true);
        th_text_center(game.fontLarge, enemyBuf,
                       x + bannerW / 2, bannerY + 20, true);
    }

    // ---- ACT 3: VS rosette stamped between the plates ----
    if (elapsed >= 24) {
        int t = elapsed - 24;
        float p = (t > 12) ? 1.0f : ((float)t / 12.0f);
        float scale = th_ease_back(p, 1.7f);
        if (elapsed >= 75) {
            float q = (float)(elapsed - 75) / 15.0f;
            if (q > 1.0f) q = 1.0f;
            scale = 1.0f - q;
        }
        int cx = SCREEN_W / 2;
        int cy = bannerY + bannerH / 2;
        int rr = (int)(24.0f * scale);
        if (rr > 2) {
            th_rosette(cx, cy, rr);
            // clean white face for the lettering (the rosette core is black)
            int fr = (rr * 7) / 8 - 2;
            if (fr > 2) GFX_ELLIPSE(cx - fr, cy - fr, fr * 2, fr * 2,
                                    kColorWhite);
            th_text_scaled(game.fontLarge, "VS", cx, cy, scale * 0.8f, false);
        }
        if (t == 12) draw_flash_overlay(true);
    }
}

void ui_pre_battle_render(void) {
    // The live battle scene shows through — the intro is purely an overlay.
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

// Spray 4 small diamonds outward from (cx, cy) — the heart-shatter on a loss.
static void draw_shatter(int cx, int cy, int t) {
    if (t < 0 || t > 14) return;
    int r = (t * 14) / 14 + 4;
    const int dirs[4][2] = {{-1,-1},{1,-1},{-1,1},{1,1}};
    for (int i = 0; i < 4; i++) {
        int x = cx + dirs[i][0] * r;
        int y = cy + dirs[i][1] * r;
        pd->graphics->fillTriangle(x, y - 3, x + 3, y, x, y + 3, kColorWhite);
        pd->graphics->fillTriangle(x, y - 3, x, y + 3, x - 3, y, kColorWhite);
    }
}

void ui_result_render(void) {
    th_backdrop(90, TH_BG_CLOUDS);
    render_topbar();

    int elapsed = (int)game.resultTimer;
    int by = 38, bh = 170;

    // Outcome-specific weather behind the banner.
    if (game.battleResult == 0) {
        draw_confetti(elapsed);
    } else if (game.battleResult == 1) {
        th_fill(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H, TH_PAT_SCRIM);
    }

    // Banner: black band, white pinlines, settles down 10px on entry.
    int yoff = (elapsed < 8) ? (-10 + (elapsed * 10) / 8) : 0;
    GFX_FILL(0, by + yoff, SCREEN_W, bh, kColorBlack);
    GFX_LINE(0, by + yoff,      SCREEN_W, by + yoff,      2, kColorWhite);
    GFX_LINE(0, by + bh + yoff, SCREEN_W, by + bh + yoff, 2, kColorWhite);
    // centre diamonds on the pinlines — a carved detail
    pd->graphics->fillTriangle(SCREEN_W / 2 - 5, by + yoff,
                               SCREEN_W / 2 + 5, by + yoff,
                               SCREEN_W / 2, by + yoff + 6, kColorWhite);
    pd->graphics->fillTriangle(SCREEN_W / 2 - 5, by + bh + yoff,
                               SCREEN_W / 2 + 5, by + bh + yoff,
                               SCREEN_W / 2, by + bh + yoff - 6, kColorWhite);

    // Title — stamps in with overshoot; rosettes flank a VICTORY.
    const char* label = (game.battleResult == 0) ? "VICTORY"
                      : (game.battleResult == 1) ? "DEFEAT"
                                                 : "STALEMATE";
    float p = (elapsed < 15) ? ((float)elapsed / 15.0f) : 1.0f;
    float scale = th_ease_back(p, 1.6f);
    th_text_scaled(game.fontLarge, label,
                   SCREEN_W / 2, by + 28 + yoff, scale, true);
    if (game.battleResult == 0 && elapsed >= 8) {
        float rp = (elapsed < 20) ? ((float)(elapsed - 8) / 12.0f) : 1.0f;
        int rr = (int)(13.0f * th_ease_back(rp, 1.6f));
        if (rr > 2) {
            // white-on-black rosettes: draw a white disc backing first
            GFX_ELLIPSE(60 - rr - 2, by + 28 - rr - 2 + yoff,
                        rr * 2 + 4, rr * 2 + 4, kColorWhite);
            GFX_ELLIPSE(SCREEN_W - 60 - rr - 2, by + 28 - rr - 2 + yoff,
                        rr * 2 + 4, rr * 2 + 4, kColorWhite);
            th_rosette(60, by + 28 + yoff, rr);
            th_rosette(SCREEN_W - 60, by + 28 + yoff, rr);
        }
    }

    // One-frame impact flash on stamp landing.
    if (elapsed == 6) draw_flash_overlay(true);

    // ---- WINS: label + the ten summits, big ----
    if (elapsed >= 15) {
        int wins = game.wins;   // already tallied in game_apply_battle_result
        char winsBuf[24];
        snprintf(winsBuf, sizeof(winsBuf), "SUMMITS  %d / %d", wins,
                 WINS_TO_VICTORY);
        th_text_center(game.fontBold, winsBuf, SCREEN_W / 2, by + 50, true);
        th_peak_meter(SCREEN_W / 2, by + 92, wins, WINS_TO_VICTORY, 14, true);
        // Pulse the freshly-climbed summit on a win.
        if (game.battleResult == 0 && wins > 0) {
            int pulseT = elapsed - 15;
            if (pulseT < 30 && (pulseT / 5) % 2 == 0) {
                int x0 = SCREEN_W / 2 - (WINS_TO_VICTORY * 14) / 2;
                int sx = x0 + (wins - 1) * 14;
                pd->graphics->drawRect(sx - 2, by + 92 - 20, 18, 24,
                                       kColorWhite);
            }
        }
    }

    // ---- LIVES: label + hearts ----
    if (elapsed >= 15) {
        th_text_center(game.fontBold, "LIVES", SCREEN_W / 2, by + 104, true);
        int lives = game.lives;
        int hStep = 28;
        int hStartX = (SCREEN_W - (STARTING_LIVES - 1) * hStep) / 2;
        int hY = by + 132;
        for (int i = 0; i < STARTING_LIVES; i++)
            th_heart(hStartX + i * hStep, hY, i < lives, true);
        if (game.battleResult == 1 && lives < STARTING_LIVES)
            draw_shatter(hStartX + lives * hStep, hY, elapsed - 15);
    }

    // PRESS A blink — bottom of the banner.
    if (elapsed > 45 && ((game.frameCount / 15) & 1))
        th_text_center(game.fontBold, "PRESS A", SCREEN_W / 2, by + 148, true);
}

// ---------------------------------------------------------------------------
// Team-name picker — SAP-style 3-of-each, adj + noun, on a hanging sign.
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
    th_wipe_start();
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
            th_wipe_start();
            game.state = STATE_TITLE;
        }
    }
}

// Pill states for the name picker:
//   hovered    — the cursor is on this pill; draws a 2px outer ring
//   confirmed  — the player A'd this pill; fills black with white text
//   neither    — default white card with hatch shadow
static void draw_pill(int cx, int cy, int w, int h, const char* label,
                      bool hovered, bool confirmed) {
    int x = cx - w / 2, y = cy - h / 2;
    if (confirmed) {
        GFX_FILL(x, y, w, h, kColorBlack);
    } else {
        th_fill(x + 2, y + 2, w, h, TH_PAT_HATCH);
        GFX_FILL(x, y, w, h, kColorWhite);
        GFX_RECT(x, y, w, h, kColorBlack);
    }
    if (hovered) {
        GFX_RECT(x - 3, y - 3, w + 6, h + 6, kColorBlack);
        GFX_RECT(x - 2, y - 2, w + 4, h + 4, kColorBlack);
    }
    if (game.fontBold) pd->graphics->setFont(game.fontBold);
    int tw = th_text_w(game.fontBold, label);
    th_text(game.fontBold, label, cx - tw / 2 - 1, cy - 8, confirmed);
}

void ui_team_name_render(void) {
    LCDBitmap* bg = image_name_bg();
    if (!bg) bg = image_title_bg();
    if (bg) pd->graphics->drawBitmap(bg, 0, 0, kBitmapUnflipped);
    else    th_backdrop(150, TH_BG_RICH);

    // Header band.
    GFX_FILL(0, 0, SCREEN_W, 26, kColorBlack);
    GFX_LINE(0, 25, SCREEN_W, 25, 1, kColorWhite);
    th_text_center(game.fontBold, "NAME YOUR HERD", SCREEN_W / 2, 5, true);
    th_cowbell_icon(SCREEN_W / 2 - 116, 13);
    th_cowbell_icon(SCREEN_W / 2 + 116, 13);

    // The sign-in-progress hangs from the band on two chains.
    char buf[40];
    team_name_str(game.nameAdjChoices[game.nameAdjPick],
                  game.nameNounChoices[game.nameNounPick],
                  buf, sizeof(buf));
    int cw = 290, ch = 34, cx = SCREEN_W / 2, cy = 56;
    GFX_LINE(cx - 90, 26, cx - 96, cy - ch / 2, 2, kColorBlack);
    GFX_LINE(cx + 90, 26, cx + 96, cy - ch / 2, 2, kColorBlack);
    th_panel(cx - cw / 2, cy - ch / 2, cw, ch);
    th_text_center(game.fontLarge, buf, cx, cy - 9, false);

    int rowAdjY = 116, rowNounY = 166;
    int pillW = 100, pillH = 22, pillGap = 10;
    int totalW = pillW * 3 + pillGap * 2;
    int leftX  = (SCREEN_W - totalW) / 2 + pillW / 2;

    int labelX = leftX - pillW / 2;
    th_text_halo(game.fontSmall, "ADJECTIVE", labelX, rowAdjY - pillH / 2 - 16);
    th_text_halo(game.fontSmall, "NOUN",      labelX, rowNounY - pillH / 2 - 16);
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

    if (!game.nameAdjConfirmed && !game.nameNounConfirmed)
        th_hint_bar("select", "back", NULL);
    else if (game.nameAdjConfirmed && game.nameNounConfirmed)
        th_hint_bar("start the run!", "unselect", NULL);
    else
        th_hint_bar("confirm", "unselect", NULL);
}

// ---------------------------------------------------------------------------
// Hall of Fame — scrollable ledger of past 10-win runs.
// ---------------------------------------------------------------------------
void ui_hof_enter(void) {
    game.hofCursor = 0;
    th_wipe_start();
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
        if (prs & (kButtonA | kButtonB)) {
            th_wipe_start();
            game.state = STATE_TITLE;
        }
        return;
    }
    if (prs & kButtonUp) {
        if (game.hofCursor > 0) game.hofCursor--;
    }
    if (prs & kButtonDown) {
        if (game.hofCursor + 1 < game.hofCount)
            game.hofCursor++;
    }
    if (prs & kButtonB) {
        th_wipe_start();
        game.state = STATE_TITLE;
    }
}

void ui_hof_render(void) {
    GFX_CLEAR(kColorWhite);

    GFX_FILL(0, 0, SCREEN_W, 26, kColorBlack);
    GFX_LINE(0, 25, SCREEN_W, 25, 1, kColorWhite);
    th_text_center(game.fontBold, "HALL OF FAME", SCREEN_W / 2, 5, true);
    // tiny white rosettes flanking the title
    for (int s = -1; s <= 1; s += 2) {
        int rx = SCREEN_W / 2 + s * 96;
        GFX_ELLIPSE(rx - 7, 6, 14, 14, kColorWhite);
        pd->graphics->drawEllipse(rx - 5, 8, 10, 10, 1, 0, 360, kColorBlack);
        GFX_ELLIPSE(rx - 2, 11, 4, 4, kColorBlack);
    }

    if (game.hofCount == 0) {
        th_panel(70, 78, 260, 84);
        th_text_center(game.fontBold, "no champions yet",
                       SCREEN_W / 2, 92, false);
        th_text_center(game.fontSmall, "win 10 battles to carve",
                       SCREEN_W / 2, 114, false);
        th_text_center(game.fontSmall, "your herd's name here",
                       SCREEN_W / 2, 128, false);
        th_peak_meter(SCREEN_W / 2, 156, 0, WINS_TO_VICTORY, 11, false);
        th_hint_bar(NULL, "back", NULL);
        return;
    }

    int firstShown = game.hofCursor - 1;
    if (firstShown < 0) firstShown = 0;
    if (firstShown + 3 > game.hofCount) firstShown = game.hofCount - 3;
    if (firstShown < 0) firstShown = 0;

    int rowH = 56, rowTop = 32;
    for (int i = 0; i < 3 && firstShown + i < game.hofCount; i++) {
        int n = firstShown + i;
        int idx = hof_index_for_n(n);
        if (idx < 0) continue;
        WinRecord* r = &game.hof[idx];
        int y = rowTop + i * (rowH + 4);
        bool sel = (n == game.hofCursor);

        th_fill(8, y + 2, SCREEN_W - 14, rowH, TH_PAT_HATCH);
        GFX_FILL(6, y, SCREEN_W - 12, rowH, kColorWhite);
        GFX_RECT(6, y, SCREEN_W - 12, rowH, kColorBlack);
        if (sel) {
            GFX_RECT(7, y + 1, SCREEN_W - 14, rowH - 2, kColorBlack);
            pd->graphics->fillTriangle(0, y + rowH / 2 - 6, 0, y + rowH / 2 + 6,
                                       6, y + rowH / 2, kColorBlack);
        }

        for (int s = 0; s < TEAM_SIZE; s++) {
            int sx = 16 + s * 36;
            if (r->pets[s] < PET_COUNT)
                draw_creature_xform(r->pets[s], sx + 16, y + 26, 1.0f,
                                    0.0f, false);
        }

        char nameBuf[40];
        team_name_str(r->name.adj, r->name.noun, nameBuf, sizeof(nameBuf));
        th_text(game.fontBold, nameBuf, 204, y + 8, false);
        char meta[24];
        snprintf(meta, sizeof(meta), "DAY %u", r->turn);
        th_chip(240, y + 40, meta, true);
        th_rosette(SCREEN_W - 30, y + rowH / 2 - 4, 11);
    }

    char nav[24];
    snprintf(nav, sizeof(nav), "%u / %u",
             (unsigned)(game.hofCursor + 1), (unsigned)game.hofCount);
    th_hint_bar(NULL, "back", nav);
}

// ---------------------------------------------------------------------------
// How-to-play — shown once on the first PLAY (then continues into the run),
// and re-readable any time from the title HELP entry.
// ---------------------------------------------------------------------------
static const char* HELP_LINES[] = {
    "A: buy a pet (3 gold) - fill your 5 slots.",
    "Buy a 2nd of the same pet to merge + level up.",
    "Hold a team pet with A, then crank to reorder.",
    "B: freeze a shop slot, or sell a team pet.",
    "Roll (1 gold) to refresh the shop.",
    "Battles auto-play - your RIGHT-most pet leads.",
    "Win 10 battles to take the run. You get 5 lives.",
    "In battle: crank or hold A to fast-forward.",
};
#define HELP_LINE_COUNT ((int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0])))

void ui_help_update(void) {
    PDButtons cur, prs, rel;
    pd->system->getButtonState(&cur, &prs, &rel);
    if (prs & kButtonA) {
        game.settings |= SETTING_HELP_SEEN;
        save_write();
        if (g_helpThenPlay) { g_helpThenPlay = false; ui_team_name_enter(); }
        else {
            th_wipe_start();
            game.state = STATE_TITLE;
        }
    } else if (prs & kButtonB) {
        game.settings |= SETTING_HELP_SEEN;
        save_write();
        g_helpThenPlay = false;
        th_wipe_start();
        game.state = STATE_TITLE;
    }
}

void ui_help_render(void) {
    LCDBitmap* bg = image_title_bg();
    if (bg) pd->graphics->drawBitmap(bg, 0, 0, kBitmapUnflipped);
    else    th_backdrop(150, TH_BG_CLOUDS);

    int px = 12, py = 12, pw = SCREEN_W - 24, ph = SCREEN_H - 34;
    th_panel(px, py, pw, ph);

    // Title strip inside the panel.
    GFX_FILL(px + 4, py + 4, pw - 8, 22, kColorBlack);
    th_text_center(game.fontBold, "HOW TO PLAY", SCREEN_W / 2, py + 7, true);

    // Lines, each with a tiny edelweiss-dot bullet.
    int ly = py + 33;
    for (int i = 0; i < HELP_LINE_COUNT; i++) {
        GFX_ELLIPSE(px + 12, ly + 5, 4, 4, kColorBlack);
        pd->graphics->drawEllipse(px + 10, ly + 3, 8, 8, 1, 0, 360,
                                  kColorBlack);
        th_text(game.fontSmall, HELP_LINES[i], px + 24, ly, false);
        ly += 17;
    }

    if (g_helpThenPlay) th_hint_bar("let's go!", NULL, NULL);
    else                th_hint_bar("back", NULL, NULL);
}
