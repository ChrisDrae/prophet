#include "SeerLookAndFeel.h"

#include <BinaryData.h>

namespace seer::ui
{
namespace
{
struct Typefaces
{
    juce::Typeface::Ptr extraBold = juce::Typeface::createSystemTypefaceFor(
        SeerFonts::ArchivoExpandedExtraBold_ttf, static_cast<size_t>(SeerFonts::ArchivoExpandedExtraBold_ttfSize));
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor(
        SeerFonts::ArchivoExpandedBold_ttf, static_cast<size_t>(SeerFonts::ArchivoExpandedBold_ttfSize));
    juce::Typeface::Ptr semiBold = juce::Typeface::createSystemTypefaceFor(
        SeerFonts::ArchivoSemiExpandedSemiBold_ttf, static_cast<size_t>(SeerFonts::ArchivoSemiExpandedSemiBold_ttfSize));
};

const Typefaces& typefaces()
{
    static const Typefaces instance; // [C++] a function-local static is created once, thread-safely
    return instance;
}

juce::Font makeFont(const juce::Typeface::Ptr& face, float height, float tracking)
{
    if (face == nullptr)
        return juce::Font(juce::FontOptions(height, juce::Font::bold)).withExtraKerningFactor(tracking);
    return juce::Font(juce::FontOptions(face).withHeight(height).withKerningFactor(tracking));
}

juce::Rectangle<float> floatBounds(int x, int y, int w, int h)
{
    return { static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h) };
}
} // namespace

juce::Font titleFont(float height) { return makeFont(typefaces().extraBold, height, 0.62f); }
juce::Font labelFont(float height, float tracking) { return makeFont(typefaces().bold, height, tracking); }
juce::Font valueFont(float height, float tracking) { return makeFont(typefaces().semiBold, height, tracking); }

void drawCaps(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
              juce::Colour colour, juce::Justification justification)
{
    // Letter spacing also follows the last letter; shift centred text by half of it so it looks centred.
    if (justification.testFlags(juce::Justification::horizontallyCentred))
        area = area.translated(font.getExtraKerningFactor() * font.getHeight() * 0.5f, 0.0f);
    g.setColour(colour);
    g.setFont(font);
    g.drawText(text.toUpperCase(), area, justification, false);
}

void drawLed(juce::Graphics& g, juce::Point<float> c, float size, bool on)
{
    const float r = size * 0.5f;
    if (on)
    {
        // The glow around a lit lamp.
        g.setGradientFill(juce::ColourGradient(colours::ledOn.withAlpha(0.55f), c.x, c.y, colours::ledOn.withAlpha(0.0f), c.x + r * 2.8f, c.y, true));
        g.fillEllipse(juce::Rectangle<float>(r * 5.6f, r * 5.6f).withCentre(c));
    }
    g.setColour(juce::Colours::black);
    g.fillEllipse(juce::Rectangle<float>(size + 5.0f, size + 5.0f).withCentre(c));
    g.setColour(juce::Colour(0xff8c8a84)); // the chrome bezel
    g.fillEllipse(juce::Rectangle<float>(size + 3.0f, size + 3.0f).withCentre(c));
    const auto lamp = juce::Rectangle<float>(size, size).withCentre(c);
    if (on)
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffd2c8), c.x - r * 0.3f, c.y - r * 0.4f, juce::Colour(0xffb01418), c.x + r, c.y + r, true));
    else
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff6a1c1e), c.x - r * 0.3f, c.y - r * 0.4f, colours::ledOff, c.x + r * 0.6f, c.y + r * 0.6f, true));
    g.fillEllipse(lamp);
    if (on)
    {
        g.setColour(colours::ledOn.withAlpha(0.75f));
        g.fillEllipse(lamp.reduced(size * 0.18f));
        g.setColour(juce::Colour(0xffffd2c8).withAlpha(0.8f));
        g.fillEllipse(juce::Rectangle<float>(size * 0.3f, size * 0.3f).withCentre(c.translated(-r * 0.25f, -r * 0.3f)));
    }
}

