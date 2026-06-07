#include "combat/ItemSystem.hpp"
#include "constants/AIConstants.hpp"
#include "core/Board.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/DamageSystem.hpp"
#include "combat/ItemEffectSystem.hpp"
#include "combat/StatSystem.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

static int countStacksByName(const Unit& unit, const std::string& effectName)
{
    int stacks = 0;
    for (const StatusEffect& e : unit.statusEffects())
    {
        if (e.remainingMs <= 0)
        {
            continue;
        }
        if (e.name == effectName)
        {
            ++stacks;
        }
    }
    return stacks;
}

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

static std::vector<Unit*> resolveTargets(GameState& state,
                                        Unit& owner,
                                        Unit& target,
                                        const AbilityEffect& effect)
{
    std::vector<Unit*> out;

    if (effect.areaShape == AreaShape::Self)
    {
        out.push_back(&owner);
        return out;
    }

    if (effect.areaShape == AreaShape::SingleTarget)
    {
        out.push_back(&target);
        return out;
    }

    const StatusEffectType t = effect.appliedStatusEffect.effectType;
    const bool isFriendly =
        t == StatusEffectType::Buff ||
        t == StatusEffectType::Shield ||
        t == StatusEffectType::HealOverTime ||
        t == StatusEffectType::DamageReduction ||
        t == StatusEffectType::BonusAttackDamage ||
        t == StatusEffectType::BonusAbilityPower ||
        t == StatusEffectType::BonusAttackSpeed ||
        t == StatusEffectType::BonusArmor ||
        t == StatusEffectType::BonusMagicResist ||
        t == StatusEffectType::CritChanceBonus ||
        t == StatusEffectType::CritDamageBonus;

    const TeamId teamFilter =
        (effect.appliesStatusEffect && isFriendly) ? owner.getTeamId() : target.getTeamId();

    const Position origin =
        effect.areaShape == AreaShape::CircleRadius ? owner.getPosition() : target.getPosition();

    std::vector<Unit*> unitsInArea =
        getUnitsInArea(
            state.board(),
            state.units(),
            origin,
            effect.areaShape,
            effect.radius,
            teamFilter,
            target.getPosition()
        );

    for (Unit* u : unitsInArea)
    {
        if (u && u->isAlive())
        {
            out.push_back(u);
        }
    }
    return out;
}

static void applyStatusWithStacking(GameState& state,
                                   Unit& owner,
                                   Unit& target,
                                   const AbilityEffect& effect,
                                   const std::string& itemName)
{
    if (!effect.appliesStatusEffect)
    {
        return;
    }

    const int stacks = countStacksByName(target, effect.appliedStatusEffect.name);
    if (effect.maxStacks > 0 && stacks >= effect.maxStacks)
    {
        return;
    }

    target.addStatusEffect(effect.appliedStatusEffect);

    std::ostringstream ss;
    ss << state.timeMs() << "ms ";
    if (!effect.name.empty())
    {
        ss << effect.name;
    }
    else
    {
        ss << itemName << " applies status [" << effect.appliedStatusEffect.name << "]";
    }
    ss << " to " << target.getName();
    if (effect.maxStacks > 0)
    {
        ss << " (" << (stacks + 1) << ")";
    }
    state.logger().combat(ss.str());
}

