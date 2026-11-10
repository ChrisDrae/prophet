// =================================================================================================
//  SeerRender — plays test phrases through the engine into WAV files, and prints their levels.
//  For listening to the engine without a host, and for checking loudness.
//
//      SeerRender.exe <output folder>
// =================================================================================================
#include "engine/SynthEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <functional>
#include <iostream>

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Note
{
    double start, length; // in beats
    int note;
    float velocity;
};

/// A legato lead line, a bass riff and a chord, at 100 bpm.
const std::vector<Note> kLead = { { 0, 1, 62, 0.8f },   { 1, 0.5, 65, 0.7f },   { 1.5, 1.5, 69, 0.9f }, { 3, 0.5, 67, 0.6f },
                                  { 3.5, 0.5, 65, 0.6f }, { 4, 2, 72, 1.0f },     { 6, 0.5, 69, 0.7f },   { 6.5, 0.5, 70, 0.7f },
                                  { 7, 3, 69, 0.85f } };
const std::vector<Note> kBass = { { 0, 0.75, 38, 1.0f }, { 1, 0.25, 38, 0.6f }, { 1.5, 0.5, 41, 0.8f }, { 2, 0.75, 43, 0.9f },
                                  { 3, 0.5, 36, 0.8f },  { 3.5, 0.5, 37, 0.7f }, { 4, 0.75, 38, 1.0f },  { 5, 0.25, 50, 0.6f },
                                  { 5.5, 0.5, 45, 0.8f }, { 6, 2, 38, 0.9f } };
const std::vector<Note> kChords = { { 0, 3.5, 50, 0.7f }, { 0, 3.5, 57, 0.7f }, { 0, 3.5, 60, 0.7f }, { 0, 3.5, 64, 0.7f }, { 0, 3.5, 65, 0.7f },
                                    { 4, 4, 48, 0.8f },   { 4, 4, 55, 0.8f },   { 4, 4, 59, 0.8f },   { 4, 4, 62, 0.8f },   { 4, 4, 64, 0.8f } };

struct Take
{
    const char* name;
    const std::vector<Note>* phrase;
    std::function<void(seer::EngineParams&)> setup;
    /// Called every block with the time in seconds: for moving a control while playing.
    std::function<void(seer::EngineParams&, seer::SynthEngine&, double)> perform;
};

bool render(const Take& take, const juce::File& folder)
{
    seer::SynthEngine engine;
    engine.prepare(kRate, kBlock);
    seer::EngineParams params;
    if (take.setup)
        take.setup(params);

    const double beat = 0.6;
    double end = 0.0;
    juce::MidiBuffer events;
    for (const auto& n : *take.phrase)
    {
        events.addEvent(juce::MidiMessage::noteOn(1, n.note, n.velocity), static_cast<int>(n.start * beat * kRate));
        events.addEvent(juce::MidiMessage::noteOff(1, n.note), static_cast<int>((n.start + n.length) * beat * kRate));
        end = std::max(end, (n.start + n.length) * beat);
    }
    const int total = static_cast<int>((end + 2.0) * kRate);
    juce::AudioBuffer<float> out(2, total), block(2, kBlock);
    for (int position = 0; position < total; position += kBlock)
    {
        const int n = std::min(kBlock, total - position);
        juce::MidiBuffer slice;
        slice.addEvents(events, position, n, -position);
        if (take.perform)
            take.perform(params, engine, position / kRate);
        juce::AudioBuffer<float> view(block.getArrayOfWritePointers(), 2, n);
        engine.process(view, slice, params);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom(ch, position, view, ch, 0, n);
    }

    const float peak = out.getMagnitude(0, total);
    const float rms = std::sqrt(0.5f * (out.getRMSLevel(0, 0, total) * out.getRMSLevel(0, 0, total) + out.getRMSLevel(1, 0, total) * out.getRMSLevel(1, 0, total)));
    std::cout << juce::String(take.name).paddedRight(' ', 22) << "peak " << juce::String(juce::Decibels::gainToDecibels(peak), 1).paddedLeft(' ', 6)
              << " dB   rms " << juce::String(juce::Decibels::gainToDecibels(rms), 1).paddedLeft(' ', 6) << " dB\n";

    const auto file = folder.getChildFile(juce::String(take.name) + ".wav");
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    juce::WavAudioFormat wav;
    const auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(kRate).withNumChannels(2).withBitsPerSample(24));
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer(out, 0, total);
}
} // namespace

