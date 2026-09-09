#pragma once

#include <cstdint>
#include <string>
#include "combat/DamageType.hpp"
#include "content/Ability.hpp"
#include "core/UnitId.hpp"

enum class CombatEventType
{
    DebugMarker,
    AutoAttackRelease,
    ProjectileHit,
    SpellResolve,
    SpellEnd,
    AbilityEffect
};

enum class CombatEventTargetPolicy
{
    RequireAliveSourceAndTarget,
    RequireAliveSource,
    RequireExistingTarget,
    AllowDeadSourceRequireAliveTarget
};

struct ProjectilePayload
{
    DamageType damageType = DamageType::Physical;
    std::int32_t rawDamage = 0;
    bool didCrit = false;
    float critChanceUsed = 0.0f;
    float critDamageUsed = 1.0f;
    std::int32_t rawBeforeCrit = 0;
    std::int32_t rawAfterCrit = 0;
};

struct CombatEvent
{
    CombatEventType type = CombatEventType::DebugMarker;
    std::int32_t executeAtMs = 0;
    std::uint64_t sequence = 0;
    UnitId sourceId{};
    UnitId targetId{};
    AbilityTrigger trigger = AbilityTrigger::OnCast;
    TargetType targetType = TargetType::CurrentEnemy;
    AbilityEffect abilityEffect{};
    ProjectilePayload projectile{};
    CombatEventTargetPolicy policy = CombatEventTargetPolicy::RequireAliveSourceAndTarget;
    std::string debugName{};
};

const char* toString(CombatEventType type);
