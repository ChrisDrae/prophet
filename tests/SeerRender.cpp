// =================================================================================================
//  SeerRender — plays a phrase through every factory preset into WAV files, and prints their
//  levels. For listening to the presets without a host, and for matching their loudness.
//
//      SeerRender.exe <output folder> [--set <id>=<value>]...
//
//  Leads get a legato line (with the mod wheel and some pressure towards the end), basses a riff,
//  everything else two chords.
// =================================================================================================
#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;
constexpr double kBeat = 0.6; // seconds: 100 bpm

struct Note
{
    double start, length; // in beats
    int note;
    float velocity;
};

// The lead line overlaps some notes (legato: they glide) and separates others.
const std::vector<Note> kLead = { { 0, 1.1, 62, 0.8f },  { 1, 0.6, 65, 0.7f },  { 1.5, 1.4, 69, 0.9f }, { 3, 0.45, 67, 0.6f },
                                  { 3.5, 0.6, 65, 0.6f }, { 4, 2.1, 72, 1.0f },  { 6, 0.6, 69, 0.7f },   { 6.5, 0.6, 70, 0.7f },
                                  { 7, 3, 69, 0.85f } };
const std::vector<Note> kBass = { { 0, 0.75, 38, 1.0f },  { 1, 0.25, 38, 0.6f }, { 1.5, 0.5, 41, 0.8f }, { 2, 0.75, 43, 0.9f },
                                  { 3, 0.5, 36, 0.8f },   { 3.5, 0.6, 37, 0.7f }, { 4, 0.75, 38, 1.0f },  { 5, 0.25, 50, 0.6f },
                                  { 5.5, 0.5, 45, 0.8f }, { 6, 2, 38, 0.9f } };
const std::vector<Note> kChords = { { 0, 3.5, 50, 0.7f }, { 0, 3.5, 57, 0.7f }, { 0, 3.5, 60, 0.7f }, { 0, 3.5, 64, 0.7f }, { 0, 3.5, 65, 0.7f },
                                    { 4, 4, 48, 0.8f },   { 4, 4, 55, 0.8f },   { 4, 4, 59, 0.8f },   { 4, 4, 62, 0.8f },   { 4, 4, 64, 0.8f } };

bool render(SeerProcessor& processor, int index, const juce::File& folder)
{
    const auto& preset = seer::factoryPresets()[static_cast<size_t>(index)];
    const bool lead = preset.category == "Lead";
    const auto& phrase = lead ? kLead : preset.category == "Bass" ? kBass : kChords;

    processor.prepareToPlay(kRate, kBlock); // also silences the previous preset's tail

    double end = 0.0;
    juce::MidiBuffer events;
    for (const auto& n : phrase)
    {
        events.addEvent(juce::MidiMessage::noteOn(1, n.note, n.velocity), static_cast<int>(n.start * kBeat * kRate));
        events.addEvent(juce::MidiMessage::noteOff(1, n.note), static_cast<int>((n.start + n.length) * kBeat * kRate));
        end = std::max(end, (n.start + n.length) * kBeat);
    }
    if (lead)
    {
        // The last long note: the mod wheel comes up, then pressure.
        for (int step = 0; step <= 20; ++step)
        {
            const int at = static_cast<int>((7.5 + 0.06 * step) * kBeat * kRate);
            events.addEvent(juce::MidiMessage::controllerEvent(1, 1, step * 2), at);
            events.addEvent(juce::MidiMessage::channelPressureChange(1, step * 5), at + static_cast<int>(0.8 * kBeat * kRate));
        }
        events.addEvent(juce::MidiMessage::controllerEvent(1, 1, 0), static_cast<int>(end * kRate));
        events.addEvent(juce::MidiMessage::channelPressureChange(1, 0), static_cast<int>(end * kRate));
    }

    const int total = static_cast<int>((end + 2.5) * kRate);
    juce::AudioBuffer<float> out(2, total), block(2, kBlock);
    for (int position = 0; position < total; position += kBlock)
    {
        const int n = std::min(kBlock, total - position);
        juce::MidiBuffer slice;
        slice.addEvents(events, position, n, -position);
        juce::AudioBuffer<float> view(block.getArrayOfWritePointers(), 2, n);
        processor.processBlock(view, slice);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom(ch, position, view, ch, 0, n);
    }

    // Loudness while notes sound (the tail would drag an average down).
    const int playing = static_cast<int>(end * kRate);
    const float peak = out.getMagnitude(0, total);
    const float l = out.getRMSLevel(0, 0, playing), r = out.getRMSLevel(1, 0, playing);
    const float rms = std::sqrt(0.5f * (l * l + r * r));
    std::cout << juce::String(index).paddedLeft(' ', 2) << "  " << preset.category.paddedRight(' ', 5) << preset.name.paddedRight(' ', 16) << "peak "
              << juce::String(juce::Decibels::gainToDecibels(peak), 1).paddedLeft(' ', 6) << " dB   rms "
              << juce::String(juce::Decibels::gainToDecibels(rms), 1).paddedLeft(' ', 6) << " dB\n";

    const auto file = folder.getChildFile(juce::String(index + 1).paddedLeft('0', 2) + " " + preset.name + ".wav");
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    juce::WavAudioFormat wav;
    const auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(kRate).withNumChannels(2).withBitsPerSample(24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(out, 0, total);
}
} // namespace

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "renders");
    if (!folder.createDirectory())
    {
        std::cerr << "Can't create " << folder.getFullPathName() << "\n";
        return 1;
    }
    juce::StringPairArray settings;
    for (int i = 2; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == "--set")
        {
            const juce::String pair(argv[++i]);
            settings.set(pair.upToFirstOccurrenceOf("=", false, false), pair.fromFirstOccurrenceOf("=", false, false));
        }

    bool ok = true;
    {
        SeerProcessor processor;
        for (int i = 0; i < processor.getNumPrograms(); ++i)
        {
            processor.getPresets().load(i, false);
            for (const auto& id : settings.getAllKeys())
                if (auto* parameter = processor.getState().getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(settings[id].getFloatValue()));
            ok = render(processor, i, folder) && ok;
        }
    }
    std::cout << (ok ? "Wrote " : "FAILED writing ") << folder.getFullPathName() << "\n";
    return ok ? 0 : 1;
}
