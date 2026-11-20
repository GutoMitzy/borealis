#pragma once
#include <array>
#include <borealis/core/input.hpp>
#include <cstddef>
#include <cstdint>
namespace brls
{
enum class HalfJoyConSide
{
    NONE,
    LEFT,
    RIGHT
};

enum class JoyConInputMode
{
    SEPARATE,
    PAIRED
};
enum class HalfJoyConButton
{
    NORTH,
    EAST,
    SOUTH,
    WEST,
    STICK,
    SL,
    SR,
    MENU,
    SYSTEM,
    SHOULDER,
    TRIGGER,
    CHAT,
    COUNT
};
using HalfJoyConMapping            = std::array<int, static_cast<std::size_t>(HalfJoyConButton::COUNT)>;
constexpr int HALF_JOYCON_UNMAPPED = -1;

inline int halfJoyConOrder(HalfJoyConSide side)
{
    return side == HalfJoyConSide::LEFT ? 0 : side == HalfJoyConSide::RIGHT ? 1 : 2;
}

inline unsigned halfJoyConStickIndex(HalfJoyConSide side)
{
    return side == HalfJoyConSide::RIGHT ? 1U : 0U;
}

inline std::array<float, 2> halfJoyConStickAxes(HalfJoyConSide side, float x, float y)
{
    return side == HalfJoyConSide::LEFT
        ? std::array<float, 2>{ -y, -x }
        : std::array<float, 2>{ y, x };
}

inline bool validHalfJoyConAction(int action)
{
    return action >= HALF_JOYCON_UNMAPPED && action < BUTTON_NAV_UP;
}

inline HalfJoyConMapping defaultHalfJoyConMapping(HalfJoyConSide side)
{
    // Face positions are relative to the controller held horizontally.
    const int shoulder = side == HalfJoyConSide::LEFT ? BUTTON_LB : BUTTON_RB;
    const int trigger = side == HalfJoyConSide::LEFT ? BUTTON_LT : BUTTON_RT;
    return { BUTTON_X, BUTTON_A, BUTTON_B, BUTTON_Y, BUTTON_LSB, shoulder, trigger,
        side == HalfJoyConSide::LEFT ? BUTTON_BACK : BUTTON_START,
        side == HalfJoyConSide::LEFT ? BUTTON_START : BUTTON_GUIDE,
        shoulder, trigger, HALF_JOYCON_UNMAPPED };
}

inline uint32_t halfJoyConSystemClaims(const HalfJoyConMapping& mapping, HalfJoyConSide side)
{
    if (side == HalfJoyConSide::NONE)
        return 0;
    uint32_t result = mapping[static_cast<std::size_t>(HalfJoyConButton::SYSTEM)] != HALF_JOYCON_UNMAPPED
        ? (side == HalfJoyConSide::LEFT ? 1U : 2U)
        : 0;
#ifdef __SWITCH2__
    if (side == HalfJoyConSide::RIGHT && mapping[static_cast<std::size_t>(HalfJoyConButton::CHAT)] != HALF_JOYCON_UNMAPPED)
        result |= 4U;
#endif
    return result;
}

} // namespace brls
