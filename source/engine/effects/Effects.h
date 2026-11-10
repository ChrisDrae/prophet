#pragma once

#include "../Filters.h"
#include "Reverb.h"

#include <array>
#include <cmath>
#include <vector>

// =================================================================================================
//  The effects section: two slots in series (A, then B), each with a type, a mix and two knobs.
//
//      type            knob 1        knob 2
//      BBD delay       time          feedback      warm, darker with every repeat and with longer times
//      Digital delay   time          feedback      clean
//      Chorus          rate          depth         two modulated short delays, left and right in opposition
//      Phaser 1        rate          depth         six stages, strong resonance
//      Phaser 2        rate          depth         six stages, mild resonance
//      Hall/Room/Plate time          early refl.   slot B only
//      Spring          decay         tone          slot B only
//
//  With the type Off or the mix at zero a slot does nothing at all: the signal passes untouched.
//  [RT] process() never allocates; prepare() sizes every buffer.
// =================================================================================================
namespace seer
{
enum class EffectType
{
    off,
    bbdDelay,
    digitalDelay,
    chorus,
    phaser1,
    phaser2,
    hall,
    room,
    plate,
    spring
};

class EffectSlot
{
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    /// [RT] The settings for the audio that follows. Knobs are 0..1.
    void set(EffectType type, float mix, float knob1, float knob2) noexcept;

    /// [RT] In place.
    void process(float* left, float* right, int numSamples) noexcept;

    /// Knob 1 as a delay time in seconds (20 ms .. 1 s).
    static double delaySeconds(float knob) noexcept { return 0.02 * std::pow(50.0, static_cast<double>(knob)); }
    /// Knob 1 as an LFO rate in Hz (0.05 .. 8 Hz).
    static double rateHz(float knob) noexcept { return 0.05 * std::pow(160.0, static_cast<double>(knob)); }

private:
    void processDelay(float* left, float* right, int numSamples, bool bbd) noexcept;
    void processChorus(float* left, float* right, int numSamples) noexcept;
    void processPhaser(float* left, float* right, int numSamples, float resonance) noexcept;
    void processReverb(float* left, float* right, int numSamples) noexcept;

    double sampleRate = 48000.0;
    EffectType type = EffectType::off, activeType = EffectType::off;
    float mix = 0.0f, knob1 = 0.5f, knob2 = 0.5f;
    float mixSmooth = 0.0f, mixCoef = 0.01f;
    bool wasIdle = true;

    // Delays and chorus
    std::array<dsp::DelayLine, 2> delay;
    float delaySamples = 4800.0f, delayCoef = 0.001f;
    std::array<dsp::OnePole, 2> loopTone1, loopTone2;
    std::array<dsp::DcBlocker, 2> loopDc;
    dsp::Lfo lfo;

    // Phaser: six first-order all-pass stages per channel
    std::array<std::array<float, 6>, 2> phaserState {};
    std::array<float, 2> phaserFeedback {};

    Reverb reverb;
    std::vector<float> wetLeft, wetRight;
};
} // namespace seer
