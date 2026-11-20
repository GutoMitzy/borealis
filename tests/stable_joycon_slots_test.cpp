#include <borealis/platforms/switch/stable_joycon_slots.hpp>
#include <cassert>

int main()
{
    using namespace brls;
    std::array<StableJoyConPad, 8> pads = {};
    pads[0] = {true, false, false, true};
    pads[1] = {true, false, true, false};
    auto slots = stableJoyConSlots(pads, false);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{1, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));

    pads[0] = {true, false, true, false};
    pads[1] = {true, false, false, true};
    slots = stableJoyConSlots(pads, false);
    assert((slots[0] == StableJoyConSlot{0, 1}));
    assert((slots[1] == StableJoyConSlot{1, 2}));

    pads[2] = {true, false, false, true};
    pads[3] = {true, false, true, false};
    slots = stableJoyConSlots(pads, false);
    assert(slots.size() == 4);
    assert((slots[0] == StableJoyConSlot{0, 1}));
    assert((slots[1] == StableJoyConSlot{1, 2}));
    assert((slots[2] == StableJoyConSlot{3, 1}));
    assert((slots[3] == StableJoyConSlot{2, 2}));

    pads = {};
    pads[0] = {true, true, true, true};
    pads[2] = {true, true, true, true};
    slots = stableJoyConSlots(pads, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{0, 0}));
    assert((slots[1] == StableJoyConSlot{2, 0}));

    slots = stableJoyConSlots(pads, false);
    assert(slots.size() == 4);
    assert((slots[0] == StableJoyConSlot{0, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));
    assert((slots[2] == StableJoyConSlot{2, 1}));
    assert((slots[3] == StableJoyConSlot{2, 2}));

    pads = {};
    pads[0] = {true, false, false, true};
    pads[1] = {true, false, true, false};
    slots = stableJoyConSlots(pads, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{1, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));

    pads[1] = {};
    slots = stableJoyConSlots(pads, false);
    assert(slots.size() == 1);
    assert((slots[0] == StableJoyConSlot{0, 2}));

    pads = {};
    pads[0] = {true, false, false, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{-1, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));

    pads[1] = {true, false, true, false};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{1, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));

    pads[1] = {};
    slots = stableJoyConSlots(pads, false, true);
    assert((slots[0] == StableJoyConSlot{-1, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));

    pads[0] = {};
    pads[1] = {true, false, true, false};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{1, 1}));
    assert((slots[1] == StableJoyConSlot{-1, 2}));

    pads = {};
    pads[2] = {true, false, false, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 4);
    assert((slots[0] == StableJoyConSlot{-1, 1}));
    assert((slots[1] == StableJoyConSlot{-1, 2}));
    assert((slots[2] == StableJoyConSlot{-1, 1}));
    assert((slots[3] == StableJoyConSlot{2, 2}));

    pads[3] = {true, false, true, false};
    slots = stableJoyConSlots(pads, false, true);
    assert((slots[2] == StableJoyConSlot{3, 1}));
    assert((slots[3] == StableJoyConSlot{2, 2}));

    pads = {};
    pads[0] = {true, true, true, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{0, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));
    slots = stableJoyConSlots(pads, true, true);
    assert(slots.size() == 1);
    assert((slots[0] == StableJoyConSlot{0, 0}));

    pads = {};
    pads[0] = {true, false, false, false};
    pads[1] = {true, false, false, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{0, 0}));
    assert((slots[1] == StableJoyConSlot{1, 2}));

    // Two complete Joy-Con pairs can occupy adjacent HID indices.
    // Every JoyDual must produce two logical pads (P1 L/R, P2 L/R).
    pads = {};
    pads[0] = {true, true, true, true};
    pads[1] = {true, true, true, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 4);
    assert((slots[0] == StableJoyConSlot{0, 1}));
    assert((slots[1] == StableJoyConSlot{0, 2}));
    assert((slots[2] == StableJoyConSlot{1, 1}));
    assert((slots[3] == StableJoyConSlot{1, 2}));

    // The paired mode must still report only two complete gamepads.
    slots = stableJoyConSlots(pads, true, true);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{0, 0}));
    assert((slots[1] == StableJoyConSlot{1, 0}));

    // Three adjacent JoyDual HID devices: six independent logical halves.
    pads[2] = {true, true, true, true};
    slots = stableJoyConSlots(pads, false, true);
    assert(slots.size() == 6);
    assert((slots[4] == StableJoyConSlot{2, 1}));
    assert((slots[5] == StableJoyConSlot{2, 2}));

    pads = {};
    pads[0] = {true, false, false, false};
    pads[1] = {true, false, false, true};
    slots = stableJoyConSlots(pads, false);
    assert(slots.size() == 2);
    assert((slots[0] == StableJoyConSlot{0, 0}));
    assert((slots[1] == StableJoyConSlot{1, 2}));
}
