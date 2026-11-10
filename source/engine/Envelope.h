#pragma once

#include <algorithm>
#include <cmath>

// =================================================================================================
//  Envelope — a four-stage ADSR with the curves of an analog envelope generator.
//
//  An analog envelope charges and discharges a capacitor, so every stage is a piece of an
//  exponential. The attack aims above full level (at 1.3) and is cut off at 1.0, which gives the
//  slightly bowed rise of the hardware; decay and release fall towards their target and slow down
//  as they get close.
//
//  Times: Attack is the time to reach full level. Decay and Release are the time to cover 95 %
//  of the way (three time constants).
// =================================================================================================
namespace seer::dsp
{
class Envelope
{
public:
    enum class Stage
    {
        idle,
        attack,
        decay, ///< includes the sustain: the level settles on it
        release
    };

    void setSampleRate(double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void set(double attackSeconds, double decaySeconds, float sustainLevel, double releaseSeconds) noexcept
    {
        constexpr double kAttackCurve = 1.4663; // ln(1.3 / 0.3): the time to reach 1.0 when aiming at 1.3
        attack = coefficient(attackSeconds / kAttackCurve);
        decay = coefficient(decaySeconds / 3.0);
        release = coefficient(releaseSeconds / 3.0);
        sustain = std::clamp(sustainLevel, 0.0f, 1.0f);
    }

    /// Key down: start (or restart from the current level) the attack.
    void gateOn() noexcept { stage = Stage::attack; }
    /// Key up.
    void gateOff() noexcept
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }
    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

    /// [RT][DSP] One sample; returns the level, 0..1.
    float tick() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += attack * (kAttackTarget - level);
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::decay;
                }
                break;
            case Stage::decay:
                level += decay * (sustain - level);
                break;
            case Stage::release:
                level -= release * level;
                if (level < 1.0e-4f)
                {
                    level = 0.0f;
                    stage = Stage::idle;
                }
                break;
            case Stage::idle:
                break;
        }
        return level;
    }

private:
    static constexpr float kAttackTarget = 1.3f;

    float coefficient(double tauSeconds) const noexcept
    {
        return static_cast<float>(1.0 - std::exp(-1.0 / (std::max(1.0e-5, tauSeconds) * sampleRate)));
    }

    double sampleRate = 48000.0;
    float attack = 1.0f, decay = 0.01f, release = 0.01f, sustain = 1.0f;
    float level = 0.0f;
    Stage stage = Stage::idle;
};
} // namespace seer::dsp
