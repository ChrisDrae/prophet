#pragma once

#include "DspMath.h"

#include <array>
#include <cmath>

// =================================================================================================
//  Band-limited steps and ramps for the oscillators.
//
//  A sawtooth's reset or a pulse's edge is an instant jump. Sampled as it is, a jump contains
//  partials far above half the sample rate, and those fold back as out-of-tune tones (aliasing).
//  The cure: replace each jump with the jump a perfect low-pass filter would have let through, a
//  smooth S-curve with a little ripple before and after. This table holds that curve (the running
//  sum of a windowed sinc), and the oscillator adds the *difference* between it and the hard jump
//  around every jump it makes. Corners (a triangle's peaks) get the same treatment with the
//  curve's integral, a band-limited ramp.
//
//  The curve is symmetric, so half of it lies *before* the jump: the oscillator's output is
//  therefore delayed by kHalf samples.
// =================================================================================================
namespace seer::dsp
{
class BlepTable
{
public:
    static constexpr int kHalf = 8;          ///< zero crossings on each side: the delay in samples
    static constexpr int kTaps = 2 * kHalf;  ///< samples touched by one jump
    static constexpr int kOver = 64;         ///< table points per sample

    static const BlepTable& get()
    {
        static const BlepTable table;
        return table;
    }

    /// [DSP] What to add to a hard unit step to make it band-limited, `t` samples after the step
    /// (negative: before it). Zero outside +-kHalf.
    float stepResidual(float t) const noexcept { return raw(step, t) - (t >= 0.0f ? 1.0f : 0.0f); }

    /// [DSP] The same for a corner: a slope that changes by 1 per sample at t = 0.
    float rampResidual(float t) const noexcept { return raw(ramp, t) - std::max(t, 0.0f); }

private:
    static constexpr int kSize = (kTaps + 2) * kOver + 2; // a margin past the end, where step = 1

    std::array<float, kSize> step {}, ramp {};

    BlepTable()
    {
        // The low-pass kernel: sinc (cutoff at half the sample rate) under a Blackman-Harris window.
        const int points = kTaps * kOver + 1;
        std::array<double, kTaps * kOver + 1> kernel {};
        double total = 0.0;
        for (int i = 0; i < points; ++i)
        {
            const double t = static_cast<double>(i) / kOver - kHalf;
            const double sinc = std::abs(t) < 1.0e-9 ? 1.0 : std::sin(kPi * t) / (kPi * t);
            const double w = kTwoPi * i / (points - 1);
            const double window = 0.35875 - 0.48829 * std::cos(w) + 0.14128 * std::cos(2.0 * w) - 0.01168 * std::cos(3.0 * w);
            kernel[static_cast<size_t>(i)] = sinc * window;
            total += kernel[static_cast<size_t>(i)];
        }
        // Step = running integral of the kernel (trapezoids), scaled to end at exactly 1.
        // Ramp = running integral of the step.
        double s = 0.0, r = 0.0, previousStep = 0.0;
        total -= 0.5 * (kernel.front() + kernel.back());
        for (int i = 0; i < kSize; ++i)
        {
            if (i > 0 && i < points)
                s += 0.5 * (kernel[static_cast<size_t>(i - 1)] + kernel[static_cast<size_t>(i)]) / total;
            else if (i >= points)
                s = 1.0;
            if (i > 0)
                r += 0.5 * (previousStep + s) / kOver;
            previousStep = s;
            step[static_cast<size_t>(i)] = static_cast<float>(s);
            ramp[static_cast<size_t>(i)] = static_cast<float>(r);
        }
        // The ramp's integral must land on the straight line t exactly (it does up to rounding).
        const double drift = r - (static_cast<double>(kSize - 1) / kOver - kHalf);
        for (int i = 0; i < kSize; ++i)
            ramp[static_cast<size_t>(i)] -= static_cast<float>(drift * step[static_cast<size_t>(i)]);
    }

    static float raw(const std::array<float, kSize>& table, float t) noexcept
    {
        const float x = (t + kHalf) * kOver;
        if (x <= 0.0f)
            return 0.0f;
        const int i = std::min(static_cast<int>(x), kSize - 2);
        const float f = std::min(x - static_cast<float>(i), 1.0f);
        return table[static_cast<size_t>(i)] + f * (table[static_cast<size_t>(i + 1)] - table[static_cast<size_t>(i)]);
    }
};
} // namespace seer::dsp
