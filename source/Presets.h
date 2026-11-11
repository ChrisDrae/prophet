#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <vector>

// =================================================================================================
//  Presets — the factory sounds, user presets saved as files, and the glide between them.
//
//  A preset sets every *sound* parameter. Parameters it doesn't list go back to their defaults, so
//  a preset always sounds the same, whatever was loaded before. Master volume and tune, the bend
//  range, transpose, the voice count and the quality are the player's settings, and presets leave
//  them alone (params::isPlayerSetting).
//
//  Switching glides the knobs to their new places over about a quarter of a second, so the sound
//  morphs instead of jumping. Choices and switches change at once.
//
//  User presets are XML files in Documents/Seer/Presets, grouped into banks: every folder in
//  there is a bank, named after the folder. Copy a folder to another computer and the bank goes
//  with it. Message thread only.
// =================================================================================================
namespace seer
{
struct Preset
{
    juce::String name;
    juce::String bank;                    ///< "Factory", or the name of a user bank (a folder)
    juce::String category;                ///< factory presets: "Lead", "Bass", "Pad" or "Keys"
    std::map<juce::String, float> values; ///< parameter ID -> plain value (a choice's index)
    juce::File file;                      ///< empty for factory presets
};

/// The factory presets: leads first, then basses, then the rest. "Init" (the defaults) is last.
const std::vector<Preset>& factoryPresets();

class PresetManager final : private juce::Timer
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);
    ~PresetManager() override;

    int getNumPresets() const noexcept { return static_cast<int>(presets.size()); }
    int getNumFactoryPresets() const noexcept;
    const Preset& getPreset(int index) const { return presets[static_cast<size_t>(index)]; }
    int getCurrentIndex() const noexcept { return current; }
    juce::String getCurrentName() const;

    /// Loads a preset; with `glide`, the knobs move there over about a quarter of a second.
    void load(int index, bool glide = true);
    void next() { load((current + 1) % getNumPresets()); }
    void previous() { load((current + getNumPresets() - 1) % getNumPresets()); }

    /// Saves the current sound into a user bank (overwrites a preset of the same name there).
    /// The bank is created if it doesn't exist yet.
    juce::File saveUserPreset(const juce::String& name, const juce::String& bank = kDefaultBank);
    /// Deletes a user preset's file. Factory presets can't be deleted.
    bool deleteUserPreset(int index);
    /// The user banks, in alphabetical order (also empty ones).
    juce::StringArray getUserBanks() const;
    /// Makes an empty bank (a folder). Returns false if the name can't be a folder name.
    bool createBank(const juce::String& name);
    static inline const juce::String kFactoryBank { "Factory" }, kDefaultBank { "My Sounds" };
    static juce::File userFolder();
    void rescanUserPresets();

    /// Restores the current preset's name after the host loads a session (no parameters change).
    void restoreSelection(const juce::String& name);

    /// Called (on the message thread) when the current preset changes.
    std::function<void()> onPresetChanged;

private:
    void timerCallback() override;
    void finishGlide();

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> presets;
    int current = 0;

    struct Move
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float from = 0.0f, to = 0.0f; // normalised
    };
    std::vector<Move> glide;
    double glideStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
} // namespace seer
