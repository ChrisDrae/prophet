#pragma once

#include "SeerLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <functional>
#include <memory>

// =================================================================================================
//  The building blocks of the window: panels and the controls on them.
//
//  Every control is attached to a host parameter, so automation, MIDI mapping and presets move it.
//  Interaction is the same everywhere: drag up or down (Shift for fine steps), double-click to
//  reset, scroll wheel. A control's caption turns into its value while the mouse is over it.
// =================================================================================================
namespace seer::ui
{
using Apvts = juce::AudioProcessorValueTreeState;

/// One section of the synth: a near-black plate with a thin cream frame and its name on a rule.
/// It lays out what is put on it: an optional row of dropdowns on top, the main row of knobs or
/// faders (with an optional column at its right), and the switches below, wrapped over lines.
class Panel final : public juce::Component
{
public:
    Panel(juce::String title, juce::Colour hue);

    void addTop(juce::Component& c, int width, int height);
    void addMain(juce::Component& c, int width, int height);
    void addSide(juce::Component& c, int width, int height);
    void addSwitch(juce::Component& c, int width);

    /// While a control on the panel is being moved, the frame takes the section's colour.
    void setLive(bool isLive);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Item
    {
        juce::Component* component;
        int width, height;
    };
    juce::String title;
    juce::Colour hue;
    bool live = false;
    std::vector<Item> top, main, side, switches;
};

/// A slider bound to a parameter, with the house interaction.
class ParamSlider final : public juce::Slider
{
public:
    ParamSlider(Apvts& state, const juce::String& parameterId, juce::Slider::SliderStyle style);
    void mouseDown(const juce::MouseEvent&) override;
    juce::RangedAudioParameter& parameter;

private:
    Apvts::SliderAttachment attachment; // declared last: it's destroyed first
};

/// A knob over its caption. The caption shows the value while the mouse is over the knob.
class KnobControl final : public juce::Component
{
public:
    static constexpr int kWidth = 50, kHeight = 60, kBigWidth = 96, kBigHeight = 106;

    KnobControl(Apvts& state, const juce::String& parameterId, juce::String caption, Panel* panel, bool big = false);

    void setCaption(const juce::String& newCaption);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    ParamSlider slider;

private:
    juce::String caption;
    bool big, hover = false;
};

/// A vertical fader over its one-letter caption (the envelopes).
class FaderControl final : public juce::Component
{
public:
    static constexpr int kWidth = 38, kHeight = 124;

    FaderControl(Apvts& state, const juce::String& parameterId, juce::String caption, Panel* panel);

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    ParamSlider slider;

private:
    juce::String caption;
    bool hover = false;
};

/// A switch: an LED with its name, bound to an on/off parameter.
class LedToggle final : public juce::ToggleButton
{
public:
    LedToggle(Apvts& state, const juce::String& parameterId, const juce::String& text);
    int getPreferredWidth() const;

private:
    Apvts::ButtonAttachment attachment;
};

/// A choice in a recessed display window, in red lit letters like the preset readout, with small
/// arrows either side to step through the options. Clicking the name opens the whole list; the
/// scroll wheel and the arrow keys step too.
class ChoiceControl final : public juce::Component
{
public:
    static constexpr int kHeight = 26 + 4 + 11;

    ChoiceControl(Apvts& state, const juce::String& parameterId, juce::String caption);

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    int zoneAt(juce::Point<int> position) const; ///< -1 the left arrow, +1 the right arrow, 0 the name
    void step(int delta);
    void choose(int newIndex);
    void showList();

    juce::String caption;
    juce::StringArray options;
    int index = 0, hoverZone = 2; // 2: the mouse is elsewhere
    juce::ParameterAttachment attachment; // declared last: it's destroyed first
};

/// A choice with only a few options, as a column of lamps with their names (the filters' keyboard
/// tracking: Off, Half, Full). Click a name to choose it.
class LedSelector final : public juce::Component
{
public:
    static constexpr int kRow = 16;

