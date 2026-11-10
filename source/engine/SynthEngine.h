#pragma once

#include "EngineParams.h"
#include "HalfBand.h"
#include "Voice.h"
#include "effects/Effects.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

// =================================================================================================
//  SynthEngine — the whole instrument without a window: MIDI in, stereo audio out.
//
//      MIDI -> voice allocation -> voices (at 2x or 4x the sample rate) -> sum -> distortion
//           -> half-band filters, back to the host's sample rate -> effect A -> effect B -> volume
//
//  Time is cut into control ticks (about 0.1 ms). At each tick the knob values glide a step
//  towards their targets, the LFO advances, and every voice recomputes its pitch, cutoff and levels.
//
//  Voice allocation has two modes. Polyphonic: every key gets its own voice. Unison: one note at
//  a time, played by a stack of 1 to 6 detuned voices, with a key priority (low, high or last)
//  deciding which of the held keys sounds.
//
//  Real-time rules: prepare() allocates; nothing else does. No locks anywhere.
// =================================================================================================
namespace seer
{
class SynthEngine
{
public:
    static constexpr int kMaxVoices = 12;
    static constexpr int kMaxOversampling = 4;

    /// Message thread.
    void prepare(double sampleRate, int maximumBlockSize);

    /// The delay from a note to its sound that the oversampling adds, in host samples.
    int getLatencySamples() const noexcept { return 28; }

    /// [RT] The settings for the audio that follows. Continuous controls glide to the new values.
    void setParams(const EngineParams& newParams) noexcept;

    /// [RT] Notes, wheels, pedal, pressure.
    void handleMidi(const juce::MidiMessage& message) noexcept;
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void setPitchWheel(float position) noexcept { pitchWheel = std::clamp(position, -1.0f, 1.0f); } ///< -1..1
    void setModWheel(float position) noexcept { modWheel = std::clamp(position, 0.0f, 1.0f); }      ///< 0..1
    void setAftertouch(float pressure) noexcept { aftertouch = std::clamp(pressure, 0.0f, 1.0f); }  ///< 0..1
    void setSustainPedal(bool down) noexcept;
    void allNotesOff() noexcept;

    /// [RT] Renders `numSamples` (at most the prepared block size) into left and right.
    void render(float* left, float* right, int numSamples) noexcept;

    /// [RT] A whole host block: applies each MIDI event at its sample position.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const EngineParams& newParams) noexcept;

    int getNumActiveVoices() const noexcept;
    int getOversampling() const noexcept { return oversampling; }

private:
    void configure(int newOversampling) noexcept;
    void controlTick() noexcept;
    Modulation currentModulation() const noexcept;

    // Polyphonic mode
    Voice* findVoiceFor(int note) noexcept;
    void releaseNote(int note) noexcept;
    // Unison mode
    int pickUnisonNote() const noexcept;
    void startUnison(int note, float velocity, bool legato) noexcept;
    void releaseUnison() noexcept;
    bool anyKeyDown() const noexcept;

    double sampleRate = 48000.0, oversampledRate = 96000.0;
    int maxBlock = 0;
    int oversampling = 2;
    int tick = 8; ///< oversampled samples per control tick

    EngineParams target, current;
    bool paramsSet = false;
    float smoothing = 0.1f;      ///< per-tick glide coefficient for knobs
    float wheelSmoothing = 0.1f; ///< the same for the wheels and pressure (a little quicker)

    std::array<Voice, kMaxVoices> voices;
    int numVoices = 6;
    uint64_t clock = 0; ///< counts note-ons: the higher a voice's stamp, the newer its note
    std::array<bool, 128> keyDown {}, sustained {};
    std::array<uint64_t, 128> keyTime {}; ///< when each key went down (for last-note priority)
    std::array<float, 128> keyVelocity {};
    bool pedal = false;
    float lastPitch = 60.0f; ///< where a glide starts from
    bool haveLastPitch = false;
    int unisonNote = -1;     ///< the note the stack is playing, or -1

    float pitchWheel = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    float pitchWheelSmooth = 0.0f, modWheelSmooth = 0.0f, aftertouchSmooth = 0.0f;

    // LFO
    double lfoPhase = 0.0;
    float lfoValue = 0.0f, lfoRandom = 0.0f;
    dsp::Noise lfoNoise { 0xC0FFEEu };

    int samplesToTick = 0;
    float masterGain = 1.0f;

    // Output chain
    dsp::SmoothSaturator distortion;
    std::array<dsp::DcBlocker, 2> distortionDc;
    std::vector<float> highLeft, highRight, midLeft, midRight, scratch;
    std::array<dsp::HalfBand, 2> firstHalving; ///< 4x -> 2x (only in high quality)
    std::array<dsp::HalfBand, 2> lastHalving;  ///< 2x -> 1x
    EffectSlot effectA, effectB;
};
} // namespace seer
