#pragma once

#include "../Presets.h"
#include "SeerLookAndFeel.h"

#include <functional>

// =================================================================================================
//  PresetBrowser — the preset list as a panel that opens under the preset readout.
//
//    left    groups: the factory sounds by kind (All, Leads, Basses, Pads, Keys), then your banks
//    right   the presets of the chosen group, in columns; click one to hear it, the panel stays open
//    bottom  Save, New bank, Open folder
//
//  A bank is a folder in Documents/Seer/Presets. Right-click one of your presets to delete it.
// =================================================================================================
namespace seer::ui
{
class PresetBrowser final : public juce::Component
{
public:
    static constexpr int kWidth = 780, kHeight = 452;

    explicit PresetBrowser(PresetManager& presets);

    /// Rebuilds the lists (after a save, a delete, or a new bank) and shows the current preset's group.
    void refresh(bool followCurrent = true);
    /// The user bank that is selected, or the default bank when a factory group is.
    juce::String getSelectedUserBank() const;

    std::function<void()> onClose, onSave, onNewBank;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    struct Group
    {
        juce::String label;
        juce::String bank;     ///< empty for a heading
        juce::String category; ///< factory groups: empty = all
        bool heading = false;
    };
    enum class Action
    {
        none,
        save,
        newBank,
        openFolder,
        close
    };

    juce::Rectangle<int> groupRow(int index) const;
    juce::Rectangle<int> presetCell(int position) const; ///< position counts from the first visible row
    juce::Rectangle<int> actionArea(Action) const;
    int rowsVisible() const;
    void selectGroup(int index);
    void clampScroll();

    PresetManager& presets;
    std::vector<Group> groups;
    std::vector<int> shown; ///< preset indices of the selected group
    int selectedGroup = 1, scrollRows = 0;
    int hoverGroup = -1, hoverPreset = -1;
    Action hoverAction = Action::none;
};
} // namespace seer::ui
