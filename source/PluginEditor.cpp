#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
    #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
constexpr int kStatusHeight = 30, kKeysHeight = 90;
}

SeerEditor::SeerEditor(SeerProcessor& p)
    : AudioProcessorEditor(p),
      seerProcessor(p),
      parameterList(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible(parameterList);
    addAndMakeVisible(keyboard);
    addAndMakeVisible(status);
    keyboard.setAvailableRange(36, 84); // 49 keys, C2-C6
    keyboard.setKeyWidth(22.0f);
    status.setFont(juce::FontOptions(14.0f));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffece3d0));

    setSize(640, 760);
    enableAllMidiInputs();
    startTimerHz(20);
}

SeerEditor::~SeerEditor() = default;

void SeerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0c0e));
}

void SeerEditor::resized()
{
    auto area = getLocalBounds();
    status.setBounds(area.removeFromTop(kStatusHeight).reduced(8, 0));
    keyboard.setBounds(area.removeFromBottom(kKeysHeight));
    parameterList.setBounds(area);
}

void SeerEditor::timerCallback()
{
    // A MIDI device that is plugged in after the app started still gets switched on.
    if (++ticks % 40 == 0)
        enableAllMidiInputs();

    const uint32_t count = seerProcessor.midiEventCount.load(std::memory_order_relaxed);
    if (count != lastEventCount)
        midiLight = 4;
    lastEventCount = count;
    midiLight = std::max(0, midiLight - 1);

    status.setText(juce::String(midiLight > 0 ? "[MIDI]  " : "[    ]  ") + midiInputText
                       + "   voices " + juce::String(seerProcessor.activeVoices.load())
                       + "   pitch " + juce::String(seerProcessor.lastPitchWheel.load(), 2)
                       + "   mod " + juce::String(seerProcessor.lastModWheel.load(), 2)
                       + "   pressure " + juce::String(seerProcessor.lastAftertouch.load(), 2),
                   juce::dontSendNotification);
}

void SeerEditor::enableAllMidiInputs()
{
#if JucePlugin_Build_Standalone
    // Standalone only: switch on every connected MIDI input (the settings dialog can still turn
    // them off), and remember their names for the status line.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        auto& devices = holder->deviceManager;
        juce::StringArray names;
        for (const auto& input : juce::MidiInput::getAvailableDevices())
        {
            if (!devices.isMidiInputDeviceEnabled(input.identifier))
                devices.setMidiInputDeviceEnabled(input.identifier, true);
            names.add(input.name);
        }
        midiInputText = names.isEmpty() ? "No MIDI input found" : "In: " + names.joinIntoString(", ");
        return;
    }
#endif
    midiInputText = "MIDI from the host";
}
