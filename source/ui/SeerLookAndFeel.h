#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// =================================================================================================
//  The analog look: wooden end cheeks, near-black panels with cream silkscreen lettering,
//  physical knobs and faders, red LEDs. (docs/mockup/panel.html is the reference.)
// =================================================================================================
namespace seer::ui
{
namespace colours
{
inline const juce::Colour night { 0xff06090f };       // the base behind the light
inline const juce::Colour panel { 0xc70b0c0e };       // rgba(11, 12, 14, .78): a little light shows through
inline const juce::Colour pop { 0xff0b1018 };         // menus
inline const juce::Colour fg { 0xffece3d0 };          // silkscreen cream
inline const juce::Colour fgDim { 0xff8e96a1 };       // captions
inline const juce::Colour frame { 0x4dece3d0 };       // cream at 30 %: panel frames and rules
inline const juce::Colour line { 0x29ece3d0 };        // cream at 16 %
inline const juce::Colour slot { 0xff030405 };        // recesses: fader slots, displays
inline const juce::Colour ledOn { 0xffff4a43 };
inline const juce::Colour ledOff { 0xff3b0e10 };
inline const juce::Colour display { 0xff140506 };     // the preset readout's glass
inline const juce::Colour keysStrip { 0xf004060a };

// Each section has a hue: its dot, its frame while a control moves, its light behind the panels.
inline const juce::Colour lfo { 0xff7a56ec };
inline const juce::Colour polyMod { 0xffc23a94 };
inline const juce::Colour osc1 { 0xffe8a33d };
inline const juce::Colour osc2 { 0xffe8763a };
inline const juce::Colour drift { 0xff93708a };
inline const juce::Colour mixer { 0xfff4e3c3 };
inline const juce::Colour lowPass { 0xffe8263f };
inline const juce::Colour highPass { 0xff4f8fe0 };
inline const juce::Colour filterEnv { 0xffd64a3a };
inline const juce::Colour ampEnv { 0xffebe7e1 };
inline const juce::Colour voice { 0xffcdbfa6 };
inline const juce::Colour glide { 0xffb02a84 };
inline const juce::Colour effects { 0xff2f66c8 };
} // namespace colours

/// Fonts (Archivo, SIL OFL), loaded once from the binary. `tracking` is letter spacing in ems.
juce::Font titleFont(float height);                         ///< ExtraBold, wide tracking: the "S E E R" title
juce::Font labelFont(float height, float tracking = 0.12f); ///< Bold expanded: panel names, captions, switches
juce::Font valueFont(float height, float tracking = 0.04f); ///< SemiBold semi-expanded: values, displays, menus

/// Draws `text` in capitals with letter spacing, the way lettering is printed on a panel.
void drawCaps(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
              juce::Colour colour, juce::Justification justification = juce::Justification::centred);

/// A red LED in a chrome bezel, `size` pixels across, centred on `centre`.
void drawLed(juce::Graphics&, juce::Point<float> centre, float size, bool on);

class SeerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SeerLookAndFeel();

    /// A physical knob: a printed 11-mark scale, a dark skirt and cap with a shadow, one cream
    /// pointer line. With the slider's "big" property set the cap is brushed silver (the cutoff).
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    /// A fader: a recessed slot with a printed scale either side, and a dark cap with a cream line.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;

    /// A switch: an LED and its name.
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    /// A recessed display window with the choice in capitals.
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                      int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&, juce::Label&) override;
};
} // namespace seer::ui
