#pragma once

#include <vector>
#include "core/GameState.hpp"
#include "content/TraitActivation.hpp"
#include "content/TraitData.hpp"
class TraitEffectExecutor
{
public:
    static void apply(GameState& state,
                      TeamId team,
                      const std::vector<ActiveTrait>& activeTraits,
                      TraitHook hook,
                      Unit* source,
                      Unit* target);
};
