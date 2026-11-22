#include <borealis/platforms/switch/stable_joycon_identity.hpp>
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using brls::StableJoyConIdentityObservation;
    using brls::StableJoyConIdentitySlots;
    using brls::StableJoyConSlot;
    StableJoyConIdentitySlots identity;
    auto seats = identity.resolve({
        {1001, 0, 1}, {1002, 1, 2},
        {2001, 2, 1}, {2002, 3, 2}
    });
    assert(seats.size() == 4);
    assert((seats[0] == StableJoyConSlot{0, 1}));
    assert((seats[1] == StableJoyConSlot{1, 2}));
    assert((seats[2] == StableJoyConSlot{2, 1}));
    assert((seats[3] == StableJoyConSlot{3, 2}));

    // Connecting the second pair first, changing HID index order, and later
    // reconnecting the first pair must never exchange player seats.
    seats = identity.resolve({
        {2002, 0, 2}, {2001, 1, 1},
        {1002, 2, 2}, {1001, 3, 1}
    });
    assert((seats[0] == StableJoyConSlot{3, 1}));
    assert((seats[1] == StableJoyConSlot{2, 2}));
    assert((seats[2] == StableJoyConSlot{1, 1}));
    assert((seats[3] == StableJoyConSlot{0, 2}));

    seats = identity.resolve({{2001, 1, 1}, {2002, 0, 2}});
    assert(seats[0].padIndex == -1 && seats[1].padIndex == -1);
    assert(seats[2].padIndex == 1 && seats[3].padIndex == 0);
    seats = identity.resolve({{1001, 7, 1}, {1002, 6, 2}});
    assert(seats[0].padIndex == 7 && seats[1].padIndex == 6);
    assert(seats[2].padIndex == -1 && seats[3].padIndex == -1);

    // Only serial-derived IDs can be restored across full application runs.
    constexpr std::uint64_t serial = (1ULL << 63) | 0x1234ULL;
    identity.reset();
    identity.restore({serial, 12ULL, 0ULL, 0ULL});
    const auto saved = identity.knownIds();
    assert(saved[0] == serial);
    assert(saved[1] == 0);
    seats = identity.resolve({{serial, 4, 1}, {55ULL, 3, 2}});
    assert(seats[0].padIndex == 4);
    assert(seats[1].padIndex == 3);
}
