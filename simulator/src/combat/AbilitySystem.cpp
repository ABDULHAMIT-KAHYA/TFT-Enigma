// AbilitySystem.cpp
#include "combat/AbilitySystem.hpp"
#include "constants/AIConstants.hpp"
#include "core/Board.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/DamageSystem.hpp"
#include "validation/CombatValidation.hpp"
#include "combat/ManaSystem.hpp"
#include "combat/StatSystem.hpp"
#include "combat/TraitSystem.hpp"
#include "combat/ItemSystem.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

static const char* toString(AbilityTrigger trigger)
{
    switch (trigger)
    {
        case AbilityTrigger::Passive:        return "Passive";
        case AbilityTrigger::OnCombatStart:  return "OnCombatStart";
        case AbilityTrigger::OnAttack:       return "OnAttack";
        case AbilityTrigger::OnCast:         return "OnCast";
        case AbilityTrigger::OnHit:          return "OnHit";
        case AbilityTrigger::OnCrit:         return "OnCrit";
        case AbilityTrigger::OnKill:         return "OnKill";
        case AbilityTrigger::OnDeath:        return "OnDeath";
        case AbilityTrigger::OnLowHealth:    return "OnLowHealth";
        case AbilityTrigger::OnDamageTaken:  return "OnDamageTaken";
    }
    return "Unknown";
}

static const char* toString(DamageType type)
{
    switch (type)
    {
        case DamageType::Physical:   return "Physical";
        case DamageType::Magic:      return "Magic";
        case DamageType::TrueDamage: return "True";
    }
    return "Unknown";
}

static TeamId enemyTeamOf(TeamId team)
{
    return team == TeamId::TeamA ? TeamId::TeamB : TeamId::TeamA;
}

static std::int32_t lroundToInt(float v)
{
    return static_cast<std::int32_t>(std::lround(v));
}

static bool shouldApplyDamage(const DamageFormula& f)
{
    return f.baseDamage != 0 || f.adRatio != 0.0f || f.apRatio != 0.0f;
}

