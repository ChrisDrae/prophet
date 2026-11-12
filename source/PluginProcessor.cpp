#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
const juce::Identifier octaveDownKey { "octaveDownMidi" }, octaveUpKey { "octaveUpMidi" };

} // namespace

seer::SharedSettings::SharedSettings()
{
    juce::PropertiesFile::Options options;
    // Not "Seer.settings": the Standalone app keeps its audio setup in that file, and two objects
    // writing one file would undo each other's changes.
    options.applicationName = "Seer Preferences";
    options.folderName = "Seer";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.millisecondsBeforeSaving = -1; // saved explicitly: no timer running at shutdown
    file = std::make_unique<juce::PropertiesFile>(options);
}

seer::SharedSettings::~SharedSettings()
{
    file->saveIfNeeded();
}

SeerProcessor::SeerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Seer", seer::params::createLayout()),
      parameters(apvts)
{
    // Start on the first factory sound. (A host that has a saved session replaces it right away.)
    presets.load(0, false);

    octaveButtons.setBinding(-1, static_cast<uint32_t>(settings().getIntValue(octaveDownKey.toString())));
    octaveButtons.setBinding(+1, static_cast<uint32_t>(settings().getIntValue(octaveUpKey.toString())));
}

SeerProcessor::~SeerProcessor()
{
    cancelPendingUpdate();
}

void SeerProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    setLatencySamples(engine.getLatencySamples());
    keyboardState.reset();
    sentPitchWheel = sentModWheel = 0.0f;
    passOn.ensureSize(8192);
}

bool SeerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SeerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Merge the on-screen keyboard into the incoming MIDI (and light its keys for incoming notes).
    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);

    // The window's wheels become ordinary wheel messages.
    if (const float wheel = uiPitchWheel.load(std::memory_order_relaxed); wheel != sentPitchWheel)
    {
        midi.addEvent(juce::MidiMessage::pitchWheel(1, juce::jlimit(0, 16383, 8192 + juce::roundToInt(wheel * 8191.0f))), 0);
        sentPitchWheel = wheel;
    }
    if (const float wheel = uiModWheel.load(std::memory_order_relaxed); wheel != sentModWheel)
    {
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, juce::jlimit(0, 127, juce::roundToInt(wheel * 127.0f))), 0);
        sentModWheel = wheel;
    }

    // For the window's readouts.
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        midiEventCount.fetch_add(1, std::memory_order_relaxed);
        if (m.getRawDataSize() >= 1 && m.getRawDataSize() <= 3 && !m.isMidiClock() && !m.isActiveSense())
        {
            const auto* bytes = m.getRawData();
            lastMidiMessage.store(static_cast<uint32_t>(bytes[0]) | (m.getRawDataSize() > 1 ? static_cast<uint32_t>(bytes[1]) << 8 : 0u)
                                      | (m.getRawDataSize() > 2 ? static_cast<uint32_t>(bytes[2]) << 16 : 0u),
                                  std::memory_order_relaxed);
        }
        if (m.isNoteOn())
        {
            lastNoteOn.store(m.getNoteNumber(), std::memory_order_relaxed);
            noteOnCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (m.isPitchWheel())
            lastPitchWheel.store(static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f, std::memory_order_relaxed);
        else if (m.isControllerOfType(1))
            lastModWheel.store(static_cast<float>(m.getControllerValue()) / 127.0f, std::memory_order_relaxed);
        else if (m.isChannelPressure())
            lastAftertouch.store(static_cast<float>(m.getChannelPressureValue()) / 127.0f, std::memory_order_relaxed);
    }

    // The keyboard's octave buttons, if it sends them as messages of their own: they move the
    // transposition instead of reaching the synth.
    passOn.clear();
    bool taken = false;
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        switch (octaveButtons.handle(m))
        {
            case seer::OctaveButtons::Result::none: passOn.addEvent(m, metadata.samplePosition); break;
            case seer::OctaveButtons::Result::octaveDown: pendingOctave.fetch_sub(1, std::memory_order_relaxed); [[fallthrough]];
            case seer::OctaveButtons::Result::learned: triggerAsyncUpdate(); [[fallthrough]];
            case seer::OctaveButtons::Result::swallowed: taken = true; break;
            case seer::OctaveButtons::Result::octaveUp:
                pendingOctave.fetch_add(1, std::memory_order_relaxed);
                triggerAsyncUpdate();
                taken = true;
                break;
        }
    }
    if (taken)
        midi.swapWith(passOn);

    engine.process(buffer, midi, parameters.read());
    activeVoices.store(engine.getNumActiveVoices(), std::memory_order_relaxed);
    midi.clear();
}

void SeerProcessor::handleAsyncUpdate()
{
    if (const int delta = pendingOctave.exchange(0); delta != 0)
        shiftOctave(delta);

    // Keep learned buttons for next time, for every instance.
    const int down = static_cast<int>(octaveButtons.getBinding(-1)), up = static_cast<int>(octaveButtons.getBinding(+1));
    if (settings().getIntValue(octaveDownKey.toString()) != down || settings().getIntValue(octaveUpKey.toString()) != up)
    {
        settings().setValue(octaveDownKey.toString(), down);
        settings().setValue(octaveUpKey.toString(), up);
        settings().saveIfNeeded();
    }
}

void SeerProcessor::shiftOctave(int delta)
{
    if (auto* parameter = apvts.getParameter(seer::params::id::transpose))
    {
        const float now = parameter->convertFrom0to1(parameter->getValue());
        const float next = juce::jlimit(-2.0f, 2.0f, now + static_cast<float>(delta));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(next));
        parameter->endChangeGesture();
    }
}

juce::AudioProcessorEditor* SeerProcessor::createEditor()
{
    return new SeerEditor(*this);
}

void SeerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SeerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

            // A switch or a choice that a host left between two positions (0.3 on an on/off switch)
            // reads as one of them, so restoring that same position counts as "no change" and the
            // in-between value would stay. Put every parameter exactly on its position.
            for (auto* p : getParameters())
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p))
                {
                    const float snapped = parameter->convertTo0to1(parameter->convertFrom0to1(parameter->getValue()));
                    if (snapped != parameter->getValue())
                        parameter->setValueNotifyingHost(snapped);
                }

            const auto name = apvts.state.getProperty("preset").toString();
            if (juce::MessageManager::existsAndIsCurrentThread())
                presets.restoreSelection(name);
            else
                juce::MessageManager::callAsync([this, alive = juce::WeakReference<SeerProcessor>(this), name]
                {
                    if (alive != nullptr)
                        presets.restoreSelection(name);
                });
        }
}

int SeerProcessor::getNumPrograms()
{
    return static_cast<int>(seer::factoryPresets().size());
}

int SeerProcessor::getCurrentProgram()
{
    return std::min(presets.getCurrentIndex(), getNumPrograms() - 1);
}

void SeerProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    if (juce::MessageManager::existsAndIsCurrentThread())
        presets.load(index, false);
    else
        juce::MessageManager::callAsync([this, alive = juce::WeakReference<SeerProcessor>(this), index]
        {
            if (alive != nullptr)
                presets.load(index, false);
        });
}

const juce::String SeerProcessor::getProgramName(int index)
{
    return index >= 0 && index < getNumPrograms() ? seer::factoryPresets()[static_cast<size_t>(index)].name : juce::String();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeerProcessor();
}
