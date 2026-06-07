#pragma once

#include <cstdint>
#include "content/Ability.hpp"
#include "core/GameState.hpp"
#include "content/Item.hpp"
#include "core/Unit.hpp"
namespace ItemSystem
{
    void onCombatStart(GameState& state);

    void onAttack(GameState& state, Unit& attacker, Unit& target);
    void onHit(GameState& state,
               Unit& attacker,
               Unit& target,
               std::int32_t finalDamage,
               DamageType damageType,
               bool wasCrit);
    void onCrit(GameState& state, Unit& attacker, Unit& target);
    void onCast(GameState& state, Unit& caster, const Ability& ability, Unit* target);
    void onLowHealth(GameState& state, Unit& unit);
    void onDeath(GameState& state, Unit& unit);
}

