#pragma once

#include <vector>
#include "core/Unit.hpp"
namespace TargetingSystem
{
    Unit* findNearestEnemy(Unit& attacker, std::vector<Unit>& candidates);
}