static void applyDamageEffect(GameState& state,
                             Unit& owner,
                             Unit& target,
                             const AbilityEffect& effect,
                             const std::string& itemName,
                             bool allowCrit)
{
    const float ad = StatSystem::getFinalStat(owner, StatType::AttackDamage);
    const float ap = StatSystem::getFinalStat(owner, StatType::AbilityPower);

    const std::int32_t adContribution = lroundToInt(ad * effect.damageFormula.adRatio);
    const std::int32_t apContribution = lroundToInt(ap * effect.damageFormula.apRatio);

    std::int32_t raw =
        effect.damageFormula.baseDamage + adContribution + apContribution;

    if (effect.targetMaxHpPercentDamage > 0.0f)
    {
        const std::int32_t targetMaxHp = std::max(1, StatSystem::getFinalStatInt(target, StatType::MaxHp));
        if (effect.targetMaxHpThreshold > 0 && targetMaxHp < effect.targetMaxHpThreshold)
        {
            return;
        }
        raw += lroundToInt(static_cast<float>(targetMaxHp) * effect.targetMaxHpPercentDamage);
    }

    if (raw <= 0)
    {
        return;
    }

    bool didCrit = false;
    float critChanceUsed = 0.0f;
    float critDamageUsed = 1.0f;
    std::int32_t rawAfterCrit = raw;

    if (allowCrit && effect.damageFormula.damageType == DamageType::Physical)
    {
        critChanceUsed =
            effect.critChanceOverride >= 0.0f
                ? effect.critChanceOverride
                : StatSystem::getFinalStat(owner, StatType::CritChance);

        critDamageUsed =
            effect.critDamageOverride >= 0.0f
                ? effect.critDamageOverride
                : StatSystem::getFinalStat(owner, StatType::CritDamage);

        critChanceUsed = std::clamp(critChanceUsed, 0.0f, 1.0f);
        critDamageUsed = std::max(1.0f, critDamageUsed);

        if (DamageSystem::rollChance(critChanceUsed))
        {
            didCrit = true;
            rawAfterCrit = lroundToInt(static_cast<float>(raw) * critDamageUsed);
        }
    }

    const float amp = std::max(0.0f, StatSystem::getFinalStat(owner, StatType::DamageAmplification));
    const std::int32_t rawAfterAmp =
        lroundToInt(static_cast<float>(rawAfterCrit) * (1.0f + amp));

    DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(rawAfterAmp, effect.damageFormula.damageType, target);

    target.applyDamage(dmg.finalDamage);

    const float omnivamp = std::clamp(StatSystem::getFinalStat(owner, StatType::Omnivamp), 0.0f, 1.0f);
    if (omnivamp > 0.0f && dmg.finalDamage > 0)
    {
        owner.heal(lroundToInt(static_cast<float>(dmg.finalDamage) * omnivamp));
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << itemName << " triggers on " << target.getName()
       << " | Raw: " << raw
       << " = " << adContribution << " AD + " << apContribution << " AP"
       << " | Type: "
       << (effect.damageFormula.damageType == DamageType::Physical ? "Physical" :
           effect.damageFormula.damageType == DamageType::Magic ? "Magic" : "True")
       << " | Final: " << dmg.finalDamage;

    if (didCrit)
    {
        ss << " | CRIT ("
           << lroundToInt(critChanceUsed * AIConstants::PercentScale) << "%, x" << critDamageUsed << ")";
    }

    state.logger().combat(ss.str());
}

static void applyHealFromDamage(GameState& state,
                               Unit& owner,
                               const AbilityEffect& effect,
                               const std::string& itemName,
                               std::int32_t damageDealt)
{
    if (effect.healPercentOfDamage <= 0.0f || damageDealt <= 0)
    {
        return;
    }

    const std::int32_t heal =
        std::max<std::int32_t>(1, lroundToInt(static_cast<float>(damageDealt) * effect.healPercentOfDamage));

    owner.heal(heal);

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << itemName << " heals " << owner.getName()
       << " for " << heal;
    state.logger().combat(ss.str());
}

static void executeItemEffects(GameState& state,
                              Unit& owner,
                              Unit& target,
                              const Item& item,
                              AbilityTrigger trigger,
                              std::int32_t damageDealt,
                              bool wasCrit)
{
    for (const AbilityEffect& effect : item.triggeredEffects)
    {
        if (effect.trigger != trigger)
        {
            continue;
        }

        const std::vector<Unit*> targets =
            resolveTargets(state, owner, target, effect);

        for (Unit* t : targets)
        {
            if (!t || !t->isAlive())
            {
                continue;
            }

            if (effect.damageFormula.baseDamage != 0 ||
                effect.damageFormula.adRatio != 0.0f ||
                effect.damageFormula.apRatio != 0.0f ||
                effect.targetMaxHpPercentDamage > 0.0f)
            {
                applyDamageEffect(state, owner, *t, effect, item.name, effect.canCrit);
            }

            applyStatusWithStacking(state, owner, *t, effect, item.name);

            if (trigger == AbilityTrigger::OnHit)
            {
                applyHealFromDamage(state, owner, effect, item.name, damageDealt);
            }

            if (trigger == AbilityTrigger::OnCrit && wasCrit)
            {
                applyStatusWithStacking(state, owner, *t, effect, item.name);
            }
        }
    }
}

namespace ItemSystem
{
    void onCombatStart(GameState& state)
    {
        ItemEffectSystem::onCombatStart(state);

        for (Unit& unit : state.units())
        {
            if (!unit.isAlive())
            {
                continue;
            }

            for (const Item& item : unit.items())
            {
                for (const AbilityEffect& effect : item.triggeredEffects)
                {
                    if (effect.trigger != AbilityTrigger::OnCombatStart)
                    {
                        continue;
                    }

                    Unit& dummyTarget = unit;
                    executeItemEffects(state, unit, dummyTarget, item, AbilityTrigger::OnCombatStart, 0, false);
                }
            }
        }
    }

    void onAttack(GameState& state, Unit& attacker, Unit& target)
    {
        for (const Item& item : attacker.items())
        {
            executeItemEffects(state, attacker, target, item, AbilityTrigger::OnAttack, 0, false);
        }
    }

    void onHit(GameState& state,
               Unit& attacker,
               Unit& target,
               std::int32_t finalDamage,
               DamageType,
               bool wasCrit)
    {
        for (const Item& item : attacker.items())
        {
            executeItemEffects(state, attacker, target, item, AbilityTrigger::OnHit, finalDamage, wasCrit);
        }
    }

    void onCrit(GameState& state, Unit& attacker, Unit& target)
    {
        for (const Item& item : attacker.items())
        {
            executeItemEffects(state, attacker, target, item, AbilityTrigger::OnCrit, 0, true);
        }
    }

    void onCast(GameState& state, Unit& caster, const Ability&, Unit* target)
    {
        if (!target)
        {
            return;
        }
        for (const Item& item : caster.items())
        {
            executeItemEffects(state, caster, *target, item, AbilityTrigger::OnCast, 0, false);
        }
    }

    void onLowHealth(GameState& state, Unit& unit)
    {
        Unit& dummyTarget = unit;
        for (const Item& item : unit.items())
        {
            executeItemEffects(state, unit, dummyTarget, item, AbilityTrigger::OnLowHealth, 0, false);
        }
    }

    void onDeath(GameState& state, Unit& unit)
    {
        Unit& dummyTarget = unit;
        for (const Item& item : unit.items())
        {
            executeItemEffects(state, unit, dummyTarget, item, AbilityTrigger::OnDeath, 0, false);
        }
    }
}
