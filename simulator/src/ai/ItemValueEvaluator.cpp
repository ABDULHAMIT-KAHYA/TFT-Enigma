#include "ai/ItemValueEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

static float statFlatWeight(StatType s)
{
    switch (s)
    {
        case StatType::AttackDamage: return AIConstants::ItemStatWeightAttackDamage;
        case StatType::AbilityPower: return AIConstants::ItemStatWeightAbilityPower;
        case StatType::AttackSpeed: return AIConstants::ItemStatWeightAttackSpeed;
        case StatType::CritChance: return AIConstants::ItemStatWeightCritChance;
        case StatType::CritDamage: return AIConstants::ItemStatWeightCritDamage;
        case StatType::Armor: return AIConstants::ItemStatWeightArmor;
        case StatType::MagicResist: return AIConstants::ItemStatWeightMagicResist;
        case StatType::MaxHp: return AIConstants::ItemStatWeightMaxHp;
        case StatType::Omnivamp: return AIConstants::ItemStatWeightOmnivamp;
        case StatType::DamageAmplification: return AIConstants::ItemStatWeightDamageAmp;
        case StatType::ManaGainOnAttack: return AIConstants::ItemStatWeightManaGainOnAttack;
        default: return 0.0f;
    }
}

static bool isOffenseStat(StatType s)
{
    return s == StatType::AttackDamage ||
           s == StatType::AttackSpeed ||
           s == StatType::CritChance ||
           s == StatType::CritDamage ||
           s == StatType::DamageAmplification ||
           s == StatType::Omnivamp;
}

static bool isDefenseStat(StatType s)
{
    return s == StatType::MaxHp ||
           s == StatType::Armor ||
           s == StatType::MagicResist ||
           s == StatType::DamageReduction;
}

static bool isCasterStat(StatType s)
{
    return s == StatType::AbilityPower || s == StatType::ManaGainOnAttack;
}

ItemValueScore ItemValueEvaluator::evaluateItem(const Item& item)
{
    ItemValueScore s{};

    float raw = 0.0f;
    for (const StatusEffect& e : item.passiveStats)
    {
        const float w = statFlatWeight(e.affectedStat);
        const float v = e.modifierType == ModifierType::Percent ? static_cast<float>(e.value) * AIConstants::PercentScale : static_cast<float>(e.value);
        raw += std::abs(v) * w;

        if (isOffenseStat(e.affectedStat)) s.offense += std::abs(v) * w;
        if (isDefenseStat(e.affectedStat)) s.defense += std::abs(v) * w;
        if (isCasterStat(e.affectedStat)) s.caster += std::abs(v) * w;
    }

    if (!item.triggeredEffects.empty())
    {
        raw += AIConstants::ItemTriggeredEffectsBaseBonus +
               static_cast<float>(item.triggeredEffects.size()) * AIConstants::ItemTriggeredEffectPerEffectBonus;
    }

    s.total = raw;
    return s;
}

ItemValueScore ItemValueEvaluator::evaluateUnitItems(const OwnedUnit& unit, const ContentManager& content)
{
    ItemValueScore out{};
    for (const std::string& itemName : unit.items)
    {
        const Item* item = content.getItem(itemName);
        if (!item)
        {
            continue;
        }
        const ItemValueScore s = evaluateItem(*item);
        out.total += s.total;
        out.offense += s.offense;
        out.defense += s.defense;
        out.caster += s.caster;
    }
    return out;
}