static void logTriggerIfAny(GameState& state, const Unit& owner, AbilityTrigger trigger)
{
    const Ability& ability = owner.getAbility();
    bool any = false;
    for (const AbilityEffect& effect : ability.effects)
    {
        if (effect.trigger == trigger)
        {
            any = true;
            break;
        }
    }
    if (!any)
    {
        return;
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << owner.getName()
       << " trigger " << toString(trigger)
       << " [" << ability.name << "]";
    state.logger().combat(ss.str());
}

static void applyStatusEffectWithLog(GameState& state, Unit& applier, Unit& target, const StatusEffect& effect)
{
    target.addStatusEffect(effect);
    std::ostringstream ss;
    ss << state.timeMs() << "ms "
       << applier.getName()
       << " applies status [" << effect.name << "] to "
       << target.getName();
    state.logger().combat(ss.str());
}

static DamageDebugResult applyDamageWithFormulaLog(GameState& state,
                                                  Unit& caster,
                                                  Unit& target,
                                                  const Ability& ability,
                                                  const AbilityEffect& effect,
                                                  std::int32_t rawBeforeCrit,
                                                  std::int32_t adContribution,
                                                  std::int32_t apContribution,
                                                  bool didCrit,
                                                  float critChanceUsed,
                                                  float critDamageUsed,
                                                  std::int32_t rawAfterCrit)
{
    const float amp = std::max(0.0f, StatSystem::getFinalStat(caster, StatType::DamageAmplification));
    const std::int32_t rawAfterAmp =
        static_cast<std::int32_t>(std::lround(static_cast<float>(rawAfterCrit) * (1.0f + amp)));

    DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(rawAfterAmp, effect.damageFormula.damageType, target);

    const std::int32_t targetHpBefore = target.getHp();
    target.applyDamage(dmg.finalDamage);
    const std::int32_t targetHpAfter = target.getHp();
    const std::int32_t hpLost = std::max<std::int32_t>(0, targetHpBefore - targetHpAfter);
    const std::int32_t manaFromDamageTaken = ManaSystem::applyDamageTakenMana(state, target, hpLost);

    const float omnivamp = std::clamp(StatSystem::getFinalStat(caster, StatType::Omnivamp), 0.0f, 1.0f);
    if (omnivamp > 0.0f && dmg.finalDamage > 0)
    {
        caster.heal(static_cast<std::int32_t>(std::lround(static_cast<float>(dmg.finalDamage) * omnivamp)));
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms ";
    if (!effect.name.empty())
    {
        ss << effect.name << " ";
    }
    ss << caster.getName() << " hits " << target.getName()
       << " | Raw: " << rawBeforeCrit
       << " = " << adContribution << " AD + " << apContribution << " AP"
       << " | Type: " << toString(effect.damageFormula.damageType)
       << " | "
       << (dmg.damageType == DamageType::Physical ? "ARMOR" :
           dmg.damageType == DamageType::Magic ? "MR" : "DEF")
       << ": " << dmg.defenseUsed
       << " | Durability: " << lroundToInt(dmg.durabilityUsed * AIConstants::PercentScale) << "%"
       << " | Final: " << dmg.finalDamage;

    if (didCrit)
    {
        ss << " | CRIT (" << lroundToInt(critChanceUsed * AIConstants::PercentScale) << "%, x" << critDamageUsed
           << ", raw->" << rawAfterCrit << ")";
    }

    state.logger().combat(ss.str());
    if (CombatValidation::enabled())
    {
        CombatValidation::logAbilityHit(
            state,
            caster,
            target,
            effect.name.empty() ? ability.name : effect.name,
            rawBeforeCrit,
            dmg,
            didCrit,
            critChanceUsed,
            critDamageUsed,
            rawAfterAmp,
            manaFromDamageTaken
        );
    }
    return dmg;
}

static void executeEffectNow(GameState& state,
                            Unit& owner,
                            Unit* primaryTarget,
                            AbilityTrigger trigger,
                            const Ability& ability,
                            const AbilityEffect& effect)
{
    Unit* primary = primaryTarget ? primaryTarget : &owner;
    Unit* resolved = AbilitySystem::resolveTarget(owner, *primary, state.units(), ability.targetType);
    if (!resolved)
    {
        resolved = &owner;
    }

    if (!owner.isAlive())
    {
        return;
    }

    const Position directionTargetPos = resolved->getPosition();

    std::vector<Unit*> affected;

    if (effect.areaShape == AreaShape::Self)
    {
        affected.push_back(&owner);
    }
    else if (effect.areaShape == AreaShape::SingleTarget)
    {
        if (resolved->isAlive())
        {
            affected.push_back(resolved);
        }
    }
    else
    {
        const TeamId teamFilter = resolved->getTeamId();
        affected = getUnitsInArea(
            state.board(),
            state.units(),
            resolved->getPosition(),
            effect.areaShape,
            effect.radius,
            teamFilter,
            directionTargetPos
        );
    }

    for (Unit* target : affected)
    {
        if (!target || !target->isAlive())
        {
            continue;
        }

        if (effect.healAmount > 0)
        {
            target->heal(effect.healAmount);
            std::ostringstream ss;
            ss << state.timeMs() << "ms ";
            if (!effect.name.empty())
            {
                ss << effect.name << " ";
            }
            ss << owner.getName() << " heals " << target->getName()
               << " for " << effect.healAmount
               << " | " << target->getName() << " HP: " << target->getHp();
            state.logger().combat(ss.str());
        }

        if (shouldApplyDamage(effect.damageFormula))
        {
            const float ad = StatSystem::getFinalStat(owner, StatType::AttackDamage);
            const float ap = StatSystem::getFinalStat(owner, StatType::AbilityPower);

            const std::int32_t adContribution = lroundToInt(ad * effect.damageFormula.adRatio);
            const std::int32_t apContribution = lroundToInt(ap * effect.damageFormula.apRatio);

            const std::int32_t rawBeforeCrit =
                effect.damageFormula.baseDamage + adContribution + apContribution;

            bool didCrit = false;
            float critChanceUsed = 0.0f;
            float critDamageUsed = 1.0f;
            std::int32_t rawAfterCrit = rawBeforeCrit;

            if (effect.canCrit && effect.damageFormula.damageType == DamageType::Physical)
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
                    rawAfterCrit = lroundToInt(static_cast<float>(rawBeforeCrit) * critDamageUsed);
                }
            }

            DamageDebugResult dmg = applyDamageWithFormulaLog(
                state,
                owner,
                *target,
                ability,
                effect,
                rawBeforeCrit,
                adContribution,
                apContribution,
                didCrit,
                critChanceUsed,
                critDamageUsed,
                rawAfterCrit
            );

        if (didCrit)
        {
            TraitSystem::onCrit(state, owner, *target);
            ItemSystem::onCrit(state, owner, *target);
        }

            AbilitySystem::executeTrigger(state, owner, target, AbilityTrigger::OnHit);
            AbilitySystem::executeTrigger(state, *target, &owner, AbilityTrigger::OnDamageTaken);
        TraitSystem::afterDamage(state, owner, *target);
        ItemSystem::onHit(state, owner, *target, dmg.finalDamage, dmg.damageType, didCrit);

            if (!target->isAlive())
            {
                AbilitySystem::executeTrigger(state, owner, target, AbilityTrigger::OnKill);
                AbilitySystem::executeTrigger(state, *target, &owner, AbilityTrigger::OnDeath);
            TraitSystem::onKill(state, owner, *target);
            ItemSystem::onDeath(state, *target);
            }
            else
            {
                const float effectiveMaxHp = std::max(1.0f, StatSystem::getFinalStat(*target, StatType::MaxHp));
                const float hpPct = static_cast<float>(target->getHp()) / effectiveMaxHp;
                if (hpPct <= CombatConstants::LowHealthThresholdPct)
                {
                    ItemSystem::onLowHealth(state, *target);
                }
            }
        }

        if (effect.appliesStatusEffect)
        {
            applyStatusEffectWithLog(state, owner, *target, effect.appliedStatusEffect);
        }
    }
}

