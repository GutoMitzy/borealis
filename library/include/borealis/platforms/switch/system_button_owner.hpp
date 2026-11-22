#pragma once

#include <cstdint>

namespace brls
{
inline int resolveWirelessHomeOwner(int lastActiveNpad,
                                    std::uint64_t candidates,
                                    int maxPads)
{
    if (lastActiveNpad >= 0 && lastActiveNpad < maxPads &&
        (candidates & (1ULL << lastActiveNpad)))
        return lastActiveNpad;
    for (int i = 0; i < maxPads && i < 64; ++i)
        if (candidates & (1ULL << i))
            return i;
    return -1;
}
}
