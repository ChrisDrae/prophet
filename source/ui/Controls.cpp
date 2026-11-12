#include "Controls.h"

namespace seer::ui
{
namespace
{
float textWidth(const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth(font, text.toUpperCase());
}

float textWidthOf(const juce::Font& font, const juce::String& text)
{
    // Letter spacing adds a little after every letter.
    return textWidth(font, text) + font.getExtraKerningFactor() * font.getHeight();
}

juce::String valueText(const juce::RangedAudioParameter& parameter)
{
    return parameter.getCurrentValueAsText();
}
} // namespace

// =================================================================================================
//  Panel
// =================================================================================================
Panel::Panel(juce::String t, juce::Colour h) : title(std::move(t)), hue(h)
{
    setInterceptsMouseClicks(false, true);
    // The light behind the panels moves all the time. Kept as an image, a panel that hasn't
    // changed is copied over each new frame of light instead of being drawn again.
    setBufferedToImage(true);
}

void Panel::addTop(juce::Component& c, int width, int height)
{
    top.push_back({ &c, width, height });
    addAndMakeVisible(c);
}
void Panel::addMain(juce::Component& c, int width, int height)
{
    main.push_back({ &c, width, height });
    addAndMakeVisible(c);
}
void Panel::addSide(juce::Component& c, int width, int height)
{
    side.push_back({ &c, width, height });
    addAndMakeVisible(c);
}
void Panel::addSwitch(juce::Component& c, int width)
{
    switches.push_back({ &c, width, 18 });
    addAndMakeVisible(c);
}

void Panel::setLive(bool isLive)
{
    if (live != isLive)
    {
        live = isLive;
        repaint();
    }
}

void Panel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colours::panel);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRect(bounds.reduced(4.0f, 0.0f).withHeight(1.0f).translated(0.0f, 1.0f));
    g.setColour(live ? colours::frame.interpolatedWith(hue, 0.6f) : colours::frame);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    // The name on a rule: ----- * NAME -----
    const auto font = labelFont(11.0f, 0.14f);
    const float nameWidth = textWidth(font, title);
    const auto row = getLocalBounds().toFloat().reduced(10.0f, 0.0f).withY(8.0f).withHeight(14.0f);
    const float block = nameWidth + 13.0f;
    const float left = row.getCentreX() - block * 0.5f;
    g.setColour(colours::frame);
    if (left - 8.0f > row.getX())
    {
        g.fillRect(juce::Rectangle<float>(row.getX(), row.getCentreY(), left - 8.0f - row.getX(), 1.0f));
        g.fillRect(juce::Rectangle<float>(left + block + 8.0f, row.getCentreY(), row.getRight() - (left + block + 8.0f), 1.0f));
    }
    g.setColour(hue.withAlpha(0.5f));
    g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre({ left + 2.5f, row.getCentreY() }));
    g.setColour(hue);
    g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre({ left + 2.5f, row.getCentreY() }));
    drawCaps(g, title, juce::Rectangle<float>(left + 13.0f, row.getY(), nameWidth + 4.0f, row.getHeight()), font, colours::fg,
             juce::Justification::centredLeft);
}

void Panel::resized()
{
    constexpr int kPad = 8, kTopGap = 8, kGap = 4, kSideGap = 8;
    const auto area = getLocalBounds().reduced(kPad, 0);
    int y = 30;

    auto placeRow = [&](const std::vector<Item>& items, int gap)
    {
        int total = -gap, height = 0;
        for (const auto& i : items)
        {
            total += i.width + gap;
            height = std::max(height, i.height);
        }
        int x = area.getCentreX() - total / 2;
        for (const auto& i : items)
        {
            i.component->setBounds(x, y, i.width, i.height);
            x += i.width + gap;
        }
        return height;
    };

    if (!top.empty())
        y += placeRow(top, kTopGap) + 8;

    if (!main.empty() || !side.empty())
    {
        int mainWidth = main.empty() ? 0 : -kGap, rowHeight = 0, sideWidth = 0, sideHeight = side.empty() ? 0 : -6;
        for (const auto& i : main)
        {
            mainWidth += i.width + kGap;
            rowHeight = std::max(rowHeight, i.height);
        }
        for (const auto& i : side)
        {
            sideWidth = std::max(sideWidth, i.width);
            sideHeight += i.height + 6;
        }
        const int total = mainWidth + (side.empty() ? 0 : sideWidth + (main.empty() ? 0 : kSideGap));
        rowHeight = std::max(rowHeight, sideHeight);
        int x = area.getCentreX() - total / 2;
        for (const auto& i : main)
        {
            i.component->setBounds(x, y, i.width, i.height);
            x += i.width + kGap;
        }
        if (!side.empty())
        {
            x += main.empty() ? 0 : kSideGap - kGap;
            int sy = y + (rowHeight - sideHeight) / 2;
            for (const auto& i : side)
            {
                i.component->setBounds(x + (sideWidth - i.width) / 2, sy, i.width, i.height);
                sy += i.height + 6;
            }
        }
        y += rowHeight + 6;
    }

    // Switches: as many per line as fit, each line centred.
    size_t first = 0;
    while (first < switches.size())
    {
        int width = switches[first].width;
        size_t last = first + 1;
        while (last < switches.size() && width + 10 + switches[last].width <= area.getWidth())
            width += 10 + switches[last++].width;
        int x = area.getCentreX() - width / 2;
        for (size_t i = first; i < last; ++i)
        {
            switches[i].component->setBounds(x, y, switches[i].width, 18);
            x += switches[i].width + 10;
        }
        y += 21;
        first = last;
    }
}

