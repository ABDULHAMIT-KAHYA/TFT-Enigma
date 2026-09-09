#include "combat/CombatEventExecutor.hpp"
#include "combat/AbilitySystem.hpp"
#include "combat/DamageSystem.hpp"
#include "combat/ItemSystem.hpp"
#include "combat/ProjectileSystem.hpp"
#include "combat/StatSystem.hpp"
#include "combat/TraitSystem.hpp"
#include "constants/CombatConstants.hpp"
#include "validation/CombatValidation.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    static std::int32_t projectileTravelMs(const Unit& attacker, const Unit& target)
    {
        if (attacker.getAttackRange() <= 1) return 0;
        const float dx = static_cast<float>(attacker.getPosition().x - target.getPosition().x);
        const float dy = static_cast<float>(attacker.getPosition().y - target.getPosition().y);
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float travelSec = dist / CombatConstants::ProjectileSpeedCellsPerSecond;
        return static_cast<std::int32_t>(std::lround(travelSec * static_cast<float>(CombatConstants::MsPerSecond)));
    }

    static bool hasAttackBlockingCc(const Unit& unit)
    {
        return unit.hasCrowdControl(CrowdControlType::Stun) ||
               unit.hasCrowdControl(CrowdControlType::Knockup) ||
               unit.hasCrowdControl(CrowdControlType::Suppression) ||
               unit.hasCrowdControl(CrowdControlType::Fear) ||
               unit.hasCrowdControl(CrowdControlType::Disarm);
    }

    static void logFizzle(GameState& state, const CombatEvent& event, const char* reason)
    {
        std::ostringstream ss;
        ss << state.timeMs() << "ms " << toString(event.type)
           << " source=" << event.sourceId
           << " target=" << event.targetId
           << " result=fizzle reason=" << reason;
        state.logger().combat(ss.str());
    }

    static void executeAutoAttackRelease(GameState& state, const CombatEvent& event)
    {
        Unit* attacker = state.findUnit(event.sourceId);
        Unit* target = state.findUnit(event.targetId);
        if (!attacker || !target) { logFizzle(state, event, "missing_unit"); return; }
        if (!attacker->isAlive()) { logFizzle(state, event, "attacker_dead"); return; }
        if (!target->isAlive()) { logFizzle(state, event, "target_dead"); return; }
        if (!target->isEnemyOf(*attacker)) { logFizzle(state, event, "not_enemy"); return; }
        if (target->isUntargetable()) { logFizzle(state, event, "target_untargetable"); return; }
        if (attacker->isCasting()) { logFizzle(state, event, "attacker_casting"); return; }
        if (hasAttackBlockingCc(*attacker)) { logFizzle(state, event, "attacker_cc"); return; }

        const std::int32_t manaGain = StatSystem::getFinalStatInt(*attacker, StatType::ManaGainOnAttack);
        attacker->gainMana(manaGain);

        TraitSystem::onAttack(state, *attacker, *target);
        ItemSystem::onAttack(state, *attacker, *target);
        AbilitySystem::executeTrigger(state, *attacker, target, AbilityTrigger::OnAttack);

        const std::int32_t rawBeforeCrit = StatSystem::getFinalStatInt(*attacker, StatType::AttackDamage);
        bool didCrit = false;
        float critChanceUsed = std::clamp(StatSystem::getFinalStat(*attacker, StatType::CritChance), 0.0f, 1.0f);
        float critDamageUsed = std::max(1.0f, StatSystem::getFinalStat(*attacker, StatType::CritDamage));
        std::int32_t rawAfterCrit = rawBeforeCrit;
        if (DamageSystem::rollChance(critChanceUsed))
        {
            didCrit = true;
            rawAfterCrit = static_cast<std::int32_t>(std::lround(static_cast<float>(rawBeforeCrit) * critDamageUsed));
        }

        ProjectileSpec spec{};
        spec.attackerId = attacker->id();
        spec.targetId = target->id();
        spec.damageType = attacker->getAutoAttackDamageType();
        spec.rawDamage = rawAfterCrit;
        spec.didCrit = didCrit;
        spec.critChanceUsed = critChanceUsed;
        spec.critDamageUsed = critDamageUsed;
        spec.rawBeforeCrit = rawBeforeCrit;
        spec.rawAfterCrit = rawAfterCrit;
        spec.travelTimeMs = projectileTravelMs(*attacker, *target);
        spec.debugName = "AutoAttack";
        ProjectileSystem::spawnAutoAttackProjectile(state, spec);
    }

    static ProjectileSpec projectileFromEvent(const CombatEvent& event)
    {
        ProjectileSpec spec{};
        spec.attackerId = event.sourceId;
        spec.targetId = event.targetId;
        spec.damageType = event.projectile.damageType;
        spec.rawDamage = event.projectile.rawDamage;
        spec.didCrit = event.projectile.didCrit;
        spec.critChanceUsed = event.projectile.critChanceUsed;
        spec.critDamageUsed = event.projectile.critDamageUsed;
        spec.rawBeforeCrit = event.projectile.rawBeforeCrit;
        spec.rawAfterCrit = event.projectile.rawAfterCrit;
        spec.debugName = event.debugName;
        return spec;
    }
}

namespace CombatEventExecutor
{
    void execute(GameState& state, const CombatEvent& event)
    {
        switch (event.type)
        {
            case CombatEventType::DebugMarker:
                return;

            case CombatEventType::AutoAttackRelease:
                executeAutoAttackRelease(state, event);
                return;

            case CombatEventType::ProjectileHit:
                ProjectileSystem::resolveAutoAttackHit(state, projectileFromEvent(event));
                return;

            case CombatEventType::SpellResolve:
            {
                Unit* caster = state.findUnit(event.sourceId);
                Unit* target = state.findUnit(event.targetId);
                if (!caster || !caster->isAlive()) { logFizzle(state, event, "caster_dead_or_missing"); return; }
                if (!caster->isCasting()) { logFizzle(state, event, "cast_interrupted"); return; }
                caster->resetManaAfterCast();
                AbilitySystem::executeTrigger(state, *caster, target, AbilityTrigger::OnCast);
                if (CombatValidation::enabled() && CombatValidation::detailedLogs())
                {
                    std::ostringstream ss;
                    ss << "CAST_RELEASE " << state.timeMs() << "ms Unit " << caster->id() << " " << caster->getName();
                    state.logger().combat(ss.str());
                }
                return;
            }

            case CombatEventType::SpellEnd:
            {
                Unit* caster = state.findUnit(event.sourceId);
                if (!caster || !caster->isAlive()) { logFizzle(state, event, "caster_dead_or_missing"); return; }
                if (!caster->isCasting()) { logFizzle(state, event, "not_casting"); return; }
                caster->endCast();
                if (CombatValidation::enabled() && CombatValidation::detailedLogs())
                {
                    std::ostringstream ss;
                    ss << "CAST_END " << state.timeMs() << "ms Unit " << caster->id() << " " << caster->getName();
                    state.logger().combat(ss.str());
                }
                return;
            }

            case CombatEventType::AbilityEffect:
                AbilitySystem::executeEffectEvent(state, event);
                return;
        }
    }
}
