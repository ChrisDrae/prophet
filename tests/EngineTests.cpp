#include "TestHelpers.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <numbers>

using namespace seer;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Harness
{
    SynthEngine engine;
    EngineParams params;

    Harness()
    {
        engine.prepare(kRate, kBlock);
        // A plain test tone: one sawtooth, filters wide open, organ-style envelope, no velocity.
        params.mixOsc1 = 1.0f;
        params.mixOsc2 = 0.0f;
        params.lpCutoff = 164.0f;
        params.lpRes = 0.0f;
        params.lpEnv = 0.0f;
        params.lpKey = KeyTrack::off;
        params.aEnvA = 0.0f;
        params.aEnvS = 1.0f;
        params.aEnvR = 0.1f;
        params.aEnvVel = false;
        params.panSpread = 0.0f;
        params.slop = 0.0f; // no drift: the tests measure exact pitches
    }

    /// Renders `seconds` and returns the left channel.
    std::vector<float> render(double seconds, std::vector<float>* rightOut = nullptr)
    {
        std::vector<float> left, l(kBlock), r(kBlock);
        const int blocks = static_cast<int>(seconds * kRate / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            engine.setParams(params);
            engine.render(l.data(), r.data(), kBlock);
            left.insert(left.end(), l.begin(), l.end());
            if (rightOut != nullptr)
                rightOut->insert(rightOut->end(), r.begin(), r.end());
        }
        return left;
    }
};

std::vector<float> lastHalf(const std::vector<float>& x)
{
    return { x.begin() + static_cast<long long>(x.size() / 2), x.end() };
}
} // namespace

TEST_CASE("Engine: silent until a key is pressed", "[engine]")
{
    Harness h;
    CHECK(test::peak(h.render(0.2)) == 0.0);
    CHECK(h.engine.getNumActiveVoices() == 0);
}

TEST_CASE("Engine: A4 is 440 Hz, and every octave is in tune", "[engine]")
{
    for (int note : { 33, 45, 57, 69, 81, 93 })
    {
        Harness h;
        h.engine.setParams(h.params);
        h.engine.noteOn(note, 1.0f);
        const auto out = lastHalf(h.render(1.0));
        const double expected = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        CAPTURE(note);
        CHECK(std::abs(test::cents(test::measureFrequency(out, kRate), expected)) < 0.3);
        CHECK(test::peak(out) > 0.05);
        CHECK(test::peak(out) < 0.9);
    }
}

TEST_CASE("Engine: oscillator frequency, fine tune and master tune shift the pitch", "[engine]")
{
    Harness h;
    h.params.osc1Freq = 36.0f; // an octave up
    h.params.masterTune = 25.0f;
    h.engine.setParams(h.params);
    h.engine.noteOn(57, 1.0f);
    CHECK(std::abs(test::cents(test::measureFrequency(lastHalf(h.render(1.0)), kRate), 440.0) - 25.0) < 0.3);

    Harness second;
    second.params.mixOsc1 = 0.0f;
    second.params.mixOsc2 = 1.0f;
    second.params.osc2Fine = -30.0f;
    second.engine.setParams(second.params);
    second.engine.noteOn(69, 1.0f);
    CHECK(std::abs(test::cents(test::measureFrequency(lastHalf(second.render(1.0)), kRate), 440.0) + 30.0) < 0.3);
}

TEST_CASE("Engine: the pitch wheel bends by the set range", "[engine]")
{
    for (int range : { 2, 12 })
    {
        Harness h;
        h.params.bendRange = range;
        h.engine.setParams(h.params);
        h.engine.noteOn(69, 1.0f);
        h.engine.setPitchWheel(1.0f);
        const double up = test::measureFrequency(lastHalf(h.render(1.0)), kRate);
        CHECK(std::abs(test::cents(up, 440.0) - 100.0 * range) < 0.5);
        h.engine.setPitchWheel(-1.0f);
        const double down = test::measureFrequency(lastHalf(h.render(1.0)), kRate);
        CHECK(std::abs(test::cents(down, 440.0) + 100.0 * range) < 0.5);
    }
}