// =================================================================================================
//  Sliders
// =================================================================================================
ParamSlider::ParamSlider(Apvts& state, const juce::String& parameterId, juce::Slider::SliderStyle style)
    : juce::Slider(style, juce::Slider::NoTextBox), parameter(*state.getParameter(parameterId)), attachment(state, parameterId, *this)
{
    setDoubleClickReturnValue(true, parameter.convertFrom0to1(parameter.getDefaultValue()));
    setMouseDragSensitivity(180);
    setVelocityBasedMode(false);
    setScrollWheelEnabled(true);
    setTitle(parameter.getName(64));
    setWantsKeyboardFocus(true);
}

void ParamSlider::mouseDown(const juce::MouseEvent& e)
{
    setMouseDragSensitivity(e.mods.isShiftDown() ? 900 : 180); // Shift: fine steps
    juce::Slider::mouseDown(e);
}

KnobControl::KnobControl(Apvts& state, const juce::String& parameterId, juce::String c, Panel* panel, bool isBig)
    : slider(state, parameterId, juce::Slider::RotaryVerticalDrag), caption(std::move(c)), big(isBig)
{
    if (big)
        slider.getProperties().set("big", true);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible(slider);
    slider.addMouseListener(this, false);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [panel] { if (panel != nullptr) panel->setLive(true); };
    slider.onDragEnd = [panel] { if (panel != nullptr) panel->setLive(false); };
    setInterceptsMouseClicks(false, true);
    setPaintingIsUnclipped(true); // a caption or a value may be wider than the knob
}

void KnobControl::setCaption(const juce::String& newCaption)
{
    if (caption != newCaption)
    {
        caption = newCaption;
        repaint();
    }
}

void KnobControl::resized()
{
    const int size = big ? 88 : 44;
    slider.setBounds((getWidth() - size) / 2, 0, size, size);
}

void KnobControl::paint(juce::Graphics& g)
{
    // The caption may be wider than the knob: it is drawn centred and allowed to overhang.
    const auto area = juce::Rectangle<float>(-30.0f, static_cast<float>(getHeight()) - 12.0f, static_cast<float>(getWidth()) + 60.0f, 11.0f);
    const bool showValue = hover || slider.isMouseButtonDown();
    if (showValue)
    {
        g.setColour(colours::fg);
        g.setFont(valueFont(10.0f, 0.0f));
        g.drawText(valueText(slider.parameter), area, juce::Justification::centred, false);
    }
    else
    {
        drawCaps(g, caption, area, labelFont(8.5f, 0.14f), colours::fgDim);
    }
}

