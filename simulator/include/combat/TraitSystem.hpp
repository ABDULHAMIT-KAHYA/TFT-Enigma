#pragma once

#include <cstdint>
#include <string>
#include "content/Ability.hpp"
#include "core/GameState.hpp"
#include "content/TraitData.hpp"
#include "core/Unit.hpp"
namespace TraitSystem
{
    void onCombatStart(GameState& state);
    void tick(GameState& state);

    void onAttack(GameState& state, Unit& attacker, Unit& target);
    void onHit(GameState& state, Unit& attacker, Unit& target);
    void onCast(GameState& state, Unit& caster, const Ability& ability, Unit* target);
    void onCrit(GameState& state, Unit& attacker, Unit& target);
    void onKill(GameState& state, Unit& killer, Unit& victim);
    void onLowHealth(GameState& state, Unit& unit);
    void afterDamage(GameState& state, Unit& source, Unit& target);
}
