#include "combat/StatSystem.hpp"
#include "constants/CombatConstants.hpp"
#include "core/Unit.hpp"
#include <algorithm>
#include <cmath>

static float baseStat(const Unit& unit, StatType statType)
{
    switch (statType)
    {
    case StatType::AttackDamage:   return static_cast<float>(unit.getAd());
    case StatType::AbilityPower:   return unit.getAbilityPower();
    case StatType::Armor:          return static_cast<float>(unit.getArmor());
    case StatType::MagicResist:    return static_cast<float>(unit.getMagicResist());
    case StatType::AttackSpeed:    return unit.getAttackSpeed();
    case StatType::CritChance:     return unit.getCritChance();
    case StatType::CritDamage:     return unit.getCritDamage();
    case StatType::MaxHp:          return static_cast<float>(unit.getMaxHp());
    case StatType::Omnivamp:       return 0.0f;
    case StatType::DamageAmplification: return 0.0f;
    case StatType::ManaGainOnAttack: return static_cast<float>(unit.getManaGainOnAttack());
    case StatType::HealingDone:    return 0.0f;
    case StatType::DamageReduction:return unit.getDurability();
    default:                       return 0.0f;
    }
}

static bool isStatAffectingEffectType(StatusEffectType type)
{
    switch (type)
    {
        case StatusEffectType::Buff:
        case StatusEffectType::Debuff:
        case StatusEffectType::DamageReduction:
        case StatusEffectType::BonusAttackDamage:
        case StatusEffectType::BonusAbilityPower:
        case StatusEffectType::BonusAttackSpeed:
        case StatusEffectType::BonusArmor:
        case StatusEffectType::BonusMagicResist:
        case StatusEffectType::CritChanceBonus:
        case StatusEffectType::CritDamageBonus:
            return true;
        default:
            return false;
    }
}

static float applyModifiers(float baseValue,
                            const std::vector<StatModifier>& modifiers,
                            StatType statType)
{
    float flat = 0.0f;
    float percent = 0.0f;

    for (const StatModifier& modifier : modifiers)
    {
        if (modifier.statType != statType)
        {
            continue;
        }

        if (modifier.modifierType == ModifierType::Flat)
        {
            flat += modifier.value;
        }
        else
        {
            percent += modifier.value;
        }
    }

    float value = (baseValue + flat) * (1.0f + percent);

    if (statType == StatType::AttackSpeed)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::CritChance)
    {
        value = std::clamp(value, 0.0f, 1.0f);
    }
    else if (statType == StatType::CritDamage)
    {
        value = std::max(1.0f, value);
    }
    else if (statType == StatType::MaxHp)
    {
        value = std::max(1.0f, value);
    }
    else if (statType == StatType::Omnivamp)
    {
        value = std::clamp(value, 0.0f, 1.0f);
    }
    else if (statType == StatType::DamageAmplification)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::ManaGainOnAttack)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::DamageReduction)
    {
        value = std::clamp(value, 0.0f, CombatConstants::MaxDamageReduction);
    }

    return value;
}

float StatSystem::getFinalStat(const Unit& unit, StatType statType)
{
    if (statType == StatType::None)
    {
        return 0.0f;
    }

    float flat = 0.0f;
    float percent = 0.0f;

    for (const StatModifier& modifier : unit.statModifiers())
    {
        if (modifier.statType != statType)
        {
            continue;
        }

        if (modifier.modifierType == ModifierType::Flat)
        {
            flat += modifier.value;
        }
        else
        {
            percent += modifier.value;
        }
    }

    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.remainingMs <= 0)
        {
            continue;
        }

        if (!isStatAffectingEffectType(effect.effectType))
        {
            continue;
        }

        if (effect.affectedStat != statType)
        {
            continue;
        }

        if (effect.modifierType == ModifierType::Flat)
        {
            flat += effect.value;
        }
        else
        {
            percent += effect.value;
        }
    }

    float value = (baseStat(unit, statType) + flat) * (1.0f + percent);

    if (statType == StatType::AttackSpeed)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::CritChance)
    {
        value = std::clamp(value, 0.0f, 1.0f);
    }
    else if (statType == StatType::CritDamage)
    {
        value = std::max(1.0f, value);
    }
    else if (statType == StatType::MaxHp)
    {
        value = std::max(1.0f, value);
    }
    else if (statType == StatType::Omnivamp)
    {
        value = std::clamp(value, 0.0f, 1.0f);
    }
    else if (statType == StatType::DamageAmplification)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::ManaGainOnAttack)
    {
        value = std::max(0.0f, value);
    }
    else if (statType == StatType::DamageReduction)
    {
        value = std::clamp(value, 0.0f, CombatConstants::MaxDamageReduction);
    }

    return value;
}

std::int32_t StatSystem::getFinalStatInt(const Unit& unit, StatType statType)
{
    const float v = getFinalStat(unit, statType);
    return static_cast<std::int32_t>(std::lround(v));
}
