#include <borealis/platforms/switch/stable_joycon_identity.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using namespace brls;
    const std::array<StableJoyConIdentityObservation, 4> physical = {{
        {1001, 0, 1}, {1002, 1, 2}, {2001, 2, 1}, {2002, 3, 2}
    }};

    // With an established mapping, all 24 possible discovery orders must
    // produce the same virtual player seats.
    std::array<int, 4> permutation = {0, 1, 2, 3};
    int runs = 0;
    do {
        StableJoyConIdentitySlots slots;
        assert(slots.assignToSeat(1001, 1, 0));
        assert(slots.assignToSeat(1002, 2, 1));
        assert(slots.assignToSeat(2001, 1, 2));
        assert(slots.assignToSeat(2002, 2, 3));
        std::vector<StableJoyConIdentityObservation> input;
        for (int index : permutation)
            input.push_back(physical[index]);
        const auto resolved = slots.resolve(input);
        for (int i = 0; i < 4; ++i)
            assert(resolved[i].padIndex == i &&
                   resolved[i].side == physical[i].side);
        ++runs;
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    assert(runs == 24);

    StableJoyConIdentitySlots slots;
    slots.resolve({{1001, 0, 1}, {1002, 1, 2},
                   {2001, 2, 1}, {2002, 3, 2}});
    const auto full = slots.knownIds();
    auto unresolved = slots.resolve({{9999, 5, 1}, {1002, 6, 2}});
    assert(slots.unassignedCount() == 1);
    assert(slots.knownIds() == full);
    assert(unresolved[0].padIndex == -1 && unresolved[1].padIndex == 6);

    // Explicit forget is required before a new physical device takes a
    // stale seat. Reconnect is stable even if the HID index changes.
    assert(slots.forgetSeat(0));
    unresolved = slots.resolve({{9999, 5, 1}});
    assert(slots.unassignedCount() == 0 && unresolved[0].padIndex == 5);
    unresolved = slots.resolve({{9999, 7, 1}, {2002, 1, 2}});
    assert(unresolved[0].padIndex == 7 && unresolved[3].padIndex == 1);

    // Two LEFT Joy-Cons represent P1 and P2 in Individual mode.
    slots.setLayout(JoyConSeatLayout::INDIVIDUAL);
    auto individual = slots.resolve({{1001, 6, 1}, {2001, 3, 1}});
    assert(individual.size() == 4);
    assert(individual[0].padIndex == 6);
    assert(individual[1].padIndex == 3);
    assert(individual[2].padIndex == -1);
    assert(individual[3].padIndex == -1);
    assert(slots.knownIds(JoyConSeatLayout::PAIRS)[0] == 9999);
    assert(slots.knownIds(JoyConSeatLayout::INDIVIDUAL)[0] == 1001);
    slots.forgetAll();
    assert(slots.knownIds(JoyConSeatLayout::INDIVIDUAL)[0] == 0);
    assert(slots.knownIds(JoyConSeatLayout::PAIRS)[0] == 9999);

    // Nonpersistent HID IDs are not resurrected across application launches.
    constexpr std::uint64_t serial = (1ULL << 63) | 0x1234ULL;
    slots.restoreV2({serial, 17ULL, 0ULL, 0ULL},
                    {serial + 1ULL, 18ULL, 0ULL, 0ULL});
    assert(slots.knownIds(JoyConSeatLayout::PAIRS)[0] == serial);
    assert(slots.knownIds(JoyConSeatLayout::PAIRS)[1] == 0);
    assert(slots.knownIds(JoyConSeatLayout::INDIVIDUAL)[0] == serial + 1ULL);
    assert(slots.knownIds(JoyConSeatLayout::INDIVIDUAL)[1] == 0);
}
