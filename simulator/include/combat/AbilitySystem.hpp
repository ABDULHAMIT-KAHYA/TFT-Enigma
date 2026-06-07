// AbilitySystem.hpp
#pragma once

#include <vector>
#include "content/Ability.hpp"
#include "core/GameState.hpp"
#include "core/Unit.hpp"
namespace AbilitySystem
{
    Unit* resolveTarget(Unit& caster,
                        Unit& primaryTarget,
                        std::vector<Unit>& allUnits,
                        TargetType targetType);

    void executeTrigger(GameState& state,
                        Unit& owner,
                        Unit* primaryTarget,
                        AbilityTrigger trigger);

    bool tryCast(GameState& state,
                 Unit& caster,
                 Unit& primaryTarget);

} 
