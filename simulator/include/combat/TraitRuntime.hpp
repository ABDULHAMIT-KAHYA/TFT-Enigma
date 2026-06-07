#pragma once

#include <cstdint>
#include <vector>
#include "content/TraitActivation.hpp"
#include "core/TeamId.hpp"
enum class TraitTriggerType
{
    CombatStart,
    OnAttack,
    OnHit,
    AfterDamage,
    OnCast,
    OnCrit,
    OnKill,
    OnLowHealth,
    Periodic,
    AuraUpdate
};

struct TraitContext
{
    TeamId team = TeamId::TeamA;
    std::int32_t timeMs = 0;
    std::int32_t dtMs = 0;
};

struct TraitRuntimeState
{
    std::vector<ActiveTrait> activeTraitsA{};
    std::vector<ActiveTrait> activeTraitsB{};

    std::int32_t nextPeriodicMsA = 0;
    std::int32_t nextPeriodicMsB = 0;
    std::int32_t nextAuraUpdateMsA = 0;
    std::int32_t nextAuraUpdateMsB = 0;
};