namespace AbilitySystem
{
    Unit* resolveTarget(Unit& caster,
                        Unit& primaryTarget,
                        std::vector<Unit>& allUnits,
                        TargetType targetType)
    {
        switch (targetType)
        {
            case TargetType::Self:
                return &caster;

            case TargetType::LowestHpAlly:
            {
                Unit* lowestHpAlly = nullptr;
                for (Unit& unit : allUnits)
                {
                    if (!unit.isAlive() || unit.isEnemyOf(caster))
                    {
                        continue;
                    }
                    if (lowestHpAlly == nullptr || unit.getHp() < lowestHpAlly->getHp())
                    {
                        lowestHpAlly = &unit;
                    }
                }
                return lowestHpAlly ? lowestHpAlly : &caster;
            }

            case TargetType::CurrentEnemy:
            case TargetType::NearestEnemy:
            default:
                return &primaryTarget;
        }
    }

    void executeTrigger(GameState& state,
                        Unit& owner,
                        Unit* primaryTarget,
                        AbilityTrigger trigger)
    {
        const Ability& ability = owner.getAbility();
        if (ability.effects.empty())
        {
            return;
        }

        logTriggerIfAny(state, owner, trigger);

        for (const AbilityEffect& effect : ability.effects)
        {
            if (effect.trigger != trigger)
            {
                continue;
            }

            if (effect.delayMs > 0)
            {
                const std::int32_t executeAt = state.timeMs() + effect.delayMs;
                const AbilityEffect effectCopy = effect;

                Unit* ownerPtr = &owner;
                Unit* primaryPtr = primaryTarget;

                std::string eventName;
                if (!effect.name.empty())
                {
                    eventName = effect.name;
                }
                else
                {
                    eventName = ability.name;
                }

                state.scheduleCombatEvent(
                    executeAt,
                    [ownerPtr, primaryPtr, &state, effectCopy]()
                    {
                        if (!ownerPtr || !ownerPtr->isAlive())
                        {
                            return;
                        }
                        const Ability& abilityAtExecution = ownerPtr->getAbility();
                        executeEffectNow(
                            state,
                            *ownerPtr,
                            primaryPtr,
                            effectCopy.trigger,
                            abilityAtExecution,
                            effectCopy
                        );
                    },
                    eventName
                );
            }
            else
            {
                executeEffectNow(state, owner, primaryTarget, trigger, ability, effect);
            }
        }
    }

    bool tryCast(GameState& state,
                 Unit& caster,
                 Unit& primaryTarget)
    {
        if (!caster.isAlive())
        {
            return false;
        }
        if (!caster.canCastNow())
        {
            return false;
        }
        if (!caster.canCastAbility())
        {
            return false;
        }

        const Ability& ability = caster.getAbility();

        std::ostringstream ss;
        ss << state.timeMs() << "ms " << caster.getName()
           << " casts " << ability.name;
        state.logger().combat(ss.str());

        TraitSystem::onCast(state, caster, ability, &primaryTarget);
        ItemSystem::onCast(state, caster, ability, &primaryTarget);

        caster.resetManaAfterCast();

        executeTrigger(state, caster, &primaryTarget, AbilityTrigger::OnCast);
        return true;
    }
}
