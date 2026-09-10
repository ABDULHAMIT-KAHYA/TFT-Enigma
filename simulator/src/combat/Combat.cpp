// Combat.cpp
#include "combat/Combat.hpp"
#include "core/Board.hpp"
#include "combat/AbilitySystem.hpp"
#include "core/GameState.hpp"
#include "combat/TargetingSystem.hpp"
#include "combat/DamageSystem.hpp"
#include "core/BoardRenderer.hpp"
#include "combat/StatSystem.hpp"
#include "combat/TraitSystem.hpp"
#include "combat/ItemSystem.hpp"
#include "validation/CombatValidation.hpp"
#include "content/ContentManager.hpp"
#include "combat/ManaSystem.hpp"
#include "combat/ProjectileSystem.hpp"
#include "combat/SpellResolver.hpp"
#include "combat/TargetSelector.hpp"
#include "constants/CombatConstants.hpp"
#include "constants/GameConstants.hpp"
#include "constants/AIConstants.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>

static bool hasAlive(const std::vector<Unit>& team)
{
    for (const Unit& unit : team)
    {
        if (unit.isAlive()) { return true; }
    }
    return false;
}

static void printStartPositions(const std::vector<Unit>& team)
{
    for (const Unit& unit : team)
    {
        std::cout << unit.getName() << " starts at ("
                  << unit.getPosition().x << ", "
                  << unit.getPosition().y << ")\n";
    }
}

static bool validateStartPositions(const std::vector<Unit>& team, const Board& board)
{
    for (const Unit& unit : team)
    {
        if (!board.isInside(unit.getPosition()))
        {
            std::cout << "ERROR: " << unit.getName() << " starts outside board\n";
            return false;
        }
    }
    return true;
}

static std::vector<Position> collectAlivePositions(const std::vector<Unit>& teamA,
                                                   const std::vector<Unit>& teamB)
{
    std::vector<Position> positions;
    positions.reserve(teamA.size() + teamB.size());

    for (const Unit& unit : teamA)
    {
        if (unit.isAlive()) { positions.push_back(unit.getPosition()); }
    }

    for (const Unit& unit : teamB)
    {
        if (unit.isAlive()) { positions.push_back(unit.getPosition()); }
    }

    return positions;
}

static void printDebugAttack(std::int32_t timeMs,
                             Unit& attacker,
                             Unit& target)
{
    const std::int32_t rawDamage =
        StatSystem::getFinalStatInt(attacker, StatType::AttackDamage);

    const float amp = std::max(0.0f, StatSystem::getFinalStat(attacker, StatType::DamageAmplification));
    const std::int32_t rawAfterAmp =
        static_cast<std::int32_t>(std::lround(static_cast<float>(rawDamage) * (1.0f + amp)));

    DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(
            rawAfterAmp,
            attacker.getAutoAttackDamageType(),
            target
        );

    target.applyDamage(dmg.finalDamage);
    attacker.gainMana(StatSystem::getFinalStatInt(attacker, StatType::ManaGainOnAttack));
    attacker.resetAttackTimer();
    attacker.setAttackedThisTurn(true);

    std::cout << timeMs << "ms "
              << attacker.getName() << " attacks "
              << target.getName()
              << " | Type: "
              << ( dmg.damageType == DamageType::Physical ? "Physical" : dmg.damageType == DamageType::Magic ? "Magic" : "True")
              << " | RAW: " << dmg.rawDamage
              << " | "
<< (
    dmg.damageType == DamageType::Physical ? "ARMOR" :
    dmg.damageType == DamageType::Magic ? "MR" :
    "DEF"
)
<< ": " << dmg.defenseUsed
              << " | FINAL: " << dmg.finalDamage
              << " | Position: (" << target.getPosition().x << ", "
              << target.getPosition().y << ")"
              << " | " << target.getName() << " HP: " << target.getHp()
              << " | Mana: " << attacker.getMana() << "/" << attacker.getMaxMana()
              << "\n";
}

