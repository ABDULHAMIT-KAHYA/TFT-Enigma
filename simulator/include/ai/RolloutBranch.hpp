#pragma once

#include "ai/RolloutResult.hpp"
#include "macro/MacroAction.hpp"

struct RolloutBranch
{
    MacroAction action{};
    float heuristicScore = 0.0f;
    RolloutResult result{};
};

