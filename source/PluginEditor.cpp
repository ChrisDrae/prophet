#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
    #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace seer::ui;
namespace id = seer::params::id;

namespace
{
constexpr int kLeft = 42, kRight = 1238; // the panels' area, inside the wooden cheeks
constexpr int kRow1 = 98, kRow1Height = 212;
constexpr int kRow2 = kRow1 + kRow1Height + 12, kRow2Height = 166;
constexpr int kRow3 = kRow2 + kRow2Height + 12, kRow3Height = 184;
constexpr int kKeys = kRow3 + kRow3Height + 14;

const juce::Identifier scaleProperty { "uiScale" };
} // namespace

void SeerEditor::Strip::paint(juce::Graphics& g)
{
    g.fillAll(colours::keysStrip);
    g.setColour(colours::line);
    g.fillRect(getLocalBounds().removeFromTop(1));
}

void SeerEditor::Title::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    drawCaps(g, "Seer", area.translated(0.0f, 1.0f), titleFont(28.0f), juce::Colours::black);
    drawCaps(g, "Seer", area, titleFont(28.0f), colours::fg);
}

SeerEditor::SeerEditor(SeerProcessor& p) : AudioProcessorEditor(p), seerProcessor(p), keyboard(p.keyboardState)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(content);
    content.addAndMakeVisible(field);

    content.addAndMakeVisible(menuButton);
    content.addAndMakeVisible(chip);
    content.addAndMakeVisible(title);
    content.addAndMakeVisible(presetDisplay);
    title.setInterceptsMouseClicks(false, false);
    menuButton.onClick = [this] { showMenu(); };
    menuButton.setTooltip("Voices, quality, pitch wheel range, transpose");

    auto& presets = seerProcessor.getPresets();
    presetDisplay.setName(presets.getCurrentName());
    presetDisplay.onPrevious = [this] { seerProcessor.getPresets().previous(); };
    presetDisplay.onNext = [this] { seerProcessor.getPresets().next(); };
    presetDisplay.onOpenList = [this] { showPresetList(); };
    presets.onPresetChanged = [this] { presetDisplay.setName(seerProcessor.getPresets().getCurrentName()); };

    buildPanels();

    // The keys strip.
    content.addAndMakeVisible(strip);
    strip.setInterceptsMouseClicks(false, false);
    auto& state = seerProcessor.getState();
    masterVolume = std::make_unique<KnobControl>(state, id::masterVol, "Volume", nullptr);
    masterTune = std::make_unique<KnobControl>(state, id::masterTune, "Tune", nullptr);
    for (auto* c : std::initializer_list<juce::Component*> { &pitchWheel, &modWheel, masterVolume.get(), masterTune.get(), &keyboard })
        content.addAndMakeVisible(c);
    pitchWheel.onMove = [this](float v) { seerProcessor.setUiPitchWheel(v); };
    modWheel.onMove = [this](float v) { seerProcessor.setUiModWheel(v); };

    content.addAndMakeVisible(overlay); // on top of everything: the grain and the wood
    layout();

    setResizable(true, true);
    setResizeLimits(kDesignWidth * 7 / 10, kDesignHeight * 7 / 10, kDesignWidth * 3 / 2, kDesignHeight * 3 / 2);
    if (auto* fixedAspect = getConstrainer())
        fixedAspect->setFixedAspectRatio(static_cast<double>(kDesignWidth) / kDesignHeight);
    const double scale = juce::jlimit(0.7, 1.5, static_cast<double>(state.state.getProperty(scaleProperty, 1.0)));
    setSize(juce::roundToInt(kDesignWidth * scale), juce::roundToInt(kDesignHeight * scale));

    enableAllMidiInputs();
    updateEffectCaptions();
    lastFrame = juce::Time::getMillisecondCounterHiRes();
    lastEventCount = seerProcessor.midiEventCount.load();
    lastNoteCount = seerProcessor.noteOnCount.load();
    animate(0.0);
    startTimerHz(30);
}

SeerEditor::~SeerEditor()
{
    stopTimer();
    seerProcessor.getPresets().onPresetChanged = nullptr;
    seerProcessor.setUiPitchWheel(0.0f);
    setLookAndFeel(nullptr);
}

// -------------------------------------------------------------------------------------------------
//  Building the panels
// -------------------------------------------------------------------------------------------------
Panel& SeerEditor::panel(const char* name, juce::Colour hue, juce::Rectangle<int> bounds)
{
    auto* p = panels.add(new Panel(juce::String::fromUTF8(name), hue));
    p->setBounds(bounds);
    content.addAndMakeVisible(p);
    return *p;
}

