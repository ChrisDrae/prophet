#include "Presets.h"

#include "Parameters.h"

namespace seer
{
namespace
{
constexpr double kGlideSeconds = 0.25;
const juce::Identifier presetProperty { "preset" };

// The tables below are written with whole numbers where a value is whole (a switch: 1, a choice: its
// index); they become floats in the map, which MSVC would warn about on every line.
JUCE_BEGIN_IGNORE_WARNINGS_MSVC(4244)

using Values = std::map<juce::String, float>;

Preset make(const char* name, const char* category, Values values)
{
    return { name, PresetManager::kFactoryBank, category, std::move(values), {} };
}

/// Ease in and out (a smoothstep-like quadratic), so the knobs start and land gently.
float ease(float u)
{
    return u < 0.5f ? 2.0f * u * u : 1.0f - std::pow(-2.0f * u + 2.0f, 2.0f) * 0.5f;
}

// Choice indices, named.
constexpr float kOff = 0, kHalf = 1, kFull = 2;                                   // keyboard tracking
constexpr float kLast = 2, kLowRetrig = 3, kLastRetrig = 5;                       // key mode
constexpr float kFixedRate = 0, kRateLegato = 1, kTimeLegato = 3;                                 // glide mode
constexpr float kBbd = 1, kDdl = 2, kChorus = 3, kPhaser1 = 4, kPhaser2 = 5;      // effect types
constexpr float kHall = 6, kRoom = 7, kPlate = 8, kSpring = 9;
constexpr float kSquare = 3;                                                      // LFO shape

/// What every mono lead shares: one voice, legato, a glide that only happens when notes overlap,
/// vibrato on the mod wheel, and pressure that opens the filter and adds vibrato.
Values lead(Values more)
{
    using namespace params;
    Values v = { { id::unison, 1 },     { id::unisonVoices, 0 }, { id::keyMode, kLast },  { id::glideOn, 1 },
                 { id::glideMode, kTimeLegato }, { id::glideRate, 0.45f }, { id::lfoRate, 0.555f }, { id::lfoAmt, 0 },
                 { id::atAmt, 0.25f },  { id::atLfo, 1 },        { id::atLp, 1 },         { id::panSpread, 0 },
                 { id::slop, 0.1f },    { id::aEnvA, 0.22f },    { id::aEnvR, 0.38f } };
    for (auto& [key, value] : more)
        v[key] = value;
    return v;
}

/// What every mono bass shares: an octave down, one voice (unless stacked), instant attack.
Values bass(Values more)
{
    using namespace params;
    Values v = { { id::unison, 1 },    { id::unisonVoices, 0 }, { id::keyMode, kLastRetrig }, { id::osc1Freq, 12 },
                 { id::osc2Freq, 12 }, { id::lpKey, kHalf },    { id::aEnvA, 0 },             { id::aEnvS, 1 },
                 { id::aEnvR, 0.22f }, { id::fEnvA, 0 },        { id::panSpread, 0 },         { id::slop, 0.06f } };
    for (auto& [key, value] : more)
        v[key] = value;
    return v;
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    using namespace params;
    static const std::vector<Preset> list = {
        // ---- leads --------------------------------------------------------------------------------
        make("Butter Lead", "Lead",
             lead({ { id::osc1Shape, 0.42f }, { id::osc2Shape, 0.35f }, { id::osc2Fine, 4 }, { id::mixOsc2, 0.7f }, { id::mixSub, 0.25f },
                    { id::lpCutoff, 86 }, { id::lpRes, 0.12f }, { id::lpEnv, 0.22f }, { id::fEnvA, 0.25f }, { id::fEnvD, 0.55f },
                    { id::fEnvS, 0.45f }, { id::fEnvR, 0.4f }, { id::aEnvD, 0.5f }, { id::fxAType, kChorus }, { id::fxAMix, 0.2f },
                    { id::fxAP1, 0.3f }, { id::fxAP2, 0.35f }, { id::fxBType, kPlate }, { id::fxBMix, 0.2f }, { id::fxBP1, 0.45f },
                    { id::fxBP2, 0.3f }, { id::voiceVol, 1.00f } })),
        make("Silk Thread", "Lead",
             lead({ { id::osc1Shape, 0.06f }, { id::osc2Shape, 0.12f }, { id::osc2Freq, 36 }, { id::osc2Fine, 2 }, { id::mixOsc2, 0.28f },
                    { id::lpCutoff, 98 }, { id::lpRes, 0.05f }, { id::lpEnv, 0.12f }, { id::fEnvA, 0.3f }, { id::glideRate, 0.5f },
                    { id::aEnvA, 0.3f }, { id::aEnvR, 0.45f }, { id::fxBType, kHall }, { id::fxBMix, 0.28f }, { id::fxBP1, 0.5f },
                    { id::voiceVol, 0.76f } })),
        make("Hollow Reed", "Lead",
             lead({ { id::osc1Shape, 1.0f }, { id::osc2Shape, 1.0f }, { id::osc1Pw, 0.5f }, { id::osc2Pw, 0.4f }, { id::osc2Fine, 5 },
                    { id::mixOsc2, 0.6f }, { id::lpCutoff, 88 }, { id::lpRes, 0.2f }, { id::lpEnv, 0.3f }, { id::fEnvA, 0.2f },
                    { id::fEnvD, 0.5f }, { id::fEnvS, 0.4f }, { id::fxAType, kBbd }, { id::fxAMix, 0.22f }, { id::fxAP1, 0.68f },
                    { id::fxAP2, 0.35f }, { id::fxBType, kRoom }, { id::fxBMix, 0.15f }, { id::voiceVol, 0.47f } })),
        make("Sync Singer", "Lead",
             lead({ { id::osc1Sync, 1 }, { id::osc1Freq, 31 }, { id::mixOsc2, 0 }, { id::pmFreq1, 1 }, { id::pmEnv, 0.5f },
                    { id::lpCutoff, 112 }, { id::lpRes, 0.08f }, { id::lpEnv, 0.1f }, { id::fEnvA, 0.15f }, { id::fEnvD, 0.62f },
                    { id::fEnvS, 0.35f }, { id::fEnvR, 0.4f }, { id::atLp, 0 }, { id::fxAType, kDdl }, { id::fxAMix, 0.2f },
                    { id::fxAP1, 0.7f }, { id::fxAP2, 0.3f }, { id::fxBType, kPlate }, { id::fxBMix, 0.15f }, { id::voiceVol, 0.79f } })),
        make("Velvet Fifths", "Lead",
             lead({ { id::osc2Freq, 31 }, { id::osc2Fine, 0 }, { id::osc1Shape, 0.45f }, { id::osc2Shape, 0.4f }, { id::mixOsc2, 0.55f },
                    { id::lpCutoff, 84 }, { id::lpRes, 0.1f }, { id::lpEnv, 0.25f }, { id::fEnvA, 0.3f }, { id::fEnvD, 0.6f },
                    { id::fxAType, kChorus }, { id::fxAMix, 0.3f }, { id::fxBType, kRoom }, { id::fxBMix, 0.2f }, { id::fxBP1, 0.6f },
                    { id::voiceVol, 1.00f } })),
        make("Night Flute", "Lead",
             lead({ { id::osc1Shape, 0.0f }, { id::mixOsc2, 0 }, { id::mixNoise, 0.07f }, { id::lpCutoff, 94 }, { id::lpRes, 0.0f },
                    { id::lpEnv, 0.15f }, { id::hpCutoff, 58 }, { id::hpKey, kHalf }, { id::fEnvA, 0.35f }, { id::aEnvA, 0.42f },
                    { id::aEnvR, 0.45f }, { id::atAmt, 0.35f }, { id::glideRate, 0.4f }, { id::fxBType, kHall }, { id::fxBMix, 0.3f },
                    { id::fxBP1, 0.55f }, { id::voiceVol, 0.81f } })),
        make("Glass Whistle", "Lead",
             lead({ { id::mixOsc1, 0 }, { id::mixOsc2, 0 }, { id::lpRes, 1.0f }, { id::lpCutoff, 80.51f }, { id::lpEnv, 0 },
                    { id::lpVel, 0 }, { id::lpKey, kFull }, { id::lfoFreq1, 0 }, { id::lfoFreq2, 0 }, { id::atLfo, 0 },
                    { id::atAmt, 0.05f }, { id::aEnvA, 0.35f }, { id::aEnvR, 0.45f }, { id::glideRate, 0.5f }, { id::fxAType, kBbd },
                    { id::fxAMix, 0.25f }, { id::fxAP1, 0.72f }, { id::fxAP2, 0.4f }, { id::fxBType, kHall }, { id::fxBMix, 0.3f },
                    { id::voiceVol, 0.78f } })),
        make("Wheel Cry", "Lead",
             lead({ { id::osc2Fine, 7 }, { id::lpCutoff, 80 }, { id::lpRes, 0.38f }, { id::lpEnv, 0.42f }, { id::fEnvA, 0.3f },
                    { id::fEnvD, 0.55f }, { id::fEnvS, 0.5f }, { id::dist, 0.35f }, { id::aEnvA, 0.15f }, { id::atAmt, 0.35f },
                    { id::fxAType, kDdl }, { id::fxAMix, 0.25f }, { id::fxAP1, 0.74f }, { id::fxAP2, 0.35f }, { id::fxBType, kSpring },
                    { id::fxBMix, 0.15f }, { id::voiceVol, 0.80f } })),

        // Three in the manner of the classic American monosynths: hot mixer, a fat low end, glide.
        make("Fusion Mono", "Lead",
             lead({ { id::osc2Freq, 12 }, { id::osc2Fine, 5 }, { id::mixOsc1, 1.0f }, { id::mixOsc2, 0.85f }, { id::mixSub, 0.4f },
                    { id::lpCutoff, 84 }, { id::lpRes, 0.22f }, { id::lpEnv, 0.35f }, { id::lpKey, kHalf }, { id::fEnvA, 0.05f },
                    { id::fEnvD, 0.5f }, { id::fEnvS, 0.55f }, { id::fEnvR, 0.35f }, { id::aEnvA, 0.08f }, { id::aEnvR, 0.3f },
                    { id::glideRate, 0.42f }, { id::dist, 0.15f }, { id::fxBType, kSpring }, { id::fxBMix, 0.12f }, { id::voiceVol, 0.7f } })),
        make("Lucky Glide", "Lead",
             lead({ { id::osc1Shape, 1.0f }, { id::osc2Shape, 1.0f }, { id::osc2Fine, 6 }, { id::mixOsc2, 0.8f }, { id::lpCutoff, 92 },
                    { id::lpRes, 0.15f }, { id::lpEnv, 0.2f }, { id::fEnvA, 0.2f }, { id::fEnvD, 0.55f }, { id::fEnvS, 0.6f },
                    { id::glideMode, kFixedRate }, { id::glideRate, 0.52f }, { id::aEnvA, 0.15f }, { id::aEnvR, 0.42f },
                    { id::fxAType, kDdl }, { id::fxAMix, 0.22f }, { id::fxAP1, 0.72f }, { id::fxAP2, 0.4f }, { id::voiceVol, 0.6f } })),
        make("Sun Brass", "Lead",
             lead({ { id::osc2Fine, 4 }, { id::mixOsc2, 0.9f }, { id::lpCutoff, 70 }, { id::lpRes, 0.1f }, { id::lpEnv, 0.55f },
                    { id::lpKey, kHalf }, { id::fEnvA, 0.4f }, { id::fEnvD, 0.55f }, { id::fEnvS, 0.6f }, { id::fEnvR, 0.38f },
                    { id::aEnvA, 0.2f }, { id::aEnvR, 0.35f }, { id::glideRate, 0.35f }, { id::fxBType, kRoom }, { id::fxBMix, 0.18f },
                    { id::voiceVol, 0.8f } })),

        // ---- basses -------------------------------------------------------------------------------
        make("Thick Six", "Bass",
             bass({ { id::unisonVoices, 5 }, { id::unisonDetune, 0.4f }, { id::mixSub, 0.5f }, { id::lpCutoff, 80 }, { id::lpRes, 0.15f },
                    { id::lpEnv, 0.4f }, { id::fEnvD, 0.45f }, { id::fEnvS, 0.2f }, { id::panSpread, 0.6f }, { id::dist, 0.2f },
                    { id::voiceVol, 0.32f } })),
        make("Sub Floor", "Bass",
             bass({ { id::osc1Shape, 0.0f }, { id::mixOsc2, 0 }, { id::mixSub, 1.0f }, { id::lpCutoff, 66 }, { id::lpRes, 0.0f },
                    { id::lpEnv, 0.15f }, { id::fEnvD, 0.4f }, { id::fEnvS, 0.3f }, { id::keyMode, kLowRetrig }, { id::voiceVol, 0.57f } })),
        make("Round Wool", "Bass",
             bass({ { id::osc1Shape, 0.2f }, { id::osc2Shape, 1.0f }, { id::mixOsc2, 0.35f }, { id::mixSub, 0.7f }, { id::lpCutoff, 70 },
                    { id::lpRes, 0.05f }, { id::lpEnv, 0.25f }, { id::fEnvD, 0.42f }, { id::fEnvS, 0.3f }, { id::aEnvD, 0.6f },
                    { id::aEnvS, 0.8f }, { id::voiceVol, 0.95f } })),
        make("Rubber", "Bass",
             bass({ { id::mixOsc2, 0.6f }, { id::lpCutoff, 62 }, { id::lpRes, 0.45f }, { id::lpEnv, 0.55f }, { id::fEnvD, 0.36f },
                    { id::fEnvS, 0.0f }, { id::keyMode, kLast }, { id::glideOn, 1 }, { id::glideMode, kRateLegato }, { id::glideRate, 0.38f },
                    { id::voiceVol, 0.96f } })),
        make("Reso Growl", "Bass",
             bass({ { id::osc1Shape, 1.0f }, { id::osc1Pw, 0.38f }, { id::osc2Fine, 9 }, { id::lpCutoff, 58 }, { id::lpRes, 0.7f },
                    { id::lpEnv, 0.5f }, { id::fEnvD, 0.55f }, { id::fEnvS, 0.15f }, { id::dist, 0.45f }, { id::voiceVol, 0.86f } })),
        make("Pluck Low", "Bass",
             { { id::osc1Freq, 12 }, { id::osc2Freq, 12 }, { id::mixSub, 0.3f }, { id::lpCutoff, 72 }, { id::lpRes, 0.2f }, { id::lpEnv, 0.5f },
               { id::lpKey, kHalf }, { id::fEnvA, 0 }, { id::fEnvD, 0.32f }, { id::fEnvS, 0 }, { id::fEnvR, 0.3f }, { id::aEnvA, 0 },
               { id::aEnvD, 0.48f }, { id::aEnvS, 0 }, { id::aEnvR, 0.4f }, { id::voiceVol, 1.00f } }),
        make("Fretless", "Bass",
             bass({ { id::osc1Shape, 0.15f }, { id::osc2Shape, 0.3f }, { id::mixOsc2, 0.5f }, { id::lpCutoff, 78 }, { id::lpRes, 0.1f },
                    { id::lpEnv, 0.2f }, { id::fEnvD, 0.5f }, { id::fEnvS, 0.4f }, { id::keyMode, kLast }, { id::glideOn, 1 },
                    { id::glideMode, kRateLegato }, { id::glideRate, 0.5f }, { id::aEnvA, 0.12f }, { id::atAmt, 0.3f }, { id::atLfo, 1 },
                    { id::lfoRate, 0.54f }, { id::fxAType, kChorus }, { id::fxAMix, 0.3f }, { id::fxAP1, 0.25f }, { id::voiceVol, 1.00f } })),
        make("Wide Detune", "Bass",
             bass({ { id::unisonVoices, 1 }, { id::unisonDetune, 0.7f }, { id::panSpread, 1.0f }, { id::osc1Shape, 1.0f }, { id::osc2Shape, 1.0f },
                    { id::osc1Pw, 0.4f }, { id::osc2Pw, 0.6f }, { id::lpCutoff, 84 }, { id::lpRes, 0.1f }, { id::lpEnv, 0.3f },
                    { id::fEnvD, 0.5f }, { id::fEnvS, 0.3f }, { id::fxAType, kChorus }, { id::fxAMix, 0.3f }, { id::voiceVol, 0.44f } })),

        // Four more in the manner of the classic American monosynths and bass pedals.
        make("Pedal Thunder", "Bass",
             bass({ { id::osc2Fine, 10 }, { id::mixOsc2, 1.0f }, { id::mixSub, 0.6f }, { id::lpCutoff, 60 }, { id::lpRes, 0.1f },
                    { id::lpEnv, 0.2f }, { id::lpKey, kOff }, { id::fEnvD, 0.6f }, { id::fEnvS, 0.6f }, { id::aEnvR, 0.38f },
                    { id::dist, 0.1f }, { id::voiceVol, 0.7f } })),
        make("Mono Punch", "Bass",
             bass({ { id::osc2Shape, 1.0f }, { id::osc2Pw, 0.5f }, { id::mixOsc2, 0.8f }, { id::mixSub, 0.5f }, { id::lpCutoff, 56 },
                    { id::lpRes, 0.3f }, { id::lpEnv, 0.65f }, { id::fEnvD, 0.34f }, { id::fEnvS, 0.1f }, { id::aEnvD, 0.55f },
                    { id::aEnvS, 0.7f }, { id::voiceVol, 0.8f } })),
        make("Funk Squelch", "Bass",
             bass({ { id::osc1Shape, 1.0f }, { id::mixOsc2, 0.0f }, { id::mixSub, 0.35f }, { id::lpCutoff, 50 }, { id::lpRes, 0.62f },
                    { id::lpEnv, 0.7f }, { id::fEnvD, 0.3f }, { id::fEnvS, 0.0f }, { id::aEnvD, 0.5f }, { id::aEnvS, 0.6f },
                    { id::aEnvR, 0.15f }, { id::voiceVol, 0.9f } })),
        make("Low Rider", "Bass",
             bass({ { id::mixOsc2, 0.7f }, { id::osc2Fine, 7 }, { id::lpCutoff, 66 }, { id::lpRes, 0.35f }, { id::lpEnv, 0.4f },
                    { id::fEnvD, 0.5f }, { id::fEnvS, 0.3f }, { id::keyMode, kLast }, { id::glideOn, 1 }, { id::glideMode, kFixedRate },
                    { id::glideRate, 0.45f }, { id::dist, 0.3f }, { id::voiceVol, 0.7f } })),

        // ---- pads and keys ------------------------------------------------------------------------
        make("Glass Pad", "Pad",
             { { id::osc1Shape, 0.9f }, { id::osc2Shape, 0.85f }, { id::osc1Pw, 0.35f }, { id::lfoPw, 1 }, { id::lfoFreq1, 0 },
               { id::lfoFreq2, 0 }, { id::lfoAmt, 0.35f }, { id::lfoRate, 0.42f }, { id::lpCutoff, 96 }, { id::lpEnv, 0.2f },
               { id::fEnvA, 0.55f }, { id::fEnvR, 0.55f }, { id::aEnvA, 0.5f }, { id::aEnvR, 0.58f }, { id::panSpread, 0.7f },
               { id::slop, 0.2f }, { id::fxBType, kHall }, { id::fxBMix, 0.35f }, { id::fxBP1, 0.6f }, { id::voiceVol, 0.31f } }),
        make("Slow Strings", "Pad",
             { { id::osc2Fine, 9 }, { id::lpCutoff, 92 }, { id::lpRes, 0.05f }, { id::lpEnv, 0.15f }, { id::hpCutoff, 52 },
               { id::fEnvA, 0.6f }, { id::fEnvR, 0.6f }, { id::aEnvA, 0.58f }, { id::aEnvS, 1 }, { id::aEnvR, 0.62f },
               { id::panSpread, 0.8f }, { id::slop, 0.18f }, { id::fxAType, kChorus }, { id::fxAMix, 0.45f }, { id::fxAP1, 0.35f },
               { id::fxAP2, 0.5f }, { id::fxBType, kHall }, { id::fxBMix, 0.3f }, { id::voiceVol, 0.55f } }),
        make("Sweep Phase", "Pad",
             { { id::osc2Fine, 6 }, { id::lpCutoff, 84 }, { id::lpRes, 0.4f }, { id::lpEnv, 0.1f }, { id::lfoLp, 1 }, { id::lfoFreq1, 0 },
               { id::lfoFreq2, 0 }, { id::lfoAmt, 0.25f }, { id::lfoRate, 0.22f }, { id::aEnvA, 0.5f }, { id::aEnvR, 0.6f },
               { id::panSpread, 0.6f }, { id::fxAType, kPhaser1 }, { id::fxAMix, 0.5f }, { id::fxAP1, 0.2f }, { id::fxAP2, 0.6f },
               { id::fxBType, kPlate }, { id::fxBMix, 0.25f }, { id::voiceVol, 0.66f } }),
        make("Solar Wind", "Pad",
             { { id::mixOsc1, 0 }, { id::mixOsc2, 0 }, { id::mixNoise, 1.0f }, { id::lpCutoff, 84 }, { id::lpRes, 0.78f }, { id::lpEnv, 0 },
               { id::lpVel, 0 }, { id::lfoLp, 1 }, { id::lfoFreq1, 0 }, { id::lfoFreq2, 0 }, { id::lfoAmt, 0.3f }, { id::lfoRate, 0.2f },
               { id::aEnvA, 0.6f }, { id::aEnvS, 1 }, { id::aEnvR, 0.65f }, { id::panSpread, 0.8f }, { id::fxBType, kHall },
               { id::fxBMix, 0.35f }, { id::fxBP1, 0.65f }, { id::voiceVol, 0.8f } }),
        make("Brass Section", "Keys",
             { { id::osc2Fine, 6 }, { id::lpCutoff, 72 }, { id::lpRes, 0.1f }, { id::lpEnv, 0.5f }, { id::fEnvA, 0.36f }, { id::fEnvD, 0.55f },
               { id::fEnvS, 0.55f }, { id::fEnvR, 0.4f }, { id::aEnvA, 0.25f }, { id::aEnvS, 1 }, { id::aEnvR, 0.35f }, { id::panSpread, 0.5f },
               { id::fxBType, kRoom }, { id::fxBMix, 0.2f }, { id::voiceVol, 0.34f } }),
        make("Poly Mod Bell", "Keys",
             { { id::osc1Shape, 0.0f }, { id::osc2Shape, 0.0f }, { id::osc2Freq, 43 }, { id::osc2Fine, 0 }, { id::mixOsc2, 0.0f },
               { id::pmFreq1, 1 }, { id::pmOsc2, 0.42f }, { id::lpCutoff, 120 }, { id::lpEnv, 0 }, { id::aEnvA, 0 }, { id::aEnvD, 0.72f },
               { id::aEnvS, 0 }, { id::aEnvR, 0.68f }, { id::panSpread, 0.5f }, { id::fxBType, kPlate }, { id::fxBMix, 0.3f },
               { id::fxBP1, 0.5f }, { id::voiceVol, 0.80f } }),
        make("Clav Pulse", "Keys",
             { { id::osc1Shape, 1.0f }, { id::osc1Pw, 0.24f }, { id::mixOsc2, 0 }, { id::lpCutoff, 84 }, { id::lpRes, 0.3f }, { id::lpEnv, 0.45f },
               { id::hpCutoff, 50 }, { id::fEnvA, 0 }, { id::fEnvD, 0.36f }, { id::fEnvS, 0 }, { id::aEnvA, 0 }, { id::aEnvD, 0.55f },
               { id::aEnvS, 0.2f }, { id::aEnvR, 0.2f }, { id::fxAType, kPhaser2 }, { id::fxAMix, 0.4f }, { id::fxAP1, 0.3f },
               { id::voiceVol, 0.80f } }),
        make("Soft Keys", "Keys",
             { { id::osc1Shape, 0.15f }, { id::osc2Shape, 0.4f }, { id::osc2Freq, 36 }, { id::osc2Fine, 0 }, { id::mixOsc2, 0.22f },
               { id::lpCutoff, 84 }, { id::lpRes, 0.05f }, { id::lpEnv, 0.32f }, { id::fEnvA, 0 }, { id::fEnvD, 0.6f }, { id::fEnvS, 0.2f },
               { id::aEnvA, 0 }, { id::aEnvD, 0.7f }, { id::aEnvS, 0.3f }, { id::aEnvR, 0.4f }, { id::fxAType, kChorus }, { id::fxAMix, 0.35f },
               { id::fxAP1, 0.3f }, { id::fxBType, kPlate }, { id::fxBMix, 0.15f }, { id::voiceVol, 0.90f } }),
        make("Gate Organ", "Keys",
             { { id::osc1Shape, 1.0f }, { id::osc2Shape, 1.0f }, { id::osc2Freq, 36 }, { id::mixOsc2, 0.5f }, { id::lpCutoff, 100 },
               { id::lpEnv, 0 }, { id::aEnvAmt, 0 }, { id::aEnvA, 0 }, { id::aEnvS, 1 }, { id::aEnvR, 0.1f }, { id::lfoShape, kSquare },
               { id::lfoAmp, 1 }, { id::lfoFreq1, 0 }, { id::lfoFreq2, 0 }, { id::lfoAmt, 1.0f }, { id::lfoRate, 0.6f },
               { id::fxAType, kBbd }, { id::fxAMix, 0.25f }, { id::fxAP1, 0.6f }, { id::voiceVol, 0.21f } }),
        make("Init", "Keys", {}),
    };
    return list;
}

JUCE_END_IGNORE_WARNINGS_MSVC

// =================================================================================================
//  PresetManager
// =================================================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s) : state(s)
{
    rescanUserPresets();
}

PresetManager::~PresetManager()
{
    stopTimer();
}

int PresetManager::getNumFactoryPresets() const noexcept
{
    return static_cast<int>(factoryPresets().size());
}

juce::String PresetManager::getCurrentName() const
{
    return presets[static_cast<size_t>(current)].name;
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Seer").getChildFile("Presets");
}

juce::StringArray PresetManager::getUserBanks() const
{
    juce::StringArray banks;
    for (const auto& folder : userFolder().findChildFiles(juce::File::findDirectories, false))
        banks.add(folder.getFileName());
    // Presets saved before there were banks sit in the main folder: they show as the default bank.
    if (!banks.contains(kDefaultBank) && !userFolder().findChildFiles(juce::File::findFiles, false, "*.seerpreset").isEmpty())
        banks.add(kDefaultBank);
    banks.sortNatural();
    return banks;
}

bool PresetManager::createBank(const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName(name.trim());
    return clean.isNotEmpty() && clean != kFactoryBank && userFolder().getChildFile(clean).createDirectory().wasOk();
}

void PresetManager::rescanUserPresets()
{
    const auto currentName = presets.empty() ? juce::String() : getCurrentName();
    const auto currentBank = presets.empty() ? juce::String() : presets[static_cast<size_t>(current)].bank;
    presets = factoryPresets();

    auto addFolder = [&](const juce::File& folder, const juce::String& bank)
    {
        auto files = folder.findChildFiles(juce::File::findFiles, false, "*.seerpreset");
        files.sort();
        for (const auto& file : files)
        {
            const auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr || !xml->hasTagName("SeerPreset"))
                continue;
            Preset preset { xml->getStringAttribute("name", file.getFileNameWithoutExtension()), bank, "User", {}, file };
            for (const auto* child : xml->getChildWithTagNameIterator("PARAM"))
                preset.values[child->getStringAttribute("id")] = static_cast<float>(child->getDoubleAttribute("value"));
            presets.push_back(std::move(preset));
        }
    };
    for (const auto& bank : getUserBanks())
    {
        if (bank == kDefaultBank)
            addFolder(userFolder(), bank); // from before there were banks
        addFolder(userFolder().getChildFile(bank), bank);
    }

