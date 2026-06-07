#pragma once
#include "core/GameState.hpp"
#include "content/Item.hpp"
#include "core/Unit.hpp"
class ItemEffectSystem
{
public:
    static void applyPassiveStats(GameState& state, Unit& unit, const Item& item);
    static void onCombatStart(GameState& state);
};