KnobControl& SeerEditor::knob(Panel& p, const char* parameterId, const char* caption, bool big, bool side)
{
    auto* k = new KnobControl(seerProcessor.getState(), parameterId, caption, &p, big);
    controls.add(k);
    const int w = big ? KnobControl::kBigWidth : KnobControl::kWidth, h = big ? KnobControl::kBigHeight : KnobControl::kHeight;
    side ? p.addSide(*k, w, h) : p.addMain(*k, w, h);
    return *k;
}

void SeerEditor::fader(Panel& p, const char* parameterId, const char* caption)
{
    auto* f = new FaderControl(seerProcessor.getState(), parameterId, caption, &p);
    controls.add(f);
    p.addMain(*f, FaderControl::kWidth, FaderControl::kHeight);
}

void SeerEditor::led(Panel& p, const char* parameterId, const char* caption, bool side)
{
    auto* l = new LedToggle(seerProcessor.getState(), parameterId, caption);
    controls.add(l);
    side ? p.addSide(*l, l->getPreferredWidth(), 18) : p.addSwitch(*l, l->getPreferredWidth());
}

void SeerEditor::choice(Panel& p, const char* parameterId, const char* caption, int width, bool side)
{
    auto* c = new ChoiceControl(seerProcessor.getState(), parameterId, caption);
    controls.add(c);
    side ? p.addSide(*c, width, ChoiceControl::kHeight) : p.addTop(*c, width, ChoiceControl::kHeight);
}

