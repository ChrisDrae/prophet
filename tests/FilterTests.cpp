#include "TestHelpers.h"
#include "engine/VoiceFilters.h"

#include <catch2/catch_test_macros.hpp>

using namespace seer::dsp;

namespace
{
constexpr double kRate = 96000.0;

/// Gain in dB of a filter for a quiet sine at `hz` (quiet, so the saturation stays out of it).
template <typename Process>
double gainDb(double hz, Process&& process)
{
    const float amplitude = 0.001f;
    std::vector<float> in, out;
    const int n = static_cast<int>(kRate);
    for (int i = 0; i < n; ++i)
    {
        const float x = amplitude * static_cast<float>(std::sin(kTwoPi * hz * i / kRate));
        in.push_back(x);
        out.push_back(process(x));
    }
    return test::toDb(test::rms(out, out.size() / 2) / test::rms(in, in.size() / 2));
}
} // namespace

TEST_CASE("Low-pass: 24 dB per octave, cutoff where it should be", "[filter]")
{
    const float g = cutoffToG(1000.0, kRate);
    auto at = [&](double hz)
    {
        LadderFilter filter;
        return gainDb(hz, [&](float x) { return filter.process(x, g, 0.0f); });
    };
    CHECK(std::abs(at(100.0)) < 0.3);          // flat in the passband
    CHECK(std::abs(at(1000.0) + 12.04) < 0.5); // four stages, 3 dB each at the cutoff
    const double slope = at(4000.0) - at(8000.0);
    INFO("slope between 4 and 8 kHz: " << slope << " dB per octave");
    CHECK(slope > 22.0);
    CHECK(slope < 26.0);
}

TEST_CASE("Low-pass: resonance peaks at the cutoff and takes some bass away", "[filter]")
{
    const float g = cutoffToG(1000.0, kRate);
    auto at = [&](double hz, float feedback)
    {
        LadderFilter filter;
        return gainDb(hz, [&](float x) { return filter.process(x, g, feedback); });
    };
    CHECK(at(1000.0, 3.0f) > at(1000.0, 0.0f) + 12.0);
    const double bass = at(100.0, 4.0f);
    INFO("passband level at full resonance: " << bass << " dB");
    CHECK(bass < -3.0);
    CHECK(bass > -9.0);
}

TEST_CASE("Low-pass: self-oscillates in tune and at a steady level", "[filter]")
{
    for (double hz : { 110.0, 440.0, 1760.0, 5000.0 })
    {
        LadderFilter filter;
        const float g = cutoffToG(hz, kRate);
        std::vector<float> out;
        for (int i = 0; i < static_cast<int>(kRate) * 2; ++i)
            out.push_back(filter.process(i == 0 ? 0.01f : 0.0f, g, 4.3f));
        std::vector<float> tail(out.end() - static_cast<int>(kRate), out.end());
        const double measured = test::measureFrequency(tail, kRate);
        const double level = test::peak(tail);
        CAPTURE(hz, measured, level);
        CHECK(std::abs(test::cents(measured, hz)) < 1.0);
        CHECK(level > 0.3);
        CHECK(level < 1.0);
    }
}

TEST_CASE("Low-pass: stays finite when driven hard at any setting", "[filter]")
{
    LadderFilter filter;
    Noise random(7);
    float g = 0.1f, feedback = 0.0f;
    for (int i = 0; i < 300000; ++i)
    {
        if (i % 64 == 0)
        {
            g = cutoffToG(20.0 * std::pow(1000.0, static_cast<double>(random.unit())), kRate);
            feedback = 4.5f * random.unit();
        }
        const float y = filter.process(8.0f * random.next(), g, feedback);
        REQUIRE(std::isfinite(y));
        REQUIRE(std::abs(y) < 100.0f);
    }
}

TEST_CASE("High-pass: 12 dB per octave", "[filter]")
{
    const float g = cutoffToG(1000.0, kRate);
    auto at = [&](double hz, float damping)
    {
        HighPassFilter filter;
        return gainDb(hz, [&](float x) { return filter.process(x, g, damping); });
    };
    CHECK(std::abs(at(10000.0, 1.414f)) < 0.3);
    CHECK(std::abs(at(1000.0, 1.414f) + 3.01) < 0.3);
    const double slope = at(250.0, 1.414f) - at(125.0, 1.414f);
    CHECK(std::abs(slope - 12.0) < 0.5);
    CHECK(at(1000.0, 0.1f) > 15.0); // resonance: a peak at the cutoff
}
