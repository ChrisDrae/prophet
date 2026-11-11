#include "TestHelpers.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <numbers>

using namespace seer;

// Unison, key priority, glide, Poly Mod, aftertouch, slop, quality, distortion and the effects.
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
        // A plain test tone: one sawtooth, filters wide open, organ-style envelope, nothing moving.
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
        params.slop = 0.0f;
    }

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

    double pitchAfter(double seconds) { return test::measureFrequency(tail(render(seconds), 0.1), kRate); }

    static std::vector<float> tail(const std::vector<float>& x, double seconds)
    {
        return { x.end() - static_cast<long long>(seconds * kRate), x.end() };
    }
};

double hzOf(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }
} // namespace

TEST_CASE("Unison: one note at a time, on the chosen number of voices", "[voice]")
{
    Harness h;
    h.params.unison = true;
    h.params.unisonVoices = 4;
    h.params.unisonDetune = 0.0f;
    h.engine.setParams(h.params);
    h.engine.noteOn(60, 1.0f);
    h.render(0.1);
    CHECK(h.engine.getNumActiveVoices() == 4);
    h.engine.noteOn(64, 1.0f);
    h.engine.noteOn(67, 1.0f);
    h.render(0.1);
    CHECK(h.engine.getNumActiveVoices() == 4); // still one stack: the new key took it over

    const double one = test::rms(Harness::tail(h.render(0.3), 0.2));
    Harness single;
    single.engine.setParams(single.params);
    single.engine.noteOn(67, 1.0f);
    const double mono = test::rms(Harness::tail(single.render(0.5), 0.2));
    CHECK(one > 1.5 * mono); // a stack is louder than one voice
}

TEST_CASE("Unison: key priority picks low, high or last, and falls back when a key is released", "[voice]")
{
    auto sounding = [](KeyMode mode)
    {
        Harness h;
        h.params.unison = true;
        h.params.unisonVoices = 1;
        h.params.keyMode = mode;
        h.engine.setParams(h.params);
        h.render(0.05);
        h.engine.noteOn(60, 1.0f);
        h.engine.noteOn(72, 1.0f);
        h.engine.noteOn(65, 1.0f);
        const double held = h.pitchAfter(0.3);
        h.engine.noteOff(mode == KeyMode::low ? 60 : mode == KeyMode::high ? 72 : 65); // release the sounding key
        const double after = h.pitchAfter(0.3);
        return std::pair { held, after };
    };
    auto [lowHeld, lowAfter] = sounding(KeyMode::low);
    CHECK(std::abs(test::cents(lowHeld, hzOf(60))) < 1.0);
    CHECK(std::abs(test::cents(lowAfter, hzOf(65))) < 1.0);
    auto [highHeld, highAfter] = sounding(KeyMode::high);
    CHECK(std::abs(test::cents(highHeld, hzOf(72))) < 1.0);
    CHECK(std::abs(test::cents(highAfter, hzOf(65))) < 1.0);
    auto [lastHeld, lastAfter] = sounding(KeyMode::last);
    CHECK(std::abs(test::cents(lastHeld, hzOf(65))) < 1.0);
    CHECK(std::abs(test::cents(lastAfter, hzOf(72))) < 1.0);
}

TEST_CASE("Unison: legato keeps the envelope running, retrigger modes restart it", "[voice]")
{
    auto dipAfterSecondNote = [](KeyMode mode)
    {
        Harness h;
        h.params.unison = true;
        h.params.unisonVoices = 1;
        h.params.keyMode = mode;
        h.params.aEnvA = 0.45f; // a slow attack (about 60 ms), so a restart is audible as a swell
        h.params.aEnvD = 0.3f;
        h.params.aEnvS = 0.2f;
        h.engine.setParams(h.params);
        h.engine.noteOn(60, 1.0f);
        h.render(1.0); // settled on the sustain
        const double before = test::rms(Harness::tail(h.render(0.1), 0.05));
        h.engine.noteOn(64, 1.0f);
        const double after = test::peak(h.render(0.2));
        return after / before;
    };
    CHECK(dipAfterSecondNote(KeyMode::last) < 2.0);          // legato: stays at the sustain level
    CHECK(dipAfterSecondNote(KeyMode::lastRetrigger) > 3.0); // retrigger: a new attack to full level
}

