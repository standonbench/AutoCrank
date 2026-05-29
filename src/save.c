#include "game.h"

#define SAVE_VERSION 5     // bumped: + settings byte (sound, help-seen)

typedef struct __attribute__((packed)) {
    uint32_t version;
    uint32_t totalRuns;
    uint32_t totalWins;
    uint32_t bestStreak;
    uint8_t  unlockedPets[4];
    uint8_t  petMaxLevel[PET_COUNT];
    uint8_t  petWinsAtMax[PET_COUNT];
    // run resume state
    uint8_t  inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;
    // V4 additions
    TeamName  playerName;
    WinRecord hof[HOF_SIZE];
    uint8_t   hofCount;
    uint8_t   hofNext;
    // V5 addition
    uint8_t   settings;
} SaveDataV5;

// V4 layout — the previous format, kept so V4 saves still load (settings = 0).
typedef struct __attribute__((packed)) {
    uint32_t version;
    uint32_t totalRuns;
    uint32_t totalWins;
    uint32_t bestStreak;
    uint8_t  unlockedPets[4];
    uint8_t  petMaxLevel[PET_COUNT];
    uint8_t  petWinsAtMax[PET_COUNT];
    uint8_t  inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;
    TeamName  playerName;
    WinRecord hof[HOF_SIZE];
    uint8_t   hofCount;
    uint8_t   hofNext;
} SaveDataV4;

// V3 layout — the previous save format, used so older saves still load.
typedef struct __attribute__((packed)) {
    uint32_t version;
    uint32_t totalRuns;
    uint32_t totalWins;
    uint32_t bestStreak;
    uint8_t  unlockedPets[4];
    uint8_t  petMaxLevel[PET_COUNT];
    uint8_t  petWinsAtMax[PET_COUNT];
    uint8_t  inRun;
    uint8_t  turn;
    uint8_t  wins;
    uint8_t  lives;
    uint32_t runSeed;
    Team     player;
    Shop     shop;
} SaveDataV3;

