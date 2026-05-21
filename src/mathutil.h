#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <math.h>
#include <stdint.h>
#include <string.h>

extern uint32_t _rng_state;

static inline void rng_seed(uint32_t seed) {
    _rng_state = seed ? seed : 1;
}

static inline uint32_t rng_next(void) {
    _rng_state ^= _rng_state << 13;
    _rng_state ^= _rng_state >> 17;
    _rng_state ^= _rng_state << 5;
    return _rng_state;
}

static inline int rng_range(int mn, int mx) {
    if (mn >= mx) return mn;
    return mn + (int)(rng_next() % (uint32_t)(mx - mn + 1));
}

static inline float rng_float(void) {
    return (float)(rng_next() & 0xFFFF) / 65536.0f;
}

static inline int clampi(int val, int mn, int mx) {
    if (val < mn) return mn;
    if (val > mx) return mx;
    return val;
}

static inline float clampf(float val, float mn, float mx) {
    if (val < mn) return mn;
    if (val > mx) return mx;
    return val;
}

static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

#define PI_F 3.14159265f

#endif // MATHUTIL_H