TEST_CASE("Unison detune spreads the stack around the note", "[voice]")
{
    Harness h;
    h.params.unison = true;
    h.params.unisonVoices = 2;
    h.params.unisonDetune = 1.0f;
    h.engine.setParams(h.params);
    h.engine.noteOn(69, 1.0f);
    // Two voices 70 cents apart beat: the level swells and fades about 18 times a second.
    const auto out = h.render(1.0);
    double low = 1.0e9, high = 0.0;
    for (size_t start = 24000; start + 480 <= out.size(); start += 240)
    {
        const double level = test::rms({ out.begin() + static_cast<long long>(start), out.begin() + static_cast<long long>(start + 480) });
        low = std::min(low, level);
        high = std::max(high, level);
    }
    CHECK(high > 1.6 * low);
}

TEST_CASE("Glide: fixed time takes the set time, whatever the interval", "[voice]")
{
    for (int interval : { 2, 12 })
    {
        Harness h;
        h.params.unison = true;
        h.params.unisonVoices = 1;
        h.params.glideOn = true;
        h.params.glideMode = GlideMode::fixedTime;
        h.params.glideRate = static_cast<float>(std::log(0.2 / 0.002) / std::log(2000.0)); // 200 ms
        h.engine.setParams(h.params);
        h.engine.noteOn(57, 1.0f);
        h.render(0.3);
        h.engine.noteOff(57);
        h.engine.noteOn(57 + interval, 1.0f);
        // Halfway through, the pitch is halfway (in semitones).
        const auto first = h.render(0.11);
        const double midway = test::measureFrequency(Harness::tail(first, 0.02), kRate);
        CAPTURE(interval);
        CHECK(std::abs(test::cents(midway, hzOf(57)) - 50.0 * interval) < 12.0 * interval);
        CHECK(std::abs(test::cents(h.pitchAfter(0.3), hzOf(57 + interval))) < 1.0); // and it arrives
    }
}

TEST_CASE("Glide: fixed rate takes longer for a bigger interval", "[voice]")
{
    auto centsShortAfter = [](int interval)
    {
        Harness h;
        h.params.glideOn = true;
        h.params.glideMode = GlideMode::fixedRate;
        h.params.glideRate = static_cast<float>(std::log(0.4 / 0.002) / std::log(2000.0)); // 400 ms per octave
        h.engine.setParams(h.params);
        h.engine.noteOn(57, 1.0f);
        h.render(0.2);
        h.engine.noteOff(57);
        h.render(0.3);
        h.engine.noteOn(57 + interval, 1.0f);
        // Measured over the 20 ms around 190 ms after the key: 5.7 semitones covered by then.
        return test::cents(test::measureFrequency(Harness::tail(h.render(0.2), 0.02), kRate), hzOf(57 + interval));
    };
    CHECK(std::abs(centsShortAfter(3)) < 1.0);           // 3 semitones: already there
    CHECK(std::abs(centsShortAfter(12) + 630.0) < 40.0); // an octave: still 6.3 semitones to go
}

TEST_CASE("Glide: the legato modes only glide when notes overlap", "[voice]")
{
    auto startsAtTarget = [](bool overlap)
    {
        Harness h;
        h.params.unison = true;
        h.params.unisonVoices = 1;
        h.params.glideOn = true;
        h.params.glideMode = GlideMode::fixedTimeLegato;
        h.params.glideRate = 0.7f; // about 400 ms
        h.engine.setParams(h.params);
        h.engine.noteOn(57, 1.0f);
        h.render(0.3);
        if (!overlap)
            h.engine.noteOff(57);
        h.engine.noteOn(69, 1.0f);
        return std::abs(test::cents(h.pitchAfter(0.12), 440.0)) < 2.0;
    };
    CHECK(startsAtTarget(false));
    CHECK_FALSE(startsAtTarget(true));
}

TEST_CASE("Poly Mod: the filter envelope bends oscillator 1's pitch", "[voice]")
{
    Harness h;
    h.params.pmFreq1 = true;
    h.params.pmEnv = 0.5f; // 0.5 squared x 60 semitones = 15 semitones at the envelope's peak
    h.params.fEnvA = 0.0f;
    h.params.fEnvD = 0.3f;
    h.params.fEnvS = 1.0f; // held at the top
    h.engine.setParams(h.params);
    h.engine.noteOn(57, 1.0f);
    CHECK(std::abs(test::cents(h.pitchAfter(0.4), hzOf(57)) - 1500.0) < 3.0);
}

