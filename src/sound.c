#include "game.h"

// ---------------------------------------------------------------------------
// Synth SFX engine.
//
// Every effect is built from a small set of pre-allocated mono voices
// (PDSynth). Character comes from three tricks:
//   • pitch sweeps — a one-shot envelope attached as a frequency modulator,
//     scaled in OCTAVES via setValueScale (retriggers on every note-on);
//   • little jingles — multi-note phrases scheduled into the near future
//     with playNote's `when` parameter (sound time, 44.1k samples/sec);
//   • cycling variants — repeat-heavy sounds rotate through tiny pitch
//     tables so back-to-back plays don't sound stamped out.
//
// Waveform palette is deliberately warm: triangle/sine for UI and chimes,
// sawtooth (with a detuned chorus layer) for fanfare brass, noise for
// impacts. Square is avoided — it reads as "8-bit console" instantly.
// Melodic voices get a few ms of attack to soften the chip-click onset.
//
// Determinism contract (selftest T47): nothing here may touch the sim RNG
// (_rng_state) — all variation comes from static cycling counters.
// Host-test contract: under tools/hosttest.c pd->sound is NULL and
// sound_init() never runs, so every entry point bails on !g_ready.
// ---------------------------------------------------------------------------

// --- UI + shop voices ---
static PDSynth* sUI    = NULL;   // cursor ticks, select/cancel blips (triangle)
static PDSynth* sDeny  = NULL;   // soft "can't do that" bup (sine, quiet)
static PDSynth* sCoin  = NULL;   // gold dings (triangle)
static PDSynth* sCoin2 = NULL;   // sine shimmer an octave up, layered quietly
static PDSynth* sTock  = NULL;   // wooden ratchet (crank reorder, dice settle)
static PDSynth* sRoll  = NULL;   // dice shuffle (noise)
static PDSynth* sIce   = NULL;   // freeze/unfreeze crystals, heart shatter (sine)
static PDSynth* sChomp = NULL;   // food crunch (noise)
static PDSynth* sPop   = NULL;   // pickup/place pop (runtime-flipped sweep)
static PDSynthEnvelope* ePop = NULL;

// --- jingle voices (level-up, results, fanfares) ---
static PDSynth* sLead  = NULL;   // sawtooth melody — brassy, not chippy
static PDSynth* sLead2 = NULL;   // detuned twin: chorus thickness
static PDSynth* sBass  = NULL;   // triangle support an octave down
static PDSynth* sWah   = NULL;   // drooping sad brass ("wah wah")
static PDSynth* sHorn  = NULL;   // alphorn (triangle, soft attack)
static PDSynth* sHorn2 = NULL;   // alphorn overtone
static PDSynth* sBell  = NULL;   // cowbell pair — triangle + detuned sine,
static PDSynth* sBell2 = NULL;   //   the beat frequency reads as struck metal

// --- battle voices ---
static PDSynth* sCrack  = NULL;  // impact noise transient
static PDSynth* sThump  = NULL;  // impact low body (fast falling pitch)
static PDSynth* sBoom   = NULL;  // faint explosion body (low noise wash)
static PDSynth* sDrop   = NULL;  // faint explosion floor (deep pitch drop)
static PDSynth* sWhoosh = NULL;  // projectile launch (rising noise)
static PDSynth* sSpawn  = NULL;  // summon boing (rising triangle)
static PDSynth* sSpark  = NULL;  // buff sparkle (sine glints)

static bool g_ready = false;

// milliseconds -> sound-time sample offset (44.1 kHz)
#define MS(x) ((uint32_t)((x) * 441u / 10u))

static PDSynth* make_synth(SoundWaveform wave, float a, float d, float sus, float r)
{
    PDSynth* s = pd->sound->synth->newSynth();
    if (!s) return NULL;
    pd->sound->synth->setWaveform(s, wave);
    pd->sound->synth->setAttackTime(s, a);
    pd->sound->synth->setDecayTime(s, d);
    pd->sound->synth->setSustainLevel(s, sus);
    pd->sound->synth->setReleaseTime(s, r);
    return s;
}