void Combat::run(std::vector<Unit>& teamA,
                 std::vector<Unit>& teamB,
                 Board& board)
{
    Logger logger(std::cout);
    logger.setMode(LogMode::Verbose);

    ContentManager content;
    content.loadAll("..\\..\\data");

    std::vector<Unit> allUnits;
    allUnits.reserve(teamA.size() + teamB.size());

    for (const Unit& unit : teamA) { allUnits.push_back(unit); }
    for (const Unit& unit : teamB) { allUnits.push_back(unit); }

    GameState state(board, std::move(allUnits), logger, content);
    run(state);
}

static bool validateStartPositionsAll(const std::vector<Unit>& units, const Board& board)
{
    for (const Unit& unit : units)
    {
        if (!board.isInside(unit.getPosition()))
        {
            return false;
        }
    }
    return true;
}

static std::vector<Position> collectAlivePositions(const std::vector<Unit>& units)
{
    std::vector<Position> positions;
    positions.reserve(units.size());

    for (const Unit& unit : units)
    {
        if (unit.isAlive())
        {
            positions.push_back(unit.getPosition());
        }
    }

    return positions;
}

static void performAutoAttack(GameState& state,
                              Unit& attacker,
                              Unit& target)
{
    Logger& logger = state.logger();
    const std::int32_t timeMs = state.timeMs();

    AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::Passive);
    AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::OnAttack);
    TraitSystem::onAttack(state, attacker, target);
    ItemSystem::onAttack(state, attacker, target);

    const std::int32_t rawDamage =
        StatSystem::getFinalStatInt(attacker, StatType::AttackDamage);

    const float attackSpeedUsed = StatSystem::getFinalStat(attacker, StatType::AttackSpeed);
    const std::int32_t attackIntervalMs =
        attackSpeedUsed > 0.0f
            ? std::max<std::int32_t>(
                  1,
                  static_cast<std::int32_t>(
                      std::lround(static_cast<float>(CombatConstants::MsPerSecond) / attackSpeedUsed)))
            : 0;

    bool didCrit = false;
    float critChanceUsed = 0.0f;
    float critDamageUsed = 1.0f;
    std::int32_t rawAfterCrit = rawDamage;

    if (attacker.getAutoAttackDamageType() == DamageType::Physical)
    {
        critChanceUsed = StatSystem::getFinalStat(attacker, StatType::CritChance);
        critDamageUsed = StatSystem::getFinalStat(attacker, StatType::CritDamage);

        if (DamageSystem::rollChance(critChanceUsed))
        {
            didCrit = true;
            rawAfterCrit = static_cast<std::int32_t>(std::lround(static_cast<float>(rawDamage) * critDamageUsed));
        }
    }

    const float amp = std::max(0.0f, StatSystem::getFinalStat(attacker, StatType::DamageAmplification));
    const std::int32_t rawAfterAmp =
        static_cast<std::int32_t>(std::lround(static_cast<float>(rawAfterCrit) * (1.0f + amp)));

    DamageDebugResult dmg =
        DamageSystem::calculateDamageDebug(
            rawAfterAmp,
            attacker.getAutoAttackDamageType(),
            target
        );

    const std::int32_t targetHpBefore = target.getHp();
    target.applyDamage(dmg.finalDamage);
    const std::int32_t targetHpAfter = target.getHp();
    const std::int32_t hpLost = std::max<std::int32_t>(0, targetHpBefore - targetHpAfter);

    const float omnivamp = std::clamp(StatSystem::getFinalStat(attacker, StatType::Omnivamp), 0.0f, 1.0f);
    if (omnivamp > 0.0f && dmg.finalDamage > 0)
    {
        attacker.heal(static_cast<std::int32_t>(std::lround(static_cast<float>(dmg.finalDamage) * omnivamp)));
    }

    const std::int32_t manaBefore = attacker.getMana();
    attacker.gainMana(StatSystem::getFinalStatInt(attacker, StatType::ManaGainOnAttack));
    const std::int32_t manaAfter = attacker.getMana();
    const std::int32_t manaFromAttack = std::max<std::int32_t>(0, manaAfter - manaBefore);

    const std::int32_t manaFromDamageTaken = ManaSystem::applyDamageTakenMana(state, target, hpLost);

    attacker.resetAttackTimer();
    attacker.setAttackedThisTurn(true);

    if (CombatValidation::enabled())
    {
        CombatValidation::logAutoAttack(
            state,
            attacker,
            target,
            attackSpeedUsed,
            attackIntervalMs,
            rawDamage,
            dmg,
            didCrit,
            critChanceUsed,
            critDamageUsed,
            rawAfterAmp,
            manaFromAttack,
            manaFromDamageTaken
        );
    }
    else
    {
        std::ostringstream ss;
        ss << timeMs << "ms "
           << attacker.getName() << " attacks "
           << target.getName()
           << " | Type: "
            << ( dmg.damageType == DamageType::Physical ? "Physical" : dmg.damageType == DamageType::Magic ? "Magic" : "True")
           << ""
           << " | RAW: " << rawDamage
           << " | "
    << (
        dmg.damageType == DamageType::Physical ? "ARMOR" :
        dmg.damageType == DamageType::Magic ? "MR" :
        "DEF"
    )
    << ": " << dmg.defenseUsed
           << " | Durability: " << static_cast<std::int32_t>(std::lround(dmg.durabilityUsed * AIConstants::PercentScale)) << "%"
           << " | FINAL: " << dmg.finalDamage
           << " | Position: (" << target.getPosition().x << ", "
           << target.getPosition().y << ")"
           << " | " << target.getName() << " HP: " << target.getHp()
           << " | Mana: " << attacker.getMana() << "/" << attacker.getMaxMana();

        if (didCrit)
        {
            ss << " | CRIT (" << static_cast<std::int32_t>(std::lround(critChanceUsed * AIConstants::PercentScale))
               << "%, x" << critDamageUsed << ", raw->" << rawAfterCrit << ")";
        }

        logger.combat(ss.str());
    }

    if (didCrit)
    {
        TraitSystem::onCrit(state, attacker, target);
        ItemSystem::onCrit(state, attacker, target);
    }

    AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::OnHit);
    AbilitySystem::executeTrigger(state, target, &attacker, AbilityTrigger::OnDamageTaken);
    TraitSystem::onDamageTaken(state, target, &attacker);
    TraitSystem::afterDamage(state, attacker, target);
    ItemSystem::onDamage(state, attacker, target, dmg.finalDamage);
    ItemSystem::onDamageTaken(state, target, &attacker, dmg.finalDamage);
    ItemSystem::onHit(state, attacker, target, dmg.finalDamage, dmg.damageType, didCrit);

    if (!target.isAlive())
    {
        AbilitySystem::executeTrigger(state, attacker, &target, AbilityTrigger::OnKill);
        AbilitySystem::executeTrigger(state, target, &attacker, AbilityTrigger::OnDeath);
        TraitSystem::onKill(state, attacker, target);
        TraitSystem::onDeath(state, target, &attacker);
        ItemSystem::onKill(state, attacker, target);
        ItemSystem::onDeath(state, target, &attacker);
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

static std::int32_t attackWindupMs(const Unit& attacker)
{
    if (attacker.getAttackRange() <= 1)
    {
        return CombatConstants::AttackWindupMeleeMs;
    }
    return CombatConstants::AttackWindupRangedMs;
}

static std::int32_t attackBackswingMs(const Unit& attacker)
{
    if (attacker.getAttackRange() <= 1)
    {
        return CombatConstants::AttackBackswingMeleeMs;
    }
    return CombatConstants::AttackBackswingRangedMs;
}

static std::int32_t projectileTravelMs(const Unit& attacker, const Unit& target)
{
    if (attacker.getAttackRange() <= 1)
    {
        return 0;
    }

    const float projectileSpeedCellsPerSecond = CombatConstants::ProjectileSpeedCellsPerSecond;
    const float dx = static_cast<float>(attacker.getPosition().x - target.getPosition().x);
    const float dy = static_cast<float>(attacker.getPosition().y - target.getPosition().y);
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float travelSec = dist / projectileSpeedCellsPerSecond;
    return static_cast<std::int32_t>(std::lround(travelSec * static_cast<float>(CombatConstants::MsPerSecond)));
}

static const char* teamName(TeamId team)
{
    return team == TeamId::TeamA ? "A" : "B";
}

static const char* crowdControlName(CrowdControlType type)
{
    switch (type)
    {
        case CrowdControlType::None: return "None";
        case CrowdControlType::Stun: return "Stun";
        case CrowdControlType::Knockup: return "Knockup";
        case CrowdControlType::Root: return "Root";
        case CrowdControlType::Silence: return "Silence";
        case CrowdControlType::Disarm: return "Disarm";
        case CrowdControlType::Taunt: return "Taunt";
        case CrowdControlType::Fear: return "Fear";
        case CrowdControlType::Suppression: return "Suppression";
    }
    return "Unknown";
}

static std::int32_t activeShieldValue(const Unit& unit)
{
    std::int32_t total = 0;
    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.effectType == StatusEffectType::Shield &&
            effect.remainingMs > 0 &&
            effect.value > 0.0f)
        {
            total += static_cast<std::int32_t>(std::lround(effect.value));
        }
    }
    return total;
}

