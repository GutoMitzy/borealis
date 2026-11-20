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
    assert(right.bias[0] == 0.0f);

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

    right.reset();
    assert(right.bias[0] == 0.0f);
    assert(!right.calibrationRequested);
    assert(right.process(gravity, motion, 1, false, corrected));
    assert(std::fabs(corrected[0] - motion[0]) < 1e-5f);
}
