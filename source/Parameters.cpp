#include "Parameters.h"

namespace seer::params
{
namespace
{
using namespace juce;

String percent(float v, int) { return String(roundToInt(v * 100.0f)); }
String signedPercent(float v, int) { return (v > 0.0f ? "+" : "") + String(roundToInt(v * 100.0f)); }

String hertz(double hz)
{
    return hz < 10.0 ? String(hz, 2) + " Hz" : hz < 1000.0 ? String(roundToInt(hz)) + " Hz" : String(hz / 1000.0, 1) + " kHz";
}

String seconds(double s)
{
    return s < 1.0 ? String(roundToInt(s * 1000.0)) + " ms" : String(s, 1) + " s";
}

String shapeName(float v, int)
{
    if (v < 0.04f)
        return "Triangle";
    if (std::abs(v - 0.5f) < 0.04f)
        return "Sawtooth";
    if (v > 0.96f)
        return "Pulse";
    return v < 0.5f ? "Tri > Saw " + String(roundToInt(v * 200.0f)) : "Saw > Pulse " + String(roundToInt(v * 200.0f - 100.0f));
}

/// The list of float and bool parameters, shared by the layout and the reader.
struct FloatSpec
{
    const char* id;
    const char* name;
    float min, max, step, defaultValue;
    String (*text)(float, int);
    float EngineParams::* field;
};

struct BoolSpec
{
    const char* id;
    const char* name;
    bool defaultValue;
    bool EngineParams::* field;
};

const FloatSpec kFloats[] = {
    { id::osc1Freq, "Osc 1 Frequency", 0, 60, 1, 24, [](float v, int) { return String(roundToInt(v) - 24) + " st"; }, &EngineParams::osc1Freq },
    { id::osc1Shape, "Osc 1 Shape", 0, 1, 0, 0.5f, shapeName, &EngineParams::osc1Shape },
    { id::osc1Pw, "Osc 1 Pulse Width", 0, 1, 0, 0.5f, percent, &EngineParams::osc1Pw },
    { id::osc2Freq, "Osc 2 Frequency", 0, 60, 1, 24, [](float v, int) { return String(roundToInt(v) - 24) + " st"; }, &EngineParams::osc2Freq },
    { id::osc2Fine, "Osc 2 Fine", -50, 50, 0, 3, [](float v, int) { return String(v, 1) + " ct"; }, &EngineParams::osc2Fine },
    { id::osc2Shape, "Osc 2 Shape", 0, 1, 0, 0.5f, shapeName, &EngineParams::osc2Shape },
    { id::osc2Pw, "Osc 2 Pulse Width", 0, 1, 0, 0.5f, percent, &EngineParams::osc2Pw },

    { id::mixOsc1, "Mixer Osc 1", 0, 1, 0, 1.0f, percent, &EngineParams::mixOsc1 },
    { id::mixOsc2, "Mixer Osc 2", 0, 1, 0, 0.8f, percent, &EngineParams::mixOsc2 },
    { id::mixSub, "Mixer Sub Octave", 0, 1, 0, 0.0f, percent, &EngineParams::mixSub },
    { id::mixNoise, "Mixer Noise", 0, 1, 0, 0.0f, percent, &EngineParams::mixNoise },

    { id::lpCutoff, "Low-pass Cutoff", 0, 164, 0, 110, [](float v, int) { return hertz(std::min(22000.0, curves::cutoffHz(v))); }, &EngineParams::lpCutoff },
    { id::lpRes, "Low-pass Resonance", 0, 1, 0, 0.1f, percent, &EngineParams::lpRes },
    { id::lpEnv, "Low-pass Env Amount", -1, 1, 0, 0.3f, signedPercent, &EngineParams::lpEnv },
    { id::hpCutoff, "High-pass Cutoff", 0, 164, 0, 0, [](float v, int) { return hertz(std::min(22000.0, curves::cutoffHz(v))); }, &EngineParams::hpCutoff },
    { id::hpRes, "High-pass Resonance", 0, 1, 0, 0.0f, percent, &EngineParams::hpRes },
    { id::hpEnv, "High-pass Env Amount", -1, 1, 0, 0.0f, signedPercent, &EngineParams::hpEnv },

    { id::fEnvA, "Filter Attack", 0, 1, 0, 0.1f, [](float v, int) { return seconds(curves::attackSeconds(v)); }, &EngineParams::fEnvA },
    { id::fEnvD, "Filter Decay", 0, 1, 0, 0.45f, [](float v, int) { return seconds(curves::decaySeconds(v)); }, &EngineParams::fEnvD },
    { id::fEnvS, "Filter Sustain", 0, 1, 0, 0.5f, percent, &EngineParams::fEnvS },
    { id::fEnvR, "Filter Release", 0, 1, 0, 0.35f, [](float v, int) { return seconds(curves::decaySeconds(v)); }, &EngineParams::fEnvR },
    { id::aEnvA, "Amp Attack", 0, 1, 0, 0.12f, [](float v, int) { return seconds(curves::attackSeconds(v)); }, &EngineParams::aEnvA },
    { id::aEnvD, "Amp Decay", 0, 1, 0, 0.4f, [](float v, int) { return seconds(curves::decaySeconds(v)); }, &EngineParams::aEnvD },
    { id::aEnvS, "Amp Sustain", 0, 1, 0, 0.9f, percent, &EngineParams::aEnvS },
    { id::aEnvR, "Amp Release", 0, 1, 0, 0.3f, [](float v, int) { return seconds(curves::decaySeconds(v)); }, &EngineParams::aEnvR },
    { id::aEnvAmt, "Amp Env Amount", 0, 1, 0, 1.0f, percent, &EngineParams::aEnvAmt },

    { id::lfoRate, "LFO Frequency", 0, 1, 0, 0.55f, [](float v, int) { return hertz(curves::lfoHz(v)); }, &EngineParams::lfoRate },
    { id::lfoAmt, "LFO Initial Amount", 0, 1, 0, 0.0f, percent, &EngineParams::lfoAmt },

    { id::pmEnv, "Poly Mod Filter Env", -1, 1, 0, 0.0f, signedPercent, &EngineParams::pmEnv },
    { id::pmOsc2, "Poly Mod Osc 2", -1, 1, 0, 0.0f, signedPercent, &EngineParams::pmOsc2 },
    { id::atAmt, "Aftertouch Amount", -1, 1, 0, 0.0f, signedPercent, &EngineParams::atAmt },
    { id::unisonDetune, "Unison Detune", 0, 1, 0, 0.2f, percent, &EngineParams::unisonDetune },
    { id::glideRate, "Glide Rate", 0, 1, 0, 0.3f, [](float v, int) { return v <= 0.0f ? String("Off") : seconds(curves::glideSeconds(v)); }, &EngineParams::glideRate },
    { id::slop, "Slop", 0, 1, 0, 0.08f, percent, &EngineParams::slop },
    { id::dist, "Distortion", 0, 1, 0, 0.0f, percent, &EngineParams::dist },
    { id::fxAMix, "Effect A Mix", 0, 1, 0, 0.25f, percent, &EngineParams::fxAMix },
    { id::fxAP1, "Effect A Knob 1", 0, 1, 0, 0.4f, percent, &EngineParams::fxAP1 },
    { id::fxAP2, "Effect A Knob 2", 0, 1, 0, 0.4f, percent, &EngineParams::fxAP2 },
    { id::fxBMix, "Effect B Mix", 0, 1, 0, 0.2f, percent, &EngineParams::fxBMix },
    { id::fxBP1, "Effect B Knob 1", 0, 1, 0, 0.5f, percent, &EngineParams::fxBP1 },
    { id::fxBP2, "Effect B Knob 2", 0, 1, 0, 0.4f, percent, &EngineParams::fxBP2 },

    { id::panSpread, "Pan Spread", 0, 1, 0, 0.25f, percent, &EngineParams::panSpread },
    { id::voiceVol, "Voice Volume", 0, 1, 0, 0.4f, percent, &EngineParams::voiceVol },

    { id::masterVol, "Master Volume", -60, 6, 0, 0, [](float v, int) { return v <= -60.0f ? String("-inf dB") : String(v, 1) + " dB"; }, &EngineParams::masterVolDb },
    { id::masterTune, "Master Tune", -50, 50, 0, 0, [](float v, int) { return String(v, 1) + " ct"; }, &EngineParams::masterTune },
};

// The defaults above and below are the init sound, "Butter Lead" in its polyphonic form.
const BoolSpec kBools[] = {
    { id::osc1Sync, "Osc 1 Sync", false, &EngineParams::osc1Sync },
    { id::osc2Kbd, "Osc 2 Keyboard", true, &EngineParams::osc2Kbd },
    { id::lpVel, "Low-pass Velocity", true, &EngineParams::lpVel },
    { id::hpVel, "High-pass Velocity", false, &EngineParams::hpVel },
    { id::aEnvVel, "Amp Velocity", true, &EngineParams::aEnvVel },
    { id::lfoFreq1, "LFO to Freq 1", true, &EngineParams::lfoFreq1 },
    { id::lfoFreq2, "LFO to Freq 2", true, &EngineParams::lfoFreq2 },
    { id::lfoPw, "LFO to PW 1+2", false, &EngineParams::lfoPw },
    { id::lfoAmp, "LFO to Amp", false, &EngineParams::lfoAmp },
    { id::lfoLp, "LFO to Low-pass", false, &EngineParams::lfoLp },
    { id::lfoHp, "LFO to High-pass", false, &EngineParams::lfoHp },
    { id::osc2LowFreq, "Osc 2 Low Freq", false, &EngineParams::osc2LowFreq },
    { id::pmFreq1, "Poly Mod to Freq 1", false, &EngineParams::pmFreq1 },
    { id::pmShape1, "Poly Mod to Shape 1", false, &EngineParams::pmShape1 },
    { id::pmPw1, "Poly Mod to PW 1", false, &EngineParams::pmPw1 },
    { id::pmLp, "Poly Mod to Low-pass", false, &EngineParams::pmLp },
    { id::pmHp, "Poly Mod to High-pass", false, &EngineParams::pmHp },
    { id::atFreq1, "Aftertouch to Freq 1", false, &EngineParams::atFreq1 },
    { id::atFreq2, "Aftertouch to Freq 2", false, &EngineParams::atFreq2 },
    { id::atLfo, "Aftertouch to LFO Amount", false, &EngineParams::atLfo },
    { id::atAmp, "Aftertouch to Amp", false, &EngineParams::atAmp },
    { id::atLp, "Aftertouch to Low-pass", false, &EngineParams::atLp },
    { id::atHp, "Aftertouch to High-pass", false, &EngineParams::atHp },
    { id::unison, "Unison", false, &EngineParams::unison },
    { id::glideOn, "Glide", false, &EngineParams::glideOn },
    { id::fxOn, "Effects On", true, &EngineParams::fxOn },
};

int choiceIndex(const std::atomic<float>* value, int count) noexcept
{
    return std::clamp(roundToInt(value->load(std::memory_order_relaxed)), 0, count - 1);
}
} // namespace

juce::StringArray keyTrackChoices() { return { "Off", "Half", "Full" }; }
juce::StringArray lfoShapeChoices() { return { "Triangle", "Sawtooth", "Reverse Saw", "Square", "Random" }; }
juce::StringArray keyModeChoices() { return { "Low", "High", "Last", "Low Retrig", "High Retrig", "Last Retrig" }; }
juce::StringArray glideModeChoices() { return { "Fixed Rate", "Rate Legato", "Fixed Time", "Time Legato" }; }
juce::StringArray voiceCountChoices() { return { "6", "8", "10", "12" }; }

juce::StringArray effectChoices(bool slotB)
{
    juce::StringArray names { "Off", "BBD Delay", "Digital Delay", "Chorus", "Phaser 1", "Phaser 2" };
    if (slotB)
        names.addArray({ "Hall", "Room", "Plate", "Spring" });
    return names;
}

bool isPlayerSetting(const juce::String& parameterId)
{
    for (const char* player : { id::masterVol, id::masterTune, id::bendRange, id::transpose, id::voices, id::quality })
        if (parameterId == player)
            return true;
    return false;
}

juce::StringArray allParameterIds()
{
    juce::StringArray ids;
    for (const auto& s : kFloats)
        ids.add(s.id);
    for (const auto& s : kBools)
        ids.add(s.id);
    ids.addArray({ id::lpKey, id::hpKey, id::lfoShape, id::bendRange, id::unisonVoices, id::keyMode, id::glideMode, id::fxAType,
                   id::fxBType, id::transpose, id::voices, id::quality });
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& s : kFloats)
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { s.id, 1 }, s.name, NormalisableRange<float>(s.min, s.max, s.step),
                                                         s.defaultValue,
                                                         AudioParameterFloatAttributes().withStringFromValueFunction(s.text)));
    for (const auto& s : kBools)
        layout.add(std::make_unique<AudioParameterBool>(ParameterID { s.id, 1 }, s.name, s.defaultValue));

    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::lpKey, 1 }, "Low-pass Keyboard", keyTrackChoices(), 2));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::hpKey, 1 }, "High-pass Keyboard", keyTrackChoices(), 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::lfoShape, 1 }, "LFO Shape", lfoShapeChoices(), 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { id::bendRange, 1 }, "Pitch Wheel Range", 0, 12, 2,
                                                   AudioParameterIntAttributes().withLabel("st")));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::unisonVoices, 1 }, "Unison Voices", StringArray { "1", "2", "3", "4", "5", "6" }, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::keyMode, 1 }, "Key Mode", keyModeChoices(), 2));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::glideMode, 1 }, "Glide Mode", glideModeChoices(), 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::fxAType, 1 }, "Effect A Type", effectChoices(false), 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::fxBType, 1 }, "Effect B Type", effectChoices(true), 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { id::transpose, 1 }, "Transpose", -2, 2, 0,
                                                   AudioParameterIntAttributes().withLabel("oct")));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::voices, 1 }, "Voices", voiceCountChoices(), 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::quality, 1 }, "Quality", StringArray { "2x", "4x" }, 0));
    return layout;
}

ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState& state)
{
    auto get = [&](const char* parameterId)
    {
        auto* value = state.getRawParameterValue(parameterId);
        jassert(value != nullptr); // every ID must exist in createLayout()
        return value;
    };
    for (const auto& s : kFloats)
        floats.push_back({ get(s.id), s.field });
    for (const auto& s : kBools)
        bools.push_back({ get(s.id), s.field });
    lpKey = get(id::lpKey);
    hpKey = get(id::hpKey);
    lfoShape = get(id::lfoShape);
    bendRange = get(id::bendRange);
    unisonVoices = get(id::unisonVoices);
    keyMode = get(id::keyMode);
    glideMode = get(id::glideMode);
    fxAType = get(id::fxAType);
    fxBType = get(id::fxBType);
    transpose = get(id::transpose);
    voices = get(id::voices);
    quality = get(id::quality);
}