TEST_CASE("Engine: the mod wheel brings in vibrato", "[engine]")
{
    Harness h;
    h.params.lfoFreq1 = true;
    h.params.lfoAmt = 0.0f;
    h.engine.setParams(h.params);
    h.engine.noteOn(69, 1.0f);
    const double still = test::measureFrequency(lastHalf(h.render(0.5)), kRate);
    CHECK(std::abs(test::cents(still, 440.0)) < 0.3);

    // With the wheel all the way up, the pitch swings about a semitone each way (0.3 squared x 12).
    h.engine.setModWheel(1.0f);
    const auto out = h.render(2.0);
    double lowest = 1.0e9, highest = 0.0;
    for (size_t start = 0; start + 480 <= out.size(); start += 480)
    {
        const double hz = test::measureFrequency({ out.begin() + static_cast<long long>(start), out.begin() + static_cast<long long>(start + 480) }, kRate);
        lowest = std::min(lowest, hz);
        highest = std::max(highest, hz);
    }
    CHECK(test::cents(highest, 440.0) > 70.0);
    CHECK(test::cents(highest, 440.0) < 125.0);
    CHECK(test::cents(lowest, 440.0) < -70.0);
    CHECK(test::cents(lowest, 440.0) > -125.0);

    // Halfway up it is a gentle vibrato: about a quarter of a semitone.
    h.engine.setModWheel(0.5f);
    h.render(0.5);
    const auto gentle = h.render(2.0);
    lowest = 1.0e9, highest = 0.0;
    for (size_t start = 0; start + 480 <= gentle.size(); start += 480)
    {
        const double hz = test::measureFrequency({ gentle.begin() + static_cast<long long>(start), gentle.begin() + static_cast<long long>(start + 480) }, kRate);
        lowest = std::min(lowest, hz);
        highest = std::max(highest, hz);
    }
    CHECK(test::cents(highest, 440.0) > 12.0);
    CHECK(test::cents(highest, 440.0) < 40.0);
}

TEST_CASE("Engine: a released note fades and frees its voice", "[engine]")
{
    Harness h;
    h.engine.setParams(h.params);
    h.engine.noteOn(60, 0.8f);
    h.render(0.3);
    CHECK(h.engine.getNumActiveVoices() == 1);
    h.engine.noteOff(60);
    h.render(1.0);
    CHECK(h.engine.getNumActiveVoices() == 0);
    CHECK(test::peak(h.render(0.1)) < 1.0e-6);
}

TEST_CASE("Engine: six voices, the oldest released one is taken first", "[engine]")
{
    Harness h;
    h.params.aEnvR = 0.9f; // long releases
    h.engine.setParams(h.params);
    for (int note = 60; note < 68; ++note)
        h.engine.noteOn(note, 1.0f);
    h.render(0.1);
    CHECK(h.engine.getNumActiveVoices() == 6);

    // Pressing a key that is already sounding reuses its voice.
    h.engine.allNotesOff();
    h.render(0.05);
    for (int i = 0; i < 10; ++i)
    {
        h.engine.noteOn(72, 1.0f);
        h.engine.noteOff(72);
    }
    CHECK(h.engine.getNumActiveVoices() == 6);
}

TEST_CASE("Engine: the sustain pedal holds notes", "[engine]")
{
    Harness h;
    h.engine.setParams(h.params);
    h.engine.setSustainPedal(true);
    h.engine.noteOn(60, 1.0f);
    h.engine.noteOff(60);
    h.render(1.0);
    CHECK(h.engine.getNumActiveVoices() == 1);
    CHECK(test::peak(h.render(0.1)) > 0.05);
    h.engine.setSustainPedal(false);
    h.render(1.0);
    CHECK(h.engine.getNumActiveVoices() == 0);
}

