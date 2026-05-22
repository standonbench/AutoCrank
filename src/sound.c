#include "game.h"

// ---------------------------------------------------------------------------
// Synth-based SFX. Each effect is a short shaped note (or a couple layered).
// ---------------------------------------------------------------------------
static PDSynth* sBuy    = NULL;   // bright coin ding
static PDSynth* sSell   = NULL;   // descending blip
static PDSynth* sRoll   = NULL;   // noise rattle
static PDSynth* sAttack = NULL;   // punchy noise hit
static PDSynth* sFaint  = NULL;   // descending whoosh
static PDSynth* sWin    = NULL;   // bright triangle fanfare
static PDSynth* sLose   = NULL;   // low sad tone

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

void sound_init(void)
{
    SoundChannel* ch = pd->sound->getDefaultChannel();

    sBuy    = make_synth(kWaveformSquare,   0.0f, 0.05f, 0.0f, 0.04f);
    sSell   = make_synth(kWaveformTriangle, 0.0f, 0.08f, 0.0f, 0.05f);
    sRoll   = make_synth(kWaveformNoise,    0.0f, 0.10f, 0.0f, 0.04f);
    sAttack = make_synth(kWaveformNoise,    0.0f, 0.06f, 0.0f, 0.03f);
    sFaint  = make_synth(kWaveformSawtooth, 0.0f, 0.18f, 0.0f, 0.10f);
    sWin    = make_synth(kWaveformTriangle, 0.01f, 0.12f, 0.3f, 0.20f);
    sLose   = make_synth(kWaveformSquare,   0.0f, 0.25f, 0.0f, 0.15f);

    PDSynth* all[] = { sBuy, sSell, sRoll, sAttack, sFaint, sWin, sLose };
    for (int i = 0; i < 7; i++) {
        if (all[i] && ch) pd->sound->channel->addSource(ch, (SoundSource*)all[i]);
    }
}

// note frequency helper
static void play(PDSynth* s, float hz, float vol, float len) {
    if (s) pd->sound->synth->playNote(s, hz, vol, len, 0);
}

void sfx_buy(void)    { play(sBuy,   880.0f, 0.45f, 0.08f); }
void sfx_sell(void)   { play(sSell,  330.0f, 0.40f, 0.10f); }
void sfx_roll(void)   { play(sRoll,  220.0f, 0.35f, 0.10f); }
void sfx_attack(void) { play(sAttack, 140.0f + rng_float() * 60.0f, 0.35f, 0.06f); }
void sfx_faint(void)  { play(sFaint, 200.0f, 0.40f, 0.20f); }
void sfx_win(void)    { play(sWin,   660.0f, 0.50f, 0.35f); }
void sfx_lose(void)   { play(sLose,  120.0f, 0.45f, 0.40f); }
