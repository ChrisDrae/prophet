#pragma once

#include "DspMath.h"
#include "effects/Effects.h"

// =================================================================================================
//  EngineParams — every setting the engine reads, as plain numbers (no JUCE types).
//
//  The processor fills one of these from the host parameters once per block. The meaning and range
//  of every field is listed in docs/sound-design.md; the curves that turn a 0..1 knob into
//  seconds or Hz are the functions at the bottom of this file. The defaults are the init sound,
//  and match the host parameters' defaults in Parameters.cpp.
// =================================================================================================
namespace seer
{
enum class KeyTrack
{
    off,
    half,
    full
};

/// Which key sounds when several are held in unison.
enum class KeyMode
{
    low,
    high,
    last,
    lowRetrigger,
    highRetrigger,
    lastRetrigger
};

enum class GlideMode
{
    fixedRate,       ///< bigger intervals take longer
    fixedRateLegato, ///< ...and only when notes overlap
    fixedTime,       ///< every interval takes the same time
    fixedTimeLegato
};

enum class LfoShape
{
    triangle,
    sawtooth,
    reverseSawtooth,
    square,
    random
};

struct EngineParams
{
    // Oscillators. Frequency is in semitones, 24 = concert pitch (range 0..60).
    float osc1Freq = 24.0f, osc2Freq = 24.0f;
    float osc2Fine = 3.0f; ///< cents
    float osc1Shape = 0.5f, osc2Shape = 0.5f;
    float osc1Pw = 0.5f, osc2Pw = 0.5f;
    bool osc1Sync = false;
    bool osc2LowFreq = false;
    bool osc2Kbd = true;

    // Mixer
    float mixOsc1 = 1.0f, mixOsc2 = 0.8f, mixSub = 0.0f, mixNoise = 0.0f;

    // Filters. Cutoff is 0..164: semitones above 10 Hz.
    float lpCutoff = 110.0f, lpRes = 0.1f, lpEnv = 0.3f;
    bool lpVel = true;
    KeyTrack lpKey = KeyTrack::full;
    float hpCutoff = 0.0f, hpRes = 0.0f, hpEnv = 0.0f;
    bool hpVel = false;
    KeyTrack hpKey = KeyTrack::off;

    // Envelopes (knob positions, 0..1)
    float fEnvA = 0.1f, fEnvD = 0.45f, fEnvS = 0.5f, fEnvR = 0.35f;
    float aEnvA = 0.12f, aEnvD = 0.4f, aEnvS = 0.9f, aEnvR = 0.3f;
    float aEnvAmt = 1.0f;
    bool aEnvVel = true;

    // LFO
    float lfoRate = 0.55f, lfoAmt = 0.0f;
    LfoShape lfoShape = LfoShape::triangle;
    bool lfoFreq1 = true, lfoFreq2 = true, lfoPw = false, lfoAmp = false, lfoLp = false, lfoHp = false;

    // Poly Mod: the filter envelope and oscillator 2 as modulation sources (both -1..1)
    float pmEnv = 0.0f, pmOsc2 = 0.0f;
    bool pmFreq1 = false, pmShape1 = false, pmPw1 = false, pmLp = false, pmHp = false;

    // Aftertouch (amount -1..1)
    float atAmt = 0.0f;
    bool atFreq1 = false, atFreq2 = false, atLfo = false, atAmp = false, atLp = false, atHp = false;

    // Voice
    bool unison = false;
    int unisonVoices = 1; ///< 1..6
    float unisonDetune = 0.2f;
    KeyMode keyMode = KeyMode::last;
    bool glideOn = false;
    float glideRate = 0.3f;
    GlideMode glideMode = GlideMode::fixedRate;
    float slop = 0.08f;
    float panSpread = 0.25f;

    // Output
    float dist = 0.0f;
    bool fxOn = true;
    EffectType fxAType = EffectType::off, fxBType = EffectType::off;
    float fxAMix = 0.3f, fxAP1 = 0.5f, fxAP2 = 0.4f;
    float fxBMix = 0.3f, fxBP1 = 0.5f, fxBP2 = 0.4f;
    float voiceVol = 0.75f;

    // Player settings
    float masterVolDb = 0.0f;
    float masterTune = 0.0f; ///< cents
    int bendRange = 2;       ///< semitones
    int transpose = 0;       ///< octaves, -2..2
    int voices = 6;          ///< 6, 8, 10 or 12
    bool highQuality = false; ///< 4x oversampling instead of 2x
};

namespace curves
{
/// Attack knob -> seconds: 1 ms .. 10 s.
inline double attackSeconds(float knob) noexcept { return 0.001 * std::pow(10000.0, static_cast<double>(knob)); }
/// Decay and release knobs -> seconds: 3 ms .. 20 s.
inline double decaySeconds(float knob) noexcept { return 0.003 * std::pow(20.0 / 0.003, static_cast<double>(knob)); }
/// LFO rate knob -> Hz: 0.02 .. 500 Hz.
inline double lfoHz(float knob) noexcept { return 0.02 * std::pow(25000.0, static_cast<double>(knob)); }
/// Cutoff setting (0..164, in semitones) -> Hz, before the filter's own upper limit.
inline double cutoffHz(double setting) noexcept { return 10.0 * std::pow(2.0, setting / 12.0); }

inline constexpr float kEnvToCutoffSemitones = 96.0f; ///< the filters' Env Amount at full
inline constexpr float kLfoToPitchSemitones = 12.0f;  ///< LFO to pitch at full amount (the amount is squared)
inline constexpr float kLfoToCutoffSemitones = 48.0f;
inline constexpr float kLfoToPulseWidth = 0.45f;
inline constexpr float kPolyModEnvToPitch = 60.0f;    ///< semitones at full amount (the amount is squared)
inline constexpr float kPolyModOsc2ToPitch = 48.0f;   ///< semitones each way at full amount (squared)
inline constexpr float kPolyModOsc2ToCutoff = 48.0f;  ///< semitones each way at full amount
inline constexpr float kAftertouchToPitch = 12.0f;    ///< semitones at full pressure and amount
inline constexpr float kAftertouchToCutoff = 60.0f;
inline constexpr float kSlopCents = 50.0f;            ///< the widest drift, at Slop = 1
inline constexpr float kUnisonDetuneCents = 35.0f;    ///< the outermost voices of a stack, at Detune = 1
inline constexpr float kLowFreqSemitones = 96.0f;     ///< how far Low Freq drops oscillator 2: 8 octaves

/// Glide knob -> seconds (per octave in the rate modes, per note in the time modes): 2 ms .. 4 s.
inline double glideSeconds(float knob) noexcept { return knob <= 0.0f ? 0.0 : 0.002 * std::pow(2000.0, static_cast<double>(knob)); }
} // namespace curves
} // namespace seer