TEST_CASE("Engine: velocity makes notes louder and, through the filter envelope, brighter", "[engine]")
{
    auto level = [](float velocity)
    {
        Harness h;
        h.params.aEnvVel = true;
        h.engine.setParams(h.params);
        h.engine.noteOn(60, velocity);
        return test::rms(lastHalf(h.render(0.5)));
    };
    CHECK(level(1.0f) > 2.0 * level(0.3f));
}

TEST_CASE("Engine: pan spread widens a chord, and at zero both sides are equal", "[engine]")
{
    Harness h;
    h.engine.setParams(h.params);
    for (int note : { 48, 55, 64 })
        h.engine.noteOn(note, 1.0f);
    std::vector<float> right;
    auto left = h.render(0.5, &right);
    double difference = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
        difference = std::max(difference, static_cast<double>(std::abs(left[i] - right[i])));
    CHECK(difference < 1.0e-6);

    h.params.panSpread = 1.0f;
    right.clear();
    left = h.render(0.5, &right);
    difference = 0.0;
    for (size_t i = left.size() / 2; i < left.size(); ++i)
        difference = std::max(difference, static_cast<double>(std::abs(left[i] - right[i])));
    CHECK(difference > 0.05);
}

TEST_CASE("Engine: the output is clean above the note (no aliasing through the whole chain)", "[engine]")
{
    // A high sawtooth at full level: after oversampling, filtering and resampling, whatever is not
    // a harmonic must be far below the note. At full level the limit is set by the filter's input
    // saturation (measured: -71 dB at 2x oversampling); at a quarter of the level, by the
    // oscillator and the resampling filter (-87 dB).
    Harness h;
    h.engine.setParams(h.params);
    constexpr size_t size = 32768;
    const double hz = 2731.0 * kRate / size; // on an FFT bin
    h.params.masterTune = static_cast<float>(test::cents(hz, 440.0 * std::pow(2.0, (96 - 69) / 12.0)));
    h.engine.setParams(h.params);
    h.engine.noteOn(96, 1.0f);
    auto out = h.render(2.0);
    std::vector<float> window(out.end() - size, out.end());
    // A Hann window, since the pitch sits on a bin only up to rounding.
    for (size_t i = 0; i < size; ++i)
        window[i] *= static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / size));
    const auto magnitude = test::spectrum(window);
    double worst = 0.0;
    for (size_t bin = 8; bin < size * 20000 / static_cast<size_t>(kRate); ++bin)
    {
        const size_t nearest = (bin + 1365) / 2731 * 2731;
        if (nearest == 0 || (bin > nearest ? bin - nearest : nearest - bin) > 6)
            worst = std::max(worst, magnitude[bin]);
    }
    const double db = test::toDb(worst / magnitude[2731]);
    INFO("worst non-harmonic component: " << db << " dB");
    CHECK(db < -68.0);

    Harness quiet;
    quiet.params.mixOsc1 = 0.25f;
    quiet.params.masterTune = h.params.masterTune;
    quiet.engine.setParams(quiet.params);
    quiet.engine.noteOn(96, 1.0f);
    out = quiet.render(2.0);
    window.assign(out.end() - size, out.end());
    for (size_t i = 0; i < size; ++i)
        window[i] *= static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / size));
    const auto quietMagnitude = test::spectrum(window);
    worst = 0.0;
    for (size_t bin = 8; bin < size * 20000 / static_cast<size_t>(kRate); ++bin)
    {
        const size_t nearest = (bin + 1365) / 2731 * 2731;
        if (nearest == 0 || (bin > nearest ? bin - nearest : nearest - bin) > 6)
            worst = std::max(worst, quietMagnitude[bin]);
    }
    CHECK(test::toDb(worst / quietMagnitude[2731]) < -80.0);
}

