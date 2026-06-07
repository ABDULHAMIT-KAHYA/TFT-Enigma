#pragma once

#include <cstdint>
#include "core/GameState.hpp"
#include "core/Unit.hpp"
namespace ManaSystem
{
    std::int32_t gainFromDamageTaken(std::int32_t hpLost);
    std::int32_t applyDamageTakenMana(GameState& state, Unit& unit, std::int32_t hpLost);
}