void KnobControl::mouseEnter(const juce::MouseEvent&)
{
    hover = true;
    repaint();
}
void KnobControl::mouseExit(const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

FaderControl::FaderControl(Apvts& state, const juce::String& parameterId, juce::String c, Panel* panel)
    : slider(state, parameterId, juce::Slider::LinearVertical), caption(std::move(c))
{
    addAndMakeVisible(slider);
    slider.addMouseListener(this, false);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [panel] { if (panel != nullptr) panel->setLive(true); };
    slider.onDragEnd = [panel] { if (panel != nullptr) panel->setLive(false); };
    setInterceptsMouseClicks(false, true);
    setPaintingIsUnclipped(true);
}

void FaderControl::resized()
{
    slider.setBounds(0, 0, getWidth(), getHeight() - 14);
}

void FaderControl::paint(juce::Graphics& g)
{
    const auto area = juce::Rectangle<float>(-30.0f, static_cast<float>(getHeight()) - 12.0f, static_cast<float>(getWidth()) + 60.0f, 11.0f);
    if (hover || slider.isMouseButtonDown())
    {
        g.setColour(colours::fg);
        g.setFont(valueFont(10.0f, 0.0f));
        g.drawText(valueText(slider.parameter), area, juce::Justification::centred, false);
    }
    else
    {
        drawCaps(g, caption, area, labelFont(8.5f, 0.14f), colours::fgDim);
    }
}

void FaderControl::mouseEnter(const juce::MouseEvent&)
{
    hover = true;
    repaint();
}
void FaderControl::mouseExit(const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

// =================================================================================================
//  Switches and dropdowns
// =================================================================================================
LedToggle::LedToggle(Apvts& state, const juce::String& parameterId, const juce::String& text)
    : juce::ToggleButton(text), attachment(state, parameterId, *this)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

int LedToggle::getPreferredWidth() const
{
    return juce::roundToInt(textWidth(labelFont(9.5f, 0.12f), getButtonText())) + 22;
}

ChoiceControl::ChoiceControl(Apvts& state, const juce::String& parameterId, juce::String cap)
    : caption(std::move(cap)),
      attachment(*state.getParameter(parameterId), [this](float plain) { index = juce::roundToInt(plain); repaint(); })
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        options = choice->choices;
    setTitle(caption);
    setWantsKeyboardFocus(true);
    setPaintingIsUnclipped(true); // the caption may be wider than the window
    attachment.sendInitialUpdate();
}

int ChoiceControl::zoneAt(juce::Point<int> position) const
{
    if (position.y > 26)
        return 2;
    return position.x < 16 ? -1 : position.x >= getWidth() - 16 ? 1 : 0;
}

void ChoiceControl::step(int delta)
{
    if (!options.isEmpty())
        choose((index + delta + options.size()) % options.size());
}

void ChoiceControl::choose(int newIndex)
{
    if (newIndex != index)
        attachment.setValueAsCompleteGesture(static_cast<float>(newIndex));
}

void ChoiceControl::showList()
{
    juce::PopupMenu menu;
    for (int i = 0; i < options.size(); ++i)
        menu.addItem(i + 1, options[i], true, i == index);
    juce::Component::SafePointer<ChoiceControl> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()).withStandardItemHeight(26),
                       [safe](int result)
                       {
                           if (safe != nullptr && result > 0)
                               safe->choose(result - 1);
                       });
}

void ChoiceControl::paint(juce::Graphics& g)
{
    const auto window = getLocalBounds().withHeight(26).toFloat().reduced(0.5f);
    g.setColour(colours::display);
    g.fillRoundedRectangle(window, 3.0f);
    // The recess: a shadow along the top edge and a faint highlight along the bottom.
    g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::black.withAlpha(0.85f), window.getY(), juce::Colours::transparentBlack, window.getY() + 7.0f));
    g.fillRoundedRectangle(window.withHeight(7.0f), 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRect(window.reduced(3.0f, 0.0f).withTop(window.getBottom() - 1.0f));
    g.setColour(hoverZone != 2 || hasKeyboardFocus(false) ? colours::fg.withAlpha(0.55f) : colours::frame);
    g.drawRoundedRectangle(window, 3.0f, 1.0f);

    // The arrows.
    for (const int side : { -1, 1 })
    {
        const float x = side < 0 ? window.getX() + 8.5f : window.getRight() - 8.5f, y = window.getCentreY();
        juce::Path arrow;
        arrow.addTriangle(x - 2.5f * static_cast<float>(side), y - 3.5f, x - 2.5f * static_cast<float>(side), y + 3.5f, x + 3.0f * static_cast<float>(side), y);
        g.setColour(colours::fg.withAlpha(hoverZone == side ? 0.95f : 0.4f));
        g.fillPath(arrow);
    }

    // The choice, in lit red letters (smaller if a long name needs it).
    const auto text = options[index].toUpperCase();
    const auto area = window.reduced(16.0f, 0.0f);
    auto font = valueFont(11.5f, 0.07f);
    for (float height = 11.5f; height > 8.0f && textWidthOf(font, text) > area.getWidth(); height -= 0.5f)
        font = valueFont(height, 0.05f);
    drawCaps(g, text, area.translated(0.0f, 0.5f).expanded(1.0f), font, colours::ledOn.withAlpha(hoverZone == 0 ? 0.35f : 0.22f));
    drawCaps(g, text, area, font, colours::ledOn);

    drawCaps(g, caption, juce::Rectangle<float>(-20.0f, 30.0f, static_cast<float>(getWidth()) + 40.0f, 11.0f), labelFont(8.5f, 0.14f), colours::fgDim);
}

