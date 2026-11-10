#include "engine/EngineParams.h"
#include "engine/Envelope.h"

#include <catch2/catch_test_macros.hpp>

using namespace seer::dsp;

namespace
{
constexpr double kRate = 48000.0;
}

TEST_CASE("Envelope: attack, decay, sustain and release take the set times", "[env]")
{
    Envelope env;
    env.setSampleRate(kRate);
    env.set(0.1, 0.3, 0.5f, 0.2);
    CHECK_FALSE(env.isActive());

    env.gateOn();
    int samples = 0;
    while (env.getStage() == Envelope::Stage::attack && samples < 100000)
    {
        env.tick();
        ++samples;
    }
    CHECK(std::abs(samples / kRate - 0.1) < 0.003); // reaches full level after the attack time
    CHECK(env.getLevel() == 1.0f);

    // Decay: 95 % of the way from 1.0 to the sustain level after the decay time.
    for (int i = 0; i < static_cast<int>(0.3 * kRate); ++i)
        env.tick();
    CHECK(std::abs(env.getLevel() - (0.5f + 0.05f * 0.5f)) < 0.005f);

    for (int i = 0; i < static_cast<int>(3.0 * kRate); ++i)
        env.tick();
    CHECK(std::abs(env.getLevel() - 0.5f) < 0.001f); // settles on the sustain

    env.gateOff();
    for (int i = 0; i < static_cast<int>(0.2 * kRate); ++i)
        env.tick();
    CHECK(std::abs(env.getLevel() - 0.05f * 0.5f) < 0.003f);

    for (int i = 0; i < static_cast<int>(2.0 * kRate); ++i)
        env.tick();
    CHECK_FALSE(env.isActive()); // and ends
    CHECK(env.getLevel() == 0.0f);
}

TEST_CASE("Envelope: the attack bows like a charging capacitor", "[env]")
{
    Envelope env;
    env.setSampleRate(kRate);
    env.set(0.1, 0.3, 1.0f, 0.2);
    env.gateOn();
    for (int i = 0; i < static_cast<int>(0.05 * kRate); ++i)
        env.tick();
    // Halfway through the attack time the level is above the halfway mark.
    CHECK(env.getLevel() > 0.6f);
    CHECK(env.getLevel() < 0.75f);
}

TEST_CASE("Envelope: a retrigger continues from the current level", "[env]")
{
    Envelope env;
    env.setSampleRate(kRate);
    env.set(0.05, 0.1, 0.8f, 0.5);
    env.gateOn();
    for (int i = 0; i < 20000; ++i)
        env.tick();
    env.gateOff();
    for (int i = 0; i < 2000; ++i)
        env.tick();
    const float before = env.getLevel();
    env.gateOn();
    const float after = env.tick();
    CHECK(after >= before);
    CHECK(after - before < 0.01f); // no jump
}

TEST_CASE("Knob curves cover the documented ranges", "[env]")
{
    using namespace seer::curves;
    CHECK(std::abs(attackSeconds(0.0f) - 0.001) < 1.0e-9);
    CHECK(std::abs(attackSeconds(1.0f) - 10.0) < 1.0e-6);
    CHECK(std::abs(decaySeconds(0.0f) - 0.003) < 1.0e-9);
    CHECK(std::abs(decaySeconds(1.0f) - 20.0) < 1.0e-6);
    CHECK(std::abs(lfoHz(0.0f) - 0.02) < 1.0e-9);
    CHECK(std::abs(lfoHz(1.0f) - 500.0) < 1.0e-6);
    CHECK(std::abs(cutoffHz(0.0) - 10.0) < 1.0e-9);
    CHECK(cutoffHz(164.0) > 22000.0);
}
