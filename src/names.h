#ifndef NAMES_H
#define NAMES_H

#include <stdint.h>
#include <stddef.h>

// Swiss-themed but unhinged team-name word pool. Player and AI pick from
// the same lists; presented 3-of-each à la SAP. Indices live in TeamName
// (game.h) and persist in saves. Append new entries to the END only —
// inserting in the middle re-indexes existing saved names.

#define NAME_ADJ_COUNT  24
#define NAME_NOUN_COUNT 24

extern const char* const NAME_ADJECTIVES[NAME_ADJ_COUNT];
extern const char* const NAME_NOUNS[NAME_NOUN_COUNT];

// Compose "Adj Noun" into out (NUL-terminated). Truncates to cap.
void team_name_str(uint8_t adj, uint8_t noun, char* out, size_t cap);

#endif // NAMES_H