void ChoiceControl::mouseMove(const juce::MouseEvent& e)
{
    if (const int zone = zoneAt(e.getPosition()); zone != hoverZone)
    {
        hoverZone = zone;
        setMouseCursor(zone == 2 ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void ChoiceControl::mouseExit(const juce::MouseEvent&)
{
    hoverZone = 2;
    repaint();
}

void ChoiceControl::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mouseWasClicked())
        return;
    switch (zoneAt(e.getPosition()))
    {
        case -1: step(-1); break;
        case 1: step(1); break;
        case 0: showList(); break;
        default: break;
    }
}

void ChoiceControl::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY != 0.0f)
        step(wheel.deltaY > 0.0f ? -1 : 1);
}

bool ChoiceControl::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
        step(-1);
    else if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
        step(1);
    else if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        showList();
    else
        return false;
    return true;
}

LedSelector::LedSelector(Apvts& state, const juce::String& parameterId, juce::String cap)
    : caption(std::move(cap)),
      attachment(*state.getParameter(parameterId), [this](float plain) { index = juce::roundToInt(plain); repaint(); })
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        options = choice->choices;
    setTitle(caption);
    setPaintingIsUnclipped(true);
    attachment.sendInitialUpdate();
}

int LedSelector::getPreferredWidth() const
{
    float widest = textWidthOf(labelFont(8.5f, 0.14f), caption);
    for (const auto& option : options)
        widest = std::max(widest, textWidthOf(labelFont(9.5f, 0.12f), option) + 19.0f);
    return juce::roundToInt(widest) + 4;
}

int LedSelector::rowAt(juce::Point<int> position) const
{
    const int row = position.y / kRow;
    return row >= 0 && row < options.size() ? row : -1;
}

void LedSelector::paint(juce::Graphics& g)
{
    for (int i = 0; i < options.size(); ++i)
    {
        const auto row = juce::Rectangle<float>(0.0f, static_cast<float>(i * kRow), static_cast<float>(getWidth()), static_cast<float>(kRow));
        drawLed(g, { 8.0f, row.getCentreY() }, 8.0f, i == index);
        drawCaps(g, options[i], row.withTrimmedLeft(19.0f), labelFont(9.5f, 0.12f), colours::fg.withAlpha(i == hover ? 1.0f : 0.85f),
                 juce::Justification::centredLeft);
    }
    drawCaps(g, caption, juce::Rectangle<float>(-10.0f, static_cast<float>(options.size() * kRow + 3), static_cast<float>(getWidth()) + 20.0f, 11.0f),
             labelFont(8.5f, 0.14f), colours::fgDim);
}

