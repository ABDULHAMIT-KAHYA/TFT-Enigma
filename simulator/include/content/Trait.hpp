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
    AfterDamage,
    Periodic,
    AuraUpdate
};

enum class TraitEffectType
{
    ApplyStatusToTraitUnits,
    ApplyStatusToEnemyTeam,
    StackStatusOnAttack,
    ShieldOnCombatStart,
    ExecuteBelowHpPercent,
    TempCritBonusVsLowHp
};
