#include "theme.h"
#include <stdio.h>

// ===========================================================================
// Dither swatches. LCDPattern = 8 pattern rows + 8 mask rows (all opaque).
// A 0-bit prints black.
// ===========================================================================
LCDPattern TH_PAT_06 = {
    0xFF,0xFF,0xFF,0xEF,0xFF,0xFF,0xFF,0xFE,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
LCDPattern TH_PAT_12 = {
    0xEE,0xFF,0xBB,0xFF,0xEE,0xFF,0xBB,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
LCDPattern TH_PAT_25 = {
    0xAA,0xFF,0x55,0xFF,0xAA,0xFF,0x55,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
LCDPattern TH_PAT_50 = {
    0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
LCDPattern TH_PAT_75 = {
    0x22,0x88,0x22,0x88,0x22,0x88,0x22,0x88,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
LCDPattern TH_PAT_HATCH = {
    0x77,0xBB,0xDD,0xEE,0x77,0xBB,0xDD,0xEE,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
// Scrim: prints black on a 50% checker and leaves the other half untouched
// (mask rows), so one fillRect dims a whole scene without erasing it.
LCDPattern TH_PAT_SCRIM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55
};

void th_fill(int x, int y, int w, int h, LCDPattern pat) {
    pd->graphics->fillRect(x, y, w, h, (LCDColor)pat);
}

// ===========================================================================
// Easing
// ===========================================================================
float th_ease_out(float p) {
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return 1.0f - (1.0f - p) * (1.0f - p);
}

float th_ease_in_out(float p) {
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p * p * (3.0f - 2.0f * p);
}

float th_ease_back(float p, float overshoot) {
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    float c3 = overshoot + 1.0f;
    float q = p - 1.0f;
    return 1.0f + c3 * q * q * q + overshoot * q * q;
}

int th_lerpi(int from, int to, float p) {
    if (p <= 0.0f) return from;
    if (p >= 1.0f) return to;
    return from + (int)((float)(to - from) * p);
}

// ===========================================================================
// Text
// ===========================================================================
int th_text_w(LCDFont* f, const char* s) {
    if (!s) return 0;
    if (!f) return (int)strlen(s) * 7;
    return pd->graphics->getTextWidth(f, s, strlen(s), kASCIIEncoding, 0);
}

void th_text(LCDFont* f, const char* s, int x, int y, bool white) {
    if (!s || !s[0]) return;
    if (f) pd->graphics->setFont(f);
    pd->graphics->setDrawMode(white ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x, y);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

void th_text_center(LCDFont* f, const char* s, int cx, int y, bool white) {
    th_text(f, s, cx - th_text_w(f, s) / 2, y, white);
}

void th_text_halo(LCDFont* f, const char* s, int x, int y) {
    if (!s || !s[0]) return;
    if (f) pd->graphics->setFont(f);
    pd->graphics->setDrawMode(kDrawModeFillWhite);
    for (int ox = -1; ox <= 1; ox++)
        for (int oy = -1; oy <= 1; oy++)
            pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x + ox, y + oy);
    pd->graphics->setDrawMode(kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, x, y);
    pd->graphics->setDrawMode(kDrawModeCopy);
}

void th_text_halo_center(LCDFont* f, const char* s, int cx, int y) {
    th_text_halo(f, s, cx - th_text_w(f, s) / 2, y);
}

void th_text_scaled(LCDFont* f, const char* s, int cx, int cy,
                    float scale, bool white) {
    if (!s || !s[0] || scale <= 0.02f) return;
    if (f) pd->graphics->setFont(f);
    int w = th_text_w(f, s);
    int h = f ? pd->graphics->getFontHeight(f) : 16;
    if (scale > 0.98f && scale < 1.02f) {
        th_text(f, s, cx - w / 2, cy - h / 2, white);
        return;
    }
    LCDBitmap* tmp = pd->graphics->newBitmap(w + 4, h + 4, kColorClear);
    if (!tmp) return;
    pd->graphics->pushContext(tmp);
    pd->graphics->setDrawMode(white ? kDrawModeFillWhite : kDrawModeFillBlack);
    pd->graphics->drawText(s, strlen(s), kASCIIEncoding, 2, 2);
    pd->graphics->setDrawMode(kDrawModeCopy);
    pd->graphics->popContext();
    pd->graphics->drawRotatedBitmap(tmp, cx, cy, 0.0f, 0.5f, 0.5f, scale, scale);
    pd->graphics->freeBitmap(tmp);
}

// ===========================================================================
// Panels & chrome
// ===========================================================================
#define TH_CHAMFER 5

// Fill an octagon (rect with cut corners) in one colour.
static void th_fill_octagon(int x, int y, int w, int h, LCDColor c) {
    int ch = TH_CHAMFER;
    pd->graphics->fillRect(x, y + ch, w, h - 2 * ch, c);
    pd->graphics->fillRect(x + ch, y, w - 2 * ch, h, c);
    pd->graphics->fillTriangle(x + ch, y, x, y + ch, x + ch, y + ch, c);
    pd->graphics->fillTriangle(x + w - ch, y, x + w, y + ch,
                               x + w - ch, y + ch, c);
    pd->graphics->fillTriangle(x, y + h - ch, x + ch, y + h,
                               x + ch, y + h - ch, c);
    pd->graphics->fillTriangle(x + w, y + h - ch, x + w - ch, y + h,
                               x + w - ch, y + h - ch, c);
}

// Outline the same octagon.
static void th_line_octagon(int x, int y, int w, int h, int lw, LCDColor c) {
    int ch = TH_CHAMFER;
    GFX_LINE(x + ch, y, x + w - ch, y, lw, c);              // top
    GFX_LINE(x + ch, y + h, x + w - ch, y + h, lw, c);      // bottom
    GFX_LINE(x, y + ch, x, y + h - ch, lw, c);              // left
    GFX_LINE(x + w, y + ch, x + w, y + h - ch, lw, c);      // right
    GFX_LINE(x + ch, y, x, y + ch, lw, c);                  // corners
    GFX_LINE(x + w - ch, y, x + w, y + ch, lw, c);
    GFX_LINE(x, y + h - ch, x + ch, y + h, lw, c);
    GFX_LINE(x + w, y + h - ch, x + w - ch, y + h, lw, c);
}

void th_panel(int x, int y, int w, int h) {
    th_fill(x + 3, y + 3, w, h, TH_PAT_HATCH);              // paper-cut shadow
    th_fill_octagon(x, y, w, h, kColorWhite);
    th_line_octagon(x, y, w, h, 2, kColorBlack);
    GFX_RECT(x + 4, y + 4, w - 8, h - 8, kColorBlack);      // inner pinline
}

void th_panel_dark(int x, int y, int w, int h) {
    th_fill(x + 3, y + 3, w, h, TH_PAT_HATCH);
    th_fill_octagon(x, y, w, h, kColorBlack);
    GFX_RECT(x + 3, y + 3, w - 6, h - 6, kColorWhite);      // white pinline
}

void th_chip(int cx, int cy, const char* label, bool inverted) {
    if (!label || !label[0]) return;
    LCDFont* f = game.fontBold;
    int tw = th_text_w(f, label);
    int w = tw + 14, h = 17;
    int x = cx - w / 2, y = cy - h / 2;
    if (inverted) {
        GFX_FILL(x, y, w, h, kColorBlack);
        GFX_ELLIPSE(x - h / 2 + 1, y, h, h, kColorBlack);
        GFX_ELLIPSE(x + w - h / 2 - 1, y, h, h, kColorBlack);
    } else {
        GFX_FILL(x, y, w, h, kColorWhite);
        GFX_ELLIPSE(x - h / 2 + 1, y, h, h, kColorWhite);
        GFX_ELLIPSE(x + w - h / 2 - 1, y, h, h, kColorWhite);
        pd->graphics->drawEllipse(x - h / 2 + 1, y, h, h, 1, 90.0f, 270.0f,
                                  kColorBlack);
        pd->graphics->drawEllipse(x + w - h / 2 - 1, y, h, h, 1, 270.0f, 450.0f,
                                  kColorBlack);
        GFX_LINE(x, y, x + w, y, 1, kColorBlack);
        GFX_LINE(x, y + h - 1, x + w, y + h - 1, 1, kColorBlack);
    }
    th_text(f, label, cx - tw / 2, cy - 8, inverted);
}

void th_plank_button(int x, int y, int w, int h, const char* label, bool sel) {
    if (sel) {
        th_fill(x + 3, y + 3, w, h, TH_PAT_HATCH);
        th_fill_octagon(x, y, w, h, kColorBlack);
        GFX_RECT(x + 3, y + 3, w - 6, h - 6, kColorWhite);
    } else {
        th_fill(x + 2, y + 2, w, h, TH_PAT_HATCH);
        th_fill_octagon(x, y, w, h, kColorWhite);
        th_line_octagon(x, y, w, h, 2, kColorBlack);
        // woodgrain + nail heads
        GFX_LINE(x + 9, y + 4, x + w - 9, y + 4, 1, kColorBlack);
        pd->graphics->drawEllipse(x + 6, y + h / 2 - 2, 4, 4, 1, 0, 360,
                                  kColorBlack);
        pd->graphics->drawEllipse(x + w - 10, y + h / 2 - 2, 4, 4, 1, 0, 360,
                                  kColorBlack);
    }
    LCDFont* f = game.fontBold;
    th_text(f, label, x + (w - th_text_w(f, label)) / 2, y + (h - 16) / 2, sel);
}

void th_button_glyph(int cx, int cy, char ab, bool onBlack) {
    char s[2] = { ab, 0 };
    int r = 7;
    if (onBlack) {
        GFX_ELLIPSE(cx - r, cy - r, r * 2 + 1, r * 2 + 1, kColorWhite);
        th_text(game.fontSmall, s, cx - th_text_w(game.fontSmall, s) / 2,
                cy - 7, false);
    } else {
        GFX_ELLIPSE(cx - r, cy - r, r * 2 + 1, r * 2 + 1, kColorBlack);
        th_text(game.fontSmall, s, cx - th_text_w(game.fontSmall, s) / 2,
                cy - 7, true);
    }
}

void th_hint_bar(const char* aLabel, const char* bLabel, const char* extra) {
    int barH = 17, y = SCREEN_H - barH;
    GFX_FILL(0, y, SCREEN_W, barH, kColorBlack);
    GFX_LINE(0, y, SCREEN_W, y, 1, kColorWhite);

    LCDFont* f = game.fontSmall;
    const int glyphW = 17, gap = 16;
    int total = 0;
    if (aLabel) total += glyphW + th_text_w(f, aLabel) + gap;
    if (bLabel) total += glyphW + th_text_w(f, bLabel) + gap;
    if (extra)  total += th_text_w(f, extra) + gap;
    if (total > 0) total -= gap;

    int x = (SCREEN_W - total) / 2;
    int cy = y + barH / 2;
    if (aLabel) {
        th_button_glyph(x + 7, cy, 'A', true);
        th_text(f, aLabel, x + glyphW, y + 2, true);
        x += glyphW + th_text_w(f, aLabel) + gap;
    }
    if (bLabel) {
        th_button_glyph(x + 7, cy, 'B', true);
        th_text(f, bLabel, x + glyphW, y + 2, true);
        x += glyphW + th_text_w(f, bLabel) + gap;
    }
    if (extra) th_text(f, extra, x, y + 2, true);
}

// ===========================================================================
// Festival motifs
// ===========================================================================
void th_edelweiss(int cx, int cy, int r) {
    int d = r + 3;
    for (int i = 0; i < 5; i++) {
        float a = (float)i * (2.0f * PI_F / 5.0f) - PI_F / 2.0f;
        int px = cx + (int)(cosf(a) * (float)r);
        int py = cy + (int)(sinf(a) * (float)r);
        GFX_ELLIPSE(px - d / 2, py - d / 2, d, d, kColorWhite);
        pd->graphics->drawEllipse(px - d / 2, py - d / 2, d, d, 1, 0, 360,
                                  kColorBlack);
    }
    GFX_ELLIPSE(cx - 2, cy - 2, 5, 5, kColorBlack);
}

void th_rosette(int cx, int cy, int r) {
    // forked ribbon tails first, so the disc overlaps them
    int tw = r * 2 / 3, tl = r + r / 2;
    pd->graphics->fillTriangle(cx - tw, cy + 2, cx - 2, cy + 2,
                               cx - tw / 2 - 4, cy + tl + 4, kColorBlack);
    pd->graphics->fillTriangle(cx + tw, cy + 2, cx + 2, cy + 2,
                               cx + tw / 2 + 4, cy + tl + 4, kColorBlack);
    pd->graphics->fillTriangle(cx - tw / 2 - 9, cy + tl + 9,
                               cx - tw / 2 + 1,  cy + tl + 9,
                               cx - tw / 2 - 4,  cy + tl - 2, kColorWhite);
    pd->graphics->fillTriangle(cx + tw / 2 - 1, cy + tl + 9,
                               cx + tw / 2 + 9, cy + tl + 9,
                               cx + tw / 2 + 4, cy + tl - 2, kColorWhite);
    // scalloped outer ring of petals
    int pr = (r * 5) / 8;
    for (int i = 0; i < 8; i++) {
        float a = (float)i * (PI_F / 4.0f);
        int px = cx + (int)(cosf(a) * (float)r * 0.72f);
        int py = cy + (int)(sinf(a) * (float)r * 0.72f);
        GFX_ELLIPSE(px - pr / 2, py - pr / 2, pr, pr, kColorBlack);
    }
    // face: white disc, black ring, black core
    int fr = (r * 7) / 8;
    GFX_ELLIPSE(cx - fr, cy - fr, fr * 2, fr * 2, kColorWhite);
    pd->graphics->drawEllipse(cx - fr, cy - fr, fr * 2, fr * 2, 2, 0, 360,
                              kColorBlack);
    int cr = r / 3;
    GFX_ELLIPSE(cx - cr, cy - cr, cr * 2, cr * 2, kColorBlack);
}

void th_pennant_garland(int x0, int x1, int y, int sag) {
    if (x1 <= x0) return;
    int span = x1 - x0;
    // rope — sampled shallow curve
    int prevX = x0, prevY = y;
    for (int i = 1; i <= 12; i++) {
        float t = (float)i / 12.0f;
        int px = x0 + (int)((float)span * t);
        int py = y + (int)(sinf(t * PI_F) * (float)sag);
        GFX_LINE(prevX, prevY, px, py, 1, kColorBlack);
        prevX = px; prevY = py;
    }
    // pennants — alternating solid / outline, with a 1-2px wind sway
    int step = 26;
    int k = 0;
    for (int px = x0 + step / 2; px < x1 - 6; px += step, k++) {
        float t = (float)(px - x0) / (float)span;
        int py = y + (int)(sinf(t * PI_F) * (float)sag);
        int sway = (int)(sinf((float)game.frameCount * 0.06f + (float)k) * 2.0f);
        int tipX = px + sway, tipY = py + 13;
        if (k & 1) {
            pd->graphics->fillTriangle(px - 5, py + 1, px + 5, py + 1,
                                       tipX, tipY, kColorWhite);
            GFX_LINE(px - 5, py + 1, tipX, tipY, 1, kColorBlack);
            GFX_LINE(px + 5, py + 1, tipX, tipY, 1, kColorBlack);
            GFX_LINE(px - 5, py + 1, px + 5, py + 1, 1, kColorBlack);
        } else {
            pd->graphics->fillTriangle(px - 5, py + 1, px + 5, py + 1,
                                       tipX, tipY, kColorBlack);
        }
    }
}

void th_pine(int cx, int baseY, int h) {
    int w = (h * 9) / 20;
    int by[3], ay[3], tw[3];
    for (int t = 0; t < 3; t++) {
        tw[t] = (int)((float)w * (1.0f - 0.30f * (float)t));
        by[t] = baseY - 5 - (int)((float)h * 0.34f * (float)t);
        ay[t] = by[t] - (int)((float)h * 0.52f);
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

void th_chalet(int x, int baseY) {
    int bw = 48, bh = 30, bx = x, by = baseY - bh;
    GFX_FILL(bx, by, bw, bh, kColorWhite);
    GFX_RECT(bx, by, bw, bh, kColorBlack);
    GFX_RECT(bx + 1, by + 1, bw - 2, bh - 2, kColorBlack);
    int apexY = by - 20, rcx = bx + bw / 2;
    pd->graphics->fillTriangle(bx - 9, by + 2, rcx, apexY,
                               bx + bw + 9, by + 2, kColorBlack);
    GFX_LINE(bx - 9, by + 2, bx + bw + 9, by + 2, 3, kColorBlack);
    pd->graphics->fillTriangle(rcx, apexY, rcx - 8, apexY + 11,
                               rcx + 8, apexY + 11, kColorWhite);
    GFX_LINE(rcx, apexY, rcx - 8, apexY + 11, 1, kColorBlack);
    GFX_LINE(rcx, apexY, rcx + 8, apexY + 11, 1, kColorBlack);
    int dw = 12, dh = 14, dx = bx + bw / 2 - dw / 2;
    GFX_FILL(dx, baseY - dh, dw, dh, kColorBlack);
    GFX_ELLIPSE(dx + dw - 4, baseY - dh / 2 - 1, 3, 3, kColorWhite);
    for (int i = 0; i < 2; i++) {
        int wx = bx + 6 + i * (bw - 24), wy = by + 6;
        GFX_FILL(wx, wy, 11, 11, kColorWhite);
        GFX_RECT(wx, wy, 11, 11, kColorBlack);
        GFX_LINE(wx + 5, wy, wx + 5, wy + 10, 1, kColorBlack);
        GFX_LINE(wx, wy + 5, wx + 10, wy + 5, 1, kColorBlack);
    }
}

void th_bird(int cx, int cy) {
    GFX_LINE(cx - 7, cy,     cx - 3, cy - 5, 2, kColorBlack);
    GFX_LINE(cx - 3, cy - 5, cx,     cy - 1, 2, kColorBlack);
    GFX_LINE(cx,     cy - 1, cx + 3, cy - 5, 2, kColorBlack);
    GFX_LINE(cx + 3, cy - 5, cx + 7, cy,     2, kColorBlack);
}

void th_coin(int cx, int cy) {
    GFX_ELLIPSE(cx - 5, cy - 5, 11, 11, kColorWhite);
    GFX_ELLIPSE(cx - 4, cy - 4, 9, 9, kColorBlack);
    pd->graphics->drawEllipse(cx - 2, cy - 2, 5, 5, 1, 0, 360, kColorWhite);
}

void th_heart(int cx, int cy, bool filled, bool white) {
    LCDColor c = white ? kColorWhite : kColorBlack;
    if (filled) {
        GFX_ELLIPSE(cx - 7, cy - 5, 8, 7, c);
        GFX_ELLIPSE(cx - 1, cy - 5, 8, 7, c);
        GFX_FILL(cx - 6, cy - 2, 13, 4, c);
        pd->graphics->fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2,
                                   cx, cy + 8, c);
    } else {
        pd->graphics->drawEllipse(cx - 7, cy - 5, 8, 7, 1, 0, 360, c);
        pd->graphics->drawEllipse(cx - 1, cy - 5, 8, 7, 1, 0, 360, c);
        GFX_LINE(cx - 6, cy + 1, cx, cy + 8, 1, c);
        GFX_LINE(cx + 6, cy + 1, cx, cy + 8, 1, c);
    }
}

void th_cowbell_icon(int cx, int cy) {
    pd->graphics->drawEllipse(cx - 2, cy - 7, 5, 5, 1, 0, 360, kColorBlack);
    pd->graphics->fillTriangle(cx - 3, cy - 4, cx + 3, cy - 4,
                               cx + 5, cy + 4, kColorBlack);
    pd->graphics->fillTriangle(cx - 3, cy - 4, cx - 5, cy + 4,
                               cx + 5, cy + 4, kColorBlack);
    GFX_FILL(cx - 6, cy + 3, 13, 2, kColorBlack);
    GFX_FILL(cx - 1, cy + 5, 3, 3, kColorBlack);
}

void th_peak_meter(int cx, int cy, int n, int total, int pw, bool onBlack) {
    if (total <= 0 || pw < 4) return;
    int ph = pw + 1;
    int x0 = cx - (total * pw) / 2;
    LCDColor ink   = onBlack ? kColorWhite : kColorBlack;
    LCDColor paper = onBlack ? kColorBlack : kColorWhite;
    for (int i = 0; i < total; i++) {
        int bx = x0 + i * pw;
        int apexX = bx + pw / 2;
        int hh = (i == total - 1) ? ph + 3 : ph;     // final summit stands taller
        if (i < n) {
            // climbed: solid peak with a snowcap notch
            pd->graphics->fillTriangle(bx, cy, apexX, cy - hh, bx + pw, cy, ink);
            pd->graphics->fillTriangle(apexX, cy - hh, apexX - 2, cy - hh + 4,
                                       apexX + 2, cy - hh + 4, paper);
        } else {
            GFX_LINE(bx, cy, apexX, cy - hh, 1, ink);
            GFX_LINE(apexX, cy - hh, bx + pw, cy, 1, ink);
        }
        if (i == total - 1 && pw >= 10) {
            // summit flag
            GFX_LINE(apexX, cy - hh, apexX, cy - hh - 5, 1, ink);
            pd->graphics->fillTriangle(apexX, cy - hh - 5, apexX + 5, cy - hh - 3,
                                       apexX, cy - hh - 1, ink);
        }
    }
    GFX_LINE(x0 - 2, cy, x0 + total * pw + 2, cy, 1, ink);
}

void th_sign_plank(int xL, int xR, int cy, const char* label,
                   bool selected, bool pointLeft) {
    int top = cy - 14, bot = cy + 14;
    int bodyL = pointLeft ? xL + 14 : xL;
    int bodyR = pointLeft ? xR : xR - 14;
    int tipX  = pointLeft ? xL : xR;
    LCDColor fill = selected ? kColorBlack : kColorWhite;
    // hatch drop shadow
    pd->graphics->fillTriangle(tipX + 3, cy + 4, bodyL + 3, top + 4,
                               bodyL + 3, bot + 4, (LCDColor)TH_PAT_HATCH);
    th_fill(bodyL + 3, top + 4, bodyR - bodyL, bot - top, TH_PAT_HATCH);
    // body + point
    pd->graphics->fillTriangle(tipX, cy, pointLeft ? bodyL : bodyR, top,
                               pointLeft ? bodyL : bodyR, bot, fill);
    GFX_FILL(bodyL, top, bodyR - bodyL, bot - top, fill);
    // outline
    GFX_LINE(bodyL, top, bodyR, top, 2, kColorBlack);
    GFX_LINE(bodyL, bot, bodyR, bot, 2, kColorBlack);
    GFX_LINE(pointLeft ? bodyR : bodyL, top,
             pointLeft ? bodyR : bodyL, bot, 2, kColorBlack);
    GFX_LINE(pointLeft ? bodyL : bodyR, top, tipX, cy, 2, kColorBlack);
    GFX_LINE(pointLeft ? bodyL : bodyR, bot, tipX, cy, 2, kColorBlack);
    if (!selected) {
        // nail heads at the butt end (grain lines crowded the label)
        int nailX = pointLeft ? bodyR - 7 : bodyL + 3;
        pd->graphics->drawEllipse(nailX, top + 5, 4, 4, 1, 0, 360, kColorBlack);
        pd->graphics->drawEllipse(nailX, bot - 9, 4, 4, 1, 0, 360, kColorBlack);
    }
    LCDFont* f = game.fontBold;
    int tx = (bodyL + bodyR) / 2 - th_text_w(f, label) / 2;
    th_text(f, label, tx, cy - 8, selected);
}

// ===========================================================================
// Backdrop
// ===========================================================================
static void th_cloud(int cx, int cy) {
    GFX_ELLIPSE(cx - 14, cy - 4, 18, 12, kColorWhite);
    GFX_ELLIPSE(cx - 4,  cy - 9, 20, 16, kColorWhite);
    GFX_ELLIPSE(cx + 8,  cy - 5, 17, 13, kColorWhite);
    GFX_ELLIPSE(cx - 8,  cy,     30, 10, kColorWhite);
}

static void th_sun(int cx, int cy, int r) {
    float spin = (float)game.frameCount * 0.006f;
    for (int i = 0; i < 8; i++) {
        float a = spin + (float)i * (PI_F / 4.0f);
        int x1 = cx + (int)(cosf(a) * (float)(r + 3));
        int y1 = cy + (int)(sinf(a) * (float)(r + 3));
        int x2 = cx + (int)(cosf(a) * (float)(r + 9));
        int y2 = cy + (int)(sinf(a) * (float)(r + 9));
        GFX_LINE(x1, y1, x2, y2, 2, kColorBlack);
    }
    GFX_ELLIPSE(cx - r, cy - r, r * 2, r * 2, kColorWhite);
    pd->graphics->drawEllipse(cx - r, cy - r, r * 2, r * 2, 2, 0, 360,
                              kColorBlack);
}

void th_backdrop(int horizonY, uint8_t flags) {
    pd->graphics->setDrawMode(kDrawModeCopy);

    // --- sky ---
    th_fill(0, 0, SCREEN_W, horizonY, TH_PAT_06);

    if (flags & TH_BG_SUN) th_sun(48, 44, 13);

    // --- drifting clouds (wrap around with a slow constant drift) ---
    if (flags & TH_BG_CLOUDS) {
        const int span = SCREEN_W + 90;
        int drift = (int)((float)game.frameCount * 0.20f);
        int cx0 = (70  + span - (drift       % span)) % span - 45;
        int cx1 = (220 + span - ((drift * 3 / 4) % span)) % span - 45;
        int cx2 = (330 + span - ((drift / 2) % span)) % span - 45;
        th_cloud(cx0, 30);
        th_cloud(cx1, 20);
        th_cloud(cx2, 36);
    }

    if (flags & TH_BG_BIRDS) {
        int bob = (int)(sinf((float)game.frameCount * 0.05f) * 2.0f);
        th_bird(150, horizonY - 86 + bob);
        th_bird(180, horizonY - 76 - bob);
    }

    // --- far ridge: light dither, behind the main range ---
    static const int farH[5] = { 44, 64, 38, 58, 48 };
    int farSeg = SCREEN_W / 5 + 1;
    int farBase = horizonY - 16;
    for (int i = 0; i < 5; i++) {
        int px = i * farSeg - farSeg / 3, nx = px + farSeg + farSeg / 2;
        int peakX = (px + nx) / 2;
        int peakY = farBase - farH[i];
        if (peakY < 20) peakY = 20;
        pd->graphics->fillTriangle(px, farBase, peakX, peakY, nx, farBase,
                                   (LCDColor)TH_PAT_12);
    }

    // --- main range: checker stone, outlined, snowcapped ---
    static const int peakH[7] = { 34, 58, 26, 70, 40, 54, 30 };
    int seg = SCREEN_W / 7 + 1;
    int mtnBase = horizonY - 10;
    for (int i = 0; i < 7; i++) {
        int px = i * seg, peakX = px + seg / 2, nx = px + seg;
        int peakY = mtnBase - peakH[i];
        if (peakY < 21) peakY = 21;
        pd->graphics->fillTriangle(px, mtnBase, peakX, peakY, nx, mtnBase,
                                   (LCDColor)TH_PAT_50);
        GFX_LINE(px, mtnBase, peakX, peakY, 1, kColorBlack);
        GFX_LINE(peakX, peakY, nx, mtnBase, 1, kColorBlack);
        pd->graphics->fillTriangle(peakX, peakY,
                                   peakX - 7, peakY + 9,
                                   peakX + 7, peakY + 9, kColorWhite);
        GFX_LINE(peakX, peakY, peakX - 7, peakY + 9, 1, kColorBlack);
        GFX_LINE(peakX, peakY, peakX + 7, peakY + 9, 1, kColorBlack);
    }

    // --- forest band with scalloped treetops ---
    int fTop = horizonY - 12;
    th_fill(0, fTop, SCREEN_W, 12, TH_PAT_75);
    for (int x = -4; x < SCREEN_W + 4; x += 11) {
        GFX_ELLIPSE(x, fTop - 5, 13, 11, (LCDColor)TH_PAT_75);
    }

    // --- pasture ---
    GFX_FILL(0, horizonY, SCREEN_W, SCREEN_H - horizonY, kColorWhite);
    GFX_LINE(0, horizonY, SCREEN_W, horizonY, 1, kColorBlack);
    th_fill(0, SCREEN_H - 14, SCREEN_W, 14, TH_PAT_12);

    if (flags & TH_BG_FLOWERS) {
        // deterministic scatter of edelweiss + grass tufts
        static const int fx[4] = { 38, 196, 287, 369 };
        static const int fdy[4] = { 26, 40, 18, 34 };
        for (int i = 0; i < 4; i++) {
            int gy = horizonY + fdy[i];
            if (gy > SCREEN_H - 12) continue;
            th_edelweiss(fx[i], gy, 3);
        }
        static const int gx[6] = { 18, 102, 156, 238, 322, 388 };
        static const int gdy[6] = { 48, 22, 52, 30, 46, 24 };
        for (int i = 0; i < 6; i++) {
            int gy = horizonY + gdy[i];
            if (gy > SCREEN_H - 8) continue;
            GFX_LINE(gx[i] - 3, gy, gx[i] - 1, gy - 5, 1, kColorBlack);
            GFX_LINE(gx[i],     gy, gx[i],     gy - 6, 1, kColorBlack);
            GFX_LINE(gx[i] + 3, gy, gx[i] + 1, gy - 4, 1, kColorBlack);
        }
    }
}

// ===========================================================================
// Ridge-curtain wipe
// ===========================================================================
#define TH_WIPE_FRAMES 16
static int g_wipe = 0;

void th_wipe_start(void) { g_wipe = TH_WIPE_FRAMES; }

void th_wipe_draw(void) {
    if (g_wipe <= 0) return;
    float p = 1.0f - (float)g_wipe / (float)TH_WIPE_FRAMES;   // 0 → 1
    g_wipe--;
    int edge = th_lerpi(-30, SCREEN_W + 30, th_ease_in_out(p));
    if (edge >= SCREEN_W) return;
    pd->graphics->setDrawMode(kDrawModeCopy);
    // solid curtain to the right of the edge
    if (edge + 14 < SCREEN_W)
        GFX_FILL(edge + 14, 0, SCREEN_W - edge - 14, SCREEN_H, kColorBlack);
    // ridgeline teeth on the leading edge — paper-cut mountains
    for (int y = 0; y < SCREEN_H; y += 24) {
        pd->graphics->fillTriangle(edge + 15, y, edge + 15, y + 24,
                                   edge - 2, y + 12, kColorBlack);
    }
    // dither fringe ahead of the teeth
    th_fill(edge - 14, 0, 13, SCREEN_H, TH_PAT_50);
}
