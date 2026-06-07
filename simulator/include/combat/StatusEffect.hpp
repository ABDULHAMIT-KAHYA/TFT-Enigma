#pragma once

#include <cstdint>
#include <string>
#include "combat/CrowdControlType.hpp"
#include "combat/DamageType.hpp"
#include "combat/ModifierType.hpp"
#include "combat/StatType.hpp"
#include "combat/StatusEffectType.hpp"
struct StatusEffect
{
    std::string name;

    StatusEffectType effectType;
    CrowdControlType crowdControlType;

    StatType affectedStat;

    ModifierType modifierType;

    float value;

    std::int32_t durationMs;

    std::int32_t remainingMs;

    std::int32_t tickIntervalMs;
    std::int32_t tickTimerMs;

    DamageType damageType;
};
