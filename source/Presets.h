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
//  User presets are XML files in Documents/Seer/Presets. Message thread only.
// =================================================================================================
namespace seer
{
struct Preset
{
    juce::String name;
    juce::String category;                ///< "Lead", "Bass", "Pad", "Keys", or "User"
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

    /// Saves the current sound as a user preset (overwrites one with the same name).
    juce::File saveUserPreset(const juce::String& name);
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
