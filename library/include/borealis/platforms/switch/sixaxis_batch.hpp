#pragma once

#include <cstddef>
#include <cstdint>

namespace brls {

inline constexpr std::size_t SIXAXIS_BATCH_CAPACITY = 6;

inline float sixAxisDeltaSeconds(std::uint64_t nanoseconds)
{
    const double dt = static_cast<double>(nanoseconds) * 1.0e-9;
    return dt >= 0.001 && dt <= 0.05 ? static_cast<float>(dt) : 0.0f;
}

template <typename Sample, typename Callback>
void forEachSixAxisOldestFirst(const Sample* newestFirst, std::size_t count,
                               Callback callback)
{
    for (std::size_t i = count; i > 0; --i)
        callback(newestFirst[i - 1]);
}

}
