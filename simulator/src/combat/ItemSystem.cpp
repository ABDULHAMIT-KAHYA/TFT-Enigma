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

static void applyDirectHeal(GameState& state,
                            Unit& owner,
                            Unit& target,
                            const AbilityEffect& effect,
                            const std::string& itemName,
                            bool& triggered)
{
    if (effect.healAmount <= 0)
    {
        return;
    }

    const std::int32_t before = target.getHp();
    target.heal(effect.healAmount);
    if (target.getHp() == before)
    {
        return;
    }

    triggered = true;
    std::ostringstream ss;
    ss << state.timeMs() << "ms " << itemName << " heals " << target.getName()
       << " for " << (target.getHp() - before)
       << " from " << owner.getName();
    state.logger().combat(ss.str());
}

static void applyDirectShield(GameState& state,
                              Unit& owner,
                              Unit& target,
                              const AbilityEffect& effect,
                              const std::string& itemName,
                              bool& triggered)
{
    if (effect.shieldAmount <= 0)
    {
        return;
    }

    StatusEffect shield{};
    shield.name = effect.name.empty() ? itemName + " Shield" : effect.name;
    shield.effectType = StatusEffectType::Shield;
    shield.crowdControlType = CrowdControlType::None;
    shield.affectedStat = StatType::None;
    shield.modifierType = ModifierType::Flat;
    shield.value = static_cast<float>(effect.shieldAmount);
    shield.durationMs = CombatConstants::TraitShieldOnCombatStartDurationMs;
    shield.remainingMs = shield.durationMs;
    shield.tickIntervalMs = 0;
    shield.tickTimerMs = 0;
    shield.damageType = DamageType::TrueDamage;

    target.addStatusEffect(shield);
    triggered = true;

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << itemName << " shields " << target.getName()
       << " for " << effect.shieldAmount
       << " from " << owner.getName();
    state.logger().combat(ss.str());
}

static std::size_t findUnitIndex(GameState& state, const Unit& unit)
{
    const std::vector<Unit>& units = state.units();
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        if (&units[i] == &unit)
        {
            return i;
        }
    }
    return units.size();
}

static Unit* firstAliveEnemy(GameState& state, const Unit& owner)
{
    for (Unit& unit : state.units())
    {
        if (unit.isAlive() && unit.isEnemyOf(owner))
        {
            return &unit;
        }
    }
    return nullptr;
}

static void executeItemEffects(GameState& state,
                              Unit& owner,
                              Unit& target,
                              const Item& item,
                              std::size_t ownerIndex,
                              std::size_t itemIndex,
                              AbilityTrigger trigger,
                              std::int32_t damageDealt,
                              bool wasCrit)
{
    for (std::size_t effectIndex = 0; effectIndex < item.triggeredEffects.size(); ++effectIndex)
    {
        const AbilityEffect& effect = item.triggeredEffects[effectIndex];
        if (effect.trigger != trigger)
        {
            continue;
        }
        if (!state.canTriggerItemEffect(ownerIndex, itemIndex, effectIndex, effect.cooldownMs, effect.oncePerCombat))
        {
            continue;
        }

        const std::vector<Unit*> targets =
            resolveTargets(state, owner, target, effect);

        bool triggered = false;
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
                triggered = true;
            }

            applyStatusWithStacking(state, owner, *t, effect, item.name);
            if (effect.appliesStatusEffect)
            {
                triggered = true;
            }

            applyDirectHeal(state, owner, *t, effect, item.name, triggered);
            applyDirectShield(state, owner, *t, effect, item.name, triggered);

            if (trigger == AbilityTrigger::OnHit || trigger == AbilityTrigger::OnDamage)
            {
                applyHealFromDamage(state, owner, effect, item.name, damageDealt);
                if (effect.healPercentOfDamage > 0.0f && damageDealt > 0)
                {
                    triggered = true;
                }
            }
        }
        if (triggered)
        {
            state.recordItemEffectTrigger(ownerIndex, itemIndex, effectIndex);
        }
    }
}

namespace ItemSystem
{
    void onCombatStart(GameState& state)
    {
        ItemEffectSystem::onCombatStart(state);

        std::vector<Unit>& units = state.units();
        for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
        {
            Unit& unit = units[unitIndex];
            if (!unit.isAlive())
            {
                continue;
            }

            const std::vector<Item>& items = unit.items();
            for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
            {
                const Item& item = items[itemIndex];
                Unit& dummyTarget = unit;
                executeItemEffects(state, unit, dummyTarget, item, unitIndex, itemIndex, AbilityTrigger::OnCombatStart, 0, false);
            }
        }
    }

