#pragma once

#include <cstdint>
#include "combat/StatType.hpp"
class Unit;

namespace StatSystem
{
    float getFinalStat(const Unit& unit, StatType statType);
    std::int32_t getFinalStatInt(const Unit& unit, StatType statType);
}
