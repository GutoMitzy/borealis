#include <borealis/platforms/switch/sixaxis_quality.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>

int main()
{
    brls::SixAxisQuality right;
    brls::SixAxisQuality left;
    float corrected[3] = {};
    const float gravity[3] = {0.0f, 0.0f, 9.80665f};
    const float bias[3] = {0.015f, -0.012f, 0.01f};

    assert(right.process(gravity, bias, 1, true, corrected));
    assert(std::fabs(corrected[0] - bias[0]) < 1e-6f);
    assert(!right.process(gravity, bias, 1, true, corrected));
    assert(right.calibrationSamples == 0);

    for (std::uint64_t sample = 2; sample <= 100; ++sample)
        assert(right.process(gravity, bias, sample, true, corrected));
    assert(right.calibrationSamples == 0);
    assert(std::fabs(right.bias[0] - bias[0]) < 1e-5f);
    assert(std::fabs(corrected[0]) < 1e-5f);

    // A second controller calibrates independently during stationary periods.
    const float leftRestBias[3] = {-0.021f, 0.008f, 0.017f};
    for (std::uint64_t sample = 1; sample <= 40; ++sample)
        assert(left.process(gravity, leftRestBias, sample, true, corrected));
    assert(std::fabs(left.bias[0] - leftRestBias[0]) < 1e-5f);
    assert(std::fabs(corrected[0]) < 1e-5f);
    left.reset();

    right.beginCalibration();
    assert(right.calibrationRequested);
    for (std::uint64_t sample = 101; sample < 141; ++sample)
        assert(right.process(gravity, bias, sample, true, corrected));
    assert(!right.calibrationRequested);
    assert(std::fabs(right.bias[0] - bias[0]) < 1e-5f);

    const float motion[3] = {1.015f, 0.488f, -0.19f};
    assert(right.process(gravity, motion, 141, false, corrected));
    assert(std::fabs(corrected[0] - 1.0f) < 1e-5f);
    assert(std::fabs(corrected[1] - 0.5f) < 1e-5f);
    assert(std::fabs(corrected[2] + 0.2f) < 1e-5f);

    for (std::uint64_t sample = 142; sample < 500; ++sample)
        assert(right.process(gravity, motion, sample, false, corrected));
    assert(std::fabs(right.bias[0] - bias[0]) < 1e-5f);

    assert(left.process(gravity, motion, 1, false, corrected));
    assert(std::fabs(corrected[0] - motion[0]) < 1e-5f);
    assert(left.bias[0] == 0.0f);

    const float invalid[3] = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
    assert(!left.process(gravity, invalid, 2, false, corrected));
    assert(left.process(gravity, motion, 2, false, corrected));

    right.beginCalibration();
    assert(right.process(gravity, bias, 500, true, corrected));
    assert(right.calibrationSamples == 1);
    assert(right.process(gravity, motion, 501, false, corrected));
    assert(right.calibrationSamples == 0);
    assert(right.calibrationRequested);
    assert(std::fabs(right.bias[0] - bias[0]) < 1e-5f);

    right.calibrationStart =
        std::chrono::steady_clock::now() - std::chrono::seconds(6);
    assert(right.process(gravity, motion, 502, false, corrected));
    assert(!right.calibrationRequested);
    assert(std::fabs(right.bias[0] - bias[0]) < 1e-5f);

    // Independent biases on four Joy-Cons must not leak between players.
    brls::SixAxisQuality pads[4];
    const float offsets[4][3] = {
        {0.014f, -0.011f, 0.007f},
        {-0.019f, 0.008f, -0.006f},
        {0.009f, 0.012f, -0.010f},
        {-0.015f, -0.007f, 0.013f}
    };
    for (int controller = 0; controller < 4; ++controller)
    {
        for (std::uint64_t sample = 1; sample <= 40; ++sample)
            assert(pads[controller].process(gravity, offsets[controller],
                                            sample, true, corrected));
        for (std::uint64_t sample = 41; sample <= 140; ++sample)
        {
            const float intended[3] = {
                0.25f + static_cast<float>(controller) * 0.10f,
                -0.20f, 0.13f
            };
            float measured[3];
            for (int axis = 0; axis < 3; ++axis)
                measured[axis] = intended[axis] + offsets[controller][axis];
            assert(pads[controller].process(gravity, measured, sample,
                                            false, corrected));
            for (int axis = 0; axis < 3; ++axis)
                assert(std::fabs(corrected[axis] - intended[axis]) < 0.002f);
        }
    }

    // A newly attached Joy-Con should not transmit stationary uncalibrated
    // angular drift into an emulator's orientation integrator.
    brls::SixAxisQuality reconnect;
    reconnect.beginCalibration();
    const float freshBias[3] = {0.036f, -0.022f, 0.014f};
    for (std::uint64_t sample = 1; sample <= 39; ++sample)
    {
        assert(reconnect.process(gravity, freshBias, sample, true, corrected));
        for (float value : corrected)
            assert(value == 0.0f);
    }
    assert(reconnect.calibrationRequested);
    assert(reconnect.process(gravity, freshBias, 40, true, corrected));
    assert(!reconnect.calibrationRequested);
    for (int axis = 0; axis < 3; ++axis)
        assert(std::fabs(reconnect.bias[axis] - freshBias[axis]) < 1e-5f);

    // An intentional movement should pass through immediately, even when
    // the automatic startup calibration has not finished.
    brls::SixAxisQuality earlyMotion;
    earlyMotion.beginCalibration();
    assert(earlyMotion.process(gravity, motion, 1, false, corrected));
    assert(std::fabs(corrected[0] - motion[0]) < 1e-5f);
    assert(earlyMotion.calibrationRequested);

    // A HID rest snapshot cannot qualify a burst with implausible gravity,
    // nor can samples on opposite sides of a missing interval be combined.
    brls::SixAxisQuality interrupted;
    interrupted.beginCalibration();
    const float shaking[3] = {0.0f, 0.0f, 14.0f};
    for (std::uint64_t sample = 1; sample <= 40; ++sample)
        assert(interrupted.process(shaking, freshBias, sample, true, corrected));
    assert(interrupted.calibrationSamples == 0);
    for (std::uint64_t sample = 41; sample <= 79; ++sample)
        assert(interrupted.process(gravity, freshBias, sample, true, corrected));
    assert(interrupted.calibrationSamples == 39);
    assert(interrupted.process(gravity, freshBias, 81, true, corrected));
    assert(interrupted.calibrationSamples == 1);

    // A legitimately calibrated zero-bias axis still adapts gradually.
    brls::SixAxisQuality zeroBias;
    const float zero[3] = {};
    for (std::uint64_t sample = 1; sample <= 40; ++sample)
        assert(zeroBias.process(gravity, zero, sample, true, corrected));
    for (std::uint64_t sample = 41; sample <= 80; ++sample)
        assert(zeroBias.process(gravity, freshBias, sample, true, corrected));
    assert(std::fabs(zeroBias.bias[0] - freshBias[0] * 0.02f) < 1e-6f);

    right.reset();
    assert(right.bias[0] == 0.0f);
    assert(!right.calibrationRequested);
    assert(right.process(gravity, motion, 1, false, corrected));
    assert(std::fabs(corrected[0] - motion[0]) < 1e-5f);
}