    void onAttack(GameState& state, Unit& attacker, Unit& target)
    {
        std::size_t ownerIndex = 0;
        std::vector<Unit>& units = state.units();
        for (; ownerIndex < units.size(); ++ownerIndex)
        {
            if (&units[ownerIndex] == &attacker)
            {
                break;
            }
        }
        const std::vector<Item>& items = attacker.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, attacker, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnAttack, 0, false);
        }
    }

    void onHit(GameState& state,
               Unit& attacker,
               Unit& target,
               std::int32_t finalDamage,
               DamageType,
               bool wasCrit)
    {
        std::size_t ownerIndex = 0;
        std::vector<Unit>& units = state.units();
        for (; ownerIndex < units.size(); ++ownerIndex)
        {
            if (&units[ownerIndex] == &attacker)
            {
                break;
            }
        }
        const std::vector<Item>& items = attacker.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, attacker, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnHit, finalDamage, wasCrit);
        }
    }

    void onCrit(GameState& state, Unit& attacker, Unit& target)
    {
        std::size_t ownerIndex = 0;
        std::vector<Unit>& units = state.units();
        for (; ownerIndex < units.size(); ++ownerIndex)
        {
            if (&units[ownerIndex] == &attacker)
            {
                break;
            }
        }
        const std::vector<Item>& items = attacker.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, attacker, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnCrit, 0, true);
        }
    }

    void onCast(GameState& state, Unit& caster, const Ability&, Unit* target)
    {
        if (!target)
        {
            return;
        }
        std::size_t ownerIndex = 0;
        std::vector<Unit>& units = state.units();
        for (; ownerIndex < units.size(); ++ownerIndex)
        {
            if (&units[ownerIndex] == &caster)
            {
                break;
            }
        }
        const std::vector<Item>& items = caster.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, caster, *target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnCast, 0, false);
        }
    }

    void onDamage(GameState& state, Unit& source, Unit& target, std::int32_t damageDealt)
    {
        if (damageDealt <= 0 || !source.isAlive())
        {
            return;
        }

        const std::size_t ownerIndex = findUnitIndex(state, source);
        const std::vector<Item>& items = source.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, source, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnDamage, damageDealt, false);
        }
    }

    void onDamageTaken(GameState& state, Unit& unit, Unit* source, std::int32_t damageTaken)
    {
        if (damageTaken <= 0 || !unit.isAlive())
        {
            return;
        }

        Unit& target = source ? *source : unit;
        const std::size_t ownerIndex = findUnitIndex(state, unit);
        const std::vector<Item>& items = unit.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, unit, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnDamageTaken, damageTaken, false);
        }
    }

    void onKill(GameState& state, Unit& killer, Unit& victim)
    {
        if (!killer.isAlive())
        {
            return;
        }

        const std::size_t ownerIndex = findUnitIndex(state, killer);
        const std::vector<Item>& items = killer.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, killer, victim, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnKill, 0, false);
        }
    }
    void onLowHealth(GameState& state, Unit& unit)
    {
        Unit& dummyTarget = unit;
        std::size_t ownerIndex = 0;
        std::vector<Unit>& units = state.units();
        for (; ownerIndex < units.size(); ++ownerIndex)
        {
            if (&units[ownerIndex] == &unit)
            {
                break;
            }
        }
        const std::vector<Item>& items = unit.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, unit, dummyTarget, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnLowHealth, 0, false);
        }
    }

    void onDeath(GameState& state, Unit& unit, Unit* source)
    {
        Unit& target = source ? *source : unit;
        const std::size_t ownerIndex = findUnitIndex(state, unit);
        const std::vector<Item>& items = unit.items();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            executeItemEffects(state, unit, target, items[itemIndex], ownerIndex, itemIndex, AbilityTrigger::OnDeath, 0, false);
        }
    }

    void tick(GameState& state)
    {
        std::vector<Unit>& units = state.units();
        for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
        {
            Unit& unit = units[unitIndex];
            if (!unit.isAlive())
            {
                continue;
            }

            Unit* target = firstAliveEnemy(state, unit);
            if (!target)
            {
                continue;
            }

            const std::vector<Item>& items = unit.items();
            for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
            {
                executeItemEffects(state, unit, *target, items[itemIndex], unitIndex, itemIndex, AbilityTrigger::Periodic, 0, false);
            }
        }
    }
}
