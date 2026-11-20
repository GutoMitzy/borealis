#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace brls
{
inline float motionSampleIntervalSeconds(std::uint64_t deltaNs)
{
    const double seconds = static_cast<double>(deltaNs) * 1.0e-9;
    return std::isfinite(seconds) && seconds >= 0.001 &&
        seconds <= 0.050 ? static_cast<float>(seconds) : 0.0f;
}
template <typename T, typename Callback>
void forEachOldestMotionSample(const T* newestFirst, std::size_t count,
                               Callback callback)
{
    for (std::size_t i = count; i-- > 0;)
        callback(newestFirst[i]);
}
}
