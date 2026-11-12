#include "SynthEngine.h"

namespace seer
{
namespace
{
/// The continuous controls: these glide to a new value instead of jumping, so turning a knob
/// never makes a zipper noise.
constexpr float EngineParams::* kSmoothed[] = {
    &EngineParams::osc2Fine,  &EngineParams::osc1Shape, &EngineParams::osc2Shape,    &EngineParams::osc1Pw,
    &EngineParams::osc2Pw,    &EngineParams::mixOsc1,   &EngineParams::mixOsc2,      &EngineParams::mixSub,
    &EngineParams::mixNoise,  &EngineParams::lpCutoff,  &EngineParams::lpRes,        &EngineParams::lpEnv,
    &EngineParams::hpCutoff,  &EngineParams::hpRes,     &EngineParams::hpEnv,        &EngineParams::aEnvAmt,
    &EngineParams::lfoAmt,    &EngineParams::pmEnv,     &EngineParams::pmOsc2,       &EngineParams::atAmt,
    &EngineParams::slop,      &EngineParams::panSpread, &EngineParams::unisonDetune, &EngineParams::dist,
    &EngineParams::voiceVol,  &EngineParams::masterVolDb, &EngineParams::masterTune,
};

bool isRetrigger(KeyMode m) noexcept
{
    return m == KeyMode::lowRetrigger || m == KeyMode::highRetrigger || m == KeyMode::lastRetrigger;
}

bool isLegatoGlide(GlideMode m) noexcept
{
    return m == GlideMode::fixedRateLegato || m == GlideMode::fixedTimeLegato;
}
} // namespace

void SynthEngine::prepare(double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = std::max(1, maximumBlockSize);

    highLeft.assign(static_cast<size_t>(maxBlock * kMaxOversampling), 0.0f);
    highRight.assign(static_cast<size_t>(maxBlock * kMaxOversampling), 0.0f);
    midLeft.assign(static_cast<size_t>(maxBlock * 2), 0.0f);
    midRight.assign(static_cast<size_t>(maxBlock * 2), 0.0f);
    scratch.assign(static_cast<size_t>(maxBlock), 0.0f);
    for (auto& h : firstHalving)
        h.design(23, 8.0); // a wide gap between what must pass and what must go: few taps are enough
    for (auto& h : lastHalving)
        h.design(95, 8.0);
    effectA.prepare(sampleRate, maxBlock);
    effectB.prepare(sampleRate, maxBlock);

    playedAs.fill(-1);
    keyDown.fill(false);
    sustained.fill(false);
    pedal = false;
    pitchWheel = modWheel = aftertouch = pitchWheelSmooth = modWheelSmooth = aftertouchSmooth = 0.0f;
    lfoPhase = 0.0;
    paramsSet = false;
    clock = 0;
    haveLastPitch = false;
    unisonNote = -1;
    configure(2);
}

void SynthEngine::configure(int newOversampling) noexcept
{
    oversampling = newOversampling;
    oversampledRate = sampleRate * oversampling;
    tick = 4 * oversampling;
    for (int i = 0; i < kMaxVoices; ++i)
        voices[static_cast<size_t>(i)].prepare(oversampledRate, i, tick);
    smoothing = static_cast<float>(1.0 - std::exp(-tick / (0.012 * oversampledRate)));
    wheelSmoothing = static_cast<float>(1.0 - std::exp(-tick / (0.004 * oversampledRate)));
    for (auto& h : firstHalving)
        h.reset();
    for (auto& h : lastHalving)
        h.reset();
    for (auto& dc : distortionDc)
    {
        dc.prepare(oversampledRate);
        dc.reset();
    }
    distortion.reset();
    samplesToTick = 0;
    unisonNote = -1;
}

void SynthEngine::setParams(const EngineParams& newParams) noexcept
{
    const bool first = !paramsSet;
    // Changing how voices are handed out while notes sound would leave some of them stranded:
    // let everything go first.
    if (!first && (newParams.unison != target.unison || newParams.unisonVoices != target.unisonVoices || newParams.voices != target.voices))
        allNotesOff();

    target = newParams;
    target.unisonVoices = std::clamp(target.unisonVoices, 1, 6);
    numVoices = std::clamp(target.voices, 1, kMaxVoices);
    for (int i = numVoices; i < kMaxVoices; ++i)
        if (voices[static_cast<size_t>(i)].isActive())
            voices[static_cast<size_t>(i)].kill();

    if (const int wanted = target.highQuality ? 4 : 2; wanted != oversampling)
        configure(wanted);

    if (first)
    {
        current = target;
        masterGain = dsp::dbToGain(current.masterVolDb) * current.voiceVol;
        paramsSet = true;
    }
}

// -------------------------------------------------------------------------------------------------
//  MIDI
// -------------------------------------------------------------------------------------------------
void SynthEngine::handleMidi(const juce::MidiMessage& m) noexcept
{
    // Transpose shifts the notes themselves, as a keyboard's octave buttons do, so the filters'
    // key tracking follows. Each key remembers where it was sent, so a key held while the
    // transposition changes still lets go of the note it started.
    if (m.isNoteOn())
    {
        const int key = m.getNoteNumber();
        const int shifted = std::clamp(key + 12 * target.transpose, 0, 127);
        playedAs[static_cast<size_t>(key)] = shifted;
        noteOn(shifted, m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        const int key = m.getNoteNumber();
        const int sent = playedAs[static_cast<size_t>(key)];
        playedAs[static_cast<size_t>(key)] = -1;
        noteOff(sent >= 0 ? sent : key);
    }
    else if (m.isPitchWheel())
        setPitchWheel(static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType(1))
        setModWheel(static_cast<float>(m.getControllerValue()) / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        setSustainPedal(m.isSustainPedalOn());
    else if (m.isChannelPressure())
        setAftertouch(static_cast<float>(m.getChannelPressureValue()) / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff();
}

bool SynthEngine::anyKeyDown() const noexcept
{
    for (bool k : keyDown)
        if (k)
            return true;
    return false;
}

Voice* SynthEngine::findVoiceFor(int note) noexcept
{
    Voice* const begin = voices.data();
    Voice* const end = begin + numVoices;
    // 1. A voice already on this note (so repeating a key doesn't pile up voices).
    for (auto v = begin; v != end; ++v)
        if (v->isActive() && v->getNote() == note)
            return v;
    // 2. A silent voice: the one that has rested longest.
    Voice* best = nullptr;
    for (auto v = begin; v != end; ++v)
        if (!v->isActive() && (best == nullptr || v->getAge() < best->getAge()))
            best = v;
    if (best != nullptr)
        return best;
    // 3. Steal: the oldest released voice, or failing that the oldest held one.
    for (auto v = begin; v != end; ++v)
        if (!v->isHeld() && (best == nullptr || v->getAge() < best->getAge()))
            best = v;
    if (best != nullptr)
        return best;
    for (auto v = begin; v != end; ++v)
        if (best == nullptr || v->getAge() < best->getAge())
            best = v;
    return best;
}

int SynthEngine::pickUnisonNote() const noexcept
{
    int pick = -1;
    for (int n = 0; n < 128; ++n)
    {
        if (!keyDown[static_cast<size_t>(n)])
            continue;
        switch (current.keyMode)
        {
            case KeyMode::low:
            case KeyMode::lowRetrigger:
                if (pick < 0)
                    pick = n;
                break;
            case KeyMode::high:
            case KeyMode::highRetrigger: pick = n; break;
            case KeyMode::last:
            case KeyMode::lastRetrigger:
                if (pick < 0 || keyTime[static_cast<size_t>(n)] > keyTime[static_cast<size_t>(pick)])
                    pick = n;
                break;
        }
    }
    return pick;
}

void SynthEngine::startUnison(int note, float velocity, bool legato) noexcept
{
    const int stack = std::min(target.unisonVoices, numVoices);
    const bool glides = target.glideOn && target.glideRate > 0.0f && haveLastPitch && (legato || !isLegatoGlide(target.glideMode));
    const Modulation mod = currentModulation();
    ++clock;
    for (int i = 0; i < stack; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        NoteStart start;
        // Legato: the envelopes carry on, only the pitch changes, unless the key mode retriggers.
        start.retrigger = !legato || isRetrigger(current.keyMode) || !v.isHeld();
        start.glide = glides;
        start.glideFrom = v.isActive() ? v.getPitch() : lastPitch;
        // Spread the stack evenly from -1 to +1 around the note.
        const float place = stack > 1 ? 2.0f * static_cast<float>(i) / static_cast<float>(stack - 1) - 1.0f : 0.0f;
        start.detuneCents = place * target.unisonDetune * curves::kUnisonDetuneCents;
        v.noteOn(note, velocity, clock, start);
        v.updateControl(current, mod);
    }
    unisonNote = note;
    lastPitch = static_cast<float>(note);
    haveLastPitch = true;
}

void SynthEngine::releaseUnison() noexcept
{
    for (auto& v : voices)
        if (v.isHeld())
            v.noteOff();
    unisonNote = -1;
}

void SynthEngine::noteOn(int note, float velocity) noexcept
{
    if (note < 0 || note > 127)
        return;
    if (velocity <= 0.0f)
    {
        noteOff(note);
        return;
    }
    const bool legato = anyKeyDown();
    if (!legato)
        lfoPhase = 0.0; // the LFO starts its cycle again with the first key of a phrase

    keyDown[static_cast<size_t>(note)] = true;
    sustained[static_cast<size_t>(note)] = false;
    keyTime[static_cast<size_t>(note)] = ++clock;
    keyVelocity[static_cast<size_t>(note)] = velocity;

    if (target.unison)
    {
        const int pick = pickUnisonNote();
        if (pick != unisonNote || !legato)
            startUnison(pick, keyVelocity[static_cast<size_t>(pick)], legato && unisonNote >= 0);
        return;
    }

    if (auto* v = findVoiceFor(note))
    {
        NoteStart start;
        start.glide = target.glideOn && target.glideRate > 0.0f && haveLastPitch && (legato || !isLegatoGlide(target.glideMode));
        start.glideFrom = lastPitch;
        v->noteOn(note, velocity, ++clock, start);
        v->updateControl(current, currentModulation());
    }
    lastPitch = static_cast<float>(note);
    haveLastPitch = true;
}

void SynthEngine::releaseNote(int note) noexcept
{
    if (target.unison)
    {
        // Fall back to another held key, or let go.
        if (const int pick = pickUnisonNote(); pick < 0)
            releaseUnison();
        else if (pick != unisonNote)
            startUnison(pick, keyVelocity[static_cast<size_t>(pick)], true);
        return;
    }
    for (auto& v : voices)
        if (v.isHeld() && v.getNote() == note)
            v.noteOff();
}

void SynthEngine::noteOff(int note) noexcept
{
    if (note < 0 || note > 127 || !keyDown[static_cast<size_t>(note)])
        return;
    if (pedal)
    {
        // With the pedal down the key stays "down" for the synth until the pedal comes up.
        sustained[static_cast<size_t>(note)] = true;
        return;
    }
    keyDown[static_cast<size_t>(note)] = false;
    releaseNote(note);
}

void SynthEngine::setSustainPedal(bool down) noexcept
{
    pedal = down;
    if (down)
        return;
    for (int n = 0; n < 128; ++n)
        if (sustained[static_cast<size_t>(n)])
        {
            sustained[static_cast<size_t>(n)] = false;
            keyDown[static_cast<size_t>(n)] = false;
            releaseNote(n);
        }
}

void SynthEngine::allNotesOff() noexcept
{
    keyDown.fill(false);
    sustained.fill(false);
    pedal = false;
    unisonNote = -1;
    for (auto& v : voices)
        if (v.isHeld())
            v.noteOff();
}

int SynthEngine::getNumActiveVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        n += v.isActive() ? 1 : 0;
    return n;
}

// -------------------------------------------------------------------------------------------------
//  Rendering
// -------------------------------------------------------------------------------------------------
Modulation SynthEngine::currentModulation() const noexcept
{
    Modulation mod;
    mod.pitchOffset = pitchWheelSmooth * static_cast<float>(current.bendRange) + current.masterTune * 0.01f;
    mod.lfo = lfoValue;
    // The wheel adds only a part of the LFO's full depth, so its whole travel is usable: at the
    // normal setting the top of the wheel is a vibrato of about a semitone each way.
    mod.lfoAmount = std::min(1.0f, current.lfoAmt + current.wheelDepth * modWheelSmooth);
    mod.pressure = aftertouchSmooth;
    return mod;
}

void SynthEngine::controlTick() noexcept
{
    // Knobs: glide the continuous ones, copy the rest (switches, choices, whole numbers).
    EngineParams next = target;
    for (auto field : kSmoothed)
    {
        float value = current.*field;
        value += (target.*field - value) * smoothing;
        if (std::abs(target.*field - value) < 1.0e-5f)
            value = target.*field;
        next.*field = value;
    }
    current = next;
    masterGain = dsp::dbToGain(current.masterVolDb) * current.voiceVol;

    pitchWheelSmooth += (pitchWheel - pitchWheelSmooth) * wheelSmoothing;
    modWheelSmooth += (modWheel - modWheelSmooth) * wheelSmoothing;
    aftertouchSmooth += (aftertouch - aftertouchSmooth) * wheelSmoothing * 0.3f; // pressure sensors are jumpy

    // LFO
    lfoPhase += curves::lfoHz(current.lfoRate) * tick / oversampledRate;
    if (lfoPhase >= 1.0)
    {
        lfoPhase -= std::floor(lfoPhase);
        lfoRandom = lfoNoise.next(); // random: a new value each cycle ("sample and hold")
    }
    const float ph = static_cast<float>(lfoPhase);
    switch (current.lfoShape)
    {
        case LfoShape::triangle: lfoValue = ph < 0.25f ? 4.0f * ph : ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f; break;
        case LfoShape::sawtooth: lfoValue = ph; break;
        case LfoShape::reverseSawtooth: lfoValue = 1.0f - ph; break;
        case LfoShape::square: lfoValue = ph < 0.5f ? 1.0f : 0.0f; break;
        case LfoShape::random: lfoValue = lfoRandom; break;
    }

    const Modulation mod = currentModulation();
    for (auto& v : voices)
        if (v.isActive())
            v.updateControl(current, mod);
}

void SynthEngine::render(float* left, float* right, int numSamples) noexcept
{
    jassert(numSamples <= maxBlock);
    numSamples = std::min(numSamples, maxBlock);
    const int highSamples = numSamples * oversampling;
    std::fill_n(highLeft.begin(), highSamples, 0.0f);
    std::fill_n(highRight.begin(), highSamples, 0.0f);

    // --- voices, in control ticks ----------------------------------------------------------------
    int done = 0;
    while (done < highSamples)
    {
        if (samplesToTick == 0)
        {
            controlTick();
            samplesToTick = tick;
        }
        const int chunk = std::min(highSamples - done, samplesToTick);
        for (auto& v : voices)
            if (v.isActive())
                v.render(highLeft.data() + done, highRight.data() + done, chunk);
        done += chunk;
        samplesToTick -= chunk;
    }

    // --- distortion, still oversampled (at zero it is skipped: an exact bypass) ----------------------
    if (current.dist > 0.0f)
    {
        distortion.set(dsp::Saturator::Curve::tube, 1.5f * current.dist, 0.45f);
        for (int i = 0; i < highSamples; ++i)
        {
            highLeft[static_cast<size_t>(i)] = distortionDc[0].process(distortion.process(highLeft[static_cast<size_t>(i)], 0));
            highRight[static_cast<size_t>(i)] = distortionDc[1].process(distortion.process(highRight[static_cast<size_t>(i)], 1));
        }
    }

    // --- back down to the host's sample rate -----------------------------------------------------------
    if (oversampling == 4)
    {
        firstHalving[0].process(highLeft.data(), midLeft.data(), numSamples * 2);
        firstHalving[1].process(highRight.data(), midRight.data(), numSamples * 2);
        lastHalving[0].process(midLeft.data(), left, numSamples);
        lastHalving[1].process(midRight.data(), right, numSamples);
    }
    else
    {
        lastHalving[0].process(highLeft.data(), left, numSamples);
        lastHalving[1].process(highRight.data(), right, numSamples);
    }

    // --- effects, then volume ------------------------------------------------------------------------
    if (current.fxOn)
    {
        effectA.set(current.fxAType, current.fxAMix, current.fxAP1, current.fxAP2);
        effectB.set(current.fxBType, current.fxBMix, current.fxBP1, current.fxBP2);
    }
    else
    {
        effectA.set(EffectType::off, 0.0f, 0.5f, 0.5f);
        effectB.set(EffectType::off, 0.0f, 0.5f, 0.5f);
    }
    effectA.process(left, right, numSamples);
    effectB.process(left, right, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = dsp::softClip(left[i] * masterGain);
        right[i] = dsp::softClip(right[i] * masterGain);
    }
}

void SynthEngine::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const EngineParams& newParams) noexcept
{
    setParams(newParams);
    const int numSamples = buffer.getNumSamples();
    const bool stereo = buffer.getNumChannels() > 1;
    float* left = buffer.getWritePointer(0);
    float* right = stereo ? buffer.getWritePointer(1) : scratch.data();

    int position = 0;
    auto renderUpTo = [&](int end)
    {
        while (position < end)
        {
            const int n = std::min(end - position, maxBlock);
            render(left + position, stereo ? right + position : right, n);
            if (!stereo)
                for (int i = 0; i < n; ++i)
                    left[position + i] = 0.5f * (left[position + i] + right[i]);
            position += n;
        }
    };
    for (const auto metadata : midi)
    {
        renderUpTo(std::clamp(metadata.samplePosition, 0, numSamples));
        handleMidi(metadata.getMessage());
    }
    renderUpTo(numSamples);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}
} // namespace seer
