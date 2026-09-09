#pragma once

#include "combat/CombatEvent.hpp"
class GameState;

namespace CombatEventExecutor
{
    void execute(GameState& state, const CombatEvent& event);
}
