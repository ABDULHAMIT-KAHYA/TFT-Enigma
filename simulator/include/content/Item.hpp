#pragma once

#include <string>
#include <vector>
#include "content/Ability.hpp"
#include "combat/StatusEffect.hpp"
struct Item
{
    std::string name;
    std::vector<StatusEffect> passiveStats;
    std::vector<AbilityEffect> triggeredEffects;
};

