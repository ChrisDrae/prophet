// SeerUiSnapshot: renders the plugin window into an image without a host.
//
//     SeerUiSnapshot.exe <out.png|out.jpg> [--demo] [--preset <n>] [--scale <factor>] [--set <id>=<value>]... [--bench]
//
// With --demo, a chord is played through the processor first, so the keys show the notes and the
// light blooms. --preset loads a factory preset (0 = Butter Lead). --set changes a parameter
// (plain value, e.g. --set lpCutoff=90). --bench times a repaint. The docs' screenshots are made
// with this tool.

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::printf("usage: SeerUiSnapshot <out.png> [--demo] [--preset <n>] [--scale <factor>] [--set <id>=<value>]... [--bench]\n");
        return 1;
    }
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    bool demo = false, bench = false;
    int preset = -1;
    float scale = 2.0f;
    juce::StringPairArray settings;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--demo")
            demo = true;
        else if (arg == "--bench")
            bench = true;
        else if (arg == "--preset" && i + 1 < argc)
            preset = juce::String(argv[++i]).getIntValue();
        else if (arg == "--scale" && i + 1 < argc)
            scale = juce::jlimit(0.5f, 4.0f, juce::String(argv[++i]).getFloatValue());
        else if (arg == "--set" && i + 1 < argc)
        {
            const juce::String pair(argv[++i]);
            settings.set(pair.upToFirstOccurrenceOf("=", false, false), pair.fromFirstOccurrenceOf("=", false, false));
        }
    }

    auto processor = std::make_unique<SeerProcessor>();
    processor->prepareToPlay(48000.0, 512);
    if (preset >= 0)
        processor->getPresets().load(preset, false);
    for (const auto& id : settings.getAllKeys())
        if (auto* parameter = processor->getState().getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(settings[id].getFloatValue()));

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
        editor->setSize(SeerEditor::kDesignWidth, SeerEditor::kDesignHeight);
        editor->setVisible(true);

        if (demo)
        {
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            for (int note : { 45, 57, 64, 67, 72 })
                midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(96)), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 40), 0);
            processor->processBlock(buffer, midi);
            for (int b = 0; b < 20; ++b)
            {
                juce::MidiBuffer none;
                processor->processBlock(buffer, none);
            }
        }

        // Let the editor's timer run so the header, the light and the keys are up to date.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(500);

        if (bench)
        {
            juce::Image target(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true, juce::SoftwareImageType());
            const auto start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 20; ++i)
            {
                juce::Graphics g(target);
                editor->paintEntireComponent(g, true);
            }
            std::printf("full repaint %.2f ms (software renderer, 1x)\n", (juce::Time::getMillisecondCounterHiRes() - start) / 20.0);
        }

        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        out.deleteFile();
        juce::FileOutputStream stream(out);
        // .jpg for the docs (film grain makes PNGs several times larger), PNG otherwise.
        juce::PNGImageFormat png;
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality(0.9f);
        juce::ImageFileFormat& format = out.hasFileExtension("jpg;jpeg") ? static_cast<juce::ImageFileFormat&>(jpeg) : png;
        if (!stream.openedOk() || !format.writeImageToStream(image, stream))
        {
            std::printf("could not write %s\n", out.getFullPathName().toRawUTF8());
            return 1;
        }
        std::printf("wrote %s\n", out.getFullPathName().toRawUTF8());
    }
    processor.reset();
    return 0;
}
