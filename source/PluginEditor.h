#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

// =================================================================================================
//  SeerEditor — the milestone-1 window: a plain list of every parameter (JUCE's generic editor),
//  a keyboard, and a readout of what the MIDI controller sends. It exists so the engine can be
//  played and judged; the real panel (docs/ui-design.md) replaces it in milestone 3.
// =================================================================================================
class SeerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeerEditor(SeerProcessor&);
    ~SeerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void enableAllMidiInputs();

    SeerProcessor& seerProcessor;
    juce::GenericAudioProcessorEditor parameterList;
    juce::MidiKeyboardComponent keyboard;
    juce::Label status;
    juce::String midiInputText;
    uint32_t lastEventCount = 0;
    int ticks = 0, midiLight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeerEditor)
};
