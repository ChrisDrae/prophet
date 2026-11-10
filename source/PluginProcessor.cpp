#include "PluginProcessor.h"

#include "PluginEditor.h"

SeerProcessor::SeerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Seer", seer::params::createLayout()),
      parameters(apvts)
{
}

void SeerProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    setLatencySamples(engine.getLatencySamples());
    keyboardState.reset();
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

    // For the window's MIDI readout.
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        midiEventCount.fetch_add(1, std::memory_order_relaxed);
        if (m.isPitchWheel())
            lastPitchWheel.store(static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f, std::memory_order_relaxed);
        else if (m.isControllerOfType(1))
            lastModWheel.store(static_cast<float>(m.getControllerValue()) / 127.0f, std::memory_order_relaxed);
        else if (m.isChannelPressure())
            lastAftertouch.store(static_cast<float>(m.getChannelPressureValue()) / 127.0f, std::memory_order_relaxed);
    }

    engine.process(buffer, midi, parameters.read());
    activeVoices.store(engine.getNumActiveVoices(), std::memory_order_relaxed);
    midi.clear();
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
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeerProcessor();
}
