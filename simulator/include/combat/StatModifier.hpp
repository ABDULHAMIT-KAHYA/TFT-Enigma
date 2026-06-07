#pragma once

#include <cstdint>
#include <string>
#include "combat/ModifierType.hpp"
#include "combat/StatType.hpp"
struct StatModifier
{
    StatType statType;
    ModifierType modifierType;
    float value;
    std::string sourceName;
};
