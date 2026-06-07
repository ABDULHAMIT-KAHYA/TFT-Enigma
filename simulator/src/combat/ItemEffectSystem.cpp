#include "combat/ItemEffectSystem.hpp"
#include "constants/AIConstants.hpp"
#include "constants/CombatConstants.hpp"
#include <cmath>
#include <sstream>

static std::int32_t lroundToInt(float v)
{
    return static_cast<std::int32_t>(std::lround(v));
}

static std::string formatStatBonus(const StatusEffect& e)
{
    if (e.effectType == StatusEffectType::HealOverTime)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " HP regen";
    }
    if (e.effectType == StatusEffectType::DamageOverTime)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " DoT";
    }

    if (e.affectedStat == StatType::CritDamage && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value * AIConstants::PercentScale)) + "% crit damage";
    }
    if (e.affectedStat == StatType::CritChance && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value * AIConstants::PercentScale)) + "% crit chance";
    }
    if (e.affectedStat == StatType::AttackDamage && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " AD";
    }
    if (e.affectedStat == StatType::AbilityPower && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " AP";
    }
    if (e.affectedStat == StatType::MaxHp && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " HP";
    }
    if (e.affectedStat == StatType::Armor && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " Armor";
    }
    if (e.affectedStat == StatType::MagicResist && e.modifierType == ModifierType::Flat)
    {
        return "+" + std::to_string(lroundToInt(e.value)) + " MR";
    }

    if (e.modifierType == ModifierType::Percent)
    {
        if (e.affectedStat == StatType::AttackSpeed)
        {
            return "+" + std::to_string(lroundToInt(e.value * AIConstants::PercentScale)) + "% attack speed";
        }
        return "+" + std::to_string(lroundToInt(e.value * AIConstants::PercentScale)) + "%";
    }

    return "+" + std::to_string(lroundToInt(e.value)) + " stat";
}

static void addPassiveEffect(GameState& state, Unit& unit, const Item& item, const StatusEffect& effect)
{
    StatusEffect e = effect;
    if (e.durationMs <= 0)
    {
        e.durationMs = CombatConstants::DefaultPassiveStatusDurationMs;
    }
    if (e.remainingMs <= 0)
    {
        e.remainingMs = e.durationMs;
    }
    unit.addStatusEffect(e);

    if (e.affectedStat == StatType::MaxHp
        && e.modifierType == ModifierType::Flat
        && e.value > 0.0f)
    {
        unit.heal(static_cast<std::int32_t>(std::lround(e.value)));
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << item.name << " grants " << formatStatBonus(effect)
       << " to " << unit.getName();
    state.logger().combat(ss.str());
}

void ItemEffectSystem::applyPassiveStats(GameState& state, Unit& unit, const Item& item)
{
    for (const StatusEffect& s : item.passiveStats)
    {
        addPassiveEffect(state, unit, item, s);
    }
}

void ItemEffectSystem::onCombatStart(GameState& state)
{
    for (Unit& unit : state.units())
    {
        if (!unit.isAlive())
        {
            continue;
        }

        for (const Item& item : unit.items())
        {
            applyPassiveStats(state, unit, item);
        }
    }
}
