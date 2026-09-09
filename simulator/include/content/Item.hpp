#pragma once

#include <string>
#include <vector>
#include "content/Ability.hpp"
#include "combat/StatusEffect.hpp"

enum class GenericItemEffectType
{
    GrantStats,
    DealDamage,
    Heal,
    Shield,
    ApplyStatus,
    Aura,
    CooldownGate,
    OncePerCombatGate,
    ModifyMana,
    Execute,
    SummonUnit,
    GrantTrait,
    UnknownUnsupported
};

struct GenericItemEffect
{
    GenericItemEffectType effectType = GenericItemEffectType::UnknownUnsupported;
    AbilityTrigger trigger = AbilityTrigger::Passive;
    float value = 0.0f;
    std::int32_t durationMs = 0;
    std::int32_t cooldownMs = 0;
    std::string targetHint{};
    std::string rawSourceName{};
    std::string rawSourceValue{};
    bool supportedForRuntime = false;
    StatusEffect statusEffect{};
    bool hasStatusEffect = false;
    DamageFormula damageFormula{};
    bool hasDamageFormula = false;
};

struct Item
{
    std::string name;
    std::vector<StatusEffect> passiveStats;
    std::vector<AbilityEffect> triggeredEffects;
    std::vector<GenericItemEffect> genericEffects;
    ContentMetadata metadata{};
};
