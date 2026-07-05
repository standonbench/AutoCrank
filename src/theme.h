#ifndef THEME_H
#define THEME_H

// ===========================================================================
// AutoCrank visual theme — "Scherenschnitt festival".
//
// One shared vocabulary for every screen: Swiss paper-cut silhouettes
// (mountains, pines, chalets) + Combat-des-Reines festival chrome (pennant
// garlands, prize rosettes, carved wooden planks, ridge-cut black bands).
// All 1-bit, all procedural, all animated off game.frameCount — nothing in
// here may ever touch the simulation RNG (_rng_state).
// ===========================================================================

#include "game.h"

// ---------------------------------------------------------------------------
// Dither swatches — the five "greys" of the game + a diagonal hatch.
// ---------------------------------------------------------------------------
extern LCDPattern TH_PAT_06;     // breath of grey — sky
extern LCDPattern TH_PAT_12;     // light — pasture tint, far ridge
extern LCDPattern TH_PAT_25;     // mid-light — soft shading
extern LCDPattern TH_PAT_50;     // checker — stone, shadows
extern LCDPattern TH_PAT_75;     // dark — forest
extern LCDPattern TH_PAT_HATCH;  // diagonal hatch — drop shadows
extern LCDPattern TH_PAT_SCRIM;  // masked 50% darken overlay (dusk/defeat)

void th_fill(int x, int y, int w, int h, LCDPattern pat);

// ---------------------------------------------------------------------------
// Easing + animation helpers (pure float math, no state).
// ---------------------------------------------------------------------------
float th_ease_out(float p);                    // quadratic ease-out
float th_ease_in_out(float p);                 // smoothstep
float th_ease_back(float p, float overshoot);  // settles past 1.0 ("stamp")
int   th_lerpi(int from, int to, float p);     // integer lerp on eased p

// ---------------------------------------------------------------------------
// Text — draw-mode handled internally, mode restored to Copy afterwards.
// ---------------------------------------------------------------------------
int  th_text_w(LCDFont* f, const char* s);
void th_text(LCDFont* f, const char* s, int x, int y, bool white);
void th_text_center(LCDFont* f, const char* s, int cx, int y, bool white);
void th_text_halo(LCDFont* f, const char* s, int x, int y);
void th_text_halo_center(LCDFont* f, const char* s, int cx, int y);
// Scaled stamp text (renders to a scratch bitmap; keep off hot paths).
void th_text_scaled(LCDFont* f, const char* s, int cx, int cy,
                    float scale, bool white);

// ---------------------------------------------------------------------------
// Panels & chrome.
// ---------------------------------------------------------------------------
// White panel with chamfered (paper-cut) corners, double border, hatch shadow.
void th_panel(int x, int y, int w, int h);
// Solid black variant (white pinline inside).
void th_panel_dark(int x, int y, int w, int h);
// Small label chip, centred on (cx, cy). Inverted = black w/ white text.
void th_chip(int cx, int cy, const char* label, bool inverted);
// Carved wooden plank button (chamfer + nail heads). Selected = filled.
void th_plank_button(int x, int y, int w, int h, const char* label, bool sel);
// Full-width bottom hint bar with (A)/(B) glyphs. Pass NULL to omit a part.
void th_hint_bar(const char* aLabel, const char* bLabel, const char* extra);
// Round button glyph with a letter ('A' or 'B'), centred on (cx, cy).
void th_button_glyph(int cx, int cy, char ab, bool onBlack);

// ---------------------------------------------------------------------------
// Festival motifs.
// ---------------------------------------------------------------------------
void th_edelweiss(int cx, int cy, int r);             // five-petal flower
void th_rosette(int cx, int cy, int r);               // prize rosette + tails
void th_pennant_garland(int x0, int x1, int y, int sag); // swaying flag string
void th_pine(int cx, int baseY, int h);               // haloed pine tree
void th_chalet(int x, int baseY);                     // little alpine hut
void th_bird(int cx, int cy);                         // distant gull
void th_cloud(int cx, int cy);                        // shaded paper-cut puff
void th_coin(int cx, int cy);                         // 9px gold coin glyph
void th_heart(int cx, int cy, bool filled, bool white); // 13px heart
void th_cowbell_icon(int cx, int cy);                 // small bell glyph
// Wins-as-summits meter: `total` small peaks, first `n` filled (climbed).
// pw = width per peak (8 = compact topbar size, 14+ = showpiece). The final
// peak stands taller and, at pw >= 10, flies a tiny victory flag.
void th_peak_meter(int cx, int cy, int n, int total, int pw, bool onBlack);
// One arrow-shaped signpost plank spanning xL..xR, point on the left/right.
void th_sign_plank(int xL, int xR, int cy, const char* label,
                   bool selected, bool pointLeft);

// ---------------------------------------------------------------------------
// Layered scherenschnitt backdrop. horizonY = forest/pasture line.
// ---------------------------------------------------------------------------
#define TH_BG_SUN      0x01   // dithered sun with slowly turning rays
#define TH_BG_CLOUDS   0x02   // drifting clouds
#define TH_BG_BIRDS    0x04   // gulls over the ridge
#define TH_BG_FLOWERS  0x08   // edelweiss + grass tufts on the pasture
#define TH_BG_RICH     (TH_BG_SUN | TH_BG_CLOUDS | TH_BG_BIRDS | TH_BG_FLOWERS)
void th_backdrop(int horizonY, uint8_t flags);

// ---------------------------------------------------------------------------
// Scene transition — a ridgeline curtain that sweeps off to the right when a
// new screen opens. Purely cosmetic overlay: never gates input. Call
// th_wipe_start() at the moment of a state change, th_wipe_draw() as the very
// last draw of the frame (game.c update()).
// ---------------------------------------------------------------------------
void th_wipe_start(void);
void th_wipe_draw(void);

#endif // THEME_H
