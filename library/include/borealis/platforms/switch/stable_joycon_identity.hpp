#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <borealis/platforms/switch/stable_joycon_slots.hpp>

namespace brls
{
struct StableJoyConIdentityObservation
{
    std::uint64_t physicalId = 0;
    int padIndex = -1;
    int side = 0;
};

enum class JoyConSeatLayout : int
{
    PAIRS = 0,      // P1-L, P1-R, P2-L, P2-R
    INDIVIDUAL = 1, // P1, P2, P3, P4, any physical side
};

class StableJoyConIdentitySlots
{
public:
    void reset()
    {
        pairedIds = {};
        individualIds = {};
        unknownCount = 0;
    }

    void setLayout(JoyConSeatLayout value) { layout = value; }
    JoyConSeatLayout getLayout() const { return layout; }

    std::array<std::uint64_t, 4> knownIds() const
    {
        return activeIds();
    }

    std::array<std::uint64_t, 4> knownIds(JoyConSeatLayout which) const
    {
        return which == JoyConSeatLayout::PAIRS ? pairedIds : individualIds;
    }

    unsigned unassignedCount() const { return unknownCount; }

    // The v19 format contains only paired seats. Preserve them on migration.
    void restore(const std::array<std::uint64_t, 4>& saved)
    {
        pairedIds = persistentOnly(saved);
    }

    void restoreV2(const std::array<std::uint64_t, 4>& pairs,
                   const std::array<std::uint64_t, 4>& singles)
    {
        pairedIds = persistentOnly(pairs);
        individualIds = persistentOnly(singles);
    }

    bool forgetSeat(int seat)
    {
        if (seat < 0 || seat >= 4)
            return false;
        activeIds()[seat] = 0;
        return true;
    }

    void forgetAll()
    {
        activeIds() = {};
        unknownCount = 0;
    }

    // Explicit user reassignment may replace a previously reserved seat.
    // Never change the seat of a different known controller implicitly.
    bool assignToSeat(std::uint64_t id, int side, int seat)
    {
        if (!id || seat < 0 || seat >= 4 ||
            (side != 1 && side != 2))
            return false;
        if (layout == JoyConSeatLayout::PAIRS &&
            (seat % 2 + 1) != side)
            return false;
        auto& saved = activeIds();
        for (auto& other : saved)
            if (other == id)
                other = 0;
        saved[seat] = id;
        return true;
    }

    std::vector<StableJoyConSlot> resolve(
        const std::vector<StableJoyConIdentityObservation>& observations)
    {
        auto& saved = activeIds();
        const std::array<int, 4> pairedSides = {1, 2, 1, 2};
        std::array<StableJoyConSlot, 4> output = {{
            {-1, layout == JoyConSeatLayout::PAIRS ? 1 : 0},
            {-1, layout == JoyConSeatLayout::PAIRS ? 2 : 0},
            {-1, layout == JoyConSeatLayout::PAIRS ? 1 : 0},
            {-1, layout == JoyConSeatLayout::PAIRS ? 2 : 0}
        }};
        unknownCount = 0;

        // First restore known physical Joy-Cons, independent of connection
        // order, and keep disconnected identities reserved.
        for (const auto& item : observations)
        {
            if (!valid(item))
                continue;
            for (int seat = 0; seat < 4; ++seat)
                if (saved[seat] == item.physicalId &&
                    (layout == JoyConSeatLayout::INDIVIDUAL ||
                     pairedSides[seat] == item.side))
                {
                    output[seat] = {item.padIndex, item.side};
                    break;
                }
        }

        // Auto-assign previously unseen Joy-Cons to the first truly vacant
        // seat; when full, report unassigned instead of dropping silently or
        // stealing an existing player's profile.
        for (const auto& item : observations)
        {
            if (!valid(item))
                continue;
            bool known = false;
            for (int seat = 0; seat < 4; ++seat)
                if (saved[seat] == item.physicalId)
                    known = true;
            if (known)
                continue;

            bool assigned = false;
            for (int seat = 0; seat < 4; ++seat)
                if (saved[seat] == 0 &&
                    (layout == JoyConSeatLayout::INDIVIDUAL ||
                     pairedSides[seat] == item.side))
                {
                    saved[seat] = item.physicalId;
                    output[seat] = {item.padIndex, item.side};
                    assigned = true;
                    break;
                }
            if (!assigned)
                ++unknownCount;
        }
        return {output.begin(), output.end()};
    }

private:
    static bool valid(const StableJoyConIdentityObservation& value)
    {
        return value.physicalId && value.padIndex >= 0 &&
               (value.side == 1 || value.side == 2);
    }

    static std::array<std::uint64_t, 4> persistentOnly(
        const std::array<std::uint64_t, 4>& raw)
    {
        std::array<std::uint64_t, 4> result = {};
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = (raw[i] & (1ULL << 63)) ? raw[i] : 0;
        return result;
    }

    std::array<std::uint64_t, 4>& activeIds()
    {
        return layout == JoyConSeatLayout::PAIRS ? pairedIds : individualIds;
    }
    const std::array<std::uint64_t, 4>& activeIds() const
    {
        return layout == JoyConSeatLayout::PAIRS ? pairedIds : individualIds;
    }

    JoyConSeatLayout layout = JoyConSeatLayout::PAIRS;
    std::array<std::uint64_t, 4> pairedIds = {};
    std::array<std::uint64_t, 4> individualIds = {};
    unsigned unknownCount = 0;
};
}
