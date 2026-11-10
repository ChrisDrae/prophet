#pragma once

#include "Parameters.h"
#include "engine/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  SeerProcessor — the object the host (or the Standalone app) talks to.
//
//  It stays thin: parameters in, MIDI in, the engine renders.
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- for the editor ---------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    /// The on-screen keyboard and incoming MIDI meet here, so keys light up for both.
    juce::MidiKeyboardState keyboardState;
    /// What arrived last, for the MIDI readout: counts every event, and remembers the wheels.
    std::atomic<uint32_t> midiEventCount { 0 };
    std::atomic<float> lastPitchWheel { 0.0f }, lastModWheel { 0.0f }, lastAftertouch { 0.0f };
    std::atomic<int> activeVoices { 0 };

private:
    juce::AudioProcessorValueTreeState apvts;
    seer::params::ParameterReader parameters; // reads apvts: declared after it
    seer::SynthEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeerProcessor)
};
