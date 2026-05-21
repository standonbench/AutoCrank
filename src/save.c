#include "game.h"

#define SAVE_VERSION 1

typedef struct __attribute__((packed)) {
    uint32_t version;
    uint32_t totalRuns;
    uint32_t totalWins;
    uint32_t bestStreak;
    uint8_t  unlockedPets[4];
    // run resume state
    uint8_t  inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;
} SaveDataV1;

static void save_set_defaults(void) {
    game.totalRuns = 0;
    game.totalWins = 0;
    game.bestStreak = 0;
    memset(game.unlockedPets, 0, sizeof(game.unlockedPets));
    // Mark first 6 (Tier 1) pets as unlocked by default
    game.unlockedPets[0] = 0x3F;
    game.inRun = false;
}

void save_load(void) {
    SDFile* f = pd->file->open("save.bin", kFileRead);
    if (!f) {
        save_set_defaults();
        DLOG("No save file, using defaults.");
        return;
    }
    uint32_t ver = 0;
    pd->file->read(f, &ver, sizeof(ver));
    pd->file->seek(f, 0, SEEK_SET);

    if (ver == SAVE_VERSION) {
        SaveDataV1 d;
        int n = pd->file->read(f, &d, sizeof(d));
        pd->file->close(f);
        if (n == (int)sizeof(d)) {
            game.totalRuns = d.totalRuns;
            game.totalWins = d.totalWins;
            game.bestStreak = d.bestStreak;
            memcpy(game.unlockedPets, d.unlockedPets, sizeof(d.unlockedPets));
            game.inRun = d.inRun != 0;
            game.turn = d.turn;
            game.wins = d.wins;
            game.lives = d.lives;
            game.runSeed = d.runSeed;
            game.player = d.player;
            game.shop = d.shop;
            DLOG("Save v1 loaded. runs=%u wins=%u", game.totalRuns, game.totalWins);
            return;
        }
    }
    pd->file->close(f);
    DLOG("Save unknown version %u, resetting.", ver);
    save_set_defaults();
}

void save_write(void) {
    SaveDataV1 d;
    memset(&d, 0, sizeof(d));
    d.version = SAVE_VERSION;
    d.totalRuns = game.totalRuns;
    d.totalWins = game.totalWins;
    d.bestStreak = game.bestStreak;
    memcpy(d.unlockedPets, game.unlockedPets, sizeof(d.unlockedPets));
    d.inRun = game.inRun ? 1 : 0;
    d.turn = game.turn;
    d.wins = game.wins;
    d.lives = game.lives;
    d.runSeed = game.runSeed;
    d.player = game.player;
    d.shop = game.shop;

    SDFile* f = pd->file->open("save.bin", kFileWrite);
    if (f) {
        pd->file->write(f, &d, sizeof(d));
        pd->file->close(f);
    } else {
        DLOG("save: open(write) failed");
    }
}
