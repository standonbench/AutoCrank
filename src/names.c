#include "names.h"
#include <stdio.h>

// Append-only — order matters for save compatibility.
const char* const NAME_ADJECTIVES[NAME_ADJ_COUNT] = {
    "Nepo",     "Horny",    "Tipsy",    "Cursed",
    "Holy",     "Cheesy",   "Chunky",   "Greasy",
    "Hangry",   "Smug",     "Drunk",    "Salty",
    "Pious",    "Spicy",    "Feral",    "Sus",
    "Soggy",    "Crusty",   "Doomed",   "Naked",
    "Lewd",     "Boujee",   "Sweaty",   "Reckless",
};

const char* const NAME_NOUNS[NAME_NOUN_COUNT] = {
    "Bankers",  "Farmers",  "Skiers",   "Yodelers",
    "Cows",     "Goats",    "Marmots",  "Hermits",
    "Mercs",    "Bellhops", "Curdlers", "Cuckoos",
    "Monks",    "Tourists", "Gnomes",   "Bandits",
    "Heretics", "Pretzels", "Climbers", "Cultists",
    "Apostles", "Watchmen", "Brewers",  "Tickers",
};
// Note: index 19 was "Druggies" -> now "Cultists" (no save-format break:
// the slot identity is preserved, only the displayed string changes).

void team_name_str(uint8_t adj, uint8_t noun, char* out, size_t cap) {
    if (cap == 0) return;
    const char* a = (adj  < NAME_ADJ_COUNT)  ? NAME_ADJECTIVES[adj]  : "Nameless";
    const char* n = (noun < NAME_NOUN_COUNT) ? NAME_NOUNS[noun]      : "Herd";
    snprintf(out, cap, "%s %s", a, n);
}
