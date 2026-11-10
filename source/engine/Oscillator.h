#pragma once

#include "BlepTable.h"

#include <array>

// =================================================================================================
//  Oscillator — one analog-style VCO: triangle, sawtooth and pulse from a single phase, blended
//  by a continuous Shape control, with hard sync.
//
//  Shape 0 = triangle, 0.5 = sawtooth, 1 = pulse; in between, the neighbours are crossfaded.
//  Because all three come from the same phase they stay in tune and in step while blending.
//
//  Every jump and corner the waveform makes is band-limited with BlepTable, at its exact position
//  between two samples. The output is delayed by BlepTable::kHalf samples.
// =================================================================================================
namespace seer::dsp
{
class Oscillator
{
public:
    static constexpr int kDelay = BlepTable::kHalf;

    void reset(double startPhase = 0.0) noexcept
    {
        phase = startPhase - std::floor(startPhase);
        ring.fill(0.0f);
        pos = 0;
        high = true;
        wrapped = false;
        wrapTime = 0.0f;
    }

    /// True if the last tick() started a new cycle; wrapFraction() is how long ago, in samples
    /// (0..1). This is what a synced oscillator listens to.
    bool hasWrapped() const noexcept { return wrapped; }
    float wrapFraction() const noexcept { return wrapTime; }

    /// [RT][DSP] One sample.
    ///   increment   cycles per sample (frequency / sample rate)
    ///   shape       0 triangle .. 0.5 sawtooth .. 1 pulse
    ///   width       pulse width, 0..1 (0.5 = square)
    ///   syncNow     the master oscillator started a cycle `syncFraction` samples ago:
    ///               restart this one at that moment
    float tick(double increment, float shape, float width, bool syncNow = false, float syncFraction = 0.0f) noexcept
    {
        const float out = ring[static_cast<size_t>(pos)];
        ring[static_cast<size_t>(pos)] = 0.0f;

        inc = std::clamp(increment, 0.0, 0.45);
        shape = std::clamp(shape, 0.0f, 1.0f);
        pw = std::clamp(width, 0.02f, 0.98f);
        gTri = shape < 0.5f ? 1.0f - 2.0f * shape : 0.0f;
        gSaw = shape < 0.5f ? 2.0f * shape : 2.0f - 2.0f * shape;
        gPulse = shape > 0.5f ? 2.0f * shape - 1.0f : 0.0f;
        wrapped = false;

        if (inc > 1.0e-9)
        {
            if (syncNow)
            {
                // Run up to the sync moment, jump back to the start of the cycle, run the rest.
                syncFraction = std::clamp(syncFraction, 0.0f, 1.0f);
                run(inc * (1.0 - syncFraction), syncFraction);
                const float p = static_cast<float>(phase);
                const float jump = gSaw * (-2.0f * p) + gTri * (-1.0f - triangle(p)) + gPulse * (high ? 0.0f : 2.0f);
                addStep(syncFraction, jump);
                if (p >= 0.5f)
                    addRamp(syncFraction, gTri * 8.0f * static_cast<float>(inc));
                phase = 0.0;
                high = true;
                run(inc * syncFraction, 0.0f);
            }
            else
            {
                run(inc, 0.0f);
            }
        }

        // The pulse width moved past the phase without the phase crossing it: still an edge.
        if (const bool shouldBeHigh = phase < pw; shouldBeHigh != high)
        {
            addStep(0.5f, gPulse * (shouldBeHigh ? 2.0f : -2.0f));
            high = shouldBeHigh;
        }

        const float p = static_cast<float>(phase);
        const float naive = gSaw * (2.0f * p - 1.0f) + gTri * triangle(p) + gPulse * ((high ? 1.0f : -1.0f) - (2.0f * pw - 1.0f));
        ring[static_cast<size_t>((pos + kDelay) & kMask)] += naive;
        pos = (pos + 1) & kMask;
        return out;
    }

private:
    static constexpr int kMask = BlepTable::kTaps - 1;

    static float triangle(float p) noexcept { return 1.0f - 4.0f * std::abs(p - 0.5f); }

    /// Advances the phase by `amount` (< 0.5), band-limiting what it passes on the way: the
    /// triangle's peak at 0.5, the pulse's falling edge at `pw`, and the end of the cycle.
    /// `tail` is the time, in samples, between the end of this stretch and the current sample.
    void run(double amount, float tail) noexcept
    {
        const float fInc = static_cast<float>(inc);
        const double a = phase;
        double b = a + amount;
        auto since = [&](double crossing) { return std::clamp(tail + static_cast<float>((b - crossing) / inc), 0.0f, 1.0f); };

        if (a < 0.5 && b >= 0.5)
            addRamp(since(0.5), -8.0f * fInc * gTri);
        if (high && b >= pw)
        {
            addStep(since(pw), -2.0f * gPulse);
            high = false;
        }
        if (b >= 1.0)
        {
            const float t = since(1.0);
            addStep(t, -2.0f * gSaw + (high ? 0.0f : 2.0f * gPulse));
            addRamp(t, 8.0f * fInc * gTri);
            high = true;
            wrapped = true;
            wrapTime = t;
            b -= 1.0;
            if (b >= pw)
            {
                addStep(std::clamp(tail + static_cast<float>((b - pw) / inc), 0.0f, 1.0f), -2.0f * gPulse);
                high = false;
            }
        }
        phase = b;
    }

    /// Adds a band-limited step of height `amount` that happened `fraction` samples ago.
    void addStep(float fraction, float amount) noexcept
    {
        if (amount == 0.0f)
            return;
        const auto& table = BlepTable::get();
        for (int j = 0; j < BlepTable::kTaps; ++j)
            ring[static_cast<size_t>((pos + 1 + j) & kMask)] += amount * table.stepResidual(static_cast<float>(j - kDelay + 1) + fraction);
    }

    /// Adds a band-limited corner: the slope changed by `amount` per sample, `fraction` samples ago.
    void addRamp(float fraction, float amount) noexcept
    {
        if (amount == 0.0f)
            return;
        const auto& table = BlepTable::get();
        for (int j = 0; j < BlepTable::kTaps; ++j)
            ring[static_cast<size_t>((pos + 1 + j) & kMask)] += amount * table.rampResidual(static_cast<float>(j - kDelay + 1) + fraction);
    }

    double phase = 0.0, inc = 0.0;
    float pw = 0.5f, gTri = 0.0f, gSaw = 1.0f, gPulse = 0.0f;
    bool high = true, wrapped = false;
    float wrapTime = 0.0f;
    std::array<float, BlepTable::kTaps> ring {};
    int pos = 0;
};
} // namespace seer::dsp
