#pragma once

#include <cstddef>
#include <vector>
#include <borealis/platforms/switch/stable_joycon_slots.hpp>

namespace brls
{
// Keep the device type that started the session first when both kinds are
// present. FullKey pads use their native button/axis/rumble/sixaxis pipeline;
// only Joy-Con halves use the sticky physical-identity allocation.
inline std::vector<StableJoyConSlot> arrangeHybridControllerSlots(
    const std::vector<StableJoyConSlot>& fullControllers,
    const std::vector<StableJoyConSlot>& joyConSeats,
    bool fullFirst, std::size_t capacity)
{
    std::vector<StableJoyConSlot> result;
    result.reserve(capacity);
    auto append = [&](const std::vector<StableJoyConSlot>& items) {
        for (const auto& slot : items)
        {
            if (result.size() >= capacity)
                return;
            result.push_back(slot);
        }
    };
    if (fullFirst)
        append(fullControllers);
    append(joyConSeats);
    if (!fullFirst)
        append(fullControllers);
    return result;
}
}