    current = 0;
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].name == currentName && (currentBank.isEmpty() || presets[static_cast<size_t>(i)].bank == currentBank))
            current = i;
}

bool PresetManager::deleteUserPreset(int index)
{
    if (index < 0 || index >= getNumPresets() || presets[static_cast<size_t>(index)].file == juce::File())
        return false;
    const bool wasCurrent = index == current;
    if (!presets[static_cast<size_t>(index)].file.deleteFile())
        return false;
    rescanUserPresets();
    if (wasCurrent && onPresetChanged)
        onPresetChanged();
    return true;
}

void PresetManager::load(int index, bool withGlide)
{
    if (index < 0 || index >= getNumPresets())
        return;
    finishGlide();
    current = index;
    const auto& preset = presets[static_cast<size_t>(index)];
    state.state.setProperty(presetProperty, preset.name, nullptr);

    for (auto* p : state.processor.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (parameter == nullptr || params::isPlayerSetting(parameter->getParameterID()))
            continue;
        const auto it = preset.values.find(parameter->getParameterID());
        const float target = it != preset.values.end() ? parameter->convertTo0to1(it->second) : parameter->getDefaultValue();
        const float from = parameter->getValue();
        if (std::abs(target - from) < 1.0e-6f)
            continue;

        // Only continuous controls glide; a choice, a switch or a stepped control (the oscillators'
        // semitones) jumps straight to its new value.
        auto* asFloat = dynamic_cast<juce::AudioParameterFloat*>(parameter);
        const bool continuous = asFloat != nullptr && asFloat->range.interval == 0.0f;
        if (withGlide && continuous)
        {
            glide.push_back({ parameter, from, target });
            parameter->beginChangeGesture();
        }
        else
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(target);
            parameter->endChangeGesture();
        }
    }

    if (!glide.empty())
    {
        glideStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60);
    }
    if (onPresetChanged)
        onPresetChanged();
}