TEST_CASE("Poly Mod: oscillator 2 at audio rate adds sidebands (FM)", "[voice]")
{
    auto sidebandDb = [](float amount)
    {
        Harness h;
        h.params.osc1Shape = 0.0f; // triangle carrier: few harmonics of its own
        h.params.osc2Shape = 0.0f;
        h.params.osc2Freq = 31.0f; // a fifth above: the modulator
        h.params.osc2Fine = 0.0f;
        h.params.pmFreq1 = true;
        h.params.pmOsc2 = amount;
        h.engine.setParams(h.params);
        h.engine.noteOn(69, 1.0f);
        const auto out = h.render(1.5);
        std::vector<float> window(out.end() - 32768, out.end());
        for (size_t i = 0; i < window.size(); ++i)
            window[i] *= static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / 32768.0));
        const auto m = test::spectrum(window);
        const auto bin = [](double hz) { return static_cast<size_t>(std::lround(hz * 32768.0 / kRate)); };
        // Carrier 440 Hz, modulator 659.3 Hz: the first upper sideband is at their sum.
        double side = 0.0;
        for (size_t b = bin(1099.3) - 2; b <= bin(1099.3) + 2; ++b)
            side = std::max(side, m[b]);
        return test::toDb(side / m[bin(440.0)]);
    };
    CHECK(sidebandDb(0.0f) < -60.0);
    CHECK(sidebandDb(0.5f) > -25.0);
}

TEST_CASE("Poly Mod: oscillator 2 in low-frequency mode is a second LFO", "[voice]")
{
    Harness h;
    h.params.osc2LowFreq = true;
    h.params.osc2Kbd = false;
    h.params.osc2Shape = 0.0f;
    h.params.pmFreq1 = true;
    h.params.pmOsc2 = 0.3f;
    h.engine.setParams(h.params);
    h.engine.noteOn(69, 1.0f);
    const auto out = h.render(4.0);
    double lowest = 1.0e9, highest = 0.0;
    for (size_t start = 4800; start + 960 <= out.size(); start += 960)
    {
        const double hz = test::measureFrequency({ out.begin() + static_cast<long long>(start), out.begin() + static_cast<long long>(start + 960) }, kRate);
        lowest = std::min(lowest, hz);
        highest = std::max(highest, hz);
    }
    // 0.3 squared x 48 semitones = 4.3 semitones each way.
    CHECK(test::cents(highest, 440.0) > 300.0);
    CHECK(test::cents(lowest, 440.0) < -300.0);
}

TEST_CASE("Aftertouch: pressure bends the pitch, opens the filter, brings in the LFO", "[voice]")
{
    Harness h;
    h.params.atAmt = 0.5f;
    h.params.atFreq1 = true;
    h.engine.setParams(h.params);
    h.engine.noteOn(69, 1.0f);
    CHECK(std::abs(test::cents(h.pitchAfter(0.3), 440.0)) < 0.5);
    h.engine.setAftertouch(1.0f);
    CHECK(std::abs(test::cents(h.pitchAfter(0.5), 440.0) - 600.0) < 2.0); // half of 12 semitones
    h.engine.setAftertouch(0.0f);
    CHECK(std::abs(test::cents(h.pitchAfter(0.5), 440.0)) < 2.0);

    Harness filter;
    filter.params.lpCutoff = 42.0f; // about 110 Hz: only the fundamental gets through
    filter.params.atAmt = 1.0f;
    filter.params.atLp = true;
    filter.engine.setParams(filter.params);
    filter.engine.noteOn(45, 1.0f);
    const double dark = test::rms(Harness::tail(filter.render(0.4), 0.2));
    filter.engine.setAftertouch(1.0f);
    const double bright = test::rms(Harness::tail(filter.render(0.5), 0.2));
    CHECK(bright > 1.5 * dark);
}

TEST_CASE("Slop: at zero the pitch is exact, turned up it wanders", "[voice]")
{
    auto spread = [](float slop)
    {
        Harness h;
        h.params.slop = slop;
        h.engine.setParams(h.params);
        h.engine.noteOn(69, 1.0f);
        const auto out = h.render(6.0);
        double lowest = 1.0e9, highest = -1.0e9;
        for (size_t start = 4800; start + 4800 <= out.size(); start += 4800)
        {
            const double c = test::cents(test::measureFrequency({ out.begin() + static_cast<long long>(start), out.begin() + static_cast<long long>(start + 4800) }, kRate), 440.0);
            lowest = std::min(lowest, c);
            highest = std::max(highest, c);
        }
        return std::pair { lowest, highest };
    };
    auto [exactLow, exactHigh] = spread(0.0f);
    CHECK(exactLow > -0.3);
    CHECK(exactHigh < 0.3);
    auto [wideLow, wideHigh] = spread(1.0f);
    CHECK(wideHigh - wideLow > 3.0);
    CHECK(wideLow > -51.0);
    CHECK(wideHigh < 51.0);
}

