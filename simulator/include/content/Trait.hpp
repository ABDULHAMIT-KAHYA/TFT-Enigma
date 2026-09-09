#pragma once

#include <string>
#include <vector>

struct Trait
{
    std::string name;
    std::vector<int> breakpoints;
};

enum class TraitHook
{
    OnCombatStart,
    OnAttack,
    OnHit,
    OnKill,
    OnCast,
    OnCrit,
    OnLowHealth,
    OnDamageTaken,
    OnDeath,
    AfterDamage,
    Periodic,
    AuraUpdate
};

enum class TraitEffectType
{
    ApplyStatusToTraitUnits,
    ApplyStatusToAllies,
    ApplyStatusToEnemies,
    ApplyStatusToEnemyTeam,
    Shield,
    Heal,
    DealDamage,
    StackStatusOnAttack,
    ShieldOnCombatStart,
    ExecuteBelowHpPercent,
    TempCritBonusVsLowHp
};
