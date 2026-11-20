#include <borealis/platforms/switch/motion_sample_timing.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>

int main()
{
    using brls::motionSampleIntervalSeconds;
    assert(std::fabs(motionSampleIntervalSeconds(5000000ULL) - 0.005f) < 1.0e-6f);
    assert(std::fabs(motionSampleIntervalSeconds(16666667ULL) - 0.016666667f) < 1.0e-6f);
    assert(motionSampleIntervalSeconds(0ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(500000ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(100000000ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(UINT64_MAX) == 0.0f);
    const std::array<std::uint64_t, 8> samples = {8, 7, 6, 5, 4, 3, 2, 1};
    std::vector<std::uint64_t> emitted;
    brls::forEachOldestMotionSample(samples.data(), samples.size(),
        [&](std::uint64_t sample) { emitted.push_back(sample); });
    assert(emitted.size() == 8);
    for (std::size_t i = 0; i < emitted.size(); ++i)
        assert(emitted[i] == i + 1);

    emitted.clear();
    brls::forEachOldestMotionSample(samples.data(), 0,
        [&](std::uint64_t sample) { emitted.push_back(sample); });
    assert(emitted.empty());
}