TEST_CASE("Quality: 4x plays the same note, in tune, and lowers the aliasing", "[voice]")
{
    auto aliasing = [](bool highQuality)
    {
        Harness h;
        constexpr size_t size = 32768;
        const double hz = 2731.0 * kRate / size;
        h.params.highQuality = highQuality;
        h.params.masterTune = static_cast<float>(test::cents(hz, hzOf(96)));
        h.engine.setParams(h.params);
        h.engine.noteOn(96, 1.0f);
        auto out = h.render(2.0);
        std::vector<float> window(out.end() - size, out.end());
        CHECK(std::abs(test::cents(test::measureFrequency(window, kRate), hz)) < 0.5);
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
        return test::toDb(worst / magnitude[2731]);
    };
    const double two = aliasing(false), four = aliasing(true);
    INFO("aliasing of a full-level 4 kHz sawtooth: 2x " << two << " dB, 4x " << four << " dB");
    CHECK(four < two - 6.0);
    CHECK(four < -76.0);
}

TEST_CASE("Voices: the count can be raised to twelve", "[voice]")
{
    Harness h;
    h.params.voices = 12;
    h.engine.setParams(h.params);
    for (int note = 48; note < 64; ++note)
        h.engine.noteOn(note, 1.0f);
    h.render(0.1);
    CHECK(h.engine.getNumActiveVoices() == 12);
    h.params.voices = 6;
    h.render(0.1);
    CHECK(h.engine.getNumActiveVoices() <= 6);
}

TEST_CASE("Distortion and effects: at zero they change nothing at all", "[fx]")
{
    auto take = [](const std::function<void(EngineParams&)>& change)
    {
        Harness h;
        change(h.params);
        h.engine.setParams(h.params);
        h.engine.noteOn(57, 1.0f);
        return h.render(0.5);
    };
    const auto plain = take([](EngineParams&) {});
    const auto off = take([](EngineParams& p) { p.fxOn = false; p.fxAType = EffectType::chorus; p.fxAMix = 1.0f; p.fxBType = EffectType::hall; p.fxBMix = 1.0f; });
    const auto dry = take([](EngineParams& p) { p.fxAType = EffectType::bbdDelay; p.fxAMix = 0.0f; p.fxBType = EffectType::plate; p.fxBMix = 0.0f; });
    CHECK(plain == off);
    CHECK(plain == dry);
    const auto driven = take([](EngineParams& p) { p.dist = 0.8f; });
    CHECK(plain != driven);
}

TEST_CASE("Effects: each type does something, stays bounded and dies away", "[fx]")
{
    for (int type = 1; type <= 9; ++type)
    {
        Harness h;
        h.params.fxBType = static_cast<EffectType>(type);
        h.params.fxBMix = 0.5f;
        h.params.fxBP1 = 0.3f;
        h.params.fxBP2 = 0.5f;
        h.engine.setParams(h.params);
        h.engine.noteOn(57, 1.0f);
        std::vector<float> right;
        const auto left = h.render(0.5, &right);
        CAPTURE(type);
        CHECK(test::peak(left) > 0.05);
        CHECK(test::peak(left) <= 1.0);
        h.engine.noteOff(57);
        h.render(0.3);
        const bool hasTail = type == 1 || type == 2 || type >= 6;
        const double after = test::peak(h.render(0.1));
        if (hasTail)
            CHECK(after > 1.0e-4); // the echo or the reverb is still sounding
        h.render(25.0);
        CHECK(test::peak(h.render(0.2)) < 1.0e-3);
    }
}

TEST_CASE("Effects: a delay repeats the note after the set time", "[fx]")
{
    Harness h;
    h.params.fxAType = EffectType::digitalDelay;
    h.params.fxAMix = 0.5f;
    h.params.fxAP1 = static_cast<float>(std::log(0.25 / 0.02) / std::log(50.0)); // 250 ms
    h.params.fxAP2 = 0.0f;
    h.params.aEnvR = 0.0f;
    h.engine.setParams(h.params);
    h.engine.noteOn(69, 1.0f);
    h.render(0.05);
    h.engine.noteOff(69);
    const auto out = h.render(0.6);
    // Silence, then the echo: find where it starts.
    size_t start = 0;
    for (size_t i = 4800; i < out.size(); ++i)
        if (std::abs(out[i]) > 0.02f)
        {
            start = i;
            break;
        }
    const double seconds = (static_cast<double>(start) + 0.05 * kRate) / kRate;
    CHECK(std::abs(seconds - 0.25) < 0.01);
}

