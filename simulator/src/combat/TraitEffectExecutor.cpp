#include "combat/TraitEffectExecutor.hpp"
#include "constants/CombatConstants.hpp"
#include "content/ContentManager.hpp"
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
        0,
        0,
        0,
        DamageType::TrueDamage
    };

    applyStatus(state, applier, target, shield);
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
        else if (effect.type == TraitEffectType::ApplyStatusToEnemyTeam)
        {
            const TeamId enemy = enemyTeamOf(team);
            Unit* applierPtr = source;
            if (!applierPtr)
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
                    applierPtr = &u;
                    break;
                }
            }

            if (!applierPtr)
            {
                continue;
            }

            for (Unit& u : state.units())
            {
                if (!u.isAlive() || u.getTeamId() != enemy)
                {
                    continue;
                }
                applyStatus(state, *applierPtr, u, effect.statusEffect);
            }
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
