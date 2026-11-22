#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <borealis/platforms/switch/stable_joycon_slots.hpp>

namespace brls
{
// An ID names a physical Joy-Con, not the Npad index into which Nintendo's
// HID service happened to place it. Slots are sticky for a streaming session:
// P1=L0/R1, P2=L2/R3. Unplugged IDs retain their seat for reconnection.
struct StableJoyConIdentityObservation
{
    std::uint64_t physicalId = 0;
    int padIndex = -1;
    int side = 0;
};

class StableJoyConIdentitySlots
{
public:
    void reset()
    {
        ids = {};
    }

    std::vector<StableJoyConSlot> resolve(
        const std::vector<StableJoyConIdentityObservation>& observations)
    {
        const std::array<int, 4> sides = {1, 2, 1, 2};
        std::array<StableJoyConSlot, 4> output = {{
            {-1, 1}, {-1, 2}, {-1, 1}, {-1, 2}
        }};

        // Existing physical IDs keep their seats regardless of Npad order.
        for (const auto& item : observations)
        {
            if (!item.physicalId || item.padIndex < 0)
                continue;
            for (int i = 0; i < 4; ++i)
            {
                if (sides[i] == item.side && ids[i] == item.physicalId)
                {
                    output[i].padIndex = item.padIndex;
                    break;
                }
            }
        }

        // Allocate only unused identities. A disconnected controller never
        // loses its profile/seat to a newly connected second pair.
        for (const auto& item : observations)
        {
            if (!item.physicalId || item.padIndex < 0)
                continue;
            bool assigned = false;
            for (int i = 0; i < 4; ++i)
                if (sides[i] == item.side && ids[i] == item.physicalId)
                    assigned = true;
            if (assigned)
                continue;
            for (int i = 0; i < 4; ++i)
                if (sides[i] == item.side && ids[i] == 0)
                {
                    ids[i] = item.physicalId;
                    output[i].padIndex = item.padIndex;
                    break;
                }
        }
        return {output.begin(), output.end()};
    }

private:
    std::array<std::uint64_t, 4> ids = {};
};
}
