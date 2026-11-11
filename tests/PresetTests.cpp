#include "Parameters.h"
#include "Presets.h"
#include "engine/SynthEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace seer;

namespace
{
/// A processor with nothing but the parameters: enough to load presets into.
struct Host final : juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState state { *this, nullptr, "Seer", params::createLayout() };

    const juce::String getName() const override { return "Host"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
} // namespace

TEST_CASE("Parameters: every ID is unique and the reader sees the defaults", "[preset]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Host host;
    const auto ids = params::allParameterIds();
    std::set<juce::String> unique(ids.begin(), ids.end());
    CHECK(static_cast<int>(unique.size()) == ids.size());
    CHECK(ids.size() == host.getParameters().size());

    params::ParameterReader reader(host.state);
    const EngineParams p = reader.read();
    const EngineParams defaults;
    CHECK(p.lpCutoff == defaults.lpCutoff);
    CHECK(p.osc1Freq == defaults.osc1Freq);
    CHECK(p.lpKey == defaults.lpKey);
    CHECK(p.unisonVoices == 1);
    CHECK(p.voices == 6);
    CHECK_FALSE(p.highQuality);
    CHECK(p.fxAType == EffectType::off);
}

TEST_CASE("Presets: the factory list is what the docs promise", "[preset]")
{
    const auto& list = factoryPresets();
    int leads = 0, basses = 0;
    std::set<juce::String> names;
    for (const auto& preset : list)
    {
        names.insert(preset.name);
        leads += preset.category == "Lead" ? 1 : 0;
        basses += preset.category == "Bass" ? 1 : 0;
    }
    CHECK(names.size() == list.size()); // no two share a name
    CHECK(leads == 11);
    CHECK(basses == 12);
    CHECK(list.size() >= 32);
    CHECK(list.front().name == "Butter Lead");
    CHECK(list.back().name == "Init");
    CHECK(list.back().values.empty());
}

TEST_CASE("Presets: every value names a real parameter and lies inside its range", "[preset]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Host host;
    for (const auto& preset : factoryPresets())
        for (const auto& [id, value] : preset.values)
        {
            CAPTURE(preset.name, id, value);
            auto* parameter = host.state.getParameter(id);
            REQUIRE(parameter != nullptr);
            CHECK_FALSE(params::isPlayerSetting(id));
            const auto& range = parameter->getNormalisableRange();
            CHECK(value >= range.start);
            CHECK(value <= range.end);
        }
}

TEST_CASE("Presets: loading sets what a preset lists, resets the rest, leaves the player's settings", "[preset]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Host host;
    PresetManager presets(host.state);
    auto plain = [&](const char* id) { return host.state.getRawParameterValue(id)->load(); };
    auto set = [&](const char* id, float v)
    {
        auto* parameter = host.state.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
    };

    set(params::id::masterVol, -12.0f);
    set(params::id::bendRange, 7.0f);
    set(params::id::mixNoise, 0.9f);

    presets.load(0, false); // Butter Lead
    CHECK(presets.getCurrentName() == "Butter Lead");
    CHECK(plain(params::id::unison) == 1.0f);
    CHECK(plain(params::id::glideOn) == 1.0f);
    CHECK(std::abs(plain(params::id::lpCutoff) - 86.0f) < 0.01f);
    CHECK(plain(params::id::mixNoise) == 0.0f);                    // not listed: back to its default
    CHECK(std::abs(plain(params::id::masterVol) + 12.0f) < 0.01f); // the player's: untouched
    CHECK(plain(params::id::bendRange) == 7.0f);

    presets.load(presets.getNumFactoryPresets() - 1, false); // Init
    CHECK(plain(params::id::unison) == 0.0f);
    CHECK(std::abs(plain(params::id::lpCutoff) - 110.0f) < 0.01f);
}

TEST_CASE("Presets: every factory sound plays, at a sensible level, and stays in range", "[preset]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Host host;
    PresetManager presets(host.state);
    params::ParameterReader reader(host.state);
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        presets.load(i, false);
        SynthEngine engine;
        engine.prepare(48000.0, 256);
        const bool bass = presets.getPreset(i).category == "Bass";
        std::vector<float> l(256), r(256);
        double peak = 0.0, sum = 0.0;
        int count = 0;
        for (int block = 0; block < 190; ++block)
        {
            engine.setParams(reader.read());
            if (block == 0)
                for (int note : { bass ? 38 : 62, bass ? 38 : 65, bass ? 38 : 69 })
                    engine.noteOn(note, 0.8f);
            engine.render(l.data(), r.data(), 256);
            for (size_t s = 0; s < 256; ++s)
            {
                REQUIRE(std::isfinite(l[s]));
                peak = std::max(peak, static_cast<double>(std::max(std::abs(l[s]), std::abs(r[s]))));
                sum += 0.5 * (static_cast<double>(l[s]) * l[s] + static_cast<double>(r[s]) * r[s]);
                ++count;
            }
        }
        const double rmsDb = 10.0 * std::log10(std::max(1.0e-12, sum / count));
        CAPTURE(presets.getPreset(i).name, peak, rmsDb);
        CHECK(peak < 0.98);    // no preset leans on the output's safety clipper
        CHECK(rmsDb > -40.0);  // every preset makes a sound
        CHECK(rmsDb < -8.0);
    }
}
