// Ability.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "combat/DamageType.hpp"
#include "combat/StatusEffect.hpp"
enum class AbilityTrigger
{
    Passive,
    OnCombatStart,
    OnAttack,
    OnCast,
    OnHit,
    OnCrit,
    OnKill,
    OnDeath,
    OnLowHealth,
    OnDamageTaken
};

enum class TargetType
{
    CurrentEnemy,
    Self,
    LowestHpAlly,
    NearestEnemy
};

enum class AreaShape
{
    SingleTarget,
    Self,
    CircleRadius,
    Line,
    Cross,
    Grid,
    Cone
};

struct DamageFormula
{
    std::int32_t baseDamage = 0;
    float adRatio = 0.0f;
    float apRatio = 0.0f;
    DamageType damageType = DamageType::Physical;
};

struct AbilityEffect
{
    std::string name{};

    AbilityTrigger trigger = AbilityTrigger::OnCast;

    DamageFormula damageFormula{};
    std::int32_t healAmount = 0;
    float healPercentOfDamage = 0.0f;
    std::int32_t shieldAmount = 0;
    std::int32_t maxStacks = 0;
    std::int32_t targetMaxHpThreshold = 0;
    float targetMaxHpPercentDamage = 0.0f;

    AreaShape areaShape = AreaShape::SingleTarget;
    std::int32_t radius = 0;

    std::int32_t delayMs = 0;

    StatusEffect appliedStatusEffect{};
    bool appliesStatusEffect = false;

    bool canCrit = false;
    float critChanceOverride = -1.0f;
    float critDamageOverride = -1.0f;
};

struct Ability
{
    std::string name = "None";
    std::int32_t manaCost = 0;
    TargetType targetType = TargetType::CurrentEnemy;
    std::vector<AbilityEffect> effects{};
};