static std::int32_t combatVitality(const std::vector<Unit>& units)
{
    std::int32_t total = 0;
    for (const Unit& unit : units)
    {
        if (!unit.isAlive())
        {
            continue;
        }
        total += unit.getHp();
        total += activeShieldValue(unit);
    }
    return total;
}

static std::uint64_t progressSignature(const GameState& state)
{
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::uint64_t v)
    {
        h ^= v;
        h *= 1099511628211ull;
    };

    mix(static_cast<std::uint64_t>(state.scheduledEventCount()));
    mix(static_cast<std::uint64_t>(state.executedEventCount()));

    for (const Unit& unit : state.units())
    {
        mix(unit.id().value);
        mix(static_cast<std::uint64_t>(unit.isAlive() ? 1 : 0));
        mix(static_cast<std::uint64_t>(unit.getHp()));
        mix(static_cast<std::uint64_t>(activeShieldValue(unit)));
        mix(static_cast<std::uint64_t>(unit.getMana()));
        mix(static_cast<std::uint64_t>(unit.getPosition().x + 64));
        mix(static_cast<std::uint64_t>(unit.getPosition().y + 64));
    }

    return h;
}

static std::string activeCrowdControlSummary(const Unit& unit)
{
    std::ostringstream ss;
    bool first = true;
    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.remainingMs <= 0 || effect.crowdControlType == CrowdControlType::None)
        {
            continue;
        }
        if (!first)
        {
            ss << ",";
        }
        ss << crowdControlName(effect.crowdControlType) << ":" << effect.remainingMs << "ms";
        first = false;
    }
    return first ? "none" : ss.str();
}