void LedSelector::mouseMove(const juce::MouseEvent& e)
{
    if (const int row = rowAt(e.getPosition()); row != hover)
    {
        hover = row;
        setMouseCursor(row >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void LedSelector::mouseExit(const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void LedSelector::mouseUp(const juce::MouseEvent& e)
{
    if (const int row = rowAt(e.getPosition()); e.mouseWasClicked() && row >= 0 && row != index)
        attachment.setValueAsCompleteGesture(static_cast<float>(row));
}

OctaveControl::OctaveControl(Apvts& state, const juce::String& transposeId)
    : parameter(*state.getParameter(transposeId)),
      attachment(parameter, [this](float plain) { value = juce::roundToInt(plain); repaint(); })
{
    setTitle("Octave");
    setPaintingIsUnclipped(true);
    attachment.sendInitialUpdate();
}

juce::Rectangle<int> OctaveControl::displayArea() const
{
    return { (getWidth() - 56) / 2, 0, 56, 26 };
}

juce::Rectangle<int> OctaveControl::buttonArea(int direction) const
{
    const int w = 30, gap = 6;
    return { getWidth() / 2 + (direction < 0 ? -gap / 2 - w : gap / 2), 34, w, 24 };
}

int OctaveControl::buttonAt(juce::Point<int> position) const
{
    return buttonArea(-1).contains(position) ? -1 : buttonArea(1).contains(position) ? 1 : 0;
}

void OctaveControl::setLearning(int direction)
{
    learning = direction;
    blink = true;
    if (direction != 0)
        startTimerHz(3);
    else
        stopTimer();
    repaint();
}

void OctaveControl::timerCallback()
{
    blink = !blink;
    repaint();
}

void OctaveControl::paint(juce::Graphics& g)
{
    // The readout: the transposition in octaves.
    const auto window = displayArea().toFloat().reduced(0.5f);
    g.setColour(colours::display);
    g.fillRoundedRectangle(window, 3.0f);
    g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::black.withAlpha(0.85f), window.getY(), juce::Colours::transparentBlack, window.getY() + 7.0f));
    g.fillRoundedRectangle(window.withHeight(7.0f), 3.0f);
    g.setColour(colours::frame);
    g.drawRoundedRectangle(window, 3.0f, 1.0f);
    const auto text = value == 0 ? juce::String("0") : (value > 0 ? "+" : juce::String::fromUTF8("\xe2\x88\x92")) + juce::String(std::abs(value));
    drawCaps(g, text, window, valueFont(14.0f, 0.04f), value == 0 ? colours::ledOn.withAlpha(0.55f) : colours::ledOn);

    for (const int direction : { -1, 1 })
    {
        const auto area = buttonArea(direction).toFloat();
        const bool listening = learning == direction;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(area.translated(0.0f, 2.0f), 3.0f);
        g.setGradientFill(juce::ColourGradient::vertical(juce::Colour(0xff3a3d43), juce::Colour(0xff17181b), area));
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(listening && blink ? colours::ledOn : hover == direction ? colours::fg.withAlpha(0.7f) : colours::frame);
        g.drawRoundedRectangle(area.reduced(0.5f), 3.0f, listening ? 1.6f : 1.0f);
        const auto c = area.getCentre();
        g.setColour(colours::fg);
        g.fillRect(juce::Rectangle<float>(10.0f, 1.8f).withCentre(c));
        if (direction > 0)
            g.fillRect(juce::Rectangle<float>(1.8f, 10.0f).withCentre(c));
    }

    const auto captionArea = juce::Rectangle<float>(-30.0f, 62.0f, static_cast<float>(getWidth()) + 60.0f, 11.0f);
    if (learning != 0)
        drawCaps(g, learning < 0 ? "Press octave down" : "Press octave up", captionArea, labelFont(8.5f, 0.12f), colours::ledOn);
    else
        drawCaps(g, "Octave", captionArea, labelFont(8.5f, 0.14f), colours::fgDim);
}

void OctaveControl::mouseMove(const juce::MouseEvent& e)
{
    if (const int over = buttonAt(e.getPosition()); over != hover)
    {
        hover = over;
        setMouseCursor(over != 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        if (over != 0 && describe)
        {
            const auto learned = describe(over);
            setTooltip(learned.isEmpty() ? juce::String("Right-click to teach this button to your keyboard's octave button")
                                         : "Your keyboard's " + learned + ". Right-click to change");
        }
        repaint();
    }
}

void OctaveControl::mouseExit(const juce::MouseEvent&)
{
    hover = 0;
    repaint();
}

void OctaveControl::mouseUp(const juce::MouseEvent& e)
{
    const int direction = buttonAt(e.getPosition());
    if (!e.mouseWasClicked() || direction == 0)
        return;
    if (e.mods.isPopupMenu())
    {
        const auto name = juce::String(direction < 0 ? "octave down" : "octave up");
        const auto learned = describe ? describe(direction) : juce::String();
        juce::PopupMenu menu;
        menu.addSectionHeader(learned.isEmpty() ? "Keyboard button: none" : "Keyboard button: " + learned);
        juce::Component::SafePointer<OctaveControl> safe(this);
        if (learning == direction)
            menu.addItem("Stop listening", [safe] { if (safe != nullptr && safe->onLearn) safe->onLearn(0); });
        else
            menu.addItem("Learn: press your keyboard's " + name + " button next", [safe, direction] { if (safe != nullptr && safe->onLearn) safe->onLearn(direction); });
        menu.addItem("Forget", learned.isNotEmpty(), false, [safe, direction] { if (safe != nullptr && safe->onForget) safe->onForget(direction); });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        return;
    }
    const float next = juce::jlimit(-2.0f, 2.0f, static_cast<float>(value + direction));
    if (juce::roundToInt(next) != value)
        attachment.setValueAsCompleteGesture(next);
}

// =================================================================================================
//  Header
// =================================================================================================
MenuButton::MenuButton() : juce::Button("Menu")
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void MenuButton::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(colours::panel);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(highlighted ? colours::fg : colours::fg.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);
    g.setColour(colours::fg);
    for (int i = -1; i <= 1; ++i)
        g.fillRect(juce::Rectangle<float>(14.0f, 1.6f).withCentre({ bounds.getCentreX(), bounds.getCentreY() + 5.0f * static_cast<float>(i) }));
}

void StatusChip::setText(const juce::String& newText)
{
    if (text != newText)
    {
        text = newText;
        repaint();
    }
}

void StatusChip::setMidiLight(bool on)
{
    if (light != on)
    {
        light = on;
        repaint();
    }
}

void StatusChip::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(colours::line);
    g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, 1.5f);
    drawLed(g, { bounds.getX() + 15.0f, bounds.getCentreY() }, 7.0f, light);
    drawCaps(g, text, bounds.withTrimmedLeft(28.0f).withTrimmedRight(10.0f), labelFont(9.5f, 0.16f), colours::fgDim,
             juce::Justification::centredLeft);
}

