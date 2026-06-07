#pragma once

#include <cstdint>
#include <string>
#include "combat/DamageType.hpp"
#include "core/GameState.hpp"
struct ProjectileSpec
{
    std::int32_t attackerIndex = -1;
    std::int32_t targetIndex = -1;
    DamageType damageType = DamageType::Physical;
    std::int32_t rawDamage = 0;
    bool didCrit = false;
    float critChanceUsed = 0.0f;
    float critDamageUsed = 1.0f;
    std::int32_t rawBeforeCrit = 0;
    std::int32_t rawAfterCrit = 0;
    std::int32_t travelTimeMs = 0;
    std::string debugName{};
};

class ProjectileSystem
{
public:
    static void spawnAutoAttackProjectile(GameState& state, const ProjectileSpec& spec);
};