static std::int32_t effectiveAttackCooldownMs(const Unit& unit)
{
    const float attackSpeed = StatSystem::getFinalStat(unit, StatType::AttackSpeed);
    if (attackSpeed <= 0.0f)
    {
        return 0;
    }
    return std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(
            std::lround(static_cast<float>(CombatConstants::MsPerSecond) / attackSpeed)));
}

static bool hasActiveStatusNamed(const Unit& unit, const char* name)
{
    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.remainingMs > 0 && effect.name == name)
        {
            return true;
        }
    }
    return false;
}

static StatusEffect makeOvertimeStatus(const char* name,
                                       StatusEffectType effectType,
                                       StatType affectedStat,
                                       ModifierType modifierType,
                                       float value,
                                       std::int32_t durationMs)
{
    StatusEffect effect{};
    effect.name = name;
    effect.effectType = effectType;
    effect.crowdControlType = CrowdControlType::None;
    effect.affectedStat = affectedStat;
    effect.modifierType = modifierType;
    effect.value = value;
    effect.durationMs = durationMs;
    effect.remainingMs = durationMs;
    effect.tickIntervalMs = 0;
    effect.tickTimerMs = 0;
    effect.damageType = DamageType::TrueDamage;
    return effect;
}

static void applyCombatOvertime(GameState& state)
{
    static constexpr const char* DamageStatusName = "Combat Overtime Damage";
    static constexpr const char* AttackSpeedStatusName = "Combat Overtime Attack Speed";

    const std::int32_t durationMs =
        std::max<std::int32_t>(1, CombatConstants::MaxCombatDurationMs - state.timeMs());

    StatusEffect damageAmp =
        makeOvertimeStatus(DamageStatusName,
                           StatusEffectType::Buff,
                           StatType::DamageAmplification,
                           ModifierType::Flat,
                           CombatConstants::OvertimeDamageAmplification,
                           durationMs);

    StatusEffect attackSpeed =
        makeOvertimeStatus(AttackSpeedStatusName,
                           StatusEffectType::BonusAttackSpeed,
                           StatType::AttackSpeed,
                           ModifierType::Percent,
                           CombatConstants::OvertimeAttackSpeedPercent,
                           durationMs);

    int affected = 0;
    for (Unit& unit : state.units())
    {
        if (!unit.isAlive())
        {
            continue;
        }
        if (!hasActiveStatusNamed(unit, DamageStatusName))
        {
            unit.addStatusEffect(damageAmp);
        }
        if (!hasActiveStatusNamed(unit, AttackSpeedStatusName))
        {
            unit.addStatusEffect(attackSpeed);
        }
        affected += 1;
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms Combat overtime begins"
       << " | units=" << affected
       << " | damageAmp=" << static_cast<std::int32_t>(std::lround(CombatConstants::OvertimeDamageAmplification * AIConstants::PercentScale)) << "%"
       << " | attackSpeed=" << static_cast<std::int32_t>(std::lround(CombatConstants::OvertimeAttackSpeedPercent * AIConstants::PercentScale)) << "%";
    state.logger().combat(ss.str());
}
static void logCombatTimeoutDiagnostics(GameState& state,
                                        const std::vector<CombatTargetContext>& targetCtx,
                                        std::int32_t lastMeaningfulEventMs,
                                        std::int32_t lastVitalityDelta)
{
    const std::vector<Unit>& units = state.units();

    int aliveA = 0;
    int aliveB = 0;
    for (const Unit& unit : units)
    {
        if (!unit.isAlive())
        {
            continue;
        }
        if (unit.getTeamId() == TeamId::TeamA) { aliveA += 1; }
        else { aliveB += 1; }
    }

    std::ostringstream summary;
    summary << "Combat timeout diagnostics:"
            << " aliveA=" << aliveA
            << " aliveB=" << aliveB
            << " vitality=" << combatVitality(units)
            << " lastMeaningfulMs=" << lastMeaningfulEventMs
            << " idleMs=" << (state.timeMs() - lastMeaningfulEventMs)
            << " lastVitalityDelta=" << lastVitalityDelta
            << " scheduled=" << state.scheduledEventCount()
            << " executed=" << state.executedEventCount()
            << " pending=" << state.scheduledEvents().size();
    state.logger().error(summary.str());

    for (std::size_t i = 0; i < units.size(); ++i)
    {
        const Unit& unit = units[i];
        if (!unit.isAlive())
        {
            continue;
        }

        const CombatTargetContext* ctx = i < targetCtx.size() ? &targetCtx[i] : nullptr;
        const Unit* target = ctx ? state.findUnit(ctx->currentTargetId) : nullptr;

        std::ostringstream ss;
        ss << "TIMEOUT_UNIT"
           << " team=" << teamName(unit.getTeamId())
           << " id=" << unit.id()
           << " name=" << unit.getName()
           << " hp=" << unit.getHp() << "/" << StatSystem::getFinalStatInt(unit, StatType::MaxHp)
           << " shield=" << activeShieldValue(unit)
           << " pos=(" << unit.getPosition().x << "," << unit.getPosition().y << ")"
           << " target=" << (target ? target->getName() : "none");

        if (target)
        {
            ss << " targetId=" << target->id()
               << " targetHp=" << target->getHp()
               << " inRange=" << (unit.isInRange(*target) ? 1 : 0);
        }

        ss << " canMove=" << (unit.canMoveNow() ? 1 : 0)
           << " canAttack=" << (unit.canAttack() ? 1 : 0)
           << " canAutoAttack=" << (unit.canAutoAttackNow() ? 1 : 0)
           << " canCast=" << (unit.canCastNow() && unit.canCastAbility() ? 1 : 0)
           << " casting=" << (unit.isCasting() ? 1 : 0)
           << " mana=" << unit.getMana() << "/" << unit.getMaxMana()
           << " attackTimer=" << unit.getAttackTimerMs()
           << " attackCd=" << effectiveAttackCooldownMs(unit)
           << " moved=" << (unit.didMoveThisTurn() ? 1 : 0)
           << " attacked=" << (unit.didAttackThisTurn() ? 1 : 0)
           << " cast=" << (unit.didCastThisTurn() ? 1 : 0)
           << " statuses=" << unit.statusEffects().size()
           << " cc=" << activeCrowdControlSummary(unit);

        state.logger().error(ss.str());
    }
}
static void beginAutoAttackAccurate(GameState& state, std::int32_t attackerIndex, std::int32_t targetIndex)
{
    std::vector<Unit>& units = state.units();
    if (attackerIndex < 0 || targetIndex < 0 ||
        attackerIndex >= static_cast<std::int32_t>(units.size()) ||
        targetIndex >= static_cast<std::int32_t>(units.size()))
    {
        return;
    }

    Unit& attacker = units[attackerIndex];
    Unit& target = units[targetIndex];

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

    const std::int32_t windup = attackWindupMs(attacker);
    const std::int32_t backswing = attackBackswingMs(attacker);

    attacker.beginAttackLock(windup + backswing);
    attacker.resetAttackTimer();
    attacker.setAttackedThisTurn(true);

    if (CombatValidation::enabled() && CombatValidation::detailedLogs())
    {
        std::ostringstream ss;
        ss << "ATTACK_START " << state.timeMs() << "ms "
           << attacker.getName() << " -> " << target.getName()
           << " | windup=" << windup
           << " | backswing=" << backswing;
        state.logger().combat(ss.str());
    }

    CombatEvent event{};
    event.type = CombatEventType::AutoAttackRelease;
    event.executeAtMs = state.timeMs() + windup;
    event.sourceId = attacker.id();
    event.targetId = target.id();
    event.policy = CombatEventTargetPolicy::RequireAliveSourceAndTarget;
    event.debugName = "AutoAttackRelease";
    state.scheduleCombatEvent(event);
}

void Combat::run(GameState& state)
{
    Logger& logger = state.logger();
    Board& board = state.board();
    std::vector<Unit>& units = state.units();
    std::vector<Position> highlightedCells;
    std::vector<CombatTargetContext> targetCtx(units.size());

    logger.info("Battle Started");
    logger.info("");

    if (!validateStartPositionsAll(units, board))
    {
        logger.error("ERROR: a unit starts outside board");
        return;
    }

    for (const Unit& unit : units)
    {
        std::ostringstream ss;
        ss << unit.getName() << " starts at ("
           << unit.getPosition().x << ", "
           << unit.getPosition().y << ")";
        logger.info(ss.str());
    }

    logger.info("");

    state.captureSnapshot("combat_start");

    TraitSystem::onCombatStart(state);
    ItemSystem::onCombatStart(state);
    for (Unit& unit : units)
    {
        AbilitySystem::executeTrigger(state, unit, nullptr, AbilityTrigger::OnCombatStart);
    }

    const std::int32_t maxCombatMs = CombatConstants::MaxCombatDurationMs;
    std::uint64_t lastProgressSignature = progressSignature(state);
    std::int32_t previousVitality = combatVitality(units);
    std::int32_t lastMeaningfulEventMs = state.timeMs();
    std::int32_t lastVitalityDelta = 0;
    bool overtimeApplied = false;

    while (state.hasAlive(TeamId::TeamA) && state.hasAlive(TeamId::TeamB))
    {
        state.advanceTick();

        const std::int32_t timeMs = state.timeMs();
        const std::int32_t dtMs = state.dtMs();

        if (timeMs >= maxCombatMs)
        {
            std::ostringstream ss;
            ss << "Combat timeout: forced draw after " << CombatConstants::MaxCombatDurationMs << "ms";
            logger.error(ss.str());
            logCombatTimeoutDiagnostics(state, targetCtx, lastMeaningfulEventMs, lastVitalityDelta);
            return;
        }

        if (!overtimeApplied && timeMs >= CombatConstants::OvertimeStartMs)
        {
            applyCombatOvertime(state);
            overtimeApplied = true;
        }

        state.processCombatEvents();
        TraitSystem::tick(state);
        ItemSystem::tick(state);

        board.rebuildOccupancy(collectAlivePositions(units));

        const TeamId teams[2] = { TeamId::TeamA, TeamId::TeamB };

        for (TeamId team : teams)
        {
            for (std::size_t unitIndex = 0; unitIndex < units.size(); ++unitIndex)
            {
                Unit& unit = units[unitIndex];
                if (!unit.isAlive() || unit.getTeamId() != team)
                {
                    continue;
                }

                unit.updateStatusEffects(dtMs);
                unit.tick(dtMs);

                CombatTargetContext& ctx = targetCtx[unitIndex];
                if (state.findUnit(ctx.currentTargetId) && (!state.findUnit(ctx.currentTargetId)->isAlive() || state.findUnit(ctx.currentTargetId)->getTeamId() == unit.getTeamId() || state.findUnit(ctx.currentTargetId)->isUntargetable()))
                {
                    ctx.currentTargetId = UnitId{};
                    ctx.retargetLockedUntilMs = timeMs + CombatConstants::RetargetLockMs;
                }

                Unit* target =
                    TargetSelector::selectTarget(
                        unit,
                        units,
                        ctx,
                        timeMs,
                        TargetPriority::FrontlineFirst
                    );

                if (!target && isValid(ctx.currentTargetId))
                {
                    target = state.findUnit(ctx.currentTargetId);
                }

                if (CombatValidation::enabled())
                {
                    CombatValidation::logTargetChange(state, unit, state.findUnit(ctx.currentTargetId), target);
                }

                ctx.currentTargetId = target ? target->id() : UnitId{};
                if (!target) { continue; }

                if (!unit.isInRange(*target))
                {
                    if (unit.canMoveNow())
                    {
                        const Position before = unit.getPosition();
                        board.setOccupied(before, false);

                        const bool moved = unit.moveToward(*target, board);

                        const Position after = unit.getPosition();
                        board.setOccupied(moved ? after : before, true);

                        if (moved)
                        {
                            std::ostringstream ss;
                            ss << timeMs << "ms "
                               << unit.getName() << " moves to ("
                               << after.x << ", " << after.y << ")";
                            logger.move(ss.str());
                        }
                        else if (timeMs - unit.getLastBlockedWarnAtMs() >= CombatConstants::MovementBlockedWarnIntervalMs)
                        {
                            unit.setLastBlockedWarnAtMs(timeMs);

                            std::ostringstream ss;
                            ss << "WARN: " << unit.getName() << " move blocked";
                            logger.warn(ss.str());
                        }
                    }
                }

                if (unit.canAttack() && unit.isInRange(*target))
                {
                    if (unit.canCastNow() && unit.canCastAbility())
                    {
                        const bool casted = SpellResolver::beginCast(state, unit, *target);
                        if (casted)
                        {
                            unit.setCastThisTurn(true);
                            ctx.castLockedTargetId = target ? target->id() : UnitId{};
                            ctx.castLockUntilMs = timeMs + CombatConstants::CastTargetLockMs;
                        }
                    }
                    else
                    {
                        if (unit.canAutoAttackNow())
                        {
                            beginAutoAttackAccurate(
                                state,
                                static_cast<std::int32_t>(unitIndex),
                                static_cast<std::int32_t>(target - &units[0])
                            );
                        }
                    }
                }
            }
        }

        state.captureSnapshot("tick");

        const std::int32_t currentVitality = combatVitality(units);
        const std::uint64_t currentProgressSignature = progressSignature(state);
        if (currentProgressSignature != lastProgressSignature)
        {
            lastMeaningfulEventMs = timeMs;
            lastProgressSignature = currentProgressSignature;
        }
        if (currentVitality != previousVitality)
        {
            lastVitalityDelta = previousVitality - currentVitality;
            previousVitality = currentVitality;
        }

        if (timeMs % CombatConstants::VerboseBoardPrintIntervalMs == 0 && logger.mode() == LogMode::Verbose)
        {
            BoardRenderer::print(board, units, highlightedCells);
            highlightedCells.clear();
            for (Unit& unit : units)
            {
                unit.setMovedThisTurn(false);
                unit.setCastThisTurn(false);
                unit.setAttackedThisTurn(false);
            }
        }
    }

    state.captureSnapshot("combat_end");

    logger.info("");
    logger.info(state.hasAlive(TeamId::TeamA) ? "Winner: Team A" : "Winner: Team B");
}





