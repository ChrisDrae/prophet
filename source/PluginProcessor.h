#pragma once

#include "Parameters.h"
#include "Presets.h"
#include "engine/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  SeerProcessor — the object the host (or the Standalone app) talks to.
//
//  It stays thin: parameters in, MIDI in, the engine renders. The factory presets double as the
//  host's "programs".
// =================================================================================================
class SeerProcessor final : public juce::AudioProcessor
{
public:
    SeerProcessor();
    ~SeerProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // [RT] Called on the audio thread, once per block.
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; } // the longest release

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- for the editor ---------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    seer::PresetManager& getPresets() noexcept { return presets; }

    /// The on-screen keyboard and incoming MIDI meet here, so keys light up for both.
    juce::MidiKeyboardState keyboardState;

    /// The window's wheels: the audio thread turns a change into an ordinary MIDI message.
    void setUiPitchWheel(float position) noexcept { uiPitchWheel.store(position, std::memory_order_relaxed); } ///< -1..1
    void setUiModWheel(float position) noexcept { uiModWheel.store(position, std::memory_order_relaxed); }     ///< 0..1

    /// What arrived last, for the window: a count of all events, the wheels, pressure, the last note.
    std::atomic<uint32_t> midiEventCount { 0 }, noteOnCount { 0 };
    std::atomic<int> lastNoteOn { 60 };
    std::atomic<float> lastPitchWheel { 0.0f }, lastModWheel { 0.0f }, lastAftertouch { 0.0f };
    std::atomic<int> activeVoices { 0 };
    /// The last MIDI message that arrived, packed as status | data1 << 8 | data2 << 16 (0 = none yet).
    std::atomic<uint32_t> lastMidiMessage { 0 };

private:
    juce::AudioProcessorValueTreeState apvts;
    seer::params::ParameterReader parameters; // reads apvts: declared after it
    seer::PresetManager presets { apvts };
    seer::SynthEngine engine;

    std::atomic<float> uiPitchWheel { 0.0f }, uiModWheel { 0.0f };
    float sentPitchWheel = 0.0f, sentModWheel = 0.0f; // audio thread only

    JUCE_DECLARE_WEAK_REFERENCEABLE(SeerProcessor) // async callbacks check the processor still exists
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeerProcessor)
};