// Attach a one-shot pitch sweep: the envelope ramps 0->1 over `ramp` seconds
// on each note-on, bending the pitch by `octaves` (negative = downward);
// sustain 1 then holds the end pitch for the rest of the note.
static PDSynthEnvelope* attach_sweep(PDSynth* s, float ramp, float octaves)
{
    if (!s) return NULL;
    PDSynthEnvelope* env = pd->sound->envelope->newEnvelope(ramp, 0.0f, 1.0f, 0.0f);
    if (!env) return NULL;
    pd->sound->signal->setValueScale((PDSynthSignal*)env, octaves);
    pd->sound->synth->setFrequencyModulator(s, (PDSynthSignalValue*)env);
    return env;
}

void sound_init(void)
{
    SoundChannel* ch = pd->sound->getDefaultChannel();

    sUI    = make_synth(kWaveformTriangle, 0.003f, 0.05f, 0.0f,  0.03f);
    sDeny  = make_synth(kWaveformSine,     0.004f, 0.07f, 0.0f,  0.04f);
    sCoin  = make_synth(kWaveformTriangle, 0.002f, 0.09f, 0.0f,  0.06f);
    sCoin2 = make_synth(kWaveformSine,     0.002f, 0.09f, 0.0f,  0.06f);
    sTock  = make_synth(kWaveformTriangle, 0.0f,   0.045f, 0.0f, 0.03f);
    sRoll  = make_synth(kWaveformNoise,    0.0f,   0.04f, 0.0f,  0.02f);
    sIce   = make_synth(kWaveformSine,     0.002f, 0.10f, 0.0f,  0.06f);
    sChomp = make_synth(kWaveformNoise,    0.0f,   0.07f, 0.0f,  0.04f);
    sPop   = make_synth(kWaveformTriangle, 0.002f, 0.10f, 0.0f,  0.05f);
    ePop   = attach_sweep(sPop, 0.06f, 0.7f);

    sLead  = make_synth(kWaveformSawtooth, 0.008f, 0.10f, 0.25f, 0.08f);
    sLead2 = make_synth(kWaveformSawtooth, 0.010f, 0.10f, 0.25f, 0.08f);
    sBass  = make_synth(kWaveformTriangle, 0.006f, 0.12f, 0.30f, 0.10f);
    sWah   = make_synth(kWaveformSawtooth, 0.012f, 0.28f, 0.20f, 0.12f);
    attach_sweep(sWah, 0.30f, -0.35f);
    sHorn  = make_synth(kWaveformTriangle, 0.04f,  0.10f, 0.65f, 0.22f);
    sHorn2 = make_synth(kWaveformTriangle, 0.06f,  0.10f, 0.50f, 0.22f);
    sBell  = make_synth(kWaveformTriangle, 0.0f,   0.30f, 0.0f,  0.12f);
    sBell2 = make_synth(kWaveformSine,     0.0f,   0.30f, 0.0f,  0.12f);

    sCrack  = make_synth(kWaveformNoise,   0.0f,   0.05f, 0.0f,  0.03f);
    sThump  = make_synth(kWaveformTriangle, 0.0f,  0.09f, 0.0f,  0.05f);
    attach_sweep(sThump, 0.08f, -1.3f);
    sBoom   = make_synth(kWaveformNoise,   0.0f,   0.16f, 0.0f,  0.08f);
    sDrop   = make_synth(kWaveformTriangle, 0.0f,  0.14f, 0.0f,  0.07f);
    attach_sweep(sDrop, 0.12f, -1.4f);
    sWhoosh = make_synth(kWaveformNoise,   0.01f,  0.10f, 0.30f, 0.05f);
    attach_sweep(sWhoosh, 0.09f, 0.8f);
    sSpawn  = make_synth(kWaveformTriangle, 0.002f, 0.12f, 0.30f, 0.06f);
    attach_sweep(sSpawn, 0.09f, 0.9f);
    sSpark  = make_synth(kWaveformSine,    0.002f, 0.06f, 0.0f,  0.04f);

    PDSynth* all[] = {
        sUI, sDeny, sCoin, sCoin2, sTock, sRoll, sIce, sChomp, sPop,
        sLead, sLead2, sBass, sWah, sHorn, sHorn2, sBell, sBell2,
        sCrack, sThump, sBoom, sDrop, sWhoosh, sSpawn, sSpark,
    };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (all[i] && ch) pd->sound->channel->addSource(ch, (SoundSource*)all[i]);
    }
    g_ready = true;
}

