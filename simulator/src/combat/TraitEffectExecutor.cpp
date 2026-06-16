#include "combat/TraitEffectExecutor.hpp"
#include "constants/CombatConstants.hpp"
#include "content/ContentManager.hpp"
#include "combat/DamageSystem.hpp"
#include "combat/StatSystem.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

static const char* teamName(TeamId team)
{
    return team == TeamId::TeamA ? "Team A" : "Team B";
}

static TeamId enemyTeamOf(TeamId team)
{
    return team == TeamId::TeamA ? TeamId::TeamB : TeamId::TeamA;
}

static const TraitTier* findTier(const TraitDefinition& def, int activeBreakpoint)
{
    const TraitTier* best = nullptr;
    int bestBp = -1;
    for (const TraitTier& t : def.tiers)
    {
        if (t.breakpoint <= 0 || t.breakpoint > activeBreakpoint)
        {
            continue;
        }
        if (t.breakpoint >= bestBp)
        {
            bestBp = t.breakpoint;
            best = &t;
        }
    }
    return best;
}

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

static void applyStatus(GameState& state, Unit& applier, Unit& target, const StatusEffect& effect)
{
    target.addStatusEffect(effect);

    if (effect.affectedStat == StatType::MaxHp
        && effect.modifierType == ModifierType::Flat
        && effect.value > 0.0f)
    {
        target.heal(static_cast<std::int32_t>(std::lround(effect.value)));
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << applier.getName()
       << " applies status [" << effect.name << "] to " << target.getName();
    state.logger().combat(ss.str());
}

static Unit* findApplier(GameState& state, TeamId team, const TraitDefinition& def, Unit* source)
{
    if (source && source->isAlive() && source->getTeamId() == team)
    {
        return source;
    }

    for (Unit& u : state.units())
    {
        if (u.isAlive() && u.getTeamId() == team && u.hasTrait(def.trait.name))
        {
            return &u;
        }
    }

    for (Unit& u : state.units())
    {
        if (u.isAlive() && u.getTeamId() == team)
        {
            return &u;
        }
    }

    return nullptr;
}

static void applyStatusToTeam(GameState& state,
                              TeamId targetTeam,
                              Unit& applier,
                              const StatusEffect& effect)
{
    for (Unit& u : state.units())
    {
        if (!u.isAlive() || u.getTeamId() != targetTeam)
        {
            continue;
        }
        applyStatus(state, applier, u, effect);
    }
}

static void applyShield(GameState& state, Unit& applier, Unit& target, int amount, std::int32_t durationMs)
{
    if (amount <= 0)
    {
        return;
    }

    StatusEffect shield{
        "Trait Shield",
        StatusEffectType::Shield,
        CrowdControlType::None,
        StatType::None,
        ModifierType::Flat,
        static_cast<float>(amount),
        durationMs,
        durationMs,
        0,
        0,
        DamageType::TrueDamage
    };

    applyStatus(state, applier, target, shield);
}

static void applyShieldToTeam(GameState& state,
                              TeamId targetTeam,
                              Unit& applier,
                              int amount,
                              std::int32_t durationMs)
{
    for (Unit& u : state.units())
    {
        if (!u.isAlive() || u.getTeamId() != targetTeam)
        {
            continue;
        }
        applyShield(state, applier, u, amount, durationMs);
    }
}

static void applyHeal(GameState& state, Unit& applier, Unit& target, int amount)
{
    if (amount <= 0)
    {
        return;
    }

    const std::int32_t hpBefore = target.getHp();
    target.heal(amount);

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << applier.getName()
       << " heals " << target.getName()
       << " for " << (target.getHp() - hpBefore);
    state.logger().combat(ss.str());
}

static void applyHealToTeam(GameState& state,
                            TeamId targetTeam,
                            Unit& applier,
                            int amount)
{
    for (Unit& u : state.units())
    {
        if (!u.isAlive() || u.getTeamId() != targetTeam)
        {
            continue;
        }
        applyHeal(state, applier, u, amount);
    }
}

static bool shouldApplyDamage(const DamageFormula& formula)
{
    return formula.baseDamage != 0 ||
           formula.adRatio != 0.0f ||
           formula.apRatio != 0.0f;
}

static const char* damageTypeName(DamageType type)
{
    switch (type)
    {
        case DamageType::Physical: return "Physical";
        case DamageType::Magic: return "Magic";
        case DamageType::TrueDamage: return "True";
    }
    return "Unknown";
}

static std::int32_t lroundToInt(float v)
{
    return static_cast<std::int32_t>(std::lround(v));
}

static void applyDamage(GameState& state,
                        const TraitDefinition& def,
                        Unit& applier,
                        Unit& target,
                        const DamageFormula& formula)
{
    if (!shouldApplyDamage(formula))
    {
        return;
    }

    const float ad = StatSystem::getFinalStat(applier, StatType::AttackDamage);
    const float ap = StatSystem::getFinalStat(applier, StatType::AbilityPower);
    const std::int32_t adContribution = lroundToInt(ad * formula.adRatio);
    const std::int32_t apContribution = lroundToInt(ap * formula.apRatio);
    const std::int32_t raw = formula.baseDamage + adContribution + apContribution;
    if (raw <= 0)
    {
        return;
    }

    const DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(raw, formula.damageType, target);
    target.applyDamage(dmg.finalDamage);

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << applier.getName()
       << " deals trait damage [" << def.trait.name << "] to " << target.getName()
       << " | Raw: " << raw
       << " = " << formula.baseDamage << " base + "
       << adContribution << " AD + " << apContribution << " AP"
       << " | Type: " << damageTypeName(formula.damageType)
       << " | Final: " << dmg.finalDamage;
    state.logger().combat(ss.str());
}

static void applyDamageToTeam(GameState& state,
                              const TraitDefinition& def,
                              TeamId targetTeam,
                              Unit& applier,
                              const DamageFormula& formula)
{
    for (Unit& u : state.units())
    {
        if (!u.isAlive() || u.getTeamId() != targetTeam)
        {
            continue;
        }
        applyDamage(state, def, applier, u, formula);
    }
}

static void logActivation(GameState& state, TeamId team, const ActiveTrait& trait)
{
    if (trait.breakpoint <= 0)
    {
        return;
    }
    std::ostringstream ss;
    ss << state.timeMs() << "ms " << teamName(team)
       << " activates " << trait.traitName << " (" << trait.activeCount << ")";
    state.logger().combat(ss.str());
}

static void runEffectsForHook(GameState& state,
                              const TraitDefinition& def,
                              const TraitTier& tier,
                              TeamId team,
                              TraitHook hook,
                              Unit* source,
                              Unit* target)
{
    for (const TraitEffect& effect : tier.effects)
    {
        if (effect.hook != hook)
        {
            continue;
        }

        if (effect.type == TraitEffectType::ApplyStatusToTraitUnits)
        {
            for (Unit& u : state.units())
            {
                if (!u.isAlive() || u.getTeamId() != team)
                {
                    continue;
                }
                if (!u.hasTrait(def.trait.name))
                {
                    continue;
                }
                applyStatus(state, u, u, effect.statusEffect);
            }
        }
        else if (effect.type == TraitEffectType::ApplyStatusToAllies)
        {
            Unit* applierPtr = findApplier(state, team, def, source);
            if (!applierPtr)
            {
                continue;
            }

            applyStatusToTeam(state, team, *applierPtr, effect.statusEffect);
        }
        else if (effect.type == TraitEffectType::ApplyStatusToEnemies ||
                 effect.type == TraitEffectType::ApplyStatusToEnemyTeam)
        {
            const TeamId enemy = enemyTeamOf(team);
            Unit* applierPtr = findApplier(state, team, def, source);
            if (!applierPtr)
            {
                continue;
            }

            applyStatusToTeam(state, enemy, *applierPtr, effect.statusEffect);
        }
        else if (effect.type == TraitEffectType::Shield)
        {
            Unit* applierPtr = findApplier(state, team, def, source);
            if (!applierPtr)
            {
                continue;
            }

            const std::int32_t durationMs =
                effect.statusEffect.durationMs > 0
                    ? effect.statusEffect.durationMs
                    : CombatConstants::TraitShieldOnCombatStartDurationMs;
            applyShieldToTeam(state, team, *applierPtr, effect.shieldAmount, durationMs);
        }
        else if (effect.type == TraitEffectType::Heal)
        {
            Unit* applierPtr = findApplier(state, team, def, source);
            if (!applierPtr)
            {
                continue;
            }

            applyHealToTeam(state, team, *applierPtr, effect.healAmount);
        }
        else if (effect.type == TraitEffectType::DealDamage)
        {
            Unit* applierPtr = findApplier(state, team, def, source);
            if (!applierPtr)
            {
                continue;
            }

            applyDamageToTeam(state, def, enemyTeamOf(team), *applierPtr, effect.damageFormula);
        }
        else if (effect.type == TraitEffectType::StackStatusOnAttack)
        {
            if (!source || !source->isAlive() || source->getTeamId() != team)
            {
                continue;
            }
            if (!source->hasTrait(def.trait.name))
            {
                continue;
            }

            const int stacks = countStacksByName(*source, effect.statusEffect.name);
            if (effect.maxStacks > 0 && stacks >= effect.maxStacks)
            {
                continue;
            }

            source->addStatusEffect(effect.statusEffect);

            std::ostringstream ss;
            ss << state.timeMs() << "ms "
               << def.trait.name << " stack applied to " << source->getName()
               << " (" << (stacks + 1) << ")";
            state.logger().combat(ss.str());
        }
        else if (effect.type == TraitEffectType::ShieldOnCombatStart)
        {
            for (Unit& u : state.units())
            {
                if (!u.isAlive() || u.getTeamId() != team)
                {
                    continue;
                }
                if (!u.hasTrait(def.trait.name))
                {
                    continue;
                }
                applyShield(state, u, u, effect.shieldAmount, CombatConstants::TraitShieldOnCombatStartDurationMs);
            }
        }
        else if (effect.type == TraitEffectType::TempCritBonusVsLowHp)
        {
            if (!source || !target)
            {
                continue;
            }
            if (!source->isAlive() || !target->isAlive())
            {
                continue;
            }
            if (source->getTeamId() != team)
            {
                continue;
            }
            if (!source->hasTrait(def.trait.name))
            {
                continue;
            }

            const float maxHp = std::max(1.0f, StatSystem::getFinalStat(*target, StatType::MaxHp));
            const float pct = static_cast<float>(target->getHp()) / maxHp;
            if (effect.targetHpPercentThreshold >= 0.0f && pct > effect.targetHpPercentThreshold)
            {
                continue;
            }

            if (countStacksByName(*source, effect.statusEffect.name) == 0)
            {
                source->addStatusEffect(effect.statusEffect);
            }
        }
        else if (effect.type == TraitEffectType::ExecuteBelowHpPercent)
        {
            if (!source || !target)
            {
                continue;
            }
            if (!target->isAlive())
            {
                continue;
            }

            const float maxHp = std::max(1.0f, StatSystem::getFinalStat(*target, StatType::MaxHp));
            const float pct = static_cast<float>(target->getHp()) / maxHp;
            if (effect.targetHpPercentThreshold >= 0.0f && pct <= effect.targetHpPercentThreshold)
            {
                target->setHp(0);
                std::ostringstream ss;
                ss << state.timeMs() << "ms " << source->getName()
                   << " executes " << target->getName()
                   << " via " << def.trait.name;
                state.logger().combat(ss.str());
            }
        }
    }
}

void TraitEffectExecutor::apply(GameState& state,
                               TeamId team,
                               const std::vector<ActiveTrait>& activeTraits,
                               TraitHook hook,
                               Unit* source,
                               Unit* target)
{
    const auto& defs = state.content().traits();

    for (const ActiveTrait& at : activeTraits)
    {
        auto it = defs.find(at.traitName);
        if (it == defs.end())
        {
            continue;
        }
        const TraitDefinition& def = it->second;
        if (hook == TraitHook::OnCombatStart)
        {
            logActivation(state, team, at);
        }

        const TraitTier* tier = findTier(def, at.breakpoint);
        if (!tier)
        {
            continue;
        }
        runEffectsForHook(state, def, *tier, team, hook, source, target);
    }
}
