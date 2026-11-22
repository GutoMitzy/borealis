#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace brls
{
struct SixAxisQuality
{
    float bias[3] = {};
    float calibrationSum[3] = {};
    float restSum[3] = {};
    unsigned restSamples = 0;
    std::uint64_t lastSamplingNumber = 0;
    unsigned calibrationSamples = 0;
    bool hasSamplingNumber = false;
    bool calibrationRequested = false;
    std::chrono::steady_clock::time_point calibrationStart = {};

    void reset()
    {
        for (int i = 0; i < 3; ++i)
        {
            bias[i] = 0.0f;
            calibrationSum[i] = 0.0f;
            restSum[i] = 0.0f;
        }
        lastSamplingNumber = 0;
        calibrationSamples = 0;
        restSamples = 0;
        calibrationRequested = false;
        calibrationStart = {};
        hasSamplingNumber = false;
    }

    void beginCalibration()
    {
        calibrationRequested = true;
        calibrationStart = std::chrono::steady_clock::now();
        calibrationSamples = 0;
        for (float& sum : calibrationSum)
            sum = 0.0f;
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

        if (calibrationRequested &&
            std::chrono::steady_clock::now() - calibrationStart >
                std::chrono::seconds(5))
        {
            calibrationRequested = false;
            calibrationSamples = 0;
        }

        // During an initial or manual stationary calibration, do not forward
        // uncorrected angular drift to the host. Still accept movement if the
        // controller stops reporting rest. The accelerometer remains live.
        const bool suppressUncalibratedRestMotion =
            calibrationRequested && atRest && angularSq < 0.04f;

        if (calibrationRequested)
        {
            if (atRest && angularSq < 0.04f)
            {
                for (int i = 0; i < 3; ++i)
                    calibrationSum[i] += gyro[i];

                if (++calibrationSamples >= 40)
                {
                    for (int i = 0; i < 3; ++i)
                        bias[i] = std::clamp(calibrationSum[i] /
                            static_cast<float>(calibrationSamples), -0.15f, 0.15f);
                    calibrationRequested = false;
                }
            }
            else
            {
                calibrationSamples = 0;
                for (float& sum : calibrationSum)
                    sum = 0.0f;
            }
        }

        // Calibrate without changing intentional motion. Only HID-confirmed
        // rest samples contribute; a short movement resets the candidate.
        if (!calibrationRequested && atRest && angularSq < 0.04f)
        {
            for (int i = 0; i < 3; ++i)
                restSum[i] += gyro[i];
            if (++restSamples >= 40)
            {
                for (int i = 0; i < 3; ++i)
                {
                    const float observed = std::clamp(
                        restSum[i] / static_cast<float>(restSamples),
                        -0.15f, 0.15f);
                    bias[i] = bias[i] == 0.0f
                        ? observed : bias[i] * 0.98f + observed * 0.02f;
                    restSum[i] = 0.0f;
                }
                restSamples = 0;
            }
        }
        else
        {
            restSamples = 0;
            for (float& sum : restSum)
                sum = 0.0f;
        }

        for (int i = 0; i < 3; ++i)
            corrected[i] = suppressUncalibratedRestMotion
                ? 0.0f : gyro[i] - bias[i];

        return true;
    }
};
}