TEST_CASE("Engine: random settings and playing never produce a broken sample", "[engine]")
{
    SynthEngine engine;
    engine.prepare(kRate, kBlock);
    dsp::Noise random(2026);
    std::vector<float> l(kBlock), r(kBlock);
    EngineParams p;
    for (int block = 0; block < 1500; ++block)
    {
        if (block % 6 == 0)
        {
            p.osc1Freq = std::floor(random.unit() * 60.0f);
            p.osc2Freq = std::floor(random.unit() * 60.0f);
            p.osc2Fine = random.next() * 50.0f;
            p.osc1Shape = random.unit(), p.osc2Shape = random.unit();
            p.osc1Pw = random.unit(), p.osc2Pw = random.unit();
            p.osc1Sync = random.unit() < 0.5f, p.osc2Kbd = random.unit() < 0.7f;
            p.mixOsc1 = random.unit(), p.mixOsc2 = random.unit(), p.mixSub = random.unit(), p.mixNoise = random.unit();
            p.lpCutoff = random.unit() * 164.0f, p.lpRes = random.unit(), p.lpEnv = random.next();
            p.hpCutoff = random.unit() * 164.0f, p.hpRes = random.unit(), p.hpEnv = random.next();
            p.lpKey = static_cast<KeyTrack>(static_cast<int>(random.unit() * 2.99f));
            p.hpKey = static_cast<KeyTrack>(static_cast<int>(random.unit() * 2.99f));
            p.fEnvA = random.unit() * 0.5f, p.fEnvD = random.unit() * 0.6f, p.fEnvS = random.unit(), p.fEnvR = random.unit() * 0.6f;
            p.aEnvA = random.unit() * 0.5f, p.aEnvD = random.unit() * 0.6f, p.aEnvS = random.unit(), p.aEnvR = random.unit() * 0.6f;
            p.lfoRate = random.unit(), p.lfoAmt = random.unit();
            p.lfoShape = static_cast<LfoShape>(static_cast<int>(random.unit() * 4.99f));
            p.lfoFreq1 = random.unit() < 0.5f, p.lfoFreq2 = random.unit() < 0.5f, p.lfoPw = random.unit() < 0.5f;
            p.lfoAmp = random.unit() < 0.5f, p.lfoLp = random.unit() < 0.5f, p.lfoHp = random.unit() < 0.5f;
            p.panSpread = random.unit(), p.voiceVol = random.unit();
            p.bendRange = static_cast<int>(random.unit() * 12.99f);
        }
        if (random.unit() < 0.5f)
            engine.noteOn(static_cast<int>(random.unit() * 127.0f), random.unit());
        if (random.unit() < 0.4f)
            engine.noteOff(static_cast<int>(random.unit() * 127.0f));
        if (random.unit() < 0.1f)
            engine.allNotesOff();
        engine.setPitchWheel(random.next());
        engine.setModWheel(random.unit());
        engine.setParams(p);
        engine.render(l.data(), r.data(), kBlock);
        for (int i = 0; i < kBlock; ++i)
        {
            REQUIRE(std::isfinite(l[static_cast<size_t>(i)]));
            REQUIRE(std::isfinite(r[static_cast<size_t>(i)]));
            REQUIRE(std::abs(l[static_cast<size_t>(i)]) <= 1.0f);
            REQUIRE(std::abs(r[static_cast<size_t>(i)]) <= 1.0f);
        }
    }
}

TEST_CASE("Engine: speed", "[.bench]")
{
    SynthEngine engine;
    engine.prepare(kRate, kBlock);
    EngineParams p;
    p.lpCutoff = 100.0f;
    p.lpRes = 0.5f;
    p.aEnvR = 0.9f;
    engine.setParams(p);
    for (int note : { 40, 47, 52, 59, 64, 68 })
        engine.noteOn(note, 1.0f);
    std::vector<float> l(kBlock), r(kBlock);
    const auto start = std::chrono::steady_clock::now();
    const int blocks = static_cast<int>(10.0 * kRate / kBlock);
    for (int b = 0; b < blocks; ++b)
        engine.render(l.data(), r.data(), kBlock);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    WARN("6 voices, 10 s of audio rendered in " << seconds << " s = " << 100.0 * seconds / 10.0 << " % of one core");
    CHECK(seconds < 5.0);
}