// All public sfx_* go through these. snd_on() is the single gate for both
// the player mute setting and the host-test NULL-pd->sound case.
static bool snd_on(void) {
    return g_ready && !(game.settings & SETTING_SFX_MUTE);
}
static void play(PDSynth* s, float hz, float vol, float len) {
    if (s) pd->sound->synth->playNote(s, hz, vol, len, 0);
}
static void sched(PDSynth* s, float hz, float vol, float len, uint32_t at) {
    if (s) pd->sound->synth->playNote(s, hz, vol, len, at);
}
static uint32_t snd_now(void) { return pd->sound->getCurrentTime(); }

// ---------------------------------------------------------------------------
// UI / shop
// ---------------------------------------------------------------------------
void sfx_tick(void) {          // cursor move — soft alternating pip
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float tk[2] = { 950.0f, 880.0f };
    play(sUI, tk[n & 1], 0.16f, 0.03f);
    n++;
}

void sfx_select(void) {        // A-confirm — two quick rising blips
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sUI, 660.0f, 0.32f, 0.05f, t0);
    sched(sUI, 990.0f, 0.32f, 0.07f, t0 + MS(55));
}

void sfx_cancel(void) {        // B-cancel — the same shape, falling
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sUI, 494.0f, 0.28f, 0.05f, t0);
    sched(sUI, 370.0f, 0.28f, 0.07f, t0 + MS(55));
}

void sfx_deny(void) {          // action refused — a muted low "bu-bup"
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sDeny, 160.0f, 0.15f, 0.05f, t0);
    sched(sDeny, 140.0f, 0.15f, 0.06f, t0 + MS(50));
}

void sfx_buy(void) {           // coin ding — ONE note, three rotating pitches
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float coins[3] = { 1319.0f, 1397.0f, 1245.0f }; // E6 F6 Eb6
    float c = coins[n % 3];
    n++;
    play(sCoin,  c,        0.42f, 0.11f);
    play(sCoin2, c * 2.0f, 0.12f, 0.11f);   // sine shimmer an octave up
}

void sfx_food(void) {          // chomp-chomp, then a contented blip
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float ch[3][2] = {
        { 500.0f, 350.0f }, { 560.0f, 380.0f }, { 460.0f, 320.0f },
    };
    const float* c = ch[n % 3];
    n++;
    uint32_t t0 = snd_now();
    sched(sChomp, c[0], 0.40f, 0.05f, t0);
    sched(sChomp, c[1], 0.45f, 0.06f, t0 + MS(90));
    sched(sUI,  784.0f, 0.26f, 0.06f, t0 + MS(200));
}

void sfx_sell(void) {          // pet hops off, refund clinks in
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    if (ePop) pd->sound->signal->setValueScale((PDSynthSignal*)ePop, -0.5f);
    play(sPop, 392.0f, 0.34f, 0.08f);
    sched(sCoin,  988.0f, 0.34f, 0.05f, t0 + MS(110));
    sched(sCoin, 1175.0f, 0.34f, 0.09f, t0 + MS(165));
}

void sfx_roll(void) {          // fast shuffle-up, then a settle thock + glint
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sRoll, 1100.0f, 0.26f, 0.03f, t0);
    sched(sRoll, 1500.0f, 0.24f, 0.03f, t0 + MS(35));
    sched(sRoll, 1900.0f, 0.22f, 0.03f, t0 + MS(70));
    sched(sTock,  392.0f, 0.34f, 0.05f, t0 + MS(105));
    sched(sSpark, 1568.0f, 0.18f, 0.05f, t0 + MS(120));
}

