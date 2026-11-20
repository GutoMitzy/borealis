#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace brls
{
struct SixAxisQuality
{
    float bias[3] = {};
    std::uint64_t lastSamplingNumber = 0;
    unsigned stationarySamples = 0;
    bool hasSamplingNumber = false;

    void reset()
    {
        for (float& component : bias)
            component = 0.0f;
        lastSamplingNumber = 0;
        stationarySamples = 0;
        hasSamplingNumber = false;
    }

    bool process(const float accel[3], const float gyro[3],
                 std::uint64_t samplingNumber, bool atRest, float corrected[3])
    {
        if (hasSamplingNumber && samplingNumber <= lastSamplingNumber)
            return false;

        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(accel[i]) || !std::isfinite(gyro[i]))
                return false;

        const float gravitySq = accel[0] * accel[0] +
                                accel[1] * accel[1] + accel[2] * accel[2];
        const float angularSq = gyro[0] * gyro[0] +
                                gyro[1] * gyro[1] + gyro[2] * gyro[2];

        if (!std::isfinite(gravitySq) || !std::isfinite(angularSq) ||
            gravitySq < 0.01f || gravitySq > 8100.0f ||
            angularSq > 1600.0f)
            return false;

        hasSamplingNumber = true;
        lastSamplingNumber = samplingNumber;

        if (atRest && angularSq < 0.04f)
        {
            const float alpha = stationarySamples < 8 ? 0.2f : 0.025f;
            for (int i = 0; i < 3; ++i)
                bias[i] = std::clamp(bias[i] +
                    alpha * (gyro[i] - bias[i]), -0.15f, 0.15f);
            stationarySamples = std::min(stationarySamples + 1, 1000000U);
        }

        for (int i = 0; i < 3; ++i)
            corrected[i] = atRest ? 0.0f
                : gyro[i] - (stationarySamples >= 8 ? bias[i] : 0.0f);

        return true;
    }
};
}
