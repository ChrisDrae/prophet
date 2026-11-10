#pragma once

#include "Filters.h"

#include <array>

// =================================================================================================
//  The two filters of a voice.
//
//  Both are "zero-delay feedback" designs: the feedback inside an analog filter is instant, and a
//  digital copy that delays it by one sample goes out of tune and unstable as the cutoff rises.
//  Here each sample is solved so that input, output and feedback agree at the same instant
//  (the trapezoidal / "TPT" method, Zavalishin, "The Art of VA Filter Design").
//
//  Cutoff is passed as g = tan(pi * cutoff / sampleRate), which the caller computes (and can
//  interpolate), so sweeping costs no trigonometry per sample.
// =================================================================================================
namespace seer::dsp
{
/// [DSP] g for a cutoff in Hz.
inline float cutoffToG(double hz, double sampleRate) noexcept
{
    return static_cast<float>(std::tan(kPi * std::clamp(hz, 5.0, 0.45 * sampleRate) / sampleRate));
}

/// [DSP] 4-pole (24 dB/octave) resonant low-pass: four one-pole stages in a row, the output fed
/// back, inverted, to the input.
///
/// Two things in it saturate, as in the circuit:
///   - the input, softly: turn the mixer up and the filter thickens and compresses;
///   - the feedback path: this is what holds self-oscillation at a steady level, and on pitch,
///     instead of letting it grow without bound.
/// The four stages themselves are linear, which keeps the cutoff (and so the pitch of the
/// self-oscillation) exactly where the control says. The feedback loop, saturation included, is
/// solved exactly for every sample by Newton's method.
class LadderFilter
{
public:
    /// Input level at which the filter starts to bend. One oscillator at full level peaks at 1.
    static constexpr float kInputKnee = 3.0f;
    /// Level at which the feedback path starts to bend: sets how loud self-oscillation gets.
    static constexpr float kFeedbackKnee = 1.0f;
    /// Feedback at which the filter starts to oscillate by itself.
    static constexpr float kSelfOscillation = 4.0f;

    void reset() noexcept
    {
        s.fill(0.0f);
        previousInput = 0.0;
    }

    /// [RT] One sample. `feedback` is 0 (no resonance) to a little over 4 (self-oscillation).
    float process(float x, float g, float feedback) noexcept
    {
        const float G = g / (1.0f + g);
        const float G2 = G * G, G4 = G2 * G2;

        // Make up part of the level the feedback takes away, so resonance doesn't empty the bass.
        x = saturateInput(x * (1.0f + 0.375f * feedback));

        // The loop: y = G4 * (x - feedback * sat(y)) + states. Start from the answer of a
        // perfectly linear loop, then two Newton steps.
        const float states = (1.0f - G) * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);
        float y = (G4 * x + states) / (1.0f + feedback * G4);
        for (int i = 0; i < 2; ++i)
        {
            const float t = fastTanh(y * (1.0f / kFeedbackKnee));
            const float error = G4 * (x - feedback * kFeedbackKnee * t) + states - y;
            y += error / (G4 * feedback * (1.0f - t * t) + 1.0f);
        }

        float v = x - feedback * kFeedbackKnee * fastTanh(y * (1.0f / kFeedbackKnee));
        for (auto& state : s)
        {
            const float step = (v - state) * G;
            v = step + state;
            state = v + step;
        }
        return v;
    }

private:
    /// kInputKnee * tanh(x / kInputKnee), with antiderivative anti-aliasing: the output is the
    /// curve's average between the previous input and this one, which keeps the harmonics the
    /// curve adds from folding back. (Costs half a sample of delay.)
    float saturateInput(float x) noexcept
    {
        const double u = static_cast<double>(x) * (1.0 / kInputKnee);
        const double step = u - previousInput;
        const double average = std::abs(step) > 1.0e-4 ? (logCosh(u) - logCosh(previousInput)) / step
                                                         : std::tanh(0.5 * (u + previousInput));
        previousInput = u;
        return kInputKnee * static_cast<float>(average);
    }

    /// ln(cosh(u)), the integral of tanh, written so that it can't overflow.
    static double logCosh(double u) noexcept
    {
        const double a = std::abs(u);
        return a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453;
    }

    std::array<float, 4> s {};
    double previousInput = 0.0;
};

/// [DSP] 2-pole (12 dB/octave) resonant high-pass: a state-variable filter.
class HighPassFilter
{
public:
    void reset() noexcept { s1 = s2 = 0.0f; }

    /// [RT] One sample. `damping` is 1 / Q: 1.41 = no resonance, smaller = a stronger peak.
    float process(float x, float g, float damping) noexcept
    {
        const float hp = (x - (damping + g) * s1 - s2) / (1.0f + damping * g + g * g);
        const float v1 = g * hp;
        const float bp = v1 + s1;
        const float v2 = g * bp;
        const float lp = v2 + s2;
        // The resonant state is limited softly: a peak can ring loudly, but not without bound.
        s1 = kLimit * fastTanh((bp + v1) * (1.0f / kLimit));
        s2 = lp + v2;
        return hp;
    }

private:
    static constexpr float kLimit = 6.0f;
    float s1 = 0.0f, s2 = 0.0f;
};
} // namespace seer::dsp
