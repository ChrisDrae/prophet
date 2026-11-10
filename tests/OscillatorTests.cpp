#include "TestHelpers.h"
#include "engine/Filters.h"
#include "engine/Oscillator.h"

#include <catch2/catch_test_macros.hpp>

using namespace seer::dsp;

namespace
{
constexpr double kRate = 88200.0; // the voices' rate at a 44.1 kHz host
constexpr size_t kSize = 32768;
constexpr size_t kFundamentalBin = 1367; // 3679 Hz: a prime bin, so no alias lands on a harmonic

/// Renders exactly `kFundamentalBin` cycles in kSize samples, so every harmonic sits on one FFT
/// bin and anything on another bin is aliasing.
std::vector<float> renderPeriodic(float shape, float width, bool sync = false, double slaveRatio = 1.0)
{
    Oscillator master, slave;
    master.reset(0.0);
    slave.reset(0.3);
    const double inc = static_cast<double>(kFundamentalBin) / kSize;
    std::vector<float> out;
    const size_t warmUp = 4096;
    for (size_t i = 0; i < warmUp + kSize; ++i)
    {
        float y = master.tick(inc, shape, width);
        if (sync)
            y = slave.tick(inc * slaveRatio, shape, width, master.hasWrapped(), master.wrapFraction());
        if (i >= warmUp)
            out.push_back(y);
    }
    return out;
}

/// The loudest component below 20 kHz that is not a harmonic, relative to the fundamental, in dB.
double aliasingDb(const std::vector<float>& x)
{
    const auto magnitude = test::spectrum(x);
    const auto limit = static_cast<size_t>(20000.0 / kRate * kSize);
    double worst = 0.0;
    for (size_t bin = 1; bin < limit; ++bin)
        if (bin % kFundamentalBin != 0)
            worst = std::max(worst, magnitude[bin]);
    return test::toDb(worst / magnitude[kFundamentalBin]);
}
} // namespace

TEST_CASE("Oscillator: plays in tune", "[osc]")
{
    for (double hz : { 27.5, 440.0, 3520.0 })
    {
        Oscillator osc;
        std::vector<float> out;
        for (int i = 0; i < static_cast<int>(kRate) * 2; ++i)
            out.push_back(osc.tick(hz / kRate, 0.5f, 0.5f));
        CAPTURE(hz);
        CHECK(std::abs(test::cents(test::measureFrequency(out, kRate), hz)) < 0.1);
    }
}

TEST_CASE("Oscillator: the three shapes have the right harmonics", "[osc]")
{
    // Sawtooth: every harmonic, falling as 1/n. Square: odd harmonics only. Triangle: odd, as 1/n^2.
    const auto saw = test::spectrum(renderPeriodic(0.5f, 0.5f));
    CHECK(std::abs(test::toDb(saw[2 * kFundamentalBin] / saw[kFundamentalBin]) + 6.02) < 0.3);
    CHECK(std::abs(test::toDb(saw[3 * kFundamentalBin] / saw[kFundamentalBin]) + 9.54) < 0.3);

    const auto square = test::spectrum(renderPeriodic(1.0f, 0.5f));
    CHECK(test::toDb(square[2 * kFundamentalBin] / square[kFundamentalBin]) < -60.0);
    CHECK(std::abs(test::toDb(square[3 * kFundamentalBin] / square[kFundamentalBin]) + 9.54) < 0.3);

    const auto triangle = test::spectrum(renderPeriodic(0.0f, 0.5f));
    CHECK(test::toDb(triangle[2 * kFundamentalBin] / triangle[kFundamentalBin]) < -60.0);
    CHECK(std::abs(test::toDb(triangle[3 * kFundamentalBin] / triangle[kFundamentalBin]) + 19.08) < 0.4);
}

TEST_CASE("Oscillator: aliasing stays far below the note", "[osc]")
{
    const double saw = aliasingDb(renderPeriodic(0.5f, 0.5f));
    const double pulse = aliasingDb(renderPeriodic(1.0f, 0.3f));
    const double triangle = aliasingDb(renderPeriodic(0.0f, 0.5f));
    const double blend = aliasingDb(renderPeriodic(0.78f, 0.4f));
    const double synced = aliasingDb(renderPeriodic(0.5f, 0.5f, true, 2.37));
    const double syncedPulse = aliasingDb(renderPeriodic(1.0f, 0.4f, true, 1.61));
    INFO("aliasing in dB below the fundamental at 3.7 kHz: saw " << saw << ", pulse " << pulse << ", triangle " << triangle
                                                                   << ", blend " << blend << ", sync " << synced
                                                                   << ", sync pulse " << syncedPulse);
    CHECK(saw < -80.0);
    CHECK(pulse < -80.0);
    CHECK(triangle < -80.0);
    CHECK(blend < -80.0);
    CHECK(synced < -80.0);
    CHECK(syncedPulse < -80.0);
}

TEST_CASE("Oscillator: a synced oscillator repeats at the master's pitch", "[osc]")
{
    Oscillator master, slave;
    std::vector<float> out;
    const double hz = 220.0;
    for (int i = 0; i < static_cast<int>(kRate); ++i)
    {
        master.tick(hz / kRate, 0.5f, 0.5f);
        out.push_back(slave.tick(hz * 1.5 / kRate, 0.5f, 0.5f, master.hasWrapped(), master.wrapFraction()));
    }
    // One and a half cycles of a sawtooth per master cycle: its spectrum has the master's fundamental.
    std::vector<float> window(out.end() - 65536, out.end());
    const auto magnitude = test::spectrum(window);
    size_t loudest = 1;
    for (size_t bin = 1; bin < 2000; ++bin)
        if (magnitude[bin] > magnitude[loudest])
            loudest = bin;
    const double binHz = kRate / 65536.0;
    CHECK(std::abs(static_cast<double>(loudest) * binHz - hz) < 2.0 * binHz);
}

TEST_CASE("Oscillator: output stays within range for any settings", "[osc]")
{
    Oscillator osc;
    Noise random(42);
    double peak = 0.0;
    float shape = 0.5f, width = 0.5f;
    double inc = 0.01;
    for (int i = 0; i < 400000; ++i)
    {
        if (i % 37 == 0)
        {
            shape = random.unit();
            width = random.unit();
            inc = 0.4 * random.unit() * random.unit();
        }
        const float y = osc.tick(inc, shape, width, random.unit() < 0.01f, random.unit());
        REQUIRE(std::isfinite(y));
        peak = std::max(peak, static_cast<double>(std::abs(y)));
    }
    CHECK(peak < 4.0);
}
