#include <borealis/platforms/switch/hybrid_controller_slots.hpp>
#include <cassert>
#include <vector>

int main()
{
    using namespace brls;
    const std::vector<StableJoyConSlot> joyCons = {
        {1, 1}, {2, 2}, {-1, 1}, {-1, 2}
    };
    const std::vector<StableJoyConSlot> proControllers = {{0, 0}};
    const std::vector<StableJoyConSlot> noFull = {};
    const std::vector<StableJoyConSlot> noJoyCons = {};

    // Pro-only does not reserve four unrelated Joy-Con gamepads.
    auto result = arrangeHybridControllerSlots(
        proControllers, noJoyCons, true, 8);
    assert(result.size() == 1);
    assert((result[0] == StableJoyConSlot{0, 0}));

    // Pro connected first remains on P1 after two Joy-Cons join.
    result = arrangeHybridControllerSlots(proControllers, joyCons, true, 8);
    assert(result.size() == 5);
    assert((result[0] == StableJoyConSlot{0, 0}));
    assert((result[1] == StableJoyConSlot{1, 1}));
    assert((result[2] == StableJoyConSlot{2, 2}));

    // Joy-Cons connected first retain P1/P2 seats when Pro later arrives.
    result = arrangeHybridControllerSlots(proControllers, joyCons, false, 8);
    assert(result.size() == 5);
    assert((result[0] == StableJoyConSlot{1, 1}));
    assert((result[1] == StableJoyConSlot{2, 2}));
    assert((result[4] == StableJoyConSlot{0, 0}));

    // Removing all Joy-Cons preserves four neutral slots and the Pro seat.
    const std::vector<StableJoyConSlot> neutral = {
        {-1, 1}, {-1, 2}, {-1, 1}, {-1, 2}
    };
    result = arrangeHybridControllerSlots(proControllers, neutral, false, 8);
    assert(result.size() == 5);
    assert(result[0].padIndex == -1);
    assert(result[4].padIndex == 0);
    result = arrangeHybridControllerSlots(noFull, neutral, false, 8);
    assert(result.size() == 4);

    const std::vector<StableJoyConSlot> twoPro = {{0, 0}, {5, 0}};
    result = arrangeHybridControllerSlots(twoPro, neutral, true, 8);
    assert(result.size() == 6);
    assert((result[0] == StableJoyConSlot{0, 0}));
    assert((result[1] == StableJoyConSlot{5, 0}));
    assert(result[2].padIndex == -1);

    // Capacity enforcement must not write beyond the Npad/gamepad array.
    result = arrangeHybridControllerSlots(twoPro, joyCons, true, 3);
    assert(result.size() == 3);
}