PresetDisplay::PresetDisplay()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle("Preset");
}

void PresetDisplay::setName(const juce::String& newName)
{
    name = newName;
    repaint();
}

void PresetDisplay::resized()
{
    auto area = getLocalBounds();
    previousArea = area.removeFromLeft(32);
    nextArea = area.removeFromRight(32);
    nameArea = area;
}

void PresetDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colours::display);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::black.withAlpha(0.9f), bounds.getY(), juce::Colours::transparentBlack, bounds.getY() + 9.0f));
    g.fillRoundedRectangle(bounds.withHeight(9.0f), 5.0f);
    g.setColour(colours::frame);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    const auto mouse = getMouseXYRelative();
    auto arrow = [&](juce::Rectangle<int> area, bool pointsLeft)
    {
        const auto c = area.toFloat().getCentre();
        const float d = pointsLeft ? -1.0f : 1.0f;
        juce::Path p;
        p.addTriangle(c.x - 3.5f * d, c.y - 4.5f, c.x - 3.5f * d, c.y + 4.5f, c.x + 4.0f * d, c.y);
        g.setColour(colours::fg.withAlpha(area.contains(mouse) ? 1.0f : 0.75f));
        g.fillPath(p);
    };
    arrow(previousArea, true);
    arrow(nextArea, false);

    // The readout: red letters with a soft bloom, like a lit display.
    const auto font = valueFont(14.5f, 0.1f);
    drawCaps(g, name, nameArea.toFloat().translated(0.0f, 0.5f).expanded(1.0f), font, colours::ledOn.withAlpha(0.25f));
    drawCaps(g, name, nameArea.toFloat(), font, colours::ledOn);
}

void PresetDisplay::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mouseWasClicked())
        return;
    if (previousArea.contains(e.getPosition()))
    {
        if (onPrevious)
            onPrevious();
    }
    else if (nextArea.contains(e.getPosition()))
    {
        if (onNext)
            onNext();
    }
    else if (onOpenList)
    {
        onOpenList();
    }
}

// =================================================================================================
//  Wheels and keys
// =================================================================================================
WheelControl::WheelControl(juce::String c, bool springsBack) : caption(std::move(c)), spring(springsBack)
{
    setTitle(caption + " wheel");
}

void WheelControl::showValue(float newValue)
{
    if (!dragging && std::abs(newValue - value) > 0.002f)
    {
        value = newValue;
        repaint();
    }
}

void WheelControl::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto captionArea = area.removeFromBottom(14.0f);
    const auto slotArea = area.withSizeKeepingCentre(26.0f, area.getHeight() - 2.0f);

    juce::ColourGradient body(juce::Colour(0xff05070a), slotArea.getX(), 0.0f, juce::Colour(0xff05070a), slotArea.getRight(), 0.0f, false);
    body.addColour(0.5, juce::Colour(0xff2a2e35));
    g.setGradientFill(body);
    g.fillRoundedRectangle(slotArea, 7.0f);
    {
        // The ribs of the wheel, which move with it.
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(slotArea, 7.0f);
        g.reduceClipRegion(clip);
        const float offset = std::fmod(value * 22.0f, 5.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        for (float y = slotArea.getY() - 5.0f - offset; y < slotArea.getBottom(); y += 5.0f)
            g.fillRect(juce::Rectangle<float>(slotArea.getX(), y, slotArea.getWidth(), 2.0f));
        // The wheel curves away at the top and bottom.
        g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::black.withAlpha(0.85f), slotArea.getY(), juce::Colours::transparentBlack, slotArea.getY() + 16.0f));
        g.fillRect(slotArea.withHeight(16.0f));
        g.setGradientFill(juce::ColourGradient::vertical(juce::Colours::transparentBlack, slotArea.getBottom() - 16.0f, juce::Colours::black.withAlpha(0.85f), slotArea.getBottom()));
        g.fillRect(slotArea.withTop(slotArea.getBottom() - 16.0f));
    }
    g.setColour(colours::line);
    g.drawRoundedRectangle(slotArea, 7.0f, 1.0f);

    // The mark: where the wheel is.
    const float travel = slotArea.getHeight() - 16.0f;
    const float position = spring ? 0.5f - 0.5f * value : 1.0f - value;
    const float y = slotArea.getY() + 8.0f + travel * position;
    g.setColour(colours::ledOn.withAlpha(0.35f));
    g.fillRoundedRectangle(juce::Rectangle<float>(26.0f, 9.0f).withCentre({ slotArea.getCentreX(), y }), 3.0f);
    g.setColour(colours::ledOn);
    g.fillRoundedRectangle(juce::Rectangle<float>(20.0f, 4.0f).withCentre({ slotArea.getCentreX(), y }), 1.0f);

    drawCaps(g, caption, captionArea.expanded(20.0f, 0.0f), labelFont(8.5f, 0.14f), colours::fgDim);
}

