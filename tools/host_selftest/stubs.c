// Host-side stubs for the AutoCrank selftest harness.
//
// The selftest only needs the rebalanced game logic — pet ability handlers,
// trigger drain, battle sim, shop/buy logic. It does NOT need rendering,
// sound, save/load, or the live event loop. We stub all of that so we can
// compile and run the test suite as a plain Mac binary.
//
// `pd` is a real PlaydateAPI* but only system->logToConsole is wired up.
// Calls into the other subsystems would segfault — none of the selftest paths
// reach them.

#include "pd_api.h"
#include "game.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Globals normally defined in game.c / main.c
// ---------------------------------------------------------------------------
PlaydateAPI* pd = NULL;
Game game;
uint32_t _rng_state = 1;

// ---------------------------------------------------------------------------
// render.c — popup queue + slot position helpers. No drawing.
// ---------------------------------------------------------------------------
const int SHOP_COL_X[3] = { 36, 92, 148 };
const int TEAM_COL_X[5] = { 36, 92, 148, 204, 260 };
const int FOOD_COL_X[2] = { 322, 366 };

void shop_popup_text(int cx, int cy, const char* s) { (void)cx; (void)cy; (void)s; }
void shop_popup_burst(int cx, int cy)               { (void)cx; (void)cy; }
void shop_popup_clear(void)                          {}

void render_battle_reset_positions(void) {}
void render_battle_reset_slot_position(int side, int slot) { (void)side; (void)slot; }

// ---------------------------------------------------------------------------
// sound.c — no audio in the harness.
// ---------------------------------------------------------------------------
void sound_init(void) {}
void sfx_buy(void)    {}
void sfx_sell(void)   {}
void sfx_roll(void)   {}
void sfx_attack(void) {}
void sfx_faint(void)  {}
void sfx_win(void)    {}
void sfx_lose(void)   {}

// ---------------------------------------------------------------------------
// save.c — no persistence.
// ---------------------------------------------------------------------------
void save_load(void) {}
void save_write(void) {}

// ---------------------------------------------------------------------------
// game.c — minimal reproduction of game_after_battle so T24/T25 state-machine
// tests still pass. The real version writes to disk and ends the turn; here
// we only need the win/loss outcome → state transition.
// ---------------------------------------------------------------------------
void game_after_battle(void) {
    game.battle.inBattle = false;
    if (game.battleResult == 0) {
        game.wins++;
        if (game.wins > game.bestStreak) game.bestStreak = game.wins;
        if (game.wins >= WINS_TO_VICTORY) { game.state = STATE_VICTORY; return; }
    } else if (game.battleResult == 1) {
        if (game.lives > 0) game.lives--;
        if (game.lives == 0) { game.state = STATE_GAMEOVER; return; }
    }
    game.state = STATE_SHOP;
}

// ---------------------------------------------------------------------------
// PlaydateAPI stub — only logToConsole is real. Everything else is NULL.
// Tests that wander into another pd-> call will segfault visibly, which is
// the right failure mode for "you reached rendering from a unit test."
// ---------------------------------------------------------------------------
static void host_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
}

static struct playdate_sys sys_stub;
static PlaydateAPI         api_stub;

void host_setup_pd(void) {
    memset(&sys_stub, 0, sizeof(sys_stub));
    memset(&api_stub, 0, sizeof(api_stub));
    sys_stub.logToConsole = host_log;
    sys_stub.error        = host_log;
    api_stub.system       = &sys_stub;
    pd                    = &api_stub;
}
