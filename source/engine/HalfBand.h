#pragma once

#include "DspMath.h"

#include <vector>

// =================================================================================================
//  HalfBand — halves the sample rate.
//
//  The voices run at twice the host's sample rate, so that the harmonics the filter's saturation,
//  sync and audio-rate modulation create above the audible band have somewhere to go instead of
//  folding back as noise. Before every second sample is thrown away, everything above a quarter of
//  the (high) sample rate must be removed: this is that low-pass, a windowed-sinc FIR.
//
//  It delays the signal by (taps - 1) / 2 high-rate samples.
// =================================================================================================
namespace seer::dsp
{
class HalfBand
{
public:
    /// Message thread. `taps` must be odd; more taps = a steeper filter. 95 taps keep everything
    /// up to 19.7 kHz and remove everything above 24.4 kHz by 80 dB (at 44.1 kHz host rate).
    void design(int taps = 95, double kaiserBeta = 8.0)
    {
        h.assign(static_cast<size_t>(taps), 0.0f);
        const int mid = (taps - 1) / 2;
        double sum = 0.0;
        std::vector<double> c(static_cast<size_t>(taps));
        for (int i = 0; i < taps; ++i)
        {
            const int n = i - mid;
            const double sinc = n == 0 ? 0.5 : std::sin(0.5 * kPi * n) / (kPi * n);
            const double r = static_cast<double>(n) / mid;
            c[static_cast<size_t>(i)] = sinc * bessel0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel0(kaiserBeta);
            sum += c[static_cast<size_t>(i)];
        }
        for (int i = 0; i < taps; ++i)
            h[static_cast<size_t>(i)] = static_cast<float>(c[static_cast<size_t>(i)] / sum);
        history.assign(static_cast<size_t>(2 * taps), 0.0f);
        write = 0;
    }

    void reset() noexcept
    {
        std::fill(history.begin(), history.end(), 0.0f);
        write = 0;
    }

    int delayInInputSamples() const noexcept { return (static_cast<int>(h.size()) - 1) / 2; }

    /// [RT] Takes `numOutput * 2` samples from `in`, writes `numOutput` to `out`.
    void process(const float* in, float* out, int numOutput) noexcept
    {
        const int taps = static_cast<int>(h.size());
        for (int i = 0; i < numOutput; ++i)
        {
            push(in[2 * i], taps);
            push(in[2 * i + 1], taps);
            // The history is stored twice, back to back, so the newest `taps` samples are always
            // one unbroken run ending at write + taps - 1.
            const float* x = history.data() + write;
            float y = 0.0f;
            for (int k = 0; k < taps; ++k)
                y += h[static_cast<size_t>(k)] * x[k];
            out[i] = y;
        }
    }

private:
    void push(float x, int taps) noexcept
    {
        history[static_cast<size_t>(write)] = x;
        history[static_cast<size_t>(write + taps)] = x;
        write = (write + 1) % taps;
    }

    /// Modified Bessel function of the first kind, order 0 (the Kaiser window's shape).
    static double bessel0(double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    std::vector<float> h, history;
    int write = 0;
};
} // namespace seer::dsp
