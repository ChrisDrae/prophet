#pragma once

#include "EngineParams.h"
#include "Envelope.h"
#include "Oscillator.h"
#include "VoiceFilters.h"

#include <cstdint>

// =================================================================================================
//  Voice — one note's worth of synthesizer:
//
//      OSC 1 + SUB + OSC 2 + NOISE -> mixer -> low-pass -> high-pass -> VCA -> pan
//
//  It runs at the engine's oversampled rate. Slow things (pitch, glide, cutoff, levels, the LFO's
//  and aftertouch's contributions) are worked out once per control tick in updateControl();
//  render() then makes the samples, sliding the filter cutoffs smoothly from one tick's value to
//  the next. The one modulation that has to be computed for every sample is Poly Mod from
//  oscillator 2, because that source moves at audio rate.
// =================================================================================================
namespace seer
{
/// What every voice shares during a control tick.
struct Modulation
{
    float pitchOffset = 0.0f; ///< semitones: pitch wheel + master tune + transpose
    float lfo = 0.0f;         ///< the LFO's current value: -1..1 (triangle, random) or 0..1
    float lfoAmount = 0.0f;   ///< Initial Amount + mod wheel, 0..1
    float pressure = 0.0f;    ///< channel aftertouch, 0..1
};

/// How a note starts.
struct NoteStart
{
    bool retrigger = true;     ///< restart the envelopes (false: a legato change of pitch only)
    bool glide = false;        ///< slide to the note from `glideFrom`
    float glideFrom = 60.0f;   ///< semitones (a MIDI note number, possibly between keys)
    float detuneCents = 0.0f;  ///< this voice's place in a unison stack
};

class Voice
{
public:
    /// Message thread, or the audio thread when the oversampling changes (it doesn't allocate).
    /// `index` is the voice's place among the others: it sets the pan direction and this voice's
    /// small "component tolerances". `ticksLength` is the control tick in samples.
    void prepare(double oversampledRate, int index, int ticksLength);

    void noteOn(int midiNote, float velocity01, uint64_t timeStamp, const NoteStart& start = {});
    void noteOff();
    void kill();

    bool isActive() const noexcept { return ampEnv.isActive(); }
    bool isHeld() const noexcept { return held; }
    int getNote() const noexcept { return note; }
    float getPitch() const noexcept { return pitch; } ///< where the glide is right now
    uint64_t getAge() const noexcept { return age; }

    /// [RT] Once per control tick, and right after noteOn().
    void updateControl(const EngineParams& params, const Modulation& mod) noexcept;

    /// [RT] Adds `numSamples` (at most one tick) to the stereo mix.
    void render(float* left, float* right, int numSamples) noexcept;

private:
    double sampleRate = 96000.0;
    int tickLength = 8;
    double tickSeconds = 0.0001;
    int index = 0;

    // This voice's fixed deviations, like the part tolerances of one voice card.
    float cutoffTolerance = 0.0f; ///< semitones
    double timeTolerance = 1.0;   ///< envelope times are multiplied by this
    float panPosition = 0.0f;     ///< which way, and how far, Pan Spread moves it

    int note = 60;
    float velocity = 1.0f;
    bool held = false;
    uint64_t age = 0;
    bool fresh = true; ///< no control tick since the note started: don't glide into the cutoff

    // Glide
    float pitch = 60.0f;       ///< semitones, moving towards `note`
    bool gliding = false;
    float glideDistance = 0.0f; ///< the interval of the current glide, for the fixed-time modes
    float detuneCents = 0.0f;

    // Slop: each oscillator drifts on its own.
    struct Drift
    {
        float value = 0.0f, target = 0.0f, perNote = 0.0f;
        int ticksLeft = 0;
    };
    Drift drift1, drift2;
    dsp::Noise driftNoise;
    float driftCoef = 0.001f;

    dsp::Oscillator osc1, osc2;
    double subPhase = 0.0;
    dsp::Noise noise;
    dsp::LadderFilter lowPass;
    dsp::HighPassFilter highPass;
    dsp::Envelope filterEnv, ampEnv;

    // Held for one tick
    double inc1 = 0.0, inc2 = 0.0;
    float shape1 = 0.5f, shape2 = 0.5f, pw1 = 0.5f, pw2 = 0.5f;
    bool sync = false;
    float level1 = 0.0f, level2 = 0.0f, levelSub = 0.0f, levelNoise = 0.0f;
    float lpG = 0.1f, lpGStep = 0.0f, lpFeedback = 0.0f;
    float hpG = 0.001f, hpGStep = 0.0f, hpDamping = 1.414f;
    float ampGain = 0.0f;
    float gainLeft = 0.7f, gainRight = 0.7f;

    // Poly Mod from oscillator 2, per sample: depths in octaves (pitch, cutoffs) or as plain offsets.
    bool audioMod = false;
    float fmOctaves = 0.0f, shapeMod = 0.0f, pwMod = 0.0f, lpOctaves = 0.0f, hpOctaves = 0.0f;

    // Envelope settings as last applied, to skip the math while nothing changes.
    float lastFEnv[4] = { -1.0f, -1.0f, -1.0f, -1.0f }, lastAEnv[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
};
} // namespace seer
