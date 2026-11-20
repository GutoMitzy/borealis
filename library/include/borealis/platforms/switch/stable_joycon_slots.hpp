#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace brls
{
struct StableJoyConPad
{
    bool present = false;
    bool dual = false;
    bool left = false;
    bool right = false;
};

struct StableJoyConSlot
{
    int padIndex;
    int side;

    bool operator==(const StableJoyConSlot& rhs) const
    {
        return padIndex == rhs.padIndex && side == rhs.side;
    }
};

template <std::size_t N>
std::vector<StableJoyConSlot> stableJoyConSlots(
    const std::array<StableJoyConPad, N>& pads, bool paired,
    bool reserveMissingSides = false)
{
    if (reserveMissingSides && !paired)
    {
        bool hasJoyCon = false;
        bool hasOtherController = false;
        std::size_t lastJoyConGroup = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            const auto& pad = pads[i];
            if (!pad.present)
                continue;
            if (pad.left || pad.right)
            {
                hasJoyCon = true;
                lastJoyConGroup = i / 2;
            }
            else
            {
                hasOtherController = true;
            }
        }
        if (hasJoyCon && !hasOtherController)
        {
            std::vector<StableJoyConSlot> result;
            result.reserve((lastJoyConGroup + 1) * 2);
            for (std::size_t group = 0; group <= lastJoyConGroup; ++group)
            {
                const std::size_t first = group * 2;
                const std::size_t last = first + 2 < N ? first + 2 : N;
                int leftPad = -1;
                int rightPad = -1;
                for (std::size_t i = first; i < last; ++i)
                {
                    const auto& pad = pads[i];
                    if (!pad.present)
                        continue;
                    if (pad.left && leftPad == -1)
                        leftPad = static_cast<int>(i);
                    if (pad.right && rightPad == -1)
                        rightPad = static_cast<int>(i);
                }
                result.push_back({leftPad, 1});
                result.push_back({rightPad, 2});

                // Each JoyDual HID pad represents two independent logical Joy-Cons.
                // A second JoyDual in the same two-pad group must not be discarded.
                for (std::size_t i = first; i < last; ++i)
                {
                    const auto& pad = pads[i];
                    if (!pad.present || !pad.dual || !pad.left || !pad.right)
                        continue;
                    if (static_cast<int>(i) != leftPad &&
                        static_cast<int>(i) != rightPad)
                    {
                        result.push_back({static_cast<int>(i), 1});
                        result.push_back({static_cast<int>(i), 2});
                    }
                }
            }
            if (result.size() > N)
                result.resize(N);
            return result;
        }
    }

    std::vector<StableJoyConSlot> result;
    result.reserve(N);
    for (std::size_t base = 0; base < N; base += 2)
    {
        const std::size_t end = base + 2 < N ? base + 2 : N;
        for (std::size_t i = base; i < end; ++i)
        {
            const auto& p = pads[i];
            if (!p.present)
                continue;
            if ((!p.left && !p.right) ||
                (paired && p.dual && p.left && p.right))
                result.push_back({static_cast<int>(i), 0});
        }
        for (std::size_t i = base; i < end; ++i)
        {
            const auto& p = pads[i];
            if (p.present && p.left &&
                !(paired && p.dual && p.right))
                result.push_back({static_cast<int>(i), 1});
        }
        for (std::size_t i = base; i < end; ++i)
        {
            const auto& p = pads[i];
            if (p.present && p.right &&
                !(paired && p.dual && p.left))
                result.push_back({static_cast<int>(i), 2});
        }
    }
    if (result.size() > N)
        result.resize(N);
    return result;
}
} // namespace brls