void PresetManager::timerCallback()
{
    const float u = static_cast<float>(
        std::clamp((juce::Time::getMillisecondCounterHiRes() - glideStart) / (kGlideSeconds * 1000.0), 0.0, 1.0));
    const float e = ease(u);
    for (const auto& move : glide)
        move.parameter->setValueNotifyingHost(move.from + (move.to - move.from) * e);
    if (u >= 1.0f)
        finishGlide();
}

void PresetManager::finishGlide()
{
    stopTimer();
    for (const auto& move : glide)
    {
        move.parameter->setValueNotifyingHost(move.to);
        move.parameter->endChangeGesture();
    }
    glide.clear();
}

juce::File PresetManager::saveUserPreset(const juce::String& name, const juce::String& bank)
{
    const auto cleanName = name.trim().isEmpty() ? juce::String("My preset") : name.trim();
    auto cleanBank = juce::File::createLegalFileName(bank.trim());
    if (cleanBank.isEmpty() || cleanBank == kFactoryBank)
        cleanBank = kDefaultBank;
    const auto folder = userFolder().getChildFile(cleanBank);
    if (!folder.createDirectory())
        return {};

    juce::XmlElement xml("SeerPreset");
    xml.setAttribute("name", cleanName);
    xml.setAttribute("version", 1);
    for (auto* p : state.processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (!params::isPlayerSetting(parameter->getParameterID()))
            {
                auto* child = xml.createNewChildElement("PARAM");
                child->setAttribute("id", parameter->getParameterID());
                child->setAttribute("value", parameter->convertFrom0to1(parameter->getValue()));
            }

    const auto file = folder.getChildFile(juce::File::createLegalFileName(cleanName) + ".seerpreset");
    if (!xml.writeTo(file))
        return {};

    rescanUserPresets();
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].file == file)
            current = i;
    state.state.setProperty(presetProperty, cleanName, nullptr);
    if (onPresetChanged)
        onPresetChanged();
    return file;
}

void PresetManager::restoreSelection(const juce::String& name)
{
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].name == name)
        {
            current = i;
            if (onPresetChanged)
                onPresetChanged();
            return;
        }
}
} // namespace seer
