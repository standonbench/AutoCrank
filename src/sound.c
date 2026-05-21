#include "game.h"

// SFX stubs — wire real PDSynths in the sound pass.
// For now, all calls are no-ops so the rest of the code compiles + runs silently.

void sound_init(void) {}

void sfx_buy(void)    {}
void sfx_sell(void)   {}
void sfx_roll(void)   {}
void sfx_attack(void) {}
void sfx_faint(void)  {}
void sfx_win(void)    {}
void sfx_lose(void)   {}
