#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>

// =================================================================================================
//  OctaveButtons — a MIDI keyboard's octave buttons, when they send a message of their own.
//
//  Most keyboards shift the notes they send when their octave buttons are pressed, and then Seer
//  needs nothing. Some (or some modes of some, such as a DAW mode) send a control change or a
//  program change instead. For those, the window can "learn" each button: the next button-like
//  message that arrives becomes the binding for octave down or up.
//
//  A binding is a message type and channel (the status byte) plus its first data byte, packed as
//  status | data1 << 8 | 1 << 16 (so 0 means "none"). A control change counts as a press when its
//  value is above zero; its release (value 0) is swallowed too. The mod wheel (CC 1), the sustain
//  pedal (CC 64) and the channel-mode messages (CC 120-127) are never learned.
//
//  [RT] handle() runs on the audio thread; learn() and the bindings may be set from any thread.
// =================================================================================================
namespace seer
{
class OctaveButtons
{
public:
    enum class Result
    {
        none,       ///< not a button: pass the message on
        learned,    ///< it just became a binding
        octaveDown, ///< a press of the down button
        octaveUp,   ///< a press of the up button
        swallowed   ///< a release of a bound button
    };

    /// Starts listening for the button for `direction` (-1 down, +1 up); 0 stops listening.
    void learn(int direction) noexcept { learning.store(direction, std::memory_order_relaxed); }
    int getLearning() const noexcept { return learning.load(std::memory_order_relaxed); }

    uint32_t getBinding(int direction) const noexcept { return (direction < 0 ? down : up).load(std::memory_order_relaxed); }
    void setBinding(int direction, uint32_t binding) noexcept { (direction < 0 ? down : up).store(binding, std::memory_order_relaxed); }

    /// [RT] Looks at one incoming message.
    Result handle(const juce::MidiMessage& m) noexcept
    {
        const uint32_t key = keyOf(m);
        if (key == 0)
            return Result::none;
        const bool press = !m.isController() || m.getControllerValue() > 0;

        if (const int direction = learning.load(std::memory_order_relaxed); direction != 0)
        {
            if (!press)
                return Result::none;
            setBinding(direction, key);
            if (getBinding(-direction) == key) // one message can't be both buttons
                setBinding(-direction, 0);
            learning.store(0, std::memory_order_relaxed);
            return Result::learned;
        }
        if (key == down.load(std::memory_order_relaxed))
            return press ? Result::octaveDown : Result::swallowed;
        if (key == up.load(std::memory_order_relaxed))
            return press ? Result::octaveUp : Result::swallowed;
        return Result::none;
    }

    /// "CC 102 on channel 1", "Program 5 on channel 1", or an empty string for no binding.
    static juce::String describe(uint32_t binding)
    {
        if (binding == 0)
            return {};
        const int status = static_cast<int>(binding & 0xff), data = static_cast<int>((binding >> 8) & 0x7f);
        const juce::String channel = " on channel " + juce::String((status & 0x0f) + 1);
        return ((status & 0xf0) == 0xb0 ? "CC " : "Program ") + juce::String(data) + channel;
    }

private:
    static uint32_t keyOf(const juce::MidiMessage& m) noexcept
    {
        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 1 || cc == 64 || cc >= 120)
                return 0;
        }
        else if (!m.isProgramChange())
        {
            return 0;
        }
        const auto* bytes = m.getRawData();
        return static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8 | 1u << 16;
    }

    std::atomic<int> learning { 0 };
    std::atomic<uint32_t> down { 0 }, up { 0 };
};
} // namespace seer