void SeerEditor::buildPanels()
{
    auto row = [](int x, int width, int y, int height) { return juce::Rectangle<int>(x, y, width, height); };

    // ---- row 1: what makes the tone ------------------------------------------------------------------
    {
        auto& p = panel("LFO", colours::lfo, row(42, 192, kRow1, kRow1Height));
        choice(p, id::lfoShape, "Shape", 118);
        knob(p, id::lfoRate, "Rate");
        knob(p, id::lfoAmt, "Amount");
        led(p, id::lfoFreq1, "Freq 1");
        led(p, id::lfoFreq2, "Freq 2");
        led(p, id::lfoPw, "PW 1+2");
        led(p, id::lfoAmp, "Amp");
        led(p, id::lfoLp, "LP");
        led(p, id::lfoHp, "HP");
    }
    {
        auto& p = panel("Poly Mod", colours::polyMod, row(247, 158, kRow1, kRow1Height));
        knob(p, id::pmEnv, "Filt env");
        knob(p, id::pmOsc2, "Osc 2");
        led(p, id::pmFreq1, "Freq 1");
        led(p, id::pmShape1, "Shape 1");
        led(p, id::pmPw1, "PW 1");
        led(p, id::pmLp, "LP");
        led(p, id::pmHp, "HP");
    }
    {
        auto& p = panel("Oscillator 1", colours::osc1, row(418, 192, kRow1, kRow1Height));
        knob(p, id::osc1Freq, "Freq");
        knob(p, id::osc1Shape, "Shape");
        knob(p, id::osc1Pw, "Width");
        led(p, id::osc1Sync, "Sync");
    }
    {
        auto& p = panel("Oscillator 2", colours::osc2, row(623, 238, kRow1, kRow1Height));
        knob(p, id::osc2Freq, "Freq");
        knob(p, id::osc2Fine, "Fine");
        knob(p, id::osc2Shape, "Shape");
        knob(p, id::osc2Pw, "Width");
        led(p, id::osc2LowFreq, "Low freq");
        led(p, id::osc2Kbd, "Keyboard");
    }
    {
        auto& p = panel("Drift", colours::drift, row(874, 123, kRow1, kRow1Height));
        knob(p, id::slop, "Slop");
        knob(p, id::panSpread, "Pan");
    }
    {
        auto& p = panel("Mixer", colours::mixer, row(1010, 228, kRow1, kRow1Height));
        knob(p, id::mixOsc1, "Osc 1");
        knob(p, id::mixOsc2, "Osc 2");
        knob(p, id::mixSub, "Sub");
        knob(p, id::mixNoise, "Noise");
    }

    // ---- row 2: what shapes it -----------------------------------------------------------------------
    {
        auto& p = panel("Low-pass \xc2\xb7 24 dB", colours::lowPass, row(42, 318, kRow2, kRow2Height));
        knob(p, id::lpCutoff, "Cutoff", true);
        knob(p, id::lpRes, "Reso");
        knob(p, id::lpEnv, "Env amt");
        led(p, id::lpVel, "Velocity", true);
        choice(p, id::lpKey, "Keyboard", 64, true);
    }
    {
        auto& p = panel("High-pass \xc2\xb7 12 dB", colours::highPass, row(373, 267, kRow2, kRow2Height));
        knob(p, id::hpCutoff, "Cutoff");
        knob(p, id::hpRes, "Reso");
        knob(p, id::hpEnv, "Env amt");
        led(p, id::hpVel, "Velocity", true);
        choice(p, id::hpKey, "Keyboard", 64, true);
    }
    {
        auto& p = panel("Filter envelope", colours::filterEnv, row(653, 214, kRow2, kRow2Height));
        fader(p, id::fEnvA, "A");
        fader(p, id::fEnvD, "D");
        fader(p, id::fEnvS, "S");
        fader(p, id::fEnvR, "R");
    }
    {
        auto& p = panel("Amp envelope", colours::ampEnv, row(880, 358, kRow2, kRow2Height));
        fader(p, id::aEnvA, "A");
        fader(p, id::aEnvD, "D");
        fader(p, id::aEnvS, "S");
        fader(p, id::aEnvR, "R");
        knob(p, id::aEnvAmt, "Amount", false, true);
        led(p, id::aEnvVel, "Velocity", true);
    }

    // ---- row 3: how it plays and where it goes ---------------------------------------------------------
    {
        auto& p = panel("Voice", colours::voice, row(42, 212, kRow3, kRow3Height));
        choice(p, id::unisonVoices, "Voices", 44);
        choice(p, id::keyMode, "Key mode", 112);
        knob(p, id::unisonDetune, "Detune");
        led(p, id::unison, "Unison");
    }
    {
        auto& p = panel("Glide", colours::glide, row(267, 151, kRow3, kRow3Height));
        choice(p, id::glideMode, "Mode", 122);
        knob(p, id::glideRate, "Rate");
        led(p, id::glideOn, "Glide");
    }
    {
        auto& p = panel("Aftertouch", colours::lfo, row(431, 161, kRow3, kRow3Height));
        knob(p, id::atAmt, "Amount");
        led(p, id::atFreq1, "Freq 1");
        led(p, id::atFreq2, "Freq 2");
        led(p, id::atLfo, "LFO amt");
        led(p, id::atAmp, "Amp");
        led(p, id::atLp, "LP");
        led(p, id::atHp, "HP");
    }
    {
        auto& p = panel("Output", colours::lowPass, row(605, 121, kRow3, kRow3Height));
        knob(p, id::dist, "Distort");
        knob(p, id::voiceVol, "Level");
    }
    {
        auto& p = panel("Effect A", colours::effects, row(739, 242, kRow3, kRow3Height));
        choice(p, id::fxAType, "Type", 136);
        knob(p, id::fxAMix, "Mix");
        effectKnobs[0][0] = &knob(p, id::fxAP1, "Time");
        effectKnobs[0][1] = &knob(p, id::fxAP2, "Feedback");
    }
    {
        auto& p = panel("Effect B", colours::effects, row(994, 244, kRow3, kRow3Height));
        choice(p, id::fxBType, "Type", 136);
        knob(p, id::fxBMix, "Mix");
        effectKnobs[1][0] = &knob(p, id::fxBP1, "Time");
        effectKnobs[1][1] = &knob(p, id::fxBP2, "Early");
        led(p, id::fxOn, "Effects on");
    }

    // The panels were sized before anything was on them: lay each one out now.
    for (auto* p : panels)
        p->resized();
}

void SeerEditor::layout()
{
    content.setBounds(0, 0, kDesignWidth, kDesignHeight);
    field.setBounds(content.getLocalBounds());
    overlay.setBounds(content.getLocalBounds());

    menuButton.setBounds(kLeft, 14, 36, 36);
    chip.setBounds(kLeft + 46, 16, 520, 32);
    presetDisplay.setBounds(kRight - 264, 14, 264, 36);
    title.setBounds(kDesignWidth / 2 - 200, 54, 400, 36);

    // The keys strip.
    strip.setBounds(0, kKeys, kDesignWidth, kDesignHeight - kKeys);
    const int top = kKeys + 10;
    pitchWheel.setBounds(kLeft, top, 40, 96);
    modWheel.setBounds(kLeft + 44, top, 40, 96);
    masterVolume->setBounds(kLeft + 96, top + 18, KnobControl::kWidth, KnobControl::kHeight);
    masterTune->setBounds(kLeft + 150, top + 18, KnobControl::kWidth, KnobControl::kHeight);
    keyboard.setBounds(kLeft + 214, top, kRight - (kLeft + 214), 92);
}

void SeerEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::night);
}

void SeerEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth);
    content.setTransform(juce::AffineTransform::scale(scale));
    seerProcessor.getState().state.setProperty(scaleProperty, static_cast<double>(scale), nullptr);
}

// -------------------------------------------------------------------------------------------------
//  Animation and readouts
// -------------------------------------------------------------------------------------------------
float SeerEditor::value(const char* parameterId) const
{
    return seerProcessor.getState().getRawParameterValue(parameterId)->load(std::memory_order_relaxed);
}

void SeerEditor::animate(double seconds)
{
    // The light follows the sound.
    std::array<float, LightField::numLights> levels {};
    const float effects = value(id::fxOn) > 0.5f ? (value(id::fxAType) > 0.5f ? value(id::fxAMix) : 0.0f) + (value(id::fxBType) > 0.5f ? value(id::fxBMix) : 0.0f) : 0.0f;
    levels[LightField::amber] = (value(id::mixOsc1) + value(id::mixOsc2) + value(id::mixSub)) / 3.0f;
    levels[LightField::red] = value(id::lpCutoff) / 164.0f * 0.8f + value(id::lpRes) * 0.3f;
    levels[LightField::blue] = std::min(1.2f, value(id::hpCutoff) / 164.0f + effects * 0.6f + 0.15f);
    levels[LightField::violet] = std::min(1.3f, value(id::lfoAmt) + std::abs(value(id::pmEnv)) + std::abs(value(id::pmOsc2)) + seerProcessor.lastModWheel.load() + 0.12f);
    levels[LightField::magenta] = value(id::lpRes) * 0.8f + value(id::dist) * 0.5f;
    field.setLevels(levels, 0.08f + value(id::lfoAmt) * 0.5f + value(id::slop) * 0.4f);

    if (const uint32_t notes = seerProcessor.noteOnCount.load(); notes != lastNoteCount)
    {
        lastNoteCount = notes;
        field.noteOn(std::clamp((static_cast<float>(seerProcessor.lastNoteOn.load()) - 36.0f) / 48.0f, 0.0f, 1.0f));
    }
    field.advance(seconds);
}

void SeerEditor::timerCallback()
{
    // The light drifts slowly: 15 frames a second are plenty, and half the work.
    if (ticks % 2 == 0 && isShowing())
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        animate(std::min(0.2, (now - lastFrame) * 0.001));
        lastFrame = now;
    }

    // A MIDI device that is plugged in after the app started still gets switched on.
    if (++ticks % 60 == 0)
        enableAllMidiInputs();

    const uint32_t count = seerProcessor.midiEventCount.load(std::memory_order_relaxed);
    if (count != lastEventCount)
        midiLight = 5;
    lastEventCount = count;
    midiLight = std::max(0, midiLight - 1);
    chip.setMidiLight(midiLight > 0);

    const int playing = seerProcessor.activeVoices.load();
    const int bend = juce::roundToInt(value(id::bendRange));
    chip.setText(midiInputText + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(playing) + " of " + juce::String(6 + 2 * juce::roundToInt(value(id::voices)))
                 + " voices" + juce::String::fromUTF8(" \xc2\xb7 ") + (value(id::quality) > 0.5f ? "4x" : "2x")
                 + juce::String::fromUTF8(" \xc2\xb7 bend \xc2\xb1") + juce::String(bend));

    pitchWheel.showValue(seerProcessor.lastPitchWheel.load());
    modWheel.showValue(seerProcessor.lastModWheel.load());
    updateEffectCaptions();
}

void SeerEditor::updateEffectCaptions()
{
    const char* typeIds[2] = { id::fxAType, id::fxBType };
    for (int slot = 0; slot < 2; ++slot)
    {
        const int type = juce::roundToInt(value(typeIds[slot]));
        if (type == lastEffectType[slot])
            continue;
        lastEffectType[slot] = type;
        const bool delay = type == 1 || type == 2, modulation = type >= 3 && type <= 5, spring = type == 9;
        effectKnobs[slot][0]->setCaption(delay ? "Time" : modulation ? "Rate" : spring ? "Decay" : type == 0 ? "Knob 1" : "Time");
        effectKnobs[slot][1]->setCaption(delay ? "Feedback" : modulation ? "Depth" : spring ? "Tone" : type == 0 ? "Knob 2" : "Early");
    }
}