int main(int argc, char* argv[])
{
    using seer::EngineParams;
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "renders");
    if (!folder.createDirectory())
    {
        std::cerr << "Can't create " << folder.getFullPathName() << "\n";
        return 1;
    }

    const std::vector<Take> takes = {
        { "01-init-lead", &kLead, {}, {} },
        { "02-init-chords", &kChords, [](EngineParams& p) { p.aEnvR = 0.5f; p.panSpread = 0.6f; }, {} },
        { "03-saw-open", &kLead, [](EngineParams& p) { p.mixOsc2 = 0.0f; p.lpCutoff = 164.0f; p.lpEnv = 0.0f; }, {} },
        { "04-pulse-pwm-chords", &kChords,
          [](EngineParams& p) { p.osc1Shape = p.osc2Shape = 1.0f; p.lfoPw = true; p.lfoFreq1 = p.lfoFreq2 = false; p.lfoAmt = 0.6f; p.lfoRate = 0.4f;
                                p.aEnvA = 0.45f; p.aEnvR = 0.55f; p.fEnvA = 0.5f; p.lpCutoff = 96.0f; p.panSpread = 0.8f; }, {} },
        { "05-bass-sub", &kBass,
          [](EngineParams& p) { p.osc1Freq = p.osc2Freq = 12.0f; p.mixSub = 0.7f; p.lpCutoff = 76.0f; p.lpRes = 0.25f; p.lpEnv = 0.45f; p.lpKey = seer::KeyTrack::half;
                                p.fEnvA = 0.0f; p.fEnvD = 0.38f; p.fEnvS = 0.15f; p.aEnvA = 0.0f; p.aEnvS = 1.0f; p.aEnvR = 0.15f; }, {} },
        { "06-resonant-sweep", &kChords,
          [](EngineParams& p) { p.lpRes = 0.8f; p.lpEnv = 0.0f; p.lpKey = seer::KeyTrack::off; },
          [](EngineParams& p, seer::SynthEngine&, double t) { p.lpCutoff = 50.0f + 80.0f * static_cast<float>(0.5 - 0.5 * std::cos(t * 1.4)); } },
        { "07-self-oscillation", &kLead,
          [](EngineParams& p) { p.mixOsc1 = p.mixOsc2 = 0.0f; p.lpRes = 1.0f; p.lpCutoff = 65.4f; p.lpEnv = 0.0f; p.lpKey = seer::KeyTrack::full; }, {} },
        { "08-sync-sweep", &kLead,
          [](EngineParams& p) { p.osc1Sync = true; p.mixOsc2 = 0.0f; p.lpCutoff = 150.0f; p.lpEnv = 0.0f; },
          [](EngineParams& p, seer::SynthEngine&, double t) { p.osc1Freq = std::round(24.0f + 30.0f * static_cast<float>(0.5 - 0.5 * std::cos(t * 1.1))); } },
        { "09-mod-wheel-vibrato", &kLead, {},
          [](EngineParams&, seer::SynthEngine& e, double t) { e.setModWheel(t > 2.0 ? static_cast<float>(std::min(0.25, (t - 2.0) * 0.1)) : 0.0f); } },
        { "10-pitch-bend", &kLead, {},
          [](EngineParams&, seer::SynthEngine& e, double t) { e.setPitchWheel(static_cast<float>(std::sin(t * 3.0) * (t > 2.5 ? 1.0 : 0.0))); } },
        { "11-high-pass-thin", &kChords, [](EngineParams& p) { p.hpCutoff = 80.0f; p.hpRes = 0.5f; p.hpKey = seer::KeyTrack::full; }, {} },
        { "12-noise-snare", &kBass,
          [](EngineParams& p) { p.mixOsc1 = p.mixOsc2 = 0.0f; p.mixNoise = 1.0f; p.lpCutoff = 120.0f; p.lpEnv = 0.0f; p.aEnvA = 0.0f; p.aEnvD = 0.3f; p.aEnvS = 0.0f; p.aEnvR = 0.3f; }, {} },
    };

    bool ok = true;
    for (const auto& take : takes)
        ok = render(take, folder) && ok;
    std::cout << (ok ? "Wrote " : "FAILED writing ") << folder.getFullPathName() << "\n";
    return ok ? 0 : 1;
}