EngineParams ParameterReader::read() const noexcept
{
    EngineParams p;
    for (const auto& f : floats)
        p.*f.field = f.source->load(std::memory_order_relaxed);
    for (const auto& b : bools)
        p.*b.field = b.source->load(std::memory_order_relaxed) > 0.5f;
    p.lpKey = static_cast<KeyTrack>(choiceIndex(lpKey, 3));
    p.hpKey = static_cast<KeyTrack>(choiceIndex(hpKey, 3));
    p.lfoShape = static_cast<LfoShape>(choiceIndex(lfoShape, 5));
    p.bendRange = choiceIndex(bendRange, 13);
    p.unisonVoices = choiceIndex(unisonVoices, 6) + 1;
    p.keyMode = static_cast<KeyMode>(choiceIndex(keyMode, 6));
    p.glideMode = static_cast<GlideMode>(choiceIndex(glideMode, 4));
    p.fxAType = static_cast<EffectType>(choiceIndex(fxAType, 6));
    p.fxBType = static_cast<EffectType>(choiceIndex(fxBType, 10));
    p.transpose = std::clamp(roundToInt(transpose->load(std::memory_order_relaxed)), -2, 2);
    p.voices = 6 + 2 * choiceIndex(voices, 4);
    p.highQuality = choiceIndex(quality, 2) == 1;
    return p;
}
} // namespace seer::params
