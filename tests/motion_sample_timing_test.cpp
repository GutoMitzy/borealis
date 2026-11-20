#include <borealis/platforms/switch/motion_sample_timing.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>

int main()
{
    using brls::motionSampleIntervalSeconds;
    assert(std::fabs(motionSampleIntervalSeconds(5000000ULL) - 0.005f) < 1.0e-6f);
    assert(std::fabs(motionSampleIntervalSeconds(16666667ULL) - 0.016666667f) < 1.0e-6f);
    assert(motionSampleIntervalSeconds(0ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(500000ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(100000000ULL) == 0.0f);
    assert(motionSampleIntervalSeconds(UINT64_MAX) == 0.0f);
}
