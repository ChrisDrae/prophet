#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/SeerLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

// =================================================================================================
//  SeerEditor — the plugin window: the whole synth panel on one screen (docs/ui-design.md).
//
//    header    menu · status (MIDI lamp, inputs, voices) · preset readout
//    row 1     LFO · Poly Mod · Oscillator 1 · Oscillator 2 · Drift · Mixer
//    row 2     Low-pass · High-pass · Filter envelope · Amp envelope
//    row 3     Voice · Glide · Aftertouch · Output · Effect A · Effect B
//    keys      pitch and mod wheels · master volume and tune · 49 keys
//
//  Everything is laid out at a design size of 1280 x 812 and scaled as a whole when the window is
//  resized. Click the on-screen keys once and the computer keyboard plays too (A W S E D ...).
// =================================================================================================
class SeerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeerEditor(SeerProcessor&);
    ~SeerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kDesignWidth = 1280;
    static constexpr int kDesignHeight = 812;

    // --- for the screenshot tool -------------------------------------------------------------------
    /// Runs one frame of the animation (normally the timer does this).
    void animate(double seconds);

private:
    using Panel = seer::ui::Panel;

    void timerCallback() override;
    void buildPanels();
    void layout();
    void enableAllMidiInputs();
    void showMenu();
    void showPresetList();
    void savePresetDialog();
    void setParameter(const char* parameterId, float plainValue);
    float value(const char* parameterId) const;
    void updateEffectCaptions();

    // Builders: each makes a control, keeps it alive, and puts it on a panel.
    seer::ui::KnobControl& knob(Panel& panel, const char* parameterId, const char* caption, bool big = false, bool side = false);
    void fader(Panel& panel, const char* parameterId, const char* caption);
    void led(Panel& panel, const char* parameterId, const char* caption, bool side = false);
    void choice(Panel& panel, const char* parameterId, const char* caption, int width, bool side = false);
    Panel& panel(const char* title, juce::Colour hue, juce::Rectangle<int> bounds);

    SeerProcessor& seerProcessor;
    seer::ui::SeerLookAndFeel lookAndFeel; // must outlive every component that uses it: declared first
    juce::TooltipWindow tooltips { this, 700 };

    juce::Component content; // the window at design size; scaled by a transform
    seer::ui::LightField field;
    juce::OwnedArray<Panel> panels;
    juce::OwnedArray<juce::Component> controls;
    seer::ui::KnobControl* effectKnobs[2][2] = {};

    seer::ui::MenuButton menuButton;
    seer::ui::StatusChip chip;
    seer::ui::PresetDisplay presetDisplay;
    struct Title final : juce::Component
    {
        void paint(juce::Graphics&) override;
    } title;
    struct Strip final : juce::Component
    {
        void paint(juce::Graphics&) override;
    } strip;

    seer::ui::WheelControl pitchWheel { "Pitch", true }, modWheel { "Mod", false };
    std::unique_ptr<seer::ui::KnobControl> masterVolume, masterTune;
    seer::ui::SeerKeyboard keyboard;
    seer::ui::Overlay overlay;

    double lastFrame = 0.0;
    uint32_t lastEventCount = 0, lastNoteCount = 0;
    int ticks = 0, midiLight = 0;
    int lastEffectType[2] = { -1, -1 };
    juce::String midiInputText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeerEditor)
};