static void save_set_defaults(void) {
    game.totalRuns = 0;
    game.totalWins = 0;
    game.bestStreak = 0;
    memset(game.unlockedPets, 0, sizeof(game.unlockedPets));
    memset(game.petMaxLevel, 0, sizeof(game.petMaxLevel));
    memset(game.petWinsAtMax, 0, sizeof(game.petWinsAtMax));
    // Mark first 6 (Tier 1) pets as unlocked by default
    game.unlockedPets[0] = 0x3F;
    game.inRun = false;
    memset(&game.playerName, 0, sizeof(game.playerName));
    memset(game.hof, 0, sizeof(game.hof));
    game.hofCount = 0;
    game.hofNext = 0;
    game.settings = 0;
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

    if (ver == 5) {
        SaveDataV5 d;
        int n = pd->file->read(f, &d, sizeof(d));
        pd->file->close(f);
        if (n == (int)sizeof(d)) {
            game.totalRuns = d.totalRuns;
            game.totalWins = d.totalWins;
            game.bestStreak = d.bestStreak;
            memcpy(game.unlockedPets, d.unlockedPets, sizeof(d.unlockedPets));
            memcpy(game.petMaxLevel,  d.petMaxLevel,  sizeof(d.petMaxLevel));
            memcpy(game.petWinsAtMax, d.petWinsAtMax, sizeof(d.petWinsAtMax));
            game.inRun = d.inRun != 0;
            game.turn = d.turn;
            game.wins = d.wins;
            game.lives = d.lives;
            game.runSeed = d.runSeed;
            game.player = d.player;
            game.shop = d.shop;
            game.playerName = d.playerName;
            memcpy(game.hof, d.hof, sizeof(d.hof));
            game.hofCount = d.hofCount;
            game.hofNext  = d.hofNext;
            game.settings = d.settings;
            DLOG("Save v5 loaded. runs=%u wins=%u hof=%u",
                 game.totalRuns, game.totalWins, game.hofCount);
            return;
        }
    } else if (ver == 4) {
        SaveDataV4 d;
        int n = pd->file->read(f, &d, sizeof(d));
        pd->file->close(f);
        if (n == (int)sizeof(d)) {
            game.totalRuns = d.totalRuns;
            game.totalWins = d.totalWins;
            game.bestStreak = d.bestStreak;
            memcpy(game.unlockedPets, d.unlockedPets, sizeof(d.unlockedPets));
            memcpy(game.petMaxLevel,  d.petMaxLevel,  sizeof(d.petMaxLevel));
            memcpy(game.petWinsAtMax, d.petWinsAtMax, sizeof(d.petWinsAtMax));
            game.inRun = d.inRun != 0;
            game.turn = d.turn;
            game.wins = d.wins;
            game.lives = d.lives;
            game.runSeed = d.runSeed;
            game.player = d.player;
            game.shop = d.shop;
            game.playerName = d.playerName;
            memcpy(game.hof, d.hof, sizeof(d.hof));
            game.hofCount = d.hofCount;
            game.hofNext  = d.hofNext;
            game.settings = 0;          // V4 -> V5: default (sound on, help unseen)
            DLOG("Save v4 -> v5 migrated. runs=%u wins=%u",
                 game.totalRuns, game.totalWins);
            return;
        }
    } else if (ver == 3) {
        SaveDataV3 d;
        int n = pd->file->read(f, &d, sizeof(d));
        pd->file->close(f);
        if (n == (int)sizeof(d)) {
            save_set_defaults();           // zero the new V4 fields
            game.totalRuns = d.totalRuns;
            game.totalWins = d.totalWins;
            game.bestStreak = d.bestStreak;
            memcpy(game.unlockedPets, d.unlockedPets, sizeof(d.unlockedPets));
            memcpy(game.petMaxLevel,  d.petMaxLevel,  sizeof(d.petMaxLevel));
            memcpy(game.petWinsAtMax, d.petWinsAtMax, sizeof(d.petWinsAtMax));
            game.inRun = d.inRun != 0;
            game.turn = d.turn;
            game.wins = d.wins;
            game.lives = d.lives;
            game.runSeed = d.runSeed;
            game.player = d.player;
            game.shop = d.shop;
            DLOG("Save v3 -> v5 migrated. runs=%u wins=%u",
                 game.totalRuns, game.totalWins);
            return;
        }
    }
    pd->file->close(f);
    DLOG("Save unknown version %u, resetting.", ver);
    save_set_defaults();
}

void save_write(void) {
    SaveDataV5 d;
    memset(&d, 0, sizeof(d));
    d.version = SAVE_VERSION;
    d.totalRuns = game.totalRuns;
    d.totalWins = game.totalWins;
    d.bestStreak = game.bestStreak;
    memcpy(d.unlockedPets, game.unlockedPets, sizeof(d.unlockedPets));
    memcpy(d.petMaxLevel,  game.petMaxLevel,  sizeof(d.petMaxLevel));
    memcpy(d.petWinsAtMax, game.petWinsAtMax, sizeof(d.petWinsAtMax));
    d.inRun = game.inRun ? 1 : 0;
    d.turn = game.turn;
    d.wins = game.wins;
    d.lives = game.lives;
    d.runSeed = game.runSeed;
    d.player = game.player;
    d.shop = game.shop;
    d.playerName = game.playerName;
    memcpy(d.hof, game.hof, sizeof(d.hof));
    d.hofCount = game.hofCount;
    d.hofNext  = game.hofNext;
    d.settings = game.settings;

    SDFile* f = pd->file->open("save.bin", kFileWrite);
    if (f) {
        pd->file->write(f, &d, sizeof(d));
        pd->file->close(f);
    } else {
        DLOG("save: open(write) failed");
    }
}
