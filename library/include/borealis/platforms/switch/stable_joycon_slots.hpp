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
    const std::array<StableJoyConPad, N>& pads, bool paired)
{
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