SeerLookAndFeel::SeerLookAndFeel()
{
    setColour(juce::ComboBox::textColourId, colours::fg);
    setColour(juce::ComboBox::backgroundColourId, colours::slot);
    setColour(juce::ComboBox::outlineColourId, colours::frame);
    setColour(juce::ComboBox::arrowColourId, colours::fg);
    setColour(juce::PopupMenu::backgroundColourId, colours::pop);
    setColour(juce::PopupMenu::textColourId, colours::fg);
    setColour(juce::PopupMenu::headerTextColourId, colours::fgDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::ledOn.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::fg);
    setColour(juce::Label::textColourId, colours::fg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::backgroundColourId, colours::pop);
    setColour(juce::TooltipWindow::textColourId, colours::fg);
    setColour(juce::TooltipWindow::outlineColourId, colours::line);
    setColour(juce::TextEditor::backgroundColourId, colours::slot);
    setColour(juce::TextEditor::textColourId, colours::fg);
    setColour(juce::TextEditor::outlineColourId, colours::frame);
    setColour(juce::TextEditor::focusedOutlineColourId, colours::fg);
    setColour(juce::AlertWindow::backgroundColourId, colours::pop);
    setColour(juce::AlertWindow::textColourId, colours::fg);
    setColour(juce::AlertWindow::outlineColourId, colours::line);
    setColour(juce::TextButton::buttonColourId, colours::slot);
    setColour(juce::TextButton::textColourOffId, colours::fg);
}

void SeerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                       juce::Slider& slider)
{
    const bool big = slider.getProperties().contains("big");
    const auto bounds = floatBounds(x, y, width, height);
    const auto c = bounds.getCentre();
    const float r = 0.5f * std::min(bounds.getWidth(), bounds.getHeight());
    constexpr float start = juce::MathConstants<float>::pi * 1.25f, sweep = juce::MathConstants<float>::pi * 1.5f;
    auto polar = [&](float angle, float radius) { return juce::Point<float>(c.x + radius * std::sin(angle), c.y - radius * std::cos(angle)); };

    // The printed scale: eleven marks, the first, middle and last a little longer.
    g.setColour(colours::fg.withAlpha(0.7f));
    for (int i = 0; i < 11; ++i)
    {
        const float a = start + sweep * static_cast<float>(i) / 10.0f;
        g.drawLine(juce::Line<float>(polar(a, r * (i % 5 == 0 ? 0.84f : 0.89f)), polar(a, r * 0.99f)), big ? 1.3f : 1.1f);
    }

    // Shadow, then the skirt.
    const float skirt = r * 0.74f;
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.7f), c.x, c.y + r * 0.1f, juce::Colours::transparentBlack, c.x, c.y + skirt * 1.3f, true));
    g.fillEllipse(juce::Rectangle<float>(skirt * 2.6f, skirt * 2.6f).withCentre(c.translated(0.0f, r * 0.1f)));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3d4046), c.x, c.y - skirt * 0.5f, juce::Colour(0xff0b0c0e), c.x, c.y + skirt, true));
    g.fillEllipse(juce::Rectangle<float>(skirt * 2.0f, skirt * 2.0f).withCentre(c));

    // The cap.
    const float cap = r * (big ? 0.58f : 0.53f);
    const auto capArea = juce::Rectangle<float>(cap * 2.0f, cap * 2.0f).withCentre(c);
    if (big)
    {
        juce::ColourGradient silver(juce::Colour(0xfff4f1ea), capArea.getTopLeft(), juce::Colour(0xff6b6964), capArea.getBottomRight(), false);
        silver.addColour(0.45, juce::Colour(0xffaeaba3));
        silver.addColour(0.6, juce::Colour(0xffd9d6ce));
        g.setGradientFill(silver);
    }
    else
    {
        g.setGradientFill(juce::ColourGradient::vertical(juce::Colour(0xff34373c), juce::Colour(0xff101113), capArea));
    }
    g.fillEllipse(capArea);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawEllipse(capArea, 1.0f);

    // The pointer: one cream line on the skirt.
    const float angle = start + sweep * sliderPos;
    g.setColour(colours::fg);
    g.drawLine(juce::Line<float>(polar(angle, r * (big ? 0.6f : 0.42f)), polar(angle, r * 0.7f)), big ? 2.6f : 2.2f);

    if (slider.hasKeyboardFocus(false))
    {
        g.setColour(colours::fg.withAlpha(0.5f));
        g.drawEllipse(juce::Rectangle<float>(skirt * 2.0f, skirt * 2.0f).withCentre(c).expanded(1.5f), 1.0f);
    }
}

