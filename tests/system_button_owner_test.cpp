#include <borealis/platforms/switch/system_button_owner.hpp>
#include <cassert>

int main()
{
    using brls::resolveWirelessHomeOwner;
    constexpr int pads = 8;
    assert(resolveWirelessHomeOwner(2, (1ULL << 0) | (1ULL << 2), pads) == 2);
    assert(resolveWirelessHomeOwner(7, (1ULL << 0) | (1ULL << 2), pads) == 0);
    assert(resolveWirelessHomeOwner(-1, (1ULL << 3), pads) == 3);
    assert(resolveWirelessHomeOwner(-1, 0, pads) == -1);
    assert(resolveWirelessHomeOwner(9, 0, pads) == -1);
    assert(resolveWirelessHomeOwner(1, (1ULL << 4), pads) == 4);
}