    LedSelector(Apvts& state, const juce::String& parameterId, juce::String caption);
    int getPreferredHeight() const noexcept { return options.size() * kRow + 3 + 11; }
    int getPreferredWidth() const;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int rowAt(juce::Point<int> position) const;

    juce::String caption;
    juce::StringArray options;
    int index = 0, hover = -1;
    juce::ParameterAttachment attachment;
};

/// The key strip's octave buttons: a readout of the transposition between a down and an up button.
/// Right-click a button to teach it the matching button on your MIDI keyboard (for keyboards whose
/// octave buttons send a message instead of shifting their notes).
class OctaveControl final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OctaveControl(Apvts& state, const juce::String& transposeId);

    /// The window connects these to the processor's OctaveButtons.
    std::function<void(int direction)> onLearn;  ///< start listening for that button (0: stop)
    std::function<void(int direction)> onForget; ///< forget that button
    std::function<juce::String(int direction)> describe; ///< the learned message, or empty

    /// Shows which button is listening (0: none). The button blinks while it waits.
    void setLearning(int direction);
    int getLearning() const noexcept { return learning; }

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> buttonArea(int direction) const;
    juce::Rectangle<int> displayArea() const;
    int buttonAt(juce::Point<int> position) const;

    int value = 0, hover = 0, learning = 0;
    bool blink = false;
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
};


/// The square menu button.
class MenuButton final : public juce::Button
{
public:
    MenuButton();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
};

/// The status line in the header: a MIDI lamp and a line of capitals.
class StatusChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setText(const juce::String& newText);
    void setMidiLight(bool on);
    void paint(juce::Graphics&) override;

private:
    juce::String text;
    bool light = false;
};

/// The preset readout: red LED-style letters behind glass, between two arrows.
class PresetDisplay final : public juce::Component
{
public:
    PresetDisplay();
    void setName(const juce::String& newName);

    std::function<void()> onPrevious, onNext, onOpenList;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<int> previousArea, nextArea, nameArea;
    juce::String name;
};

/// A performance wheel in its slot. The pitch wheel springs back to the middle when let go.
class WheelControl final : public juce::Component
{
public:
    WheelControl(juce::String caption, bool springsBack);

    /// Called while the wheel is dragged: -1..1 (pitch) or 0..1 (mod).
    std::function<void(float)> onMove;
    /// Shows a position that came from outside (the MIDI keyboard's own wheel).
    void showValue(float newValue);
    bool isBeingDragged() const noexcept { return dragging; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String caption;
    bool spring, dragging = false;
    float value = 0.0f, dragStart = 0.0f;
};

/// The on-screen keys: 49 of them, C2 to C6, ivory, blushing red when played.
class SeerKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit SeerKeyboard(juce::MidiKeyboardState& state);
    void resized() override;

private:
    void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour lineColour,
                       juce::Colour textColour) override;
    void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour noteFillColour) override;
};

/// The slow field of coloured light behind the panels. Each light follows one part of the sound.
class LightField final : public juce::Component
{
public:
    enum Light
    {
        amber,  ///< oscillator levels
        red,    ///< low-pass cutoff
        blue,   ///< high-pass and effects
        violet, ///< LFO and Poly Mod
        magenta, ///< resonance and distortion
        numLights
    };

    LightField();
    void setLevels(const std::array<float, numLights>& newLevels, float newSpeed);
    void noteOn(float position01);
    /// One animation frame: `seconds` since the last one.
    void advance(double seconds);

    void paint(juce::Graphics&) override;

private:
    std::array<float, numLights> levels {};
    float speed = 0.1f;
    double time = 0.0;
    struct Glow
    {
        float x, life;
    };
    std::vector<Glow> glows;
    juce::Image canvas;
};

/// Film grain over the whole window, and the wooden cheeks at its sides.
class Overlay final : public juce::Component
{
public:
    static constexpr int kCheek = 24;
    Overlay();
    void paint(juce::Graphics&) override;

private:
    juce::Image grain;
};
} // namespace seer::ui
