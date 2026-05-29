#include "game.h"
#include <stdio.h>

static LCDBitmap* g_petBmp[PET_COUNT];
static LCDBitmap* g_foodBmp[FOOD_COUNT];
static LCDBitmap* g_titleBg;
static LCDBitmap* g_nameBg;

void images_init(void)
{
    for (int i = 0; i < PET_COUNT; i++) g_petBmp[i] = NULL;
    for (int i = 0; i < FOOD_COUNT; i++) g_foodBmp[i] = NULL;
    g_titleBg = NULL;
    g_nameBg = NULL;

    // Load pets if PNG assets are present (Source/images/pets/pet_NN.png).
    // Indirection through PETS[i].spriteId lets the roster swap which sprite
    // a pet uses without renaming any files — handy for matching Kenney's
    // 1-bit pack art to our Alpine pet names. Missing files produce a NULL
    // bitmap; renderer falls back to procedural art.
    char path[64];
    const char* err = NULL;
    for (int i = 0; i < PET_COUNT; i++) {
        if (PETS[i].spriteId == NO_SPRITE) continue;  // procedural-only pet
        snprintf(path, sizeof(path), "images/pets/pet_%02d",
                 (int)PETS[i].spriteId);
        g_petBmp[i] = pd->graphics->loadBitmap(path, &err);
        // A missing PNG drops this pet onto the procedural fallback in
        // draw_creature_xform, which allocates a 32x32 scratch bitmap EVERY
        // frame it's drawn — a per-frame malloc in the battle hot path. All
        // 18 ship today, so warn loudly if one ever fails to load.
        if (!g_petBmp[i])
            DLOG("WARN: pet %d sprite '%s' missing (%s) — procedural fallback",
                 i, path, err ? err : "?");
    }
    for (int i = 0; i < FOOD_COUNT; i++) {
        snprintf(path, sizeof(path), "images/food/food_%02d", i);
        g_foodBmp[i] = pd->graphics->loadBitmap(path, &err);
    }
    g_titleBg = pd->graphics->loadBitmap("images/title_bg", &err);
    g_nameBg  = pd->graphics->loadBitmap("images/name_bg",  &err);
}

LCDBitmap* image_pet(uint8_t id)  { return id < PET_COUNT  ? g_petBmp[id]  : NULL; }
LCDBitmap* image_food(uint8_t id) { return id < FOOD_COUNT ? g_foodBmp[id] : NULL; }
LCDBitmap* image_title_bg(void)   { return g_titleBg; }
LCDBitmap* image_name_bg(void)    { return g_nameBg; }