void sfx_freeze(void) {        // crystalline rising "ching"
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sIce, 1568.0f, 0.30f, 0.05f, t0);
    sched(sIce, 2093.0f, 0.30f, 0.10f, t0 + MS(70));
}

void sfx_unfreeze(void) {      // the thaw — same crystal, falling, softer
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sIce, 1046.0f, 0.26f, 0.05f, t0);
    sched(sIce,  784.0f, 0.26f, 0.08f, t0 + MS(60));
}

void sfx_pickup(void) {        // rising pop — pet lifted for reorder
    if (!snd_on()) return;
    if (ePop) pd->sound->signal->setValueScale((PDSynthSignal*)ePop, 0.7f);
    play(sPop, 330.0f, 0.34f, 0.09f);
}

void sfx_place(void) {         // falling pop — pet set back down
    if (!snd_on()) return;
    if (ePop) pd->sound->signal->setValueScale((PDSynthSignal*)ePop, -0.7f);
    play(sPop, 494.0f, 0.34f, 0.09f);
}

void sfx_shift(void) {         // crank-reorder ratchet: alternating wood tocks
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float tk[2] = { 262.0f, 294.0f };
    play(sTock, tk[n & 1], 0.30f, 0.04f);
    n++;
}

// Fanfare note: saw lead + a 0.9%-detuned twin (chorus) + triangle an
// octave down. The detune beat is what keeps it from sounding like a chip.
static void brass(float hz, float vol, float len, uint32_t at) {
    sched(sLead,  hz,          vol,         len, at);
    sched(sLead2, hz * 1.009f, vol * 0.6f,  len, at);
    sched(sBass,  hz * 0.5f,   vol * 0.85f, len, at);
}

void sfx_levelup(void) {       // merge payoff — bright rising arpeggio
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    static const float hi[4] = { 659.0f, 784.0f, 988.0f, 1319.0f }; // E5 G5 B5 E6
    for (int i = 0; i < 4; i++)
        brass(hi[i], 0.26f, (i == 3) ? 0.18f : 0.06f, t0 + MS(60 * i));
}

// ---------------------------------------------------------------------------
// Ceremony — battle intro, results, run bookends
// ---------------------------------------------------------------------------
void sfx_horn(void) {          // alphorn call: down the valley, to arms
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sHorn,  196.0f, 0.40f, 0.24f, t0);            // G3
    sched(sHorn,  262.0f, 0.42f, 0.55f, t0 + MS(240));  // C4
    sched(sHorn2, 392.0f, 0.16f, 0.50f, t0 + MS(260));  // G4 overtone
}

// Triangle + detuned sine a semitone apart — the beating reads as struck
// metal without the harshness of squares.
static void bell(float hz, float vol) {
    play(sBell,  hz,          vol,        0.30f);
    play(sBell2, hz * 1.059f, vol * 0.9f, 0.30f);
}

void sfx_stamp(void) {         // VS rosette lands — low cowbell clank
    if (!snd_on()) return;
    bell(370.0f, 0.50f);
    play(sCrack, 400.0f, 0.40f, 0.05f);
}

void sfx_bell(void) {          // summit claimed — high cowbell ding
    if (!snd_on()) return;
    bell(1319.0f, 0.38f);
}

void sfx_result_win(void) {    // round won — short major fanfare
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    static const float hi[4] = { 523.0f, 659.0f, 784.0f, 1047.0f }; // C5 E5 G5 C6
    for (int i = 0; i < 4; i++)
        brass(hi[i], 0.32f, (i == 3) ? 0.30f : 0.09f, t0 + MS(90 * i));
}

void sfx_result_lose(void) {   // round lost — two-note "wah wah"
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sWah, 392.0f, 0.30f, 0.22f, t0);
    sched(sWah, 330.0f, 0.30f, 0.38f, t0 + MS(260));
}

void sfx_result_draw(void) {   // stalemate — two flat horn "meh"s
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sHorn, 262.0f, 0.30f, 0.24f, t0);
    sched(sHorn, 262.0f, 0.25f, 0.30f, t0 + MS(280));
}

