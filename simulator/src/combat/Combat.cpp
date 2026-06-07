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
    TraitSystem::afterDamage(state, attacker, target);
    ItemSystem::onHit(state, attacker, target, dmg.finalDamage, dmg.damageType, didCrit);

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

    state.scheduleCombatEvent(
        state.timeMs() + windup,
        [&state, attackerIndex, targetIndex]()
        {
            std::vector<Unit>& unitsAt = state.units();
            if (attackerIndex < 0 || targetIndex < 0 ||
                attackerIndex >= static_cast<std::int32_t>(unitsAt.size()) ||
                targetIndex >= static_cast<std::int32_t>(unitsAt.size()))
            {
                return;
            }

            Unit& attackerAt = unitsAt[attackerIndex];
            Unit& targetAt = unitsAt[targetIndex];

            if (!attackerAt.isAlive() || !targetAt.isAlive())
            {
                return;
            }
            if (!targetAt.isEnemyOf(attackerAt))
            {
                return;
            }
            if (attackerAt.isCasting())
            {
                return;
            }
            if (attackerAt.hasCrowdControl(CrowdControlType::Stun) ||
                attackerAt.hasCrowdControl(CrowdControlType::Knockup) ||
                attackerAt.hasCrowdControl(CrowdControlType::Suppression) ||
                attackerAt.hasCrowdControl(CrowdControlType::Fear) ||
                attackerAt.hasCrowdControl(CrowdControlType::Disarm))
            {
                return;
            }

            const std::int32_t manaGain = StatSystem::getFinalStatInt(attackerAt, StatType::ManaGainOnAttack);
            attackerAt.gainMana(manaGain);

            TraitSystem::onAttack(state, attackerAt, targetAt);
            ItemSystem::onAttack(state, attackerAt, targetAt);
            AbilitySystem::executeTrigger(state, attackerAt, &targetAt, AbilityTrigger::OnAttack);

            const std::int32_t rawBeforeCrit =
                StatSystem::getFinalStatInt(attackerAt, StatType::AttackDamage);

            bool didCrit = false;
            float critChanceUsed = std::clamp(StatSystem::getFinalStat(attackerAt, StatType::CritChance), 0.0f, 1.0f);
            float critDamageUsed = std::max(1.0f, StatSystem::getFinalStat(attackerAt, StatType::CritDamage));

            std::int32_t rawAfterCrit = rawBeforeCrit;
            if (DamageSystem::rollChance(critChanceUsed))
            {
                didCrit = true;
                rawAfterCrit = static_cast<std::int32_t>(std::lround(static_cast<float>(rawBeforeCrit) * critDamageUsed));
            }

            const std::int32_t travel = projectileTravelMs(attackerAt, targetAt);

            ProjectileSpec spec{};
            spec.attackerIndex = attackerIndex;
            spec.targetIndex = targetIndex;
            spec.damageType = attackerAt.getAutoAttackDamageType();
            spec.rawDamage = rawAfterCrit;
            spec.didCrit = didCrit;
            spec.critChanceUsed = critChanceUsed;
            spec.critDamageUsed = critDamageUsed;
            spec.rawBeforeCrit = rawBeforeCrit;
            spec.rawAfterCrit = rawAfterCrit;
            spec.travelTimeMs = travel;
            spec.debugName = "AutoAttack";

            ProjectileSystem::spawnAutoAttackProjectile(state, spec);
        },
        "AutoAttackRelease"
    );
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

    TraitSystem::onCombatStart(state);
    ItemSystem::onCombatStart(state);
    for (Unit& unit : units)
    {
        AbilitySystem::executeTrigger(state, unit, nullptr, AbilityTrigger::OnCombatStart);
    }

    const std::int32_t maxCombatMs = CombatConstants::MaxCombatDurationMs;
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
            return;
        }

        state.processCombatEvents();
        TraitSystem::tick(state);

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
                if (ctx.currentTarget && (!ctx.currentTarget->isAlive() || ctx.currentTarget->getTeamId() == unit.getTeamId() || ctx.currentTarget->isUntargetable()))
                {
                    ctx.currentTarget = nullptr;
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

                if (!target && ctx.currentTarget)
                {
                    target = ctx.currentTarget;
                }

                if (CombatValidation::enabled())
                {
                    CombatValidation::logTargetChange(state, unit, ctx.currentTarget, target);
                }

                ctx.currentTarget = target;
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
                            ctx.castLockedTarget = target;
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

    logger.info("");
    logger.info(state.hasAlive(TeamId::TeamA) ? "Winner: Team A" : "Winner: Team B");
}
