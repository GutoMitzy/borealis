#include <borealis/platforms/switch/sixaxis_batch.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

struct TestSample {
    unsigned sequence;
};

int main()
{
    TestSample newestFirst[] = {{13}, {12}, {11}, {10}};
    std::vector<unsigned> received;
    brls::forEachSixAxisOldestFirst(newestFirst, 4,
        [&](const TestSample& sample) { received.push_back(sample.sequence); });
    assert((received == std::vector<unsigned>{10, 11, 12, 13}));
    received.clear();
    brls::forEachSixAxisOldestFirst(newestFirst, 1,
        [&](const TestSample& sample) { received.push_back(sample.sequence); });
    assert((received == std::vector<unsigned>{13}));
    received.clear();
    brls::forEachSixAxisOldestFirst(newestFirst, 0,
        [&](const TestSample& sample) { received.push_back(sample.sequence); });
    assert(received.empty());
    assert(brls::SIXAXIS_BATCH_CAPACITY >= 4);
    assert(std::fabs(brls::sixAxisDeltaSeconds(5000000ULL) - 0.005f) < 1e-6f);
    assert(brls::sixAxisDeltaSeconds(0) == 0.0f);
    assert(brls::sixAxisDeltaSeconds(200000000ULL) == 0.0f);
    assert(brls::sixAxisDeltaSeconds(100000ULL) == 0.0f);
}