void WheelControl::mouseDown(const juce::MouseEvent&)
{
    dragging = true;
    dragStart = value;
}

void WheelControl::mouseDrag(const juce::MouseEvent& e)
{
    const float range = spring ? 2.0f : 1.0f;
    // Slow on purpose: a mouse is far twitchier than a thumb on a real wheel.
    const float moved = -static_cast<float>(e.getDistanceFromDragStartY()) / 200.0f * range;
    value = std::clamp(dragStart + moved, spring ? -1.0f : 0.0f, 1.0f);
    if (onMove)
        onMove(value);
    repaint();
}

void WheelControl::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    if (spring)
    {
        value = 0.0f;
        if (onMove)
            onMove(0.0f);
        repaint();
    }
}

SeerKeyboard::SeerKeyboard(juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setAvailableRange(36, 84);
    setOctaveForMiddleC(4);
    setScrollButtonsVisible(false);
    setBlackNoteWidthProportion(0.62f);
    setBlackNoteLengthProportion(0.6f);
    setVelocity(0.7f, true); // lower on a key is louder
    setKeyPressBaseOctave(4); // the computer keyboard's A key plays C3
    setOpaque(false);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setTitle("On-screen keyboard");
}

void SeerKeyboard::resized()
{
    setKeyWidth(static_cast<float>(getWidth()) / 29.0f); // 29 white keys from C2 to C6
    juce::MidiKeyboardComponent::resized();
}

void SeerKeyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    const auto key = area.withTrimmedRight(1.0f);
    juce::Path shape;
    shape.addRoundedRectangle(key.getX(), key.getY(), key.getWidth(), key.getHeight(), 3.0f, 3.0f, false, false, true, true);
    g.setGradientFill(isDown ? juce::ColourGradient::vertical(juce::Colour(0xfff6cfd0), juce::Colour(0xffe2686d), key)
                             : juce::ColourGradient::vertical(juce::Colour(0xfff1ead9), juce::Colour(0xffcbc2ad), key));
    g.fillPath(shape);
    if (isOver && !isDown)
    {
        g.setColour(colours::ledOn.withAlpha(0.12f));
        g.fillPath(shape);
    }
    if (note % 12 == 0)
    {
        g.setColour(juce::Colour(0xff56606a));
        g.setFont(valueFont(8.5f, 0.0f));
        g.drawText(juce::MidiMessage::getMidiNoteName(note, true, true, 4), key.withTrimmedBottom(4.0f), juce::Justification::centredBottom, false);
    }
}

void SeerKeyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    juce::Path shape;
    shape.addRoundedRectangle(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 2.0f, 2.0f, false, false, true, true);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(area.translated(0.0f, 2.0f).expanded(0.5f, 0.0f), 2.0f);
    g.setGradientFill(isDown ? juce::ColourGradient::vertical(juce::Colour(0xff8a2329), juce::Colour(0xff4a0f13), area)
                             : juce::ColourGradient::vertical(juce::Colour(0xff1a1f26), juce::Colour(0xff07090c), area));
    g.fillPath(shape);
    if (isOver && !isDown)
    {
        g.setColour(colours::ledOn.withAlpha(0.18f));
        g.fillPath(shape);
    }
}

// =================================================================================================
//  The light behind the panels, the grain and the wood
// =================================================================================================
LightField::LightField() : canvas(juce::Image::RGB, 160, 100, true)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(true);
    advance(0.0);
}

void LightField::setLevels(const std::array<float, numLights>& newLevels, float newSpeed)
{
    levels = newLevels;
    speed = newSpeed;
}

void LightField::noteOn(float position01)
{
    if (glows.size() < 24)
        glows.push_back({ position01, 1.0f });
}

