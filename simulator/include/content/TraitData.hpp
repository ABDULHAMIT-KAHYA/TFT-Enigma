#pragma once

#include <cstdint>
#include <vector>
#include "content/Ability.hpp"
#include "content/Trait.hpp"
#include "combat/StatusEffect.hpp"
struct TraitEffect
{
    TraitHook hook = TraitHook::OnCombatStart;
    TraitEffectType type = TraitEffectType::ApplyStatusToTraitUnits;

    StatusEffect statusEffect{};

    std::int32_t shieldAmount = 0;
    float targetHpPercentThreshold = -1.0f;
    std::int32_t maxStacks = 0;
    float critChanceBonus = 0.0f;
    float critDamageBonus = 0.0f;

    std::int32_t periodMs = 0;
    std::int32_t auraRefreshMs = 0;
    bool isAura = false;
};

struct TraitTier
{
    int breakpoint = 0;
    std::vector<TraitEffect> effects{};
};

struct TraitDefinition
{
    Trait trait{};
    std::vector<TraitTier> tiers{};
};
