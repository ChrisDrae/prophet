#include "Effects.h"

namespace seer
{
namespace
{
constexpr float kChorusCentreMs = 9.0f, kChorusDepthMs = 4.5f;

bool isReverb(EffectType t) noexcept
{
    return t == EffectType::hall || t == EffectType::room || t == EffectType::plate || t == EffectType::spring;
}
} // namespace

void EffectSlot::prepare(double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    for (auto& d : delay)
        d.allocate(static_cast<int>(1.05 * sampleRate) + 16);
    reverb.prepare(sampleRate);
    wetLeft.assign(static_cast<size_t>(std::max(1, maximumBlockSize)), 0.0f);
    wetRight.assign(static_cast<size_t>(std::max(1, maximumBlockSize)), 0.0f);
    mixCoef = static_cast<float>(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
    delayCoef = static_cast<float>(1.0 - std::exp(-1.0 / (0.15 * sampleRate))); // slow: a time change bends the pitch, like a BBD
    for (auto& dc : loopDc)
        dc.prepare(sampleRate);
    activeType = EffectType::off;
    reset();
}

void EffectSlot::reset() noexcept
{
    for (auto& d : delay)
        d.clear();
    for (size_t ch = 0; ch < 2; ++ch)
    {
        loopTone1[ch].reset();
        loopTone2[ch].reset();
        loopDc[ch].reset();
        phaserState[ch].fill(0.0f);
        phaserFeedback[ch] = 0.0f;
    }
    reverb.reset();
    lfo.phase = 0.0;
    mixSmooth = mix;
    wasIdle = true;
}

void EffectSlot::set(EffectType newType, float newMix, float newKnob1, float newKnob2) noexcept
{
    type = newType;
    mix = std::clamp(newMix, 0.0f, 1.0f);
    knob1 = std::clamp(newKnob1, 0.0f, 1.0f);
    knob2 = std::clamp(newKnob2, 0.0f, 1.0f);
}

void EffectSlot::process(float* left, float* right, int numSamples) noexcept
{
    // Off, or fully dry: nothing happens to the signal. (Buffers are cleared when it comes back.)
    if (type == EffectType::off || (mix <= 0.0f && mixSmooth < 1.0e-4f))
    {
        wasIdle = true;
        mixSmooth = 0.0f;
        return;
    }
    if (wasIdle || type != activeType)
    {
        const float keep = wasIdle ? 0.0f : mixSmooth;
        reset();
        mixSmooth = keep;
        activeType = type;
        wasIdle = false;
        if (type == EffectType::bbdDelay || type == EffectType::digitalDelay)
            delaySamples = static_cast<float>(delaySeconds(knob1) * sampleRate);
        if (isReverb(type))
            reverb.setType(type == EffectType::hall ? SpaceType::hall : type == EffectType::room ? SpaceType::room
                           : type == EffectType::plate ? SpaceType::plate : SpaceType::spring);
    }

    numSamples = std::min(numSamples, static_cast<int>(wetLeft.size()));
    switch (type)
    {
        case EffectType::bbdDelay: processDelay(left, right, numSamples, true); break;
        case EffectType::digitalDelay: processDelay(left, right, numSamples, false); break;
        case EffectType::chorus: processChorus(left, right, numSamples); break;
        case EffectType::phaser1: processPhaser(left, right, numSamples, 0.72f); break;
        case EffectType::phaser2: processPhaser(left, right, numSamples, 0.3f); break;
        case EffectType::hall:
        case EffectType::room:
        case EffectType::plate:
        case EffectType::spring: processReverb(left, right, numSamples); break;
        case EffectType::off: break;
    }

    // Delays and reverbs: the dry signal stays at full level up to the middle of the mix knob,
    // so adding an echo doesn't make the note quieter. Chorus and phaser: a plain crossfade.
    const bool additive = isReverb(type) || type == EffectType::bbdDelay || type == EffectType::digitalDelay;
    for (int i = 0; i < numSamples; ++i)
    {
        mixSmooth += (mix - mixSmooth) * mixCoef;
        const float m = mixSmooth;
        const float dry = additive ? std::min(1.0f, 2.0f - 2.0f * m) : 1.0f - m;
        const float wet = additive ? std::min(1.0f, 2.0f * m) : m;
        left[i] = dry * left[i] + wet * wetLeft[static_cast<size_t>(i)];
        right[i] = dry * right[i] + wet * wetRight[static_cast<size_t>(i)];
    }
}

void EffectSlot::processDelay(float* left, float* right, int numSamples, bool bbd) noexcept
{
    const float target = static_cast<float>(delaySeconds(knob1) * sampleRate);
    const float feedback = 0.95f * knob2;
    const double seconds = delaySeconds(knob1);
    if (bbd)
    {
        // A bucket-brigade chip is clocked slower for longer delays, and its filters must then cut
        // lower: long delays are dark, short ones bright.
        const double cutoff = std::clamp(1500.0 + 9000.0 * std::sqrt(0.02 / seconds), 1800.0, 10500.0);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            loopTone1[ch].setCutoff(cutoff, sampleRate);
            loopTone2[ch].setCutoff(cutoff * 1.3, sampleRate);
        }
        lfo.setRate(0.37, sampleRate);
    }
    float* in[2] = { left, right };
    float* out[2] = { wetLeft.data(), wetRight.data() };
    for (int i = 0; i < numSamples; ++i)
    {
        delaySamples += (target - delaySamples) * delayCoef;
        const float wobble = bbd ? 1.0f + 0.0012f * lfo.next() : 1.0f; // the clock is never quite steady
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float echo = delay[ch].readFractional(delaySamples * wobble);
            if (bbd)
            {
                echo = loopTone2[ch].process(loopTone1[ch].process(echo));
                echo = 1.5f * dsp::fastTanh(echo * (1.0f / 1.5f)); // the chip's soft ceiling
                echo = loopDc[ch].process(echo);
            }
            delay[ch].push(in[ch][i] + feedback * echo);
            out[ch][i] = echo;
        }
    }
}

