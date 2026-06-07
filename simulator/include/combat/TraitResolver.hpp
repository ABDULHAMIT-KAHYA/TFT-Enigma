#pragma once

#include <vector>
#include "core/GameState.hpp"
#include "content/TraitActivation.hpp"
class TraitResolver
{
public:
    static std::vector<ActiveTrait> resolveTeamTraits(const GameState& state, TeamId team);
};