juce::Slider::SliderLayout SeerLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = slider.getLocalBounds();
    if (slider.getSliderStyle() == juce::Slider::LinearVertical)
        layout.sliderBounds = layout.sliderBounds.reduced(0, 8); // room for half the cap at each end
    return layout;
}

void SeerLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider&)
{
    const auto bounds = floatBounds(x, y, width, height);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = bounds.getCentreX();
        // The printed scale either side of the slot.
        g.setColour(colours::frame);
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = bounds.getY() + bounds.getHeight() * static_cast<float>(i) / 10.0f;
            g.fillRect(juce::Rectangle<float>(cx - 13.0f, ty - 0.5f, 6.0f, 1.0f));
            g.fillRect(juce::Rectangle<float>(cx + 7.0f, ty - 0.5f, 6.0f, 1.0f));
        }
        const auto slotArea = juce::Rectangle<float>(cx - 3.0f, bounds.getY() - 4.0f, 6.0f, bounds.getHeight() + 8.0f);
        g.setColour(colours::slot);
        g.fillRoundedRectangle(slotArea, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(slotArea, 3.0f, 1.0f);

        const auto cap = juce::Rectangle<float>(22.0f, 14.0f).withCentre({ cx, sliderPos });
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(cap.translated(0.0f, 3.0f).expanded(1.0f), 3.0f);
        g.setGradientFill(juce::ColourGradient::vertical(juce::Colour(0xff3a3d43), juce::Colour(0xff0d0e10), cap));
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(colours::fg);
        g.fillRect(cap.withSizeKeepingCentre(cap.getWidth(), 2.0f));
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRect(cap.withHeight(1.0f).reduced(2.0f, 0.0f));
    }
    else
    {
        const float cy = bounds.getCentreY();
        g.setColour(colours::slot);
        g.fillRoundedRectangle(bounds.withSizeKeepingCentre(bounds.getWidth(), 4.0f), 2.0f);
        g.setColour(colours::fg);
        g.fillRoundedRectangle(juce::Rectangle<float>(6.0f, 16.0f).withCentre({ sliderPos, cy }), 1.0f);
    }
}

void SeerLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    drawLed(g, { 8.0f, bounds.getCentreY() }, 9.0f, button.getToggleState());
    drawCaps(g, button.getButtonText(), bounds.withTrimmedLeft(19.0f), labelFont(9.5f, 0.12f),
             colours::fg.withAlpha(highlighted ? 1.0f : 0.88f), juce::Justification::centredLeft);
    if (button.hasKeyboardFocus(false))
    {
        g.setColour(colours::fg.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    }
}

void SeerLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = floatBounds(0, 0, width, height).reduced(0.5f);
    g.setColour(colours::slot);
    g.fillRoundedRectangle(bounds, 3.0f);
    // The shadow of the recess along the top edge.
    g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::black.withAlpha(0.8f), bounds.getY(), juce::Colours::transparentBlack, bounds.getY() + 5.0f));
    g.fillRoundedRectangle(bounds.withHeight(5.0f), 3.0f);
    g.setColour(box.isMouseOver(true) || box.isPopupActive() ? colours::fg.withAlpha(0.7f) : colours::frame);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void SeerLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 8, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

juce::Font SeerLookAndFeel::getComboBoxFont(juce::ComboBox&) { return valueFont(11.5f, 0.06f); }
juce::Font SeerLookAndFeel::getPopupMenuFont() { return valueFont(13.5f); }

void SeerLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(colours::pop);
    g.setColour(colours::line);
    g.drawRect(0, 0, width, height, 1);
}

juce::PopupMenu::Options SeerLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label)
{
    return LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label).withStandardItemHeight(26);
}
} // namespace seer::ui