void SeerEditor::enableAllMidiInputs()
{
#if JucePlugin_Build_Standalone
    // Standalone only: switch on every connected MIDI input (the settings dialog can still turn
    // them off), and remember their names for the status line.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        auto& devices = holder->deviceManager;
        juce::StringArray names;
        for (const auto& input : juce::MidiInput::getAvailableDevices())
        {
            if (!devices.isMidiInputDeviceEnabled(input.identifier))
                devices.setMidiInputDeviceEnabled(input.identifier, true);
            names.add(input.name);
        }
        midiInputText = names.isEmpty() ? "No MIDI input" : "In: " + names.joinIntoString(", ").substring(0, 30);
        chip.setTooltip(names.isEmpty() ? "Connect a MIDI keyboard: it is switched on by itself." : "MIDI inputs: " + names.joinIntoString(", "));
        return;
    }
#endif
    midiInputText = "MIDI from the host";
}

// -------------------------------------------------------------------------------------------------
//  Menus
// -------------------------------------------------------------------------------------------------
void SeerEditor::setParameter(const char* parameterId, float plainValue)
{
    if (auto* parameter = seerProcessor.getState().getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

void SeerEditor::showMenu()
{
    juce::PopupMenu menu;
    juce::Component::SafePointer<SeerEditor> safe(this);
    auto item = [&](juce::PopupMenu& target, const juce::String& text, bool ticked, const char* parameterId, float plain)
    {
        target.addItem(text, true, ticked, [safe, parameterId, plain] { if (safe != nullptr) safe->setParameter(parameterId, plain); });
    };

    menu.addSectionHeader("Voices");
    for (int i = 0; i < 4; ++i)
        item(menu, juce::String(6 + 2 * i) + (i == 0 ? "  (like the original)" : ""), juce::roundToInt(value(id::voices)) == i, id::voices, static_cast<float>(i));
    menu.addSectionHeader("Quality");
    item(menu, "2x oversampling", value(id::quality) < 0.5f, id::quality, 0.0f);
    item(menu, "4x oversampling  (cleaner highs, about twice the CPU)", value(id::quality) > 0.5f, id::quality, 1.0f);

    juce::PopupMenu bend, transpose;
    for (int i = 0; i <= 12; ++i)
        item(bend, juce::String(i) + (i == 1 ? " semitone" : " semitones"), juce::roundToInt(value(id::bendRange)) == i, id::bendRange, static_cast<float>(i));
    for (int i = -2; i <= 2; ++i)
        item(transpose, i == 0 ? juce::String("None") : juce::String(i > 0 ? "+" : "") + juce::String(i) + (std::abs(i) == 1 ? " octave" : " octaves"),
             juce::roundToInt(value(id::transpose)) == i, id::transpose, static_cast<float>(i));
    menu.addSeparator();
    menu.addSubMenu("Pitch wheel range", bend);
    menu.addSubMenu("Transpose", transpose);
    menu.addSeparator();
    menu.addItem("Save preset...", [safe] { if (safe != nullptr) safe->savePresetDialog(); });
    menu.addItem("Open the preset folder", [] { auto folder = seer::PresetManager::userFolder(); folder.createDirectory(); folder.revealToUser(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton).withParentComponent(this));
}

void SeerEditor::showPresetList()
{
    auto& presets = seerProcessor.getPresets();
    juce::PopupMenu menu;
    juce::String section;
    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto& preset = presets.getPreset(i);
        if (preset.category != section)
        {
            section = preset.category;
            menu.addSectionHeader(section == "Lead" ? "Leads" : section == "Bass" ? "Basses" : section == "Pad" ? "Pads" : section == "User" ? "Yours" : section);
        }
        menu.addItem(i + 1, preset.name, true, i == presets.getCurrentIndex());
    }
    menu.addSeparator();
    menu.addItem(10000, "Save preset...");
    juce::Component::SafePointer<SeerEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetDisplay).withParentComponent(this), [safe](int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result == 10000)
            safe->savePresetDialog();
        else
            safe->seerProcessor.getPresets().load(result - 1);
    });
}

void SeerEditor::savePresetDialog()
{
    auto* window = new juce::AlertWindow("Save preset", "Name this sound. It's saved in Documents/Seer/Presets.", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", seerProcessor.getPresets().getCurrentName(), "Name");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<SeerEditor> safe(this);
    window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int result)
    {
        if (result == 1 && safe != nullptr)
            safe->seerProcessor.getPresets().saveUserPreset(window->getTextEditorContents("name"));
    }), true);
}