TEST_CASE("Effects: chorus widens a centred sound", "[fx]")
{
    Harness h;
    h.params.fxAType = EffectType::chorus;
    h.params.fxAMix = 0.5f;
    h.engine.setParams(h.params);
    h.engine.noteOn(57, 1.0f);
    std::vector<float> right;
    const auto left = h.render(1.0, &right);
    double difference = 0.0;
    for (size_t i = left.size() / 2; i < left.size(); ++i)
        difference = std::max(difference, static_cast<double>(std::abs(left[i] - right[i])));
    CHECK(difference > 0.02);
}

TEST_CASE("Everything on at random: never a broken sample", "[voice]")
{
    SynthEngine engine;
    engine.prepare(kRate, kBlock);
    dsp::Noise random(77);
    std::vector<float> l(kBlock), r(kBlock);
    EngineParams p;
    auto coin = [&] { return random.unit() < 0.5f; };
    for (int block = 0; block < 3000; ++block)
    {
        if (block % 8 == 0)
        {
            p.osc1Freq = std::floor(random.unit() * 60.0f), p.osc2Freq = std::floor(random.unit() * 60.0f);
            p.osc1Shape = random.unit(), p.osc2Shape = random.unit(), p.osc1Pw = random.unit(), p.osc2Pw = random.unit();
            p.osc1Sync = coin(), p.osc2LowFreq = random.unit() < 0.2f, p.osc2Kbd = coin();
            p.mixOsc1 = random.unit(), p.mixOsc2 = random.unit(), p.mixSub = random.unit(), p.mixNoise = random.unit();
            p.lpCutoff = random.unit() * 164.0f, p.lpRes = random.unit(), p.lpEnv = random.next();
            p.hpCutoff = random.unit() * 120.0f, p.hpRes = random.unit(), p.hpEnv = random.next();
            p.pmEnv = random.next(), p.pmOsc2 = random.next();
            p.pmFreq1 = coin(), p.pmShape1 = coin(), p.pmPw1 = coin(), p.pmLp = coin(), p.pmHp = coin();
            p.atAmt = random.next(), p.atFreq1 = coin(), p.atFreq2 = coin(), p.atLfo = coin(), p.atAmp = coin(), p.atLp = coin(), p.atHp = coin();
            p.lfoRate = random.unit(), p.lfoAmt = random.unit(), p.lfoShape = static_cast<LfoShape>(static_cast<int>(random.unit() * 4.99f));
            p.lfoFreq1 = coin(), p.lfoFreq2 = coin(), p.lfoPw = coin(), p.lfoAmp = coin(), p.lfoLp = coin(), p.lfoHp = coin();
            p.unison = coin(), p.unisonVoices = 1 + static_cast<int>(random.unit() * 5.99f), p.unisonDetune = random.unit();
            p.keyMode = static_cast<KeyMode>(static_cast<int>(random.unit() * 5.99f));
            p.glideOn = coin(), p.glideRate = random.unit(), p.glideMode = static_cast<GlideMode>(static_cast<int>(random.unit() * 3.99f));
            p.slop = random.unit(), p.panSpread = random.unit(), p.dist = random.unit();
            p.fxOn = random.unit() < 0.8f;
            p.fxAType = static_cast<EffectType>(static_cast<int>(random.unit() * 5.99f));
            p.fxBType = static_cast<EffectType>(static_cast<int>(random.unit() * 9.99f));
            p.fxAMix = random.unit(), p.fxAP1 = random.unit(), p.fxAP2 = random.unit();
            p.fxBMix = random.unit(), p.fxBP1 = random.unit(), p.fxBP2 = random.unit();
            p.voices = 6 + 2 * static_cast<int>(random.unit() * 3.99f);
            p.highQuality = random.unit() < 0.3f;
            p.transpose = static_cast<int>(random.unit() * 4.99f) - 2;
        }
        if (coin())
            engine.noteOn(static_cast<int>(random.unit() * 127.0f), random.unit());
        if (random.unit() < 0.4f)
            engine.noteOff(static_cast<int>(random.unit() * 127.0f));
        if (random.unit() < 0.05f)
            engine.allNotesOff();
        if (random.unit() < 0.1f)
            engine.setSustainPedal(coin());
        engine.setPitchWheel(random.next());
        engine.setModWheel(random.unit());
        engine.setAftertouch(random.unit());
        engine.setParams(p);
        engine.render(l.data(), r.data(), kBlock);
        for (size_t i = 0; i < kBlock; ++i)
        {
            REQUIRE(std::isfinite(l[i]));
            REQUIRE(std::isfinite(r[i]));
            REQUIRE(std::abs(l[i]) <= 1.0f);
            REQUIRE(std::abs(r[i]) <= 1.0f);
        }
    }
}
