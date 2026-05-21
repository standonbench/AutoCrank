#include "game.h"
#include <stdio.h>

static LCDBitmap* g_petBmp[PET_COUNT];
static LCDBitmap* g_foodBmp[FOOD_COUNT];

void images_init(void)
{
    for (int i = 0; i < PET_COUNT; i++) g_petBmp[i] = NULL;
    for (int i = 0; i < FOOD_COUNT; i++) g_foodBmp[i] = NULL;

    // Load pets if PNG assets are present (Source/images/pets/pet_NN.png).
    // Missing files produce a NULL bitmap; renderer falls back to a stub.
    char path[64];
    const char* err = NULL;
    for (int i = 0; i < PET_COUNT; i++) {
        snprintf(path, sizeof(path), "images/pets/pet_%02d", i);
        g_petBmp[i] = pd->graphics->loadBitmap(path, &err);
        // Silent: pets without art just render the stub.
    }
    for (int i = 0; i < FOOD_COUNT; i++) {
        snprintf(path, sizeof(path), "images/food/food_%02d", i);
        g_foodBmp[i] = pd->graphics->loadBitmap(path, &err);
    }
}

LCDBitmap* image_pet(uint8_t id)  { return id < PET_COUNT  ? g_petBmp[id]  : NULL; }
LCDBitmap* image_food(uint8_t id) { return id < FOOD_COUNT ? g_foodBmp[id] : NULL; }
