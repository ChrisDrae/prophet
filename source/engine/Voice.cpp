#include "Voice.h"

namespace seer
{
namespace
{
/// One voice at full mixer levels stays well below clipping; six of them still fit.
constexpr float kVoiceGain = 0.22f;
/// The largest g the filters are run at (a cutoff of 0.45 x the sample rate).
constexpr float kMaxG = 6.3f;

float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

float keyTrackAmount(KeyTrack k) noexcept { return k == KeyTrack::full ? 1.0f : k == KeyTrack::half ? 0.5f : 0.0f; }

/// A repeatable pseudo-random number in -1..1 for voice `index`, `salt` picking which one.
float tolerance(int index, uint32_t salt) noexcept
{
    uint32_t x = static_cast<uint32_t>(index + 1) * 0x9E3779B9u ^ salt * 0x85EBCA6Bu;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return static_cast<float>(x) * (2.0f / 4294967296.0f) - 1.0f;
}
} // namespace

void Voice::prepare(double oversampledRate, int voiceIndex, int ticksLength)
{
    sampleRate = oversampledRate;
    index = voiceIndex;
    tickLength = ticksLength;
    tickSeconds = tickLength / sampleRate;
    cutoffTolerance = 0.35f * tolerance(index, 1); // about +-2 %
    timeTolerance = 1.0 + 0.03 * tolerance(index, 2);
    panPosition = (index % 2 == 0 ? -1.0f : 1.0f) * (0.4f + 0.3f * static_cast<float>((index / 2) % 3));
    noise = dsp::Noise(0x1234567u + static_cast<uint32_t>(index) * 7919u);
    driftNoise = dsp::Noise(0xBADC0DEu + static_cast<uint32_t>(index) * 104729u);
    driftCoef = static_cast<float>(1.0 - std::exp(-tickSeconds / 0.35));
    filterEnv.setSampleRate(sampleRate);
    ampEnv.setSampleRate(sampleRate);
    for (auto* last : { lastFEnv, lastAEnv })
        std::fill_n(last, 4, -1.0f);
    // Analog oscillators run all the time, each at its own point in the cycle.
    osc1.reset(0.5 + 0.5 * tolerance(index, 3));
    osc2.reset(0.5 + 0.5 * tolerance(index, 4));
    kill();
}

void Voice::noteOn(int midiNote, float velocity01, uint64_t timeStamp, const NoteStart& start)
{
    const bool sounding = ampEnv.isActive();
    note = midiNote;
    held = true;
    age = timeStamp;
    detuneCents = start.detuneCents;

    if (start.glide)
    {
        pitch = start.glideFrom;
        glideDistance = std::abs(static_cast<float>(note) - pitch);
        gliding = glideDistance > 0.001f;
    }
    else
    {
        pitch = static_cast<float>(note);
        gliding = false;
    }

    if (!sounding)
    {
        // The voice was silent: start the filters clean. (A voice that is still sounding keeps
        // its state, so a retrigger or a steal doesn't click.)
        lowPass.reset();
        highPass.reset();
        fresh = true;
    }
    if (start.retrigger || !sounding)
    {
        velocity = std::clamp(velocity01, 0.0f, 1.0f);
        drift1.perNote = driftNoise.next();
        drift2.perNote = driftNoise.next();
        filterEnv.gateOn();
        ampEnv.gateOn();
    }
}

void Voice::noteOff()
{
    held = false;
    filterEnv.gateOff();
    ampEnv.gateOff();
}

void Voice::kill()
{
    held = false;
    gliding = false;
    filterEnv.reset();
    ampEnv.reset();
    lowPass.reset();
    highPass.reset();
    fresh = true;
}

void Voice::updateControl(const EngineParams& p, const Modulation& mod) noexcept
{
    using namespace curves;

    // --- envelopes ---------------------------------------------------------------------------------
    if (p.fEnvA != lastFEnv[0] || p.fEnvD != lastFEnv[1] || p.fEnvS != lastFEnv[2] || p.fEnvR != lastFEnv[3])
    {
        filterEnv.set(attackSeconds(p.fEnvA) * timeTolerance, decaySeconds(p.fEnvD) * timeTolerance, p.fEnvS,
                      decaySeconds(p.fEnvR) * timeTolerance);
        lastFEnv[0] = p.fEnvA, lastFEnv[1] = p.fEnvD, lastFEnv[2] = p.fEnvS, lastFEnv[3] = p.fEnvR;
    }
    if (p.aEnvA != lastAEnv[0] || p.aEnvD != lastAEnv[1] || p.aEnvS != lastAEnv[2] || p.aEnvR != lastAEnv[3])
    {
        ampEnv.set(attackSeconds(p.aEnvA) * timeTolerance, decaySeconds(p.aEnvD) * timeTolerance, p.aEnvS,
                   decaySeconds(p.aEnvR) * timeTolerance);
        lastAEnv[0] = p.aEnvA, lastAEnv[1] = p.aEnvD, lastAEnv[2] = p.aEnvS, lastAEnv[3] = p.aEnvR;
    }

    // --- glide -------------------------------------------------------------------------------------
    if (gliding)
    {
        const double seconds = glideSeconds(p.glideRate);
        const bool fixedTime = p.glideMode == GlideMode::fixedTime || p.glideMode == GlideMode::fixedTimeLegato;
        // Semitones per tick: a fixed 12 per "seconds" (rate modes), or the whole interval in "seconds".
        const float step = seconds <= 0.0 ? 1000.0f : static_cast<float>((fixedTime ? glideDistance : 12.0f) * tickSeconds / seconds);
        const float target = static_cast<float>(note);
        if (std::abs(target - pitch) <= step)
        {
            pitch = target;
            gliding = false;
        }
        else
        {
            pitch += pitch < target ? step : -step;
        }
    }

    // --- slop: slow, independent drift of each oscillator ----------------------------------------------
    auto wander = [&](Drift& d)
    {
        if (--d.ticksLeft <= 0)
        {
            d.target = driftNoise.next();
            d.ticksLeft = static_cast<int>((0.3 + 0.9 * driftNoise.unit()) / tickSeconds);
        }
        d.value += (d.target - d.value) * driftCoef;
        return p.slop * p.slop * kSlopCents * 0.01f * (0.65f * d.value + 0.35f * d.perNote); // in semitones
    };
    const float slop1 = wander(drift1), slop2 = wander(drift2);

    // --- the modulation sources ------------------------------------------------------------------------
    const float env = filterEnv.getLevel();
    const float touch = mod.pressure * p.atAmt;
    const float lfoAmount = std::clamp(mod.lfoAmount + (p.atLfo ? touch : 0.0f), 0.0f, 1.0f);
    const float lfoDepth = mod.lfo * lfoAmount;
    const float vibrato = mod.lfo * lfoAmount * lfoAmount * kLfoToPitchSemitones;
    const float polyEnv = p.pmEnv * env;

    // --- pitch ---------------------------------------------------------------------------------------
    const float common = mod.pitchOffset + detuneCents * 0.01f;
    const double pitch1 = pitch + (p.osc1Freq - 24.0f) + common + slop1 + (p.lfoFreq1 ? vibrato : 0.0f)
                          + (p.atFreq1 ? touch * kAftertouchToPitch : 0.0f)
                          + (p.pmFreq1 ? std::abs(p.pmEnv) * polyEnv * kPolyModEnvToPitch : 0.0f);
    const double pitch2 = (p.osc2Kbd ? pitch : 60.0f) + (p.osc2Freq - 24.0f) + p.osc2Fine * 0.01f + common + slop2
                          + (p.lfoFreq2 ? vibrato : 0.0f) + (p.atFreq2 ? touch * kAftertouchToPitch : 0.0f)
                          - (p.osc2LowFreq ? kLowFreqSemitones : 0.0f);
    inc1 = dsp::noteToHz(pitch1) / sampleRate;
    inc2 = dsp::noteToHz(pitch2) / sampleRate;

    // --- waveform --------------------------------------------------------------------------------------
    const float lfoWidth = p.lfoPw ? lfoDepth * kLfoToPulseWidth : 0.0f;
    shape1 = p.osc1Shape + (p.pmShape1 ? polyEnv : 0.0f);
    shape2 = p.osc2Shape;
    pw1 = p.osc1Pw + lfoWidth + (p.pmPw1 ? polyEnv * 0.45f : 0.0f);
    pw2 = p.osc2Pw + lfoWidth;
    sync = p.osc1Sync;
    level1 = p.mixOsc1;
    level2 = p.mixOsc2;
    levelSub = p.mixSub;
    levelNoise = p.mixNoise;

    // --- Poly Mod from oscillator 2 (applied per sample in render) --------------------------------------
    const float o = p.pmOsc2;
    fmOctaves = p.pmFreq1 ? o * std::abs(o) * kPolyModOsc2ToPitch / 12.0f : 0.0f;
    shapeMod = p.pmShape1 ? 0.5f * o : 0.0f;
    pwMod = p.pmPw1 ? 0.45f * o : 0.0f;
    lpOctaves = p.pmLp ? o * kPolyModOsc2ToCutoff / 12.0f : 0.0f;
    hpOctaves = p.pmHp ? o * kPolyModOsc2ToCutoff / 12.0f : 0.0f;
    audioMod = fmOctaves != 0.0f || shapeMod != 0.0f || pwMod != 0.0f || lpOctaves != 0.0f || hpOctaves != 0.0f;

    // --- filters ---------------------------------------------------------------------------------------
    const float keys = pitch - 60.0f;
    auto cutoff = [&](float setting, float envAmount, bool velocityOn, KeyTrack track, bool lfoOn, bool polyOn, bool touchOn)
    {
        const float byVelocity = velocityOn ? lerp(0.2f, 1.0f, velocity) : 1.0f;
        const float semitones = setting + cutoffTolerance + keyTrackAmount(track) * keys
                                + (envAmount * byVelocity * env + (polyOn ? polyEnv : 0.0f)) * kEnvToCutoffSemitones
                                + (lfoOn ? lfoDepth * kLfoToCutoffSemitones : 0.0f)
                                + (touchOn ? touch * kAftertouchToCutoff : 0.0f);
        return dsp::cutoffToG(std::min(22000.0, cutoffHz(semitones)), sampleRate);
    };
    const float lpTarget = cutoff(p.lpCutoff, p.lpEnv, p.lpVel, p.lpKey, p.lfoLp, p.pmLp, p.atLp);
    const float hpTarget = cutoff(p.hpCutoff, p.hpEnv, p.hpVel, p.hpKey, p.lfoHp, p.pmHp, p.atHp);
    if (fresh)
    {
        lpG = lpTarget;
        hpG = hpTarget;
        fresh = false;
    }
    lpGStep = (lpTarget - lpG) / static_cast<float>(tickLength);
    hpGStep = (hpTarget - hpG) / static_cast<float>(tickLength);
    lpFeedback = 4.3f * p.lpRes;                             // self-oscillation from about 93 %
    hpDamping = 1.0f / (0.707f + 11.0f * p.hpRes * p.hpRes); // Q 0.7 .. 11.7

    // --- amplifier ---------------------------------------------------------------------------------------
    const float byVelocity = p.aEnvVel ? lerp(0.12f, 1.0f, std::pow(velocity, 1.3f)) : 1.0f;
    const float level = p.aEnvAmt * byVelocity + (p.lfoAmp ? lfoDepth : 0.0f);
    ampGain = std::max(0.0f, level * (1.0f + (p.atAmp ? touch : 0.0f))) * kVoiceGain;

    // Equal-power pan.
    const float pan = std::clamp(panPosition * p.panSpread, -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * 0.25f * static_cast<float>(dsp::kPi);
    gainLeft = std::cos(angle) * 1.41421f;
    gainRight = std::sin(angle) * 1.41421f;
}

void Voice::render(float* left, float* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        filterEnv.tick();
        const float amp = ampEnv.tick();

        // Oscillator 2 first: it is the sync master and the Poly Mod source.
        const float o2 = osc2.tick(inc2, shape2, pw2);

        lpG += lpGStep;
        hpG += hpGStep;
        double inc = inc1;
        float shape = shape1, width = pw1, gLow = lpG, gHigh = hpG;
        if (audioMod)
        {
            // The pitch and cutoff controls work in octaves, so the modulation multiplies.
            if (fmOctaves != 0.0f)
                inc *= std::exp2(fmOctaves * o2);
            shape += shapeMod * o2;
            width += pwMod * o2;
            if (lpOctaves != 0.0f)
                gLow = std::min(kMaxG, gLow * std::exp2(lpOctaves * o2));
            if (hpOctaves != 0.0f)
                gHigh = std::min(kMaxG, gHigh * std::exp2(hpOctaves * o2));
        }

        const float o1 = osc1.tick(inc, shape, width, sync && osc2.hasWrapped(), osc2.wrapFraction());

        subPhase += 0.5 * inc1;
        if (subPhase >= 1.0)
            subPhase -= 1.0;
        const float sub = 1.0f - 4.0f * std::abs(static_cast<float>(subPhase) - 0.5f);

        float x = o1 * level1 + o2 * level2 + sub * levelSub + noise.next() * levelNoise;
        x += 1.0e-6f * noise.next(); // a breath of noise: what starts self-oscillation, as in a circuit

        x = lowPass.process(x, gLow, lpFeedback);
        x = highPass.process(x, gHigh, hpDamping);

        const float y = x * amp * ampGain;
        left[i] += y * gainLeft;
        right[i] += y * gainRight;
    }
}
} // namespace seer
