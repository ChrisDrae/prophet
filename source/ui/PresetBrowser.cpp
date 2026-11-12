#include "PresetBrowser.h"

namespace seer::ui
{
namespace
{
constexpr int kPad = 14, kHeader = 40, kFooter = 46, kGroupsWidth = 190, kRow = 27, kColumns = 3;
}

PresetBrowser::PresetBrowser(PresetManager& p) : presets(p)
{
    setWantsKeyboardFocus(true);
    setTitle("Presets");
    refresh();
}

void PresetBrowser::refresh(bool followCurrent)
{
    const auto previous = selectedGroup >= 0 && selectedGroup < static_cast<int>(groups.size()) ? groups[static_cast<size_t>(selectedGroup)] : Group {};

    groups.clear();
    groups.push_back({ "Factory", {}, {}, true });
    groups.push_back({ "All", PresetManager::kFactoryBank, {}, false });
    groups.push_back({ "Leads", PresetManager::kFactoryBank, "Lead", false });
    groups.push_back({ "Basses", PresetManager::kFactoryBank, "Bass", false });
    groups.push_back({ "Pads", PresetManager::kFactoryBank, "Pad", false });
    groups.push_back({ "Keys", PresetManager::kFactoryBank, "Keys", false });
    groups.push_back({ "Your banks", {}, {}, true });
    for (const auto& bank : presets.getUserBanks())
        groups.push_back({ bank, bank, {}, false });

    // Which group to show: the one the current preset is in, or the one that was showing.
    int pick = 1;
    const auto& current = presets.getPreset(presets.getCurrentIndex());
    for (int i = 0; i < static_cast<int>(groups.size()); ++i)
    {
        const auto& g = groups[static_cast<size_t>(i)];
        if (g.heading)
            continue;
        const bool matchesCurrent = g.bank == current.bank && (g.bank != PresetManager::kFactoryBank || g.category == current.category);
        const bool matchesPrevious = g.bank == previous.bank && g.category == previous.category;
        if (followCurrent ? matchesCurrent : matchesPrevious)
            pick = i;
    }
    selectGroup(pick);
}

void PresetBrowser::showBank(const juce::String& bank)
{
    refresh(false);
    for (int i = 0; i < static_cast<int>(groups.size()); ++i)
        if (!groups[static_cast<size_t>(i)].heading && groups[static_cast<size_t>(i)].bank == bank)
            selectGroup(i);
}

juce::String PresetBrowser::getSelectedUserBank() const
{
    const auto& g = groups[static_cast<size_t>(selectedGroup)];
    return g.bank.isEmpty() || g.bank == PresetManager::kFactoryBank ? PresetManager::kDefaultBank : g.bank;
}

void PresetBrowser::selectGroup(int index)
{
    selectedGroup = index;
    const auto& g = groups[static_cast<size_t>(index)];
    shown.clear();
    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto& preset = presets.getPreset(i);
        if (preset.bank == g.bank && (g.category.isEmpty() || preset.category == g.category))
            shown.push_back(i);
    }
    scrollRows = 0;
    // Bring the current preset into view.
    for (int position = 0; position < static_cast<int>(shown.size()); ++position)
        if (shown[static_cast<size_t>(position)] == presets.getCurrentIndex())
            scrollRows = std::max(0, position / kColumns - rowsVisible() + 1);
    clampScroll();
    repaint();
}

int PresetBrowser::rowsVisible() const
{
    return (kHeight - kHeader - kFooter - 8) / kRow;
}

void PresetBrowser::clampScroll()
{
    const int rows = (static_cast<int>(shown.size()) + kColumns - 1) / kColumns;
    scrollRows = std::clamp(scrollRows, 0, std::max(0, rows - rowsVisible()));
}

juce::Rectangle<int> PresetBrowser::groupRow(int index) const
{
    return { kPad, kHeader + 4 + index * kRow, kGroupsWidth - kPad, kRow };
}

juce::Rectangle<int> PresetBrowser::presetCell(int position) const
{
    const int left = kGroupsWidth + 14, width = (kWidth - left - kPad) / kColumns;
    return { left + (position % kColumns) * width, kHeader + 4 + (position / kColumns) * kRow, width - 6, kRow };
}