void EffectSlot::processChorus(float* left, float* right, int numSamples) noexcept
{
    lfo.setRate(rateHz(knob1), sampleRate);
    const float centre = static_cast<float>(kChorusCentreMs * 0.001 * sampleRate);
    const float depth = static_cast<float>(kChorusDepthMs * 0.001 * sampleRate) * knob2;
    for (size_t ch = 0; ch < 2; ++ch)
        loopTone1[ch].setCutoff(9000.0, sampleRate);
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = lfo.next();
        delay[0].push(left[i]);
        delay[1].push(right[i]);
        // Left and right move in opposite directions: the stereo width of a classic chorus.
        wetLeft[static_cast<size_t>(i)] = loopTone1[0].process(delay[0].readFractional(centre + depth * m));
        wetRight[static_cast<size_t>(i)] = loopTone1[1].process(delay[1].readFractional(centre - depth * m));
    }
}

void EffectSlot::processPhaser(float* left, float* right, int numSamples, float resonance) noexcept
{
    lfo.setRate(rateHz(knob1), sampleRate);
    float* in[2] = { left, right };
    float* out[2] = { wetLeft.data(), wetRight.data() };
    // The stages' turning frequency sweeps around 700 Hz, up to 2.5 octaves each way at full depth.
    const float range = 2.5f * knob2;
    for (int i = 0; i < numSamples; ++i)
    {
        lfo.next();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float sweep = lfo.valueAt(ch == 0 ? 0.0 : 0.25);
            const float hz = 700.0f * std::exp2(range * sweep);
            const float t = std::tan(static_cast<float>(dsp::kPi) * std::min(hz, 0.45f * static_cast<float>(sampleRate)) / static_cast<float>(sampleRate));
            const float a = (t - 1.0f) / (t + 1.0f);
            float v = in[ch][i] + resonance * phaserFeedback[ch];
            for (auto& state : phaserState[ch])
            {
                const float y = a * v + state;
                state = v - a * y;
                v = y;
            }
            phaserFeedback[ch] = dsp::fastTanh(v);
            // Shifted + unshifted: where they are out of phase, notches appear. The mix knob then
            // fades between the plain signal and this.
            out[ch][i] = 0.5f * (in[ch][i] + v);
        }
    }
}

void EffectSlot::processReverb(float* left, float* right, int numSamples) noexcept
{
    switch (type)
    {
        case EffectType::hall: reverb.setDecay(0.6f + 7.4f * knob1 * knob1); reverb.setPreDelay(25.0f); break;
        case EffectType::room: reverb.setDecay(0.2f + 2.8f * knob1 * knob1); reverb.setPreDelay(4.0f); break;
        case EffectType::plate: reverb.setDecay(0.3f + 5.7f * knob1 * knob1); reverb.setPreDelay(10.0f); break;
        default: reverb.setDecay(0.4f + 3.6f * knob1); reverb.setPreDelay(0.0f); break;
    }
    if (type == EffectType::spring)
        reverb.setTone(knob2);
    else
    {
        reverb.setTone(0.6f);
        reverb.setEarly(knob2);
    }
    reverb.process(left, right, wetLeft.data(), wetRight.data(), numSamples);
}
} // namespace seer