void LightField::advance(double seconds)
{
    time += seconds * speed;
    for (auto& glow : glows)
        glow.life -= static_cast<float>(seconds) * 0.8f;
    std::erase_if(glows, [](const Glow& glow) { return glow.life <= 0.0f; });

    // Painted small and stretched over the window: the stretching is what makes it soft.
    juce::Graphics g(canvas);
    g.fillAll(colours::night);
    const float w = static_cast<float>(canvas.getWidth()), h = static_cast<float>(canvas.getHeight());
    auto blob = [&](float x, float y, float radius, juce::Colour colour, float alpha)
    {
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(std::clamp(alpha, 0.0f, 1.0f)), x, y, colour.withAlpha(0.0f), x + radius, y, true));
        g.fillRect(juce::Rectangle<float>(x - radius, y - radius, 2.0f * radius, 2.0f * radius));
    };
    struct Spot
    {
        juce::Colour colour;
        float fx, fy, radius, base, gain;
    };
    const Spot spots[numLights] = {
        { colours::osc2, 0.7f, 0.5f, 74.0f, 0.16f, 0.44f },    { colours::lowPass, 0.5f, 0.9f, 84.0f, 0.14f, 0.60f },
        { colours::effects, 0.9f, 0.6f, 78.0f, 0.0f, 0.50f },  { colours::lfo, 0.6f, 0.8f, 68.0f, 0.0f, 0.46f },
        { colours::polyMod, 0.4f, 0.7f, 62.0f, 0.14f, 0.42f },
    };
    for (int i = 0; i < numLights; ++i)
    {
        const auto& s = spots[i];
        const float t = static_cast<float>(time);
        const float x = w * (0.5f + 0.42f * std::sin(t * s.fx + static_cast<float>(i) * 2.1f));
        const float y = h * (0.45f + 0.4f * std::cos(t * s.fy + static_cast<float>(i) * 1.3f));
        blob(x, y, s.radius, s.colour, s.base + s.gain * levels[static_cast<size_t>(i)]);
    }
    for (const auto& glow : glows)
        blob(w * (0.08f + 0.84f * glow.x), h * (0.92f - (1.0f - glow.life) * 0.25f), 34.0f, colours::mixer, 0.45f * glow.life);
    repaint();
}

void LightField::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(canvas, getLocalBounds().toFloat());
    // Darker towards the edges.
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, b.getCentreX(), b.getHeight() * 0.4f, juce::Colour(0xe6020408), b.getX(), b.getBottom(), true));
    g.fillRect(b);
}

Overlay::Overlay() : grain(juce::Image::ARGB, 192, 192, true)
{
    setInterceptsMouseClicks(false, false);
    setBufferedToImage(true); // it never changes
    juce::Random random(20261111);
    juce::Image::BitmapData data(grain, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < grain.getHeight(); ++y)
        for (int x = 0; x < grain.getWidth(); ++x)
        {
            const bool lightSpeck = random.nextBool();
            data.setPixelColour(x, y, (lightSpeck ? juce::Colours::white : juce::Colours::black).withAlpha(random.nextFloat() * (lightSpeck ? 0.05f : 0.10f)));
        }
}

void Overlay::paint(juce::Graphics& g)
{
    g.setTiledImageFill(grain, 0, 0, 1.0f);
    g.fillAll();

    // The wooden end cheeks.
    const auto bounds = getLocalBounds().toFloat();
    for (const bool leftSide : { true, false })
    {
        const auto cheek = leftSide ? bounds.withWidth(static_cast<float>(kCheek)) : bounds.withLeft(bounds.getRight() - static_cast<float>(kCheek));
        juce::ColourGradient wood(juce::Colour(0xff2a160c), cheek.getX(), 0.0f, juce::Colour(0xff46240f), cheek.getRight(), 0.0f, false);
        wood.addColour(0.3, juce::Colour(0xff7a4722));
        wood.addColour(0.55, juce::Colour(0xff9a5c2c));
        g.setGradientFill(wood);
        g.fillRect(cheek);
        // The grain of the wood: fine vertical lines, a little irregular.
        juce::Random random(leftSide ? 11 : 23);
        for (float x = cheek.getX(); x < cheek.getRight(); x += 1.0f + random.nextFloat() * 2.5f)
        {
            g.setColour((random.nextBool() ? juce::Colours::black : juce::Colour(0xffffdcaa)).withAlpha(0.05f + 0.12f * random.nextFloat()));
            g.fillRect(juce::Rectangle<float>(x, cheek.getY(), 1.0f, cheek.getHeight()));
        }
        g.setColour(juce::Colours::black);
        g.fillRect(leftSide ? cheek.withLeft(cheek.getRight() - 1.0f) : cheek.withWidth(1.0f));
        g.setColour(juce::Colour(0xffffe6be).withAlpha(0.25f));
        g.fillRect(cheek.withHeight(1.0f));
    }
}
} // namespace seer::ui
