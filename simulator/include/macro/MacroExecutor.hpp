#pragma once

#include <iosfwd>
#include "macro/MacroAction.hpp"
#include "macro/PlayerState.hpp"
#include "core/Random.hpp"
#include "macro/ShopSystem.hpp"
class MacroExecutor
{
public:
    static bool apply(const MacroAction& action,
                      PlayerState& player,
                      ShopSystem& shop,
                      Random& rng,
                      std::ostream& out);
};

