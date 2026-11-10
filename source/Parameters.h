#pragma once

#include "engine/EngineParams.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  Every host-visible parameter of Seer, in one place.
//
//  IDs are part of the plugin's public contract: hosts store automation and MIDI mappings by ID,
//  so NEVER rename an ID once released (docs/sound-design.md lists them).
// =================================================================================================
namespace seer::params
{
namespace id
{
// Oscillators
inline constexpr const char* osc1Freq = "osc1Freq";
inline constexpr const char* osc1Shape = "osc1Shape";
inline constexpr const char* osc1Pw = "osc1Pw";
inline constexpr const char* osc1Sync = "osc1Sync";
inline constexpr const char* osc2Freq = "osc2Freq";
inline constexpr const char* osc2Fine = "osc2Fine";
inline constexpr const char* osc2Shape = "osc2Shape";
inline constexpr const char* osc2Pw = "osc2Pw";
inline constexpr const char* osc2Kbd = "osc2Kbd";

// Mixer
inline constexpr const char* mixOsc1 = "mixOsc1";
inline constexpr const char* mixOsc2 = "mixOsc2";
inline constexpr const char* mixSub = "mixSub";
inline constexpr const char* mixNoise = "mixNoise";

// Filters
inline constexpr const char* lpCutoff = "lpCutoff";
inline constexpr const char* lpRes = "lpRes";
inline constexpr const char* lpEnv = "lpEnv";
inline constexpr const char* lpVel = "lpVel";
inline constexpr const char* lpKey = "lpKey";
inline constexpr const char* hpCutoff = "hpCutoff";
inline constexpr const char* hpRes = "hpRes";
inline constexpr const char* hpEnv = "hpEnv";
inline constexpr const char* hpVel = "hpVel";
inline constexpr const char* hpKey = "hpKey";

// Envelopes
inline constexpr const char* fEnvA = "fEnvA";
inline constexpr const char* fEnvD = "fEnvD";
inline constexpr const char* fEnvS = "fEnvS";
inline constexpr const char* fEnvR = "fEnvR";
inline constexpr const char* aEnvA = "aEnvA";
inline constexpr const char* aEnvD = "aEnvD";
inline constexpr const char* aEnvS = "aEnvS";
inline constexpr const char* aEnvR = "aEnvR";
inline constexpr const char* aEnvAmt = "aEnvAmt";
inline constexpr const char* aEnvVel = "aEnvVel";

// LFO
inline constexpr const char* lfoRate = "lfoRate";
inline constexpr const char* lfoAmt = "lfoAmt";
inline constexpr const char* lfoShape = "lfoShape";
inline constexpr const char* lfoFreq1 = "lfoFreq1";
inline constexpr const char* lfoFreq2 = "lfoFreq2";
inline constexpr const char* lfoPw = "lfoPw";
inline constexpr const char* lfoAmp = "lfoAmp";
inline constexpr const char* lfoLp = "lfoLp";
inline constexpr const char* lfoHp = "lfoHp";

// Voice and output
inline constexpr const char* panSpread = "panSpread";
inline constexpr const char* voiceVol = "voiceVol";

// Player settings (presets leave these alone)
inline constexpr const char* masterVol = "masterVol";
inline constexpr const char* masterTune = "masterTune";
inline constexpr const char* bendRange = "bendRange";
} // namespace id

juce::StringArray keyTrackChoices();
juce::StringArray lfoShapeChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/// [RT] Reads the current parameter values into the engine's plain struct. Holds raw atomic
/// pointers, looked up once at construction, so reading costs nothing on the audio thread.
class ParameterReader
{
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

private:
    struct FloatField
    {
        std::atomic<float>* source;
        float EngineParams::* field;
    };
    struct BoolField
    {
        std::atomic<float>* source;
        bool EngineParams::* field;
    };

    std::vector<FloatField> floats;
    std::vector<BoolField> bools;
    std::atomic<float>*lpKey, *hpKey, *lfoShape, *bendRange;
};
} // namespace seer::params
