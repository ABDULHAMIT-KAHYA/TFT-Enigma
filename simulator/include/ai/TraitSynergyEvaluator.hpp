#pragma once

#include <string>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "content/TraitActivation.hpp"
struct TraitSynergyScore
{
    float total = 0.0f;
    float active = 0.0f;
    float near = 0.0f;
    std::vector<ActiveTrait> activeTraits{};
    std::string debug{};
};

class TraitSynergyEvaluator
{
public:
    static TraitSynergyScore evaluate(const PlayerState& player, const ContentManager& content);
};