void sfx_shatter(void) {       // a heart breaks — crack + falling crystals
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sCrack, 2500.0f, 0.38f, 0.06f, t0);
    sched(sIce,   1760.0f, 0.30f, 0.06f, t0 + MS(40));
    sched(sIce,   1175.0f, 0.28f, 0.09f, t0 + MS(110));
}

void sfx_win(void) {           // THE RUN IS WON — full fanfare + cowbell
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    static const struct { float hz; uint16_t at; float len; } ph[6] = {
        {  523.0f,   0, 0.08f }, {  659.0f,  90, 0.08f },
        {  784.0f, 180, 0.08f }, { 1047.0f, 270, 0.20f },
        {  784.0f, 470, 0.09f }, { 1047.0f, 560, 0.45f },
    };
    for (int i = 0; i < 6; i++)
        brass(ph[i].hz, 0.36f, ph[i].len, t0 + MS(ph[i].at));
    sched(sBell,  1568.0f, 0.32f, 0.30f, t0 + MS(560));
    sched(sBell2, 1661.0f, 0.28f, 0.30f, t0 + MS(560));
}

void sfx_lose(void) {          // game over — slow descending dirge
    if (!snd_on()) return;
    uint32_t t0 = snd_now();
    sched(sWah, 294.0f, 0.34f, 0.30f, t0);             // D4
    sched(sWah, 262.0f, 0.34f, 0.30f, t0 + MS(340));   // C4
    sched(sWah, 220.0f, 0.34f, 0.30f, t0 + MS(680));   // A3
    sched(sWah, 196.0f, 0.34f, 0.55f, t0 + MS(1020));  // G3
}

// ---------------------------------------------------------------------------
// Battle
// ---------------------------------------------------------------------------
// Impact for clash contact and projectile landings, weighted by damage:
// bigger hits are louder and sit lower. Deterministic pitch wobble (a
// cycling table, NOT rng_float) so repeats don't sound mechanical — and so
// audio can never perturb the battle's simulation RNG (_rng_state).
// Playback must stay reproducible; see battle.c determinism note.
void sfx_attack_hit(int dmg)
{
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float jit[4] = { 0.0f, 9.0f, -6.0f, 14.0f };
    float j = jit[n & 3];
    n++;
    if (dmg < 1)  dmg = 1;
    if (dmg > 20) dmg = 20;
    float w = (float)dmg;
    play(sCrack, 800.0f + j * 12.0f,     0.26f + w * 0.011f, 0.045f);
    play(sThump, 135.0f + j - w * 1.5f,  0.28f + w * 0.009f, 0.09f);
}

void sfx_attack(void) { sfx_attack_hit(4); }

void sfx_projectile(void) {    // ability shot leaves the bow — soft whoosh up
    if (!snd_on()) return;
    play(sWhoosh, 600.0f, 0.16f, 0.10f);
}

void sfx_faint(void) {         // a pet pops — snap, boom, and a deep drop
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float drop[3] = { 130.0f, 118.0f, 145.0f };
    float d = drop[n % 3];
    n++;
    play(sCrack, 550.0f, 0.40f, 0.05f);   // snap transient
    play(sBoom,  200.0f, 0.42f, 0.16f);   // explosion body
    play(sDrop,  d,      0.40f, 0.13f);   // floor falls out
}

void sfx_summon(void) {        // new friend boings in
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float base[2] = { 262.0f, 294.0f };
    play(sSpawn, base[n & 1], 0.34f, 0.12f);
    n++;
}

void sfx_buff(void) {          // stat sparkle — two glassy glints
    if (!snd_on()) return;
    static uint8_t n = 0;
    static const float gl[2][2] = {
        { 1047.0f, 1568.0f }, { 1175.0f, 1760.0f },
    };
    const float* g = gl[n & 1];
    n++;
    uint32_t t0 = snd_now();
    sched(sSpark, g[0], 0.26f, 0.045f, t0);
    sched(sSpark, g[1], 0.26f, 0.07f,  t0 + MS(50));
}
