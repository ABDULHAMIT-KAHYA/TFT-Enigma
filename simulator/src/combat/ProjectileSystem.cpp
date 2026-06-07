#include "combat/ProjectileSystem.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/AbilitySystem.hpp"
#include "validation/CombatValidation.hpp"
#include "combat/DamageSystem.hpp"
#include "combat/ItemSystem.hpp"
#include "combat/ManaSystem.hpp"
#include "combat/StatSystem.hpp"
#include "combat/TraitSystem.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

static std::int32_t lroundToInt(float v)
{
    return static_cast<std::int32_t>(std::lround(v));
}

static void resolveAutoAttackHit(GameState& state, const ProjectileSpec& spec)
{
    std::vector<Unit>& units = state.units();
    if (spec.attackerIndex < 0 || spec.targetIndex < 0)
    {
        return;
    }
    if (spec.attackerIndex >= static_cast<std::int32_t>(units.size()) ||
        spec.targetIndex >= static_cast<std::int32_t>(units.size()))
    {
        return;
    }

    Unit& attacker = units[spec.attackerIndex];
    Unit& target = units[spec.targetIndex];

    if (!attacker.isAlive() || !target.isAlive())
    {
        return;
    }
    if (!target.isEnemyOf(attacker))
    {
        return;
    }
    if (target.isUntargetable())
    {
        return;
    }

    const float amp = std::max(0.0f, StatSystem::getFinalStat(attacker, StatType::DamageAmplification));
    const std::int32_t rawAfterAmp =
        lroundToInt(static_cast<float>(spec.rawDamage) * (1.0f + amp));

    DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(rawAfterAmp, spec.damageType, target);

    const std::int32_t targetHpBefore = target.getHp();
    target.applyDamage(dmg.finalDamage);
    const std::int32_t targetHpAfter = target.getHp();
    const std::int32_t hpLost = std::max<std::int32_t>(0, targetHpBefore - targetHpAfter);

    const std::int32_t manaFromDamageTaken =
        ManaSystem::applyDamageTakenMana(state, target, hpLost);

    const float omnivamp = std::clamp(StatSystem::getFinalStat(attacker, StatType::Omnivamp), 0.0f, 1.0f);
    if (omnivamp > 0.0f && dmg.finalDamage > 0)
    {
        attacker.heal(lroundToInt(static_cast<float>(dmg.finalDamage) * omnivamp));
    }

    if (CombatValidation::enabled() && CombatValidation::detailedLogs())
    {
        std::ostringstream ss;
        ss << "PROJECTILE_HIT " << state.timeMs() << "ms "
           << attacker.getName() << " -> " << target.getName()
           << " | raw=" << spec.rawDamage
           << " | final=" << dmg.finalDamage
           << " | manaTaken=" << manaFromDamageTaken;
        state.logger().combat(ss.str());
    }

    if (spec.didCrit)
    {
        TraitSystem::onCrit(state, attacker, target);
        ItemSystem::onCrit(state, attacker, target);
    }

    TraitSystem::onHit(state, attacker, target);
    AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::OnHit);
    AbilitySystem::executeTrigger(state, target, &attacker, AbilityTrigger::OnDamageTaken);
    TraitSystem::afterDamage(state, attacker, target);
    ItemSystem::onHit(state, attacker, target, dmg.finalDamage, dmg.damageType, spec.didCrit);

    if (!target.isAlive())
    {
        AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::OnKill);
        AbilitySystem::executeTrigger(state, target, &attacker, AbilityTrigger::OnDeath);
        TraitSystem::onKill(state, attacker, target);
        ItemSystem::onDeath(state, target);
    }
    else
    {
        const float effectiveMaxHp = std::max(1.0f, StatSystem::getFinalStat(target, StatType::MaxHp));
        const float hpPct = static_cast<float>(target.getHp()) / effectiveMaxHp;
        if (hpPct <= CombatConstants::LowHealthThresholdPct)
        {
            TraitSystem::onLowHealth(state, target);
            ItemSystem::onLowHealth(state, target);
        }
    }
}

void ProjectileSystem::spawnAutoAttackProjectile(GameState& state, const ProjectileSpec& spec)
{
    const std::int32_t hitAt = state.timeMs() + std::max<std::int32_t>(0, spec.travelTimeMs);

    if (CombatValidation::enabled() && CombatValidation::detailedLogs())
    {
        std::ostringstream ss;
        ss << "PROJECTILE_SPAWN " << state.timeMs() << "ms "
           << spec.debugName
           << " | hitAt=" << hitAt;
        state.logger().combat(ss.str());
    }

    state.scheduleCombatEvent(
        hitAt,
        [&state, spec]()
        {
            resolveAutoAttackHit(state, spec);
        },
        spec.debugName.empty() ? "ProjectileHit" : spec.debugName
    );
}