juce::Rectangle<int> PresetBrowser::actionArea(Action action) const
{
    const int y = kHeight - kFooter + 10, h = 26;
    switch (action)
    {
        case Action::save: return { kGroupsWidth + 14, y, 190, h };
        case Action::newBank: return { kPad, y, 110, h };
        case Action::openFolder: return { kGroupsWidth + 214, y, 130, h };
        case Action::close: return { kWidth - 40, 8, 26, 26 };
        case Action::none: break;
    }
    return {};
}

void PresetBrowser::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0a0b0d));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours::frame);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Heading, and the rules that divide the panel.
    drawCaps(g, "Presets", juce::Rectangle<float>(static_cast<float>(kPad), 10.0f, 200.0f, 20.0f), labelFont(12.0f, 0.16f), colours::fg, juce::Justification::centredLeft);
    g.setColour(colours::line);
    g.fillRect(juce::Rectangle<float>(1.0f, static_cast<float>(kHeader) - 2.0f, bounds.getWidth() - 1.0f, 1.0f));
    g.fillRect(juce::Rectangle<float>(1.0f, static_cast<float>(kHeight - kFooter), bounds.getWidth() - 1.0f, 1.0f));
    g.fillRect(juce::Rectangle<float>(static_cast<float>(kGroupsWidth), static_cast<float>(kHeader) - 2.0f, 1.0f, static_cast<float>(kHeight - kFooter - kHeader) + 2.0f));

    // Groups.
    for (int i = 0; i < static_cast<int>(groups.size()); ++i)
    {
        const auto& group = groups[static_cast<size_t>(i)];
        const auto row = groupRow(i).toFloat();
        if (row.getBottom() > static_cast<float>(kHeight - kFooter))
            break;
        if (group.heading)
        {
            drawCaps(g, group.label, row.withTrimmedTop(6.0f), labelFont(8.5f, 0.18f), colours::fgDim, juce::Justification::centredLeft);
            continue;
        }
        const bool selected = i == selectedGroup;
        if (selected || i == hoverGroup)
        {
            g.setColour(colours::fg.withAlpha(selected ? 0.12f : 0.06f));
            g.fillRoundedRectangle(row.reduced(0.0f, 1.5f), 3.0f);
        }
        drawLed(g, { row.getX() + 11.0f, row.getCentreY() }, 7.0f, selected);
        drawCaps(g, group.label, row.withTrimmedLeft(26.0f), labelFont(10.5f, 0.12f), selected ? colours::fg : colours::fg.withAlpha(0.8f), juce::Justification::centredLeft);
    }
    if (presets.getUserBanks().isEmpty())
    {
        g.setColour(colours::fgDim);
        g.setFont(valueFont(11.0f, 0.0f));
        g.drawFittedText("None yet. Save a sound, or make a bank.", groupRow(static_cast<int>(groups.size())).withHeight(40).reduced(2, 0), juce::Justification::topLeft, 2);
    }

    // Presets of the chosen group.
    const int first = scrollRows * kColumns, visible = rowsVisible() * kColumns;
    for (int position = 0; position < visible && first + position < static_cast<int>(shown.size()); ++position)
    {
        const int index = shown[static_cast<size_t>(first + position)];
        const auto cell = presetCell(position).toFloat();
        const bool current = index == presets.getCurrentIndex();
        if (current || index == hoverPreset)
        {
            g.setColour(current ? colours::ledOn.withAlpha(0.16f) : colours::fg.withAlpha(0.07f));
            g.fillRoundedRectangle(cell.reduced(0.0f, 1.5f), 3.0f);
        }
        if (current)
            drawLed(g, { cell.getX() + 11.0f, cell.getCentreY() }, 7.0f, true);
        g.setColour(current ? colours::fg : colours::fg.withAlpha(0.86f));
        g.setFont(valueFont(13.0f, 0.02f));
        g.drawText(presets.getPreset(index).name, cell.withTrimmedLeft(26.0f), juce::Justification::centredLeft, true);
    }
    if (shown.empty())
    {
        g.setColour(colours::fgDim);
        g.setFont(valueFont(12.5f, 0.0f));
        g.drawText("This bank is empty. \"Save current sound\" puts one here.", presetCell(0).withWidth(kWidth - kGroupsWidth - 40).toFloat(), juce::Justification::centredLeft, true);
    }
    // A scroll mark when there is more than fits.
    const int rows = (static_cast<int>(shown.size()) + kColumns - 1) / kColumns;
    if (rows > rowsVisible())
    {
        const float track = static_cast<float>(rowsVisible() * kRow), top = static_cast<float>(kHeader + 4);
        const float size = track * static_cast<float>(rowsVisible()) / static_cast<float>(rows);
        g.setColour(colours::frame);
        g.fillRoundedRectangle(static_cast<float>(kWidth) - 8.0f, top + (track - size) * static_cast<float>(scrollRows) / static_cast<float>(rows - rowsVisible()), 3.0f, size, 1.5f);
    }

    // Actions.
    auto button = [&](Action action, const juce::String& text)
    {
        const auto area = actionArea(action).toFloat();
        g.setColour(colours::slot);
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(hoverAction == action ? colours::fg.withAlpha(0.8f) : colours::frame);
        g.drawRoundedRectangle(area.reduced(0.5f), 3.0f, 1.0f);
        drawCaps(g, text, area, labelFont(9.5f, 0.12f), colours::fg);
    };
    button(Action::save, "Save current sound...");
    button(Action::newBank, "New bank...");
    button(Action::openFolder, "Open folder");
    const auto x = actionArea(Action::close).toFloat().reduced(8.0f);
    g.setColour(hoverAction == Action::close ? colours::fg : colours::fgDim);
    g.drawLine(x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.6f);
    g.drawLine(x.getX(), x.getBottom(), x.getRight(), x.getY(), 1.6f);
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    int group = -1, preset = -1;
    Action action = Action::none;
    for (int i = 0; i < static_cast<int>(groups.size()); ++i)
        if (!groups[static_cast<size_t>(i)].heading && groupRow(i).contains(e.getPosition()))
            group = i;
    const int first = scrollRows * kColumns, visible = rowsVisible() * kColumns;
    for (int position = 0; position < visible && first + position < static_cast<int>(shown.size()); ++position)
        if (presetCell(position).contains(e.getPosition()))
            preset = shown[static_cast<size_t>(first + position)];
    for (auto a : { Action::save, Action::newBank, Action::openFolder, Action::close })
        if (actionArea(a).contains(e.getPosition()))
            action = a;
    if (group != hoverGroup || preset != hoverPreset || action != hoverAction)
    {
        hoverGroup = group;
        hoverPreset = preset;
        hoverAction = action;
        repaint();
    }
    setMouseCursor(group >= 0 || preset >= 0 || action != Action::none ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PresetBrowser::mouseExit(const juce::MouseEvent&)
{
    hoverGroup = hoverPreset = -1;
    hoverAction = Action::none;
    repaint();
}

void PresetBrowser::mouseUp(const juce::MouseEvent& e)
{
    mouseMove(e);
    if (!e.mouseWasClicked())
        return;
    if (hoverGroup >= 0)
    {
        selectGroup(hoverGroup);
    }
    else if (hoverPreset >= 0)
    {
        const int index = hoverPreset;
        if (e.mods.isPopupMenu() && presets.getPreset(index).file != juce::File())
        {
            juce::PopupMenu menu;
            juce::Component::SafePointer<PresetBrowser> safe(this);
            menu.addItem("Delete \"" + presets.getPreset(index).name + "\"", [safe, index]
            {
                if (safe != nullptr && safe->presets.deleteUserPreset(index))
                    safe->refresh(false);
            });
            menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(getTopLevelComponent()));
        }
        else
        {
            presets.load(index);
            repaint();
        }
    }
    else if (hoverAction == Action::save && onSave)
        onSave();
    else if (hoverAction == Action::newBank && onNewBank)
        onNewBank();
    else if (hoverAction == Action::openFolder)
    {
        auto folder = PresetManager::userFolder();
        folder.createDirectory();
        folder.revealToUser();
    }
    else if (hoverAction == Action::close && onClose)
        onClose();
}

void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    scrollRows += wheel.deltaY > 0.0f ? -1 : wheel.deltaY < 0.0f ? 1 : 0;
    clampScroll();
    mouseMove(e);
    repaint();
}

bool PresetBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}
} // namespace seer::ui
