#include "validation/CombatValidation.hpp"
#include "constants/AIConstants.hpp"
#include "combat/Combat.hpp"
#include "combat/AbilitySystem.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/SpellResolver.hpp"
#include "combat/ManaSystem.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/MacroActionScorer.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include "constants/GameConstants.hpp"
#include "core/Logger.hpp"
#include "core/RandomManager.hpp"
#include "combat/StatSystem.hpp"
#include "combat/TargetingSystem.hpp"
#include "combat/TraitEffectExecutor.hpp"
#include "macro/LegalActionGenerator.hpp"
#include "macro/MacroExecutor.hpp"
#include "macro/MacroSimulation.hpp"
#include "macro/EconomySystem.hpp"
#include "macro/ShopSystem.hpp"
#include "constants/MacroConstants.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "ai/RolloutPlanner.hpp"
#include "ai/BranchPruner.hpp"
#include "content/ChampionFilter.hpp"
#include "constants/ValidationConstants.hpp"
#include "core/Random.hpp"
#include "core/Json.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/RoundSchedule.hpp"
#include "ai/FutureStateEvaluator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

static bool& validationEnabled()
{
    static bool enabled = false;
    return enabled;
}

static bool& validationDetailed()
{
    static bool detailed = false;
    return detailed;
}

static std::size_t failCount(const ValidationReport& report)
{
    std::size_t c = 0;
    for (const ValidationEntry& e : report.entries)
    {
        if (e.status == ValidationStatus::Fail)
        {
            c += 1;
        }
    }
    return c;
}

static void logValidationStart(std::ostream& out, const std::string& name)
{
    out << "[VALIDATION] START: " << name << "\n";
    out << std::flush;
}

static void logValidationEnd(std::ostream& out, const std::string& name, bool passed, long long ms)
{
    out << "[VALIDATION] " << (passed ? "PASS" : "FAIL") << ": " << name << " in " << ms << "ms\n";
    out << std::flush;
}

void ValidationReport::pass(std::string message)
{
    entries.push_back(ValidationEntry{ ValidationStatus::Pass, std::move(message) });
}

void ValidationReport::warning(std::string message)
{
    entries.push_back(ValidationEntry{ ValidationStatus::Warning, std::move(message) });
}

void ValidationReport::fail(std::string message)
{
    entries.push_back(ValidationEntry{ ValidationStatus::Fail, std::move(message) });
}

bool ValidationReport::hasFail() const
{
    for (const ValidationEntry& e : entries)
    {
        if (e.status == ValidationStatus::Fail)
        {
            return true;
        }
    }
    return false;
}

static const char* statusLabel(ValidationStatus s)
{
    switch (s)
    {
        case ValidationStatus::Pass: return "PASS";
        case ValidationStatus::Warning: return "WARNING";
        case ValidationStatus::Fail: return "FAIL";
    }
    return "UNKNOWN";
}

void ValidationReport::print(std::ostream& out) const
{
    out << "\n=== VALIDATION REPORT ===\n\n";
    for (const ValidationEntry& e : entries)
    {
        out << statusLabel(e.status) << ": " << e.message << "\n";
    }
    out << "\n";
}

void CombatValidation::setEnabled(bool enabled)
{
    validationEnabled() = enabled;
}

bool CombatValidation::enabled()
{
    return validationEnabled();
}

void CombatValidation::setDetailedLogs(bool detailed)
{
    validationDetailed() = detailed;
}

bool CombatValidation::detailedLogs()
{
    return validationDetailed();
}

static float reductionPercent(const DamageDebugResult& dmg)
{
    if (dmg.rawDamage <= 0)
    {
        return 0.0f;
    }
    const float raw = static_cast<float>(dmg.rawDamage);
    const float pre = static_cast<float>(std::max<std::int32_t>(0, dmg.preDurabilityDamage));
    const float afterDefense = std::clamp(1.0f - (pre / raw), 0.0f, 1.0f);
    const float afterDur = dmg.durabilityUsed;
    const float combined = 1.0f - ((pre * (1.0f - afterDur)) / raw);
    return std::clamp(combined, 0.0f, 1.0f);
}

void CombatValidation::logAutoAttack(GameState& state,
                                     const Unit& attacker,
                                     const Unit& target,
                                     float attackSpeed,
                                     std::int32_t attackIntervalMs,
                                     std::int32_t rawDamage,
                                     const DamageDebugResult& dmg,
                                     bool didCrit,
                                     float critChanceUsed,
                                     float critDamageUsed,
                                     std::int32_t rawAfterCrit,
                                     std::int32_t manaGainFromAttack,
                                     std::int32_t manaGainFromDamageTaken)
{
    if (!enabled())
    {
        return;
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << attacker.getName() << " attacks " << target.getName() << "\n";
    ss << "AttackSpeed: " << std::fixed << std::setprecision(2) << attackSpeed
       << " -> attack interval " << attackIntervalMs << "ms\n";
    ss << "Raw Damage: " << rawDamage << "\n";
    ss << (dmg.damageType == DamageType::Physical ? "Armor: " : dmg.damageType == DamageType::Magic ? "MR: " : "Defense: ")
       << dmg.defenseUsed << "\n";
    ss << "Reduction: " << std::fixed << std::setprecision(1)
       << (reductionPercent(dmg) * AIConstants::PercentScale) << "%\n";
    ss << "Final Damage: " << dmg.finalDamage << "\n";
    ss << "Mana Gain: +" << manaGainFromAttack << " (attack), +" << manaGainFromDamageTaken << " (damage taken)\n";
    if (didCrit)
    {
        ss << "Crit: yes (" << std::lround(critChanceUsed * AIConstants::PercentScale) << "%, x" << critDamageUsed
           << ", raw->" << rawAfterCrit << ")\n";
    }
    else
    {
        ss << "Crit: no (" << std::lround(critChanceUsed * AIConstants::PercentScale) << "%)\n";
    }
    state.logger().combat(ss.str());
}

void CombatValidation::logAbilityHit(GameState& state,
                                     const Unit& caster,
                                     const Unit& target,
                                     std::string_view sourceName,
                                     std::int32_t rawDamage,
                                     const DamageDebugResult& dmg,
                                     bool didCrit,
                                     float critChanceUsed,
                                     float critDamageUsed,
                                     std::int32_t rawAfterCrit,
                                     std::int32_t manaGainFromDamageTaken)
{
    if (!enabled())
    {
        return;
    }
    if (!detailedLogs())
    {
        return;
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << caster.getName() << " hits " << target.getName()
       << " (" << sourceName << ")\n";
    ss << "Raw Damage: " << rawDamage << "\n";
    ss << (dmg.damageType == DamageType::Physical ? "Armor: " : dmg.damageType == DamageType::Magic ? "MR: " : "Defense: ")
       << dmg.defenseUsed << "\n";
    ss << "Reduction: " << std::fixed << std::setprecision(1)
       << (reductionPercent(dmg) * AIConstants::PercentScale) << "%\n";
    ss << "Final Damage: " << dmg.finalDamage << "\n";
    ss << target.getName() << " gains +" << manaGainFromDamageTaken << " mana from damage taken\n";
    if (didCrit)
    {
        ss << "Crit: yes (" << std::lround(critChanceUsed * AIConstants::PercentScale) << "%, x" << critDamageUsed
           << ", raw->" << rawAfterCrit << ")\n";
    }
    else
    {
        ss << "Crit: no (" << std::lround(critChanceUsed * AIConstants::PercentScale) << "%)\n";
    }
    state.logger().combat(ss.str());
}

void CombatValidation::logTargetChange(GameState& state,
                                       const Unit& unit,
                                       const Unit* previous,
                                       const Unit* current)
{
    if (!enabled())
    {
        return;
    }
    if (!detailedLogs())
    {
        return;
    }

    if (previous == current)
    {
        return;
    }

    std::ostringstream ss;
    ss << state.timeMs() << "ms " << unit.getName() << " ";
    if (!current && previous)
    {
        ss << "target lost, reacquiring target";
        state.logger().combat(ss.str());
        return;
    }
    if (current && !previous)
    {
        ss << "acquires target " << current->getName();
        state.logger().combat(ss.str());
        return;
    }
    if (current && previous)
    {
        ss << "retargets " << current->getName();
        state.logger().combat(ss.str());
        return;
    }
}

void CombatValidation::logArea(const Board& board,
                               const Position& origin,
                               AreaShape shape,
                               std::int32_t radius,
                               const Position& directionTarget,
                               std::ostream& out)
{
    const std::vector<Position> cells = getPositionsInArea(board, origin, shape, radius, directionTarget);
    out << "Area shape " << static_cast<int>(shape) << " affected positions:\n";
    for (const Position& p : cells)
    {
        out << "(" << p.x << "," << p.y << ")\n";
    }
}

static std::vector<std::string> splitLines(const std::string& s)
{
    std::vector<std::string> lines;
    std::string cur;
    for (char c : s)
    {
        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            lines.push_back(cur);
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        lines.push_back(cur);
    }
    return lines;
}

static void divergenceReport(const std::string& a, const std::string& b, std::ostream& out)
{
    const std::vector<std::string> la = splitLines(a);
    const std::vector<std::string> lb = splitLines(b);
    const std::size_t n = std::min(la.size(), lb.size());
    std::size_t first = n;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (la[i] != lb[i])
        {
            first = i;
            break;
        }
    }
    if (first == n && la.size() != lb.size())
    {
        first = n;
    }
    out << "Divergence at line " << (first + 1) << "\n";
    if (first < la.size())
    {
        out << "A: " << la[first] << "\n";
    }
    if (first < lb.size())
    {
        out << "B: " << lb[first] << "\n";
    }
}

static std::string runCombatAndCaptureLog(const ContentManager& content,
                                         std::uint32_t seed,
                                         const std::vector<Unit>& teamA,
                                         const std::vector<Unit>& teamB,
                                         std::int32_t dtMs,
                                         LogMode mode,
                                         bool validationMode,
                                         bool validationDetailedLogs,
                                         std::int32_t* winnerOut)
{
    RandomManager::global().setSeed(seed);
    DamageSystem::setSeed(seed);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(mode);

    Board board(10, 10);
    std::vector<Unit> all;
    all.reserve(teamA.size() + teamB.size());
    for (const Unit& u : teamA) all.push_back(u);
    for (const Unit& u : teamB) all.push_back(u);

    GameState state(std::move(board), std::move(all), std::move(logger), content);
    state.setDtMs(dtMs);

    CombatValidation::setEnabled(validationMode);
    CombatValidation::setDetailedLogs(validationDetailedLogs);

    Combat combat;
    combat.run(state);

    const bool teamAAlive = state.hasAlive(TeamId::TeamA);
    const bool teamBAlive = state.hasAlive(TeamId::TeamB);
    std::int32_t winner = 0;
    if (teamAAlive && !teamBAlive) winner = 1;
    else if (teamBAlive && !teamAAlive) winner = 2;
    if (winnerOut)
    {
        *winnerOut = winner;
    }

    return oss.str();
}

static StatusEffect makeFlatBuff(std::string name, StatusEffectType type, StatType stat, float value, std::int32_t durationMs)
{
    StatusEffect e{};
    e.name = std::move(name);
    e.effectType = type;
    e.crowdControlType = CrowdControlType::None;
    e.affectedStat = stat;
    e.modifierType = ModifierType::Flat;
    e.value = value;
    e.durationMs = durationMs;
    e.remainingMs = durationMs;
    e.tickIntervalMs = 0;
    e.tickTimerMs = 0;
    e.damageType = DamageType::TrueDamage;
    return e;
}

static StatusEffect makePercentBuff(std::string name, StatusEffectType type, StatType stat, float value, std::int32_t durationMs)
{
    StatusEffect e{};
    e.name = std::move(name);
    e.effectType = type;
    e.crowdControlType = CrowdControlType::None;
    e.affectedStat = stat;
    e.modifierType = ModifierType::Percent;
    e.value = value;
    e.durationMs = durationMs;
    e.remainingMs = durationMs;
    e.tickIntervalMs = 0;
    e.tickTimerMs = 0;
    e.damageType = DamageType::TrueDamage;
    return e;
}

static bool isPlaceholderSingleTargetMagicAbility(const Ability& ability)
{
    constexpr float Epsilon = 0.0001f;
    if (ability.effects.size() != 1)
    {
        return false;
    }

    const AbilityEffect& effect = ability.effects.front();
    return effect.trigger == AbilityTrigger::OnCast &&
           effect.damageFormula.damageType == DamageType::Magic &&
           std::fabs(effect.damageFormula.adRatio) <= Epsilon &&
           std::fabs(effect.damageFormula.apRatio - 0.8f) <= Epsilon &&
           effect.areaShape == AreaShape::SingleTarget &&
           effect.radius == 0 &&
           effect.delayMs == 0 &&
           !effect.canCrit &&
           !effect.appliesStatusEffect &&
           effect.healAmount == 0 &&
           effect.healPercentOfDamage == 0.0f &&
           effect.shieldAmount == 0 &&
           effect.maxStacks == 0 &&
           effect.targetMaxHpThreshold == 0 &&
           effect.targetMaxHpPercentDamage == 0.0f;
}

static std::string traitEffectTypeName(TraitEffectType type)
{
    switch (type)
    {
        case TraitEffectType::ApplyStatusToTraitUnits: return "ApplyStatusToTraitUnits";
        case TraitEffectType::ApplyStatusToAllies: return "ApplyStatusToAllies";
        case TraitEffectType::ApplyStatusToEnemies: return "ApplyStatusToEnemies";
        case TraitEffectType::ApplyStatusToEnemyTeam: return "ApplyStatusToEnemyTeam";
        case TraitEffectType::Shield: return "Shield";
        case TraitEffectType::Heal: return "Heal";
        case TraitEffectType::DealDamage: return "DealDamage";
        case TraitEffectType::StackStatusOnAttack: return "StackStatusOnAttack";
        case TraitEffectType::ShieldOnCombatStart: return "ShieldOnCombatStart";
        case TraitEffectType::ExecuteBelowHpPercent: return "ExecuteBelowHpPercent";
        case TraitEffectType::TempCritBonusVsLowHp: return "TempCritBonusVsLowHp";
    }
    return "ApplyStatusToTraitUnits";
}

static bool hasStatusNamed(const Unit& unit, std::string_view statusName)
{
    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.name == statusName)
        {
            return true;
        }
    }
    return false;
}

static std::string traitFanoutJson(TraitEffectType type, std::string_view traitName, std::string_view statusName)
{
    std::ostringstream ss;
    ss << "{\n"
       << "  \"name\": \"" << traitName << "\",\n"
       << "  \"breakpoints\": [1],\n"
       << "  \"tiers\": [\n"
       << "    {\n"
       << "      \"breakpoint\": 1,\n"
       << "      \"effects\": [\n"
       << "        {\n"
       << "          \"hook\": \"OnCombatStart\",\n"
       << "          \"type\": \"" << traitEffectTypeName(type) << "\",\n"
       << "          \"statusEffect\": {\n"
       << "            \"name\": \"" << statusName << "\",\n"
       << "            \"effectType\": \"Buff\",\n"
       << "            \"crowdControlType\": \"None\",\n"
       << "            \"affectedStat\": \"AttackDamage\",\n"
       << "            \"modifierType\": \"Flat\",\n"
       << "            \"value\": 7,\n"
       << "            \"durationMs\": " << ValidationConstants::LongDurationMs << ",\n"
       << "            \"remainingMs\": " << ValidationConstants::LongDurationMs << ",\n"
       << "            \"tickIntervalMs\": 0,\n"
       << "            \"tickTimerMs\": 0,\n"
       << "            \"damageType\": \"True\"\n"
       << "          }\n"
       << "        }\n"
       << "      ]\n"
       << "    }\n"
       << "  ]\n"
       << "}\n";
    return ss.str();
}

static std::string traitShieldJson(TraitEffectType type, std::string_view traitName, int shieldAmount)
{
    std::ostringstream ss;
    ss << "{\n"
       << "  \"name\": \"" << traitName << "\",\n"
       << "  \"breakpoints\": [1],\n"
       << "  \"tiers\": [\n"
       << "    {\n"
       << "      \"breakpoint\": 1,\n"
       << "      \"effects\": [\n"
       << "        {\n"
       << "          \"hook\": \"OnCombatStart\",\n"
       << "          \"type\": \"" << traitEffectTypeName(type) << "\",\n"
       << "          \"shieldAmount\": " << shieldAmount << "\n"
       << "        }\n"
       << "      ]\n"
       << "    }\n"
       << "  ]\n"
       << "}\n";
    return ss.str();
}

static std::string traitHealJson(std::string_view traitName, int healAmount)
{
    std::ostringstream ss;
    ss << "{\n"
       << "  \"name\": \"" << traitName << "\",\n"
       << "  \"breakpoints\": [1],\n"
       << "  \"tiers\": [\n"
       << "    {\n"
       << "      \"breakpoint\": 1,\n"
       << "      \"effects\": [\n"
       << "        {\n"
       << "          \"hook\": \"OnCombatStart\",\n"
       << "          \"type\": \"Heal\",\n"
       << "          \"healAmount\": " << healAmount << "\n"
       << "        }\n"
       << "      ]\n"
       << "    }\n"
       << "  ]\n"
       << "}\n";
    return ss.str();
}

static std::string damageTypeJsonName(DamageType type)
{
    switch (type)
    {
        case DamageType::Physical: return "Physical";
        case DamageType::Magic: return "Magic";
        case DamageType::TrueDamage: return "True";
    }
    return "True";
}

static std::string traitDealDamageJson(std::string_view traitName,
                                       int baseDamage,
                                       DamageType damageType)
{
    std::ostringstream ss;
    ss << "{\n"
       << "  \"name\": \"" << traitName << "\",\n"
       << "  \"breakpoints\": [1],\n"
       << "  \"tiers\": [\n"
       << "    {\n"
       << "      \"breakpoint\": 1,\n"
       << "      \"effects\": [\n"
       << "        {\n"
       << "          \"hook\": \"OnCombatStart\",\n"
       << "          \"type\": \"DealDamage\",\n"
       << "          \"damageFormula\": {\n"
       << "            \"baseDamage\": " << baseDamage << ",\n"
       << "            \"adRatio\": 0,\n"
       << "            \"apRatio\": 0,\n"
       << "            \"damageType\": \"" << damageTypeJsonName(damageType) << "\"\n"
       << "          }\n"
       << "        }\n"
       << "      ]\n"
       << "    }\n"
       << "  ]\n"
       << "}\n";
    return ss.str();
}

static ContentManager loadSyntheticTraitContent(TraitEffectType type,
                                                std::string_view traitName,
                                                std::string_view statusName)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("tft_trait_effect_validation_" + traitEffectTypeName(type));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "traits");
    std::filesystem::create_directories(root / "abilities");
    std::filesystem::create_directories(root / "items");
    std::filesystem::create_directories(root / "champions");

    {
        std::ofstream f(root / "traits" / "validation_trait.json", std::ios::out | std::ios::binary | std::ios::trunc);
        f << traitFanoutJson(type, traitName, statusName);
    }

    ContentManager synthetic;
    synthetic.loadAll(root.string());
    return synthetic;
}

static ContentManager loadSyntheticTraitDamageContent(std::string_view traitName,
                                                      int baseDamage,
                                                      DamageType damageType)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("tft_trait_damage_validation_" + damageTypeJsonName(damageType));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "traits");
    std::filesystem::create_directories(root / "abilities");
    std::filesystem::create_directories(root / "items");
    std::filesystem::create_directories(root / "champions");

    {
        std::ofstream f(root / "traits" / "validation_trait.json", std::ios::out | std::ios::binary | std::ios::trunc);
        f << traitDealDamageJson(traitName, baseDamage, damageType);
    }

    ContentManager synthetic;
    synthetic.loadAll(root.string());
    return synthetic;
}

static ContentManager loadSyntheticTraitHealContent(std::string_view traitName, int healAmount)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "tft_trait_heal_validation";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "traits");
    std::filesystem::create_directories(root / "abilities");
    std::filesystem::create_directories(root / "items");
    std::filesystem::create_directories(root / "champions");

    {
        std::ofstream f(root / "traits" / "validation_trait.json", std::ios::out | std::ios::binary | std::ios::trunc);
        f << traitHealJson(traitName, healAmount);
    }

    ContentManager synthetic;
    synthetic.loadAll(root.string());
    return synthetic;
}

static ContentManager loadSyntheticTraitShieldContent(TraitEffectType type,
                                                      std::string_view traitName,
                                                      int shieldAmount)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("tft_trait_shield_validation_" + traitEffectTypeName(type));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "traits");
    std::filesystem::create_directories(root / "abilities");
    std::filesystem::create_directories(root / "items");
    std::filesystem::create_directories(root / "champions");

    {
        std::ofstream f(root / "traits" / "validation_trait.json", std::ios::out | std::ios::binary | std::ios::trunc);
        f << traitShieldJson(type, traitName, shieldAmount);
    }

    ContentManager synthetic;
    synthetic.loadAll(root.string());
    return synthetic;
}

static Unit makeValidationTraitUnit(std::string name, TeamId team, Position position, std::string_view traitName)
{
    Unit unit(std::move(name),
              1000,
              50,
              20,
              20,
              1000,
              1,
              DamageType::Physical,
              position,
              team);
    unit.addTrait(std::string(traitName));
    return unit;
}

static Unit makeValidationUnit(std::string name, TeamId team, Position position)
{
    return Unit(std::move(name),
                1000,
                50,
                20,
                20,
                1000,
                1,
                DamageType::Physical,
                position,
                team);
}

static std::vector<Unit> runTraitFanoutEffect(TraitEffectType type,
                                              bool includeDeadUnits,
                                              std::string_view statusName)
{
    static constexpr std::string_view TraitName = "ValidationFanoutTrait";
    ContentManager synthetic = loadSyntheticTraitContent(type, TraitName, statusName);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(makeValidationTraitUnit("ValidationTraitSource", TeamId::TeamA, Position{ 1, 1 }, TraitName));
    units.push_back(makeValidationUnit("ValidationAlly", TeamId::TeamA, Position{ 2, 1 }));
    units.push_back(makeValidationUnit("ValidationEnemy", TeamId::TeamB, Position{ 1, 5 }));
    units.push_back(makeValidationUnit("ValidationEnemyTwo", TeamId::TeamB, Position{ 2, 5 }));
    if (includeDeadUnits)
    {
        units.push_back(makeValidationUnit("ValidationDeadAlly", TeamId::TeamA, Position{ 3, 1 }));
        units.back().setHp(0);
        units.push_back(makeValidationUnit("ValidationDeadEnemy", TeamId::TeamB, Position{ 3, 5 }));
        units.back().setHp(0);
    }

    GameState state(std::move(board), std::move(units), std::move(logger), synthetic);
    const std::vector<ActiveTrait> activeTraits = {
        ActiveTrait{ std::string(TraitName), 1, 1 }
    };

    TraitEffectExecutor::apply(
        state,
        TeamId::TeamA,
        activeTraits,
        TraitHook::OnCombatStart,
        nullptr,
        nullptr
    );

    return state.units();
}

static StatusEffect makeValidationShieldStatus(int amount)
{
    StatusEffect shield{};
    shield.name = "Validation Enemy Shield";
    shield.effectType = StatusEffectType::Shield;
    shield.crowdControlType = CrowdControlType::None;
    shield.affectedStat = StatType::None;
    shield.modifierType = ModifierType::Flat;
    shield.value = static_cast<float>(amount);
    shield.durationMs = ValidationConstants::LongDurationMs;
    shield.remainingMs = ValidationConstants::LongDurationMs;
    shield.tickIntervalMs = 0;
    shield.tickTimerMs = 0;
    shield.damageType = DamageType::TrueDamage;
    return shield;
}

static std::vector<Unit> runTraitDamageEffect(DamageType damageType,
                                              bool includeDeadEnemy,
                                              bool shieldEnemy,
                                              int baseDamage)
{
    static constexpr std::string_view TraitName = "ValidationDamageTrait";
    ContentManager synthetic = loadSyntheticTraitDamageContent(TraitName, baseDamage, damageType);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(makeValidationTraitUnit("ValidationTraitSource", TeamId::TeamA, Position{ 1, 1 }, TraitName));
    units.push_back(makeValidationUnit("ValidationAlly", TeamId::TeamA, Position{ 2, 1 }));
    units.push_back(makeValidationUnit("ValidationEnemy", TeamId::TeamB, Position{ 1, 5 }));
    units.push_back(makeValidationUnit("ValidationEnemyTwo", TeamId::TeamB, Position{ 2, 5 }));

    for (Unit& unit : units)
    {
        unit.setArmor(0);
        unit.setMagicResist(0);
    }
    if (shieldEnemy)
    {
        units[2].addStatusEffect(makeValidationShieldStatus(baseDamage));
    }
    if (includeDeadEnemy)
    {
        units.push_back(makeValidationUnit("ValidationDeadEnemy", TeamId::TeamB, Position{ 3, 5 }));
        units.back().setHp(0);
    }

    GameState state(std::move(board), std::move(units), std::move(logger), synthetic);
    const std::vector<ActiveTrait> activeTraits = {
        ActiveTrait{ std::string(TraitName), 1, 1 }
    };

    TraitEffectExecutor::apply(
        state,
        TeamId::TeamA,
        activeTraits,
        TraitHook::OnCombatStart,
        nullptr,
        nullptr
    );

    return state.units();
}

static std::vector<Unit> runTraitHealEffect(bool includeDeadUnits, int healAmount)
{
    static constexpr std::string_view TraitName = "ValidationHealTrait";
    ContentManager synthetic = loadSyntheticTraitHealContent(TraitName, healAmount);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(makeValidationTraitUnit("ValidationTraitSource", TeamId::TeamA, Position{ 1, 1 }, TraitName));
    units.push_back(makeValidationUnit("ValidationAlly", TeamId::TeamA, Position{ 2, 1 }));
    units.push_back(makeValidationUnit("ValidationEnemy", TeamId::TeamB, Position{ 1, 5 }));

    units[0].setHp(700);
    units[1].setHp(800);
    units[2].setHp(700);

    if (includeDeadUnits)
    {
        units.push_back(makeValidationUnit("ValidationDeadAlly", TeamId::TeamA, Position{ 3, 1 }));
        units.back().setHp(0);
        units.push_back(makeValidationUnit("ValidationDeadEnemy", TeamId::TeamB, Position{ 3, 5 }));
        units.back().setHp(0);
    }

    GameState state(std::move(board), std::move(units), std::move(logger), synthetic);
    const std::vector<ActiveTrait> activeTraits = {
        ActiveTrait{ std::string(TraitName), 1, 1 }
    };

    TraitEffectExecutor::apply(
        state,
        TeamId::TeamA,
        activeTraits,
        TraitHook::OnCombatStart,
        nullptr,
        nullptr
    );

    return state.units();
}

static std::vector<Unit> runTraitShieldEffect(TraitEffectType type,
                                              bool includeDeadUnits,
                                              int shieldAmount)
{
    static constexpr std::string_view TraitName = "ValidationShieldTrait";
    ContentManager synthetic = loadSyntheticTraitShieldContent(type, TraitName, shieldAmount);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(makeValidationTraitUnit("ValidationTraitSource", TeamId::TeamA, Position{ 1, 1 }, TraitName));
    units.push_back(makeValidationUnit("ValidationAlly", TeamId::TeamA, Position{ 2, 1 }));
    units.push_back(makeValidationUnit("ValidationEnemy", TeamId::TeamB, Position{ 1, 5 }));
    if (includeDeadUnits)
    {
        units.push_back(makeValidationUnit("ValidationDeadAlly", TeamId::TeamA, Position{ 3, 1 }));
        units.back().setHp(0);
        units.push_back(makeValidationUnit("ValidationDeadEnemy", TeamId::TeamB, Position{ 3, 5 }));
        units.back().setHp(0);
    }

    GameState state(std::move(board), std::move(units), std::move(logger), synthetic);
    const std::vector<ActiveTrait> activeTraits = {
        ActiveTrait{ std::string(TraitName), 1, 1 }
    };

    TraitEffectExecutor::apply(
        state,
        TeamId::TeamA,
        activeTraits,
        TraitHook::OnCombatStart,
        nullptr,
        nullptr
    );

    return state.units();
}

static const StatusEffect* findStatusNamed(const Unit& unit, std::string_view statusName)
{
    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.name == statusName)
        {
            return &effect;
        }
    }
    return nullptr;
}

static bool hasActiveShield(const Unit& unit, int expectedAmount)
{
    const StatusEffect* shield = findStatusNamed(unit, "Trait Shield");
    return shield &&
           shield->effectType == StatusEffectType::Shield &&
           shield->remainingMs > 0 &&
           static_cast<int>(shield->value) == expectedAmount;
}

static void traitEffectVocabularyValidationTest(ValidationReport& report)
{
    {
        static constexpr std::string_view StatusName = "Validation Allies Status";
        const std::vector<Unit> units =
            runTraitFanoutEffect(TraitEffectType::ApplyStatusToAllies, false, StatusName);
        const bool ok =
            hasStatusNamed(units[0], StatusName) &&
            hasStatusNamed(units[1], StatusName) &&
            !hasStatusNamed(units[2], StatusName) &&
            !hasStatusNamed(units[3], StatusName);
        if (ok) report.pass("TraitEffect: ApplyStatusToAllies applies to alive allies");
        else report.fail("TraitEffect: ApplyStatusToAllies applies to alive allies");
    }

    {
        static constexpr std::string_view StatusName = "Validation Enemies Status";
        const std::vector<Unit> units =
            runTraitFanoutEffect(TraitEffectType::ApplyStatusToEnemies, false, StatusName);
        const bool ok =
            !hasStatusNamed(units[0], StatusName) &&
            !hasStatusNamed(units[1], StatusName) &&
            hasStatusNamed(units[2], StatusName) &&
            hasStatusNamed(units[3], StatusName);
        if (ok) report.pass("TraitEffect: ApplyStatusToEnemies applies to alive enemies");
        else report.fail("TraitEffect: ApplyStatusToEnemies applies to alive enemies");
    }

    {
        static constexpr std::string_view StatusName = "Validation Dead Skip Status";
        const std::vector<Unit> units =
            runTraitFanoutEffect(TraitEffectType::ApplyStatusToAllies, true, StatusName);
        const bool ok =
            hasStatusNamed(units[0], StatusName) &&
            hasStatusNamed(units[1], StatusName) &&
            !hasStatusNamed(units[4], StatusName) &&
            !hasStatusNamed(units[5], StatusName);
        if (ok) report.pass("TraitEffect: status fanout skips dead units");
        else report.fail("TraitEffect: status fanout skips dead units");
    }

    {
        static constexpr std::string_view StatusName = "Validation EnemyTeam Status";
        const std::vector<Unit> units =
            runTraitFanoutEffect(TraitEffectType::ApplyStatusToEnemyTeam, false, StatusName);
        const bool ok =
            !hasStatusNamed(units[0], StatusName) &&
            !hasStatusNamed(units[1], StatusName) &&
            hasStatusNamed(units[2], StatusName) &&
            hasStatusNamed(units[3], StatusName);
        if (ok) report.pass("TraitEffect: ApplyStatusToEnemyTeam remains compatible");
        else report.fail("TraitEffect: ApplyStatusToEnemyTeam remains compatible");
    }

    {
        static constexpr int ShieldAmount = 120;
        const std::vector<Unit> units =
            runTraitShieldEffect(TraitEffectType::Shield, false, ShieldAmount);
        const bool ok =
            hasActiveShield(units[0], ShieldAmount) &&
            hasActiveShield(units[1], ShieldAmount) &&
            !hasStatusNamed(units[2], "Trait Shield");
        if (ok) report.pass("TraitEffect: Shield applies to alive allies");
        else report.fail("TraitEffect: Shield applies to alive allies");
    }

    {
        static constexpr int ShieldAmount = 120;
        const std::vector<Unit> units =
            runTraitShieldEffect(TraitEffectType::Shield, true, ShieldAmount);
        const bool ok =
            hasActiveShield(units[0], ShieldAmount) &&
            hasActiveShield(units[1], ShieldAmount) &&
            !hasStatusNamed(units[3], "Trait Shield") &&
            !hasStatusNamed(units[4], "Trait Shield");
        if (ok) report.pass("TraitEffect: Shield skips dead units");
        else report.fail("TraitEffect: Shield skips dead units");
    }

    {
        static constexpr int ShieldAmount = 120;
        static constexpr int DamageAmount = 90;
        std::vector<Unit> units =
            runTraitShieldEffect(TraitEffectType::Shield, false, ShieldAmount);
        const int hpBefore = units[0].getHp();
        units[0].applyDamage(DamageAmount);
        const bool ok =
            units[0].getHp() == hpBefore &&
            hasActiveShield(units[0], ShieldAmount - DamageAmount);
        if (ok) report.pass("TraitEffect: Shield absorbs incoming damage");
        else report.fail("TraitEffect: Shield absorbs incoming damage");
    }

    {
        static constexpr int ShieldAmount = 120;
        const std::vector<Unit> units =
            runTraitShieldEffect(TraitEffectType::ShieldOnCombatStart, false, ShieldAmount);
        const bool ok =
            hasActiveShield(units[0], ShieldAmount) &&
            !hasStatusNamed(units[1], "Trait Shield") &&
            !hasStatusNamed(units[2], "Trait Shield");
        if (ok) report.pass("TraitEffect: ShieldOnCombatStart remains compatible");
        else report.fail("TraitEffect: ShieldOnCombatStart remains compatible");
    }

    {
        static constexpr int HealAmount = 120;
        const std::vector<Unit> units = runTraitHealEffect(false, HealAmount);
        const bool ok =
            units[0].getHp() == 820 &&
            units[1].getHp() == 920;
        if (ok) report.pass("TraitEffect: Heal increases damaged allied HP");
        else report.fail("TraitEffect: Heal increases damaged allied HP");
    }

    {
        static constexpr int HealAmount = 500;
        const std::vector<Unit> units = runTraitHealEffect(false, HealAmount);
        const bool ok =
            units[0].getHp() == units[0].getMaxHp() &&
            units[1].getHp() == units[1].getMaxHp();
        if (ok) report.pass("TraitEffect: Heal does not exceed max HP");
        else report.fail("TraitEffect: Heal does not exceed max HP");
    }

    {
        static constexpr int HealAmount = 120;
        const std::vector<Unit> units = runTraitHealEffect(true, HealAmount);
        const bool ok =
            units[3].getHp() == 0 &&
            units[4].getHp() == 0;
        if (ok) report.pass("TraitEffect: Heal skips dead units");
        else report.fail("TraitEffect: Heal skips dead units");
    }

    {
        static constexpr int HealAmount = 120;
        const std::vector<Unit> units = runTraitHealEffect(false, HealAmount);
        const bool ok = units[2].getHp() == 700;
        if (ok) report.pass("TraitEffect: Heal does not affect enemies");
        else report.fail("TraitEffect: Heal does not affect enemies");
    }

    {
        static constexpr int DamageAmount = 120;
        const std::vector<Unit> units =
            runTraitDamageEffect(DamageType::TrueDamage, false, false, DamageAmount);
        const bool ok =
            units[2].getHp() == 1000 - DamageAmount &&
            units[3].getHp() == 1000 - DamageAmount;
        if (ok) report.pass("TraitEffect: DealDamage reduces enemy HP");
        else report.fail("TraitEffect: DealDamage reduces enemy HP");
    }

    {
        static constexpr int DamageAmount = 120;
        const std::vector<Unit> units =
            runTraitDamageEffect(DamageType::TrueDamage, false, false, DamageAmount);
        const bool ok =
            units[0].getHp() == 1000 &&
            units[1].getHp() == 1000;
        if (ok) report.pass("TraitEffect: DealDamage does not affect allies");
        else report.fail("TraitEffect: DealDamage does not affect allies");
    }

    {
        static constexpr int DamageAmount = 120;
        const std::vector<Unit> units =
            runTraitDamageEffect(DamageType::TrueDamage, true, false, DamageAmount);
        const bool ok = units[4].getHp() == 0;
        if (ok) report.pass("TraitEffect: DealDamage skips dead enemies");
        else report.fail("TraitEffect: DealDamage skips dead enemies");
    }

    {
        static constexpr int DamageAmount = 120;
        const std::vector<Unit> units =
            runTraitDamageEffect(DamageType::TrueDamage, false, true, DamageAmount);
        const StatusEffect* shield = findStatusNamed(units[2], "Validation Enemy Shield");
        const bool ok =
            units[2].getHp() == 1000 &&
            shield &&
            static_cast<int>(shield->value) == 0;
        if (ok) report.pass("TraitEffect: DealDamage is absorbed by shields");
        else report.fail("TraitEffect: DealDamage is absorbed by shields");
    }

    {
        static constexpr int DamageAmount = 120;
        const std::vector<Unit> physical =
            runTraitDamageEffect(DamageType::Physical, false, false, DamageAmount);
        const std::vector<Unit> magic =
            runTraitDamageEffect(DamageType::Magic, false, false, DamageAmount);
        const std::vector<Unit> trueDamage =
            runTraitDamageEffect(DamageType::TrueDamage, false, false, DamageAmount);
        const bool ok =
            physical[2].getHp() == 1000 - DamageAmount &&
            magic[2].getHp() == 1000 - DamageAmount &&
            trueDamage[2].getHp() == 1000 - DamageAmount;
        if (ok) report.pass("TraitEffect: DealDamage supports physical magic true formulas");
        else report.fail("TraitEffect: DealDamage supports physical magic true formulas");
    }
}

static std::size_t effectCountForTrait(const TraitDefinition& trait)
{
    std::size_t count = 0;
    for (const TraitTier& tier : trait.tiers)
    {
        count += tier.effects.size();
    }
    return count;
}

static std::size_t countJsonFiles(const std::filesystem::path& dir)
{
    if (!std::filesystem::exists(dir))
    {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            count += 1;
        }
    }
    return count;
}

static std::vector<std::string> orderedItemCategories()
{
    return {
        "CombatItem",
        "Emblem",
        "RadiantItem",
        "Artifact",
        "SupportItem",
        "Consumable",
        "Anvil",
        "LootObject",
        "Augment",
        "Unknown"
    };
}

struct TraitStatMappingProjection
{
    bool cacheFound = false;
    std::size_t rawTraitRecords = 0;
    std::size_t projectedTraitsWithExecutableEffects = 0;
    std::size_t mappedVariables = 0;
    std::size_t unmappedVariables = 0;
    std::vector<std::pair<std::string, std::size_t>> topUnmappedVariables;
};

static bool hasJsonKey(const JsonValue& obj, std::string_view key)
{
    return obj.isObject() && obj.hasKey(key);
}

static std::string jsonStringOr(const JsonValue& obj, std::string_view key, std::string fallback)
{
    if (!hasJsonKey(obj, key))
    {
        return fallback;
    }
    const JsonValue& v = obj.at(key);
    return v.isString() ? v.asString() : fallback;
}

static std::string compactLowerKeyForValidation(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

static std::string lowerTextForValidation(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool rawDescriptionAllowsTeamwideBonus(std::string_view varName, std::string description)
{
    description = lowerTextForValidation(std::move(description));
    const std::string marker = "@" + std::string(varName);
    std::string lowerMarker = marker;
    lowerMarker = lowerTextForValidation(std::move(lowerMarker));
    return description.find("your team gains " + lowerMarker) != std::string::npos ||
           description.find("all allies gain " + lowerMarker) != std::string::npos ||
           description.find("all units gain " + lowerMarker) != std::string::npos ||
           description.find("allies gain " + lowerMarker) != std::string::npos;
}

static bool isAllowedStep12FTraitStatVariable(std::string_view varName, const std::string& description)
{
    const std::string v = compactLowerKeyForValidation(varName);
    return v == "attackspeedpercent" ||
           v == "ad" ||
           v == "bonusad" ||
           v == "ap" ||
           v == "bonushealth" ||
           v == "healthbonus" ||
           v == "omnivamp" ||
           v == "critchance" ||
           v == "critdamage" ||
           v == "durability" ||
           v == "damagereductionpct" ||
           v == "enhanceddurability" ||
           v == "percentdamageincrease" ||
           v == "bonusarmor" ||
           v == "armor" ||
           v == "bonusmr" ||
           v == "mr" ||
           v == "magicresist" ||
           v == "teamwideas" ||
           v == "teamwideresists" ||
           v == "bonusoffensivestat" ||
           v == "bonusoffensivestats" ||
           v == "attackspeed" ||
           v == "pctresists" ||
           v == "maxhealth" ||
           v == "maxhp" ||
           v == "hp" ||
           v == "health" ||
           v == "damageamp" ||
           v == "damageincrease" ||
           v == "bonusdamage" ||
           v == "damagereduction" ||
           v == "resistances" ||
           (v == "teamwidebonus" && rawDescriptionAllowsTeamwideBonus(varName, description));
}

static const JsonValue* findLatestRawTraitArray(const JsonValue& root)
{
    if (hasJsonKey(root, "sets") && root.at("sets").isObject())
    {
        const auto& sets = root.at("sets").asObject();
        int bestSet = std::numeric_limits<int>::min();
        const JsonValue* best = nullptr;
        for (const auto& [key, setValue] : sets)
        {
            int setNumber = 0;
            bool numeric = !key.empty();
            for (char c : key)
            {
                if (c < '0' || c > '9')
                {
                    numeric = false;
                    break;
                }
                setNumber = setNumber * 10 + (c - '0');
            }
            if (!numeric || !setValue.isObject() || !hasJsonKey(setValue, "traits"))
            {
                continue;
            }
            if (setNumber >= bestSet)
            {
                bestSet = setNumber;
                best = &setValue.at("traits");
            }
        }
        if (best && best->isArray())
        {
            return best;
        }
    }

    if (hasJsonKey(root, "setData") && root.at("setData").isArray())
    {
        int bestSet = std::numeric_limits<int>::min();
        const JsonValue* best = nullptr;
        for (const JsonValue& setValue : root.at("setData").asArray())
        {
            if (!setValue.isObject() || !hasJsonKey(setValue, "traits"))
            {
                continue;
            }
            int setNumber = 0;
            if (hasJsonKey(setValue, "number") && setValue.at("number").isNumber())
            {
                setNumber = static_cast<int>(std::lround(setValue.at("number").asNumber()));
            }
            if (setNumber >= bestSet)
            {
                bestSet = setNumber;
                best = &setValue.at("traits");
            }
        }
        if (best && best->isArray())
        {
            return best;
        }
    }

    if (hasJsonKey(root, "traits") && root.at("traits").isArray())
    {
        return &root.at("traits");
    }
    return nullptr;
}

static TraitStatMappingProjection projectTraitStatMappingFromRawCache(const std::filesystem::path& cachePath)
{
    TraitStatMappingProjection projection{};
    std::ifstream f(cachePath, std::ios::in | std::ios::binary);
    if (!f.is_open())
    {
        return projection;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    const JsonValue root = parseJson(ss.str());
    const JsonValue* traits = findLatestRawTraitArray(root);
    if (!traits || !traits->isArray())
    {
        return projection;
    }

    projection.cacheFound = true;
    std::map<std::string, std::size_t> unmappedFrequency;
    for (const JsonValue& trait : traits->asArray())
    {
        if (!trait.isObject())
        {
            continue;
        }
        projection.rawTraitRecords += 1;
        const std::string description =
            jsonStringOr(trait, "desc", jsonStringOr(trait, "description", ""));
        bool mappedTrait = false;
        if (!hasJsonKey(trait, "effects") || !trait.at("effects").isArray())
        {
            continue;
        }
        auto countVariable = [&](const std::string& name, const JsonValue& value)
        {
            if (value.isNumber() &&
                std::fabs(value.asNumber()) > 0.00001 &&
                isAllowedStep12FTraitStatVariable(name, description))
            {
                projection.mappedVariables += 1;
                mappedTrait = true;
            }
            else
            {
                projection.unmappedVariables += 1;
                unmappedFrequency[name] += 1;
            }
        };

        for (const JsonValue& effect : trait.at("effects").asArray())
        {
            if (!effect.isObject())
            {
                continue;
            }
            std::vector<std::string> seenVariables;
            if (hasJsonKey(effect, "variables") && effect.at("variables").isObject())
            {
                for (const auto& [name, value] : effect.at("variables").asObject())
                {
                    seenVariables.push_back(name);
                    countVariable(name, value);
                }
            }
            for (const auto& [name, value] : effect.asObject())
            {
                if (name == "minUnits" || name == "maxUnits" || name == "style" || name == "variables")
                {
                    continue;
                }
                if (std::find(seenVariables.begin(), seenVariables.end(), name) != seenVariables.end())
                {
                    continue;
                }
                countVariable(name, value);
            }
        }
        if (mappedTrait)
        {
            projection.projectedTraitsWithExecutableEffects += 1;
        }
    }
    projection.topUnmappedVariables.assign(unmappedFrequency.begin(), unmappedFrequency.end());
    std::sort(projection.topUnmappedVariables.begin(), projection.topUnmappedVariables.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second)
        {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    if (projection.topUnmappedVariables.size() > 10)
    {
        projection.topUnmappedVariables.resize(10);
    }
    return projection;
}

static void contentFidelityValidationTest(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    const std::filesystem::path dataRoot = std::filesystem::absolute(std::filesystem::path("../../data"));
    const TraitStatMappingProjection traitMappingProjection =
        projectTraitStatMappingFromRawCache(dataRoot / "_import_cache" / "cdragon_tft_en_us.json");

    const std::size_t championFiles = countJsonFiles(dataRoot / "champions");
    const std::size_t abilityFiles = countJsonFiles(dataRoot / "abilities");
    const std::size_t traitFiles = countJsonFiles(dataRoot / "traits");
    const std::size_t itemFiles = countJsonFiles(dataRoot / "items");

    std::size_t placeholderAbilities = 0;
    std::size_t explicitPlaceholderAbilities = 0;
    for (const auto& [_, ability] : content.abilities())
    {
        if (isPlaceholderSingleTargetMagicAbility(ability))
        {
            placeholderAbilities += 1;
        }
        if (ability.metadata.isPlaceholder)
        {
            explicitPlaceholderAbilities += 1;
        }
    }
    const std::size_t nonPlaceholderAbilities = content.abilityCount() - placeholderAbilities;

    std::size_t traitsWithEffects = 0;
    std::size_t traitsWithoutEffects = 0;
    std::size_t explicitPlaceholderTraits = 0;
    std::map<std::string, std::size_t> traitVariantGroups;
    std::size_t traitVariants = 0;
    std::size_t activeTraitVariants = 0;
    for (const auto& [_, trait] : content.traits())
    {
        if (effectCountForTrait(trait) > 0)
        {
            traitsWithEffects += 1;
        }
        else
        {
            traitsWithoutEffects += 1;
        }
        if (trait.metadata.isPlaceholder)
        {
            explicitPlaceholderTraits += 1;
        }
        if (trait.metadata.isVariant)
        {
            traitVariants += 1;
            const std::string group = trait.metadata.variantGroup.empty() ? "Unknown" : trait.metadata.variantGroup;
            traitVariantGroups[group] += 1;
        }
    }

    std::size_t passiveOnlyItems = 0;
    std::size_t triggeredItems = 0;
    std::size_t noRuntimeEffectItems = 0;
    std::size_t explicitPlaceholderItems = 0;
    std::map<std::string, std::size_t> itemCategoryCounts;
    const std::vector<std::string> categoryOrder = orderedItemCategories();
    for (const std::string& category : categoryOrder)
    {
        itemCategoryCounts[category] = 0;
    }
    for (const auto& [_, item] : content.items())
    {
        const bool hasPassive = !item.passiveStats.empty();
        const bool hasTriggered = !item.triggeredEffects.empty();
        const std::string category = item.metadata.itemCategory.empty() ? "Unknown" : item.metadata.itemCategory;
        if (hasTriggered)
        {
            triggeredItems += 1;
        }
        if (hasPassive && !hasTriggered)
        {
            passiveOnlyItems += 1;
        }
        if (!hasPassive && !hasTriggered)
        {
            noRuntimeEffectItems += 1;
        }
        if (item.metadata.isPlaceholder)
        {
            explicitPlaceholderItems += 1;
        }
        itemCategoryCounts[category] += 1;
    }

    out << "Content Fidelity Summary\n";
    out << "Data root: " << dataRoot.string() << "\n";
    out << "JSON files | champions=" << championFiles
        << " abilities=" << abilityFiles
        << " traits=" << traitFiles
        << " items=" << itemFiles << "\n";
    out << "Loaded content | champions=" << content.championCount()
        << " abilities=" << content.abilityCount()
        << " traits=" << content.traitCount()
        << " items=" << content.itemCount() << "\n";
    out << "Abilities | placeholder_single_target_magic=" << placeholderAbilities
        << " non_placeholder=" << nonPlaceholderAbilities << "\n";
    out << "Traits | executable_effects=" << traitsWithEffects
        << " empty_or_no_executable_effects=" << traitsWithoutEffects << "\n";
    out << "Trait variants | groups=" << traitVariantGroups.size()
        << " variants=" << traitVariants
        << " active_variants=" << activeTraitVariants << "\n";
    out << "Trait variant groups |";
    if (traitVariantGroups.empty())
    {
        out << " none";
    }
    for (const auto& [group, count] : traitVariantGroups)
    {
        out << " " << group << "=" << count;
    }
    out << "\n";
    out << "Trait stat mapping | current_executable_before_import=" << traitsWithEffects
        << " projected_executable_after_import=" << traitMappingProjection.projectedTraitsWithExecutableEffects
        << " raw_trait_records=" << traitMappingProjection.rawTraitRecords
        << " mapped_variables=" << traitMappingProjection.mappedVariables
        << " unmapped_variables=" << traitMappingProjection.unmappedVariables << "\n";
    out << "Trait stat mapping top unmapped |";
    if (traitMappingProjection.topUnmappedVariables.empty())
    {
        out << " none";
    }
    for (const auto& [name, count] : traitMappingProjection.topUnmappedVariables)
    {
        out << " " << name << "=" << count;
    }
    out << "\n";
    out << "Items | passive_stats_only=" << passiveOnlyItems
        << " triggered_effects=" << triggeredItems
        << " no_runtime_effects=" << noRuntimeEffectItems << "\n";
    out << "Placeholder flags | abilities=" << explicitPlaceholderAbilities
        << " traits=" << explicitPlaceholderTraits
        << " items=" << explicitPlaceholderItems << "\n";
    out << "Item categories |";
    for (const std::string& category : categoryOrder)
    {
        out << " " << category << "=" << itemCategoryCounts[category];
    }
    for (const auto& [category, count] : itemCategoryCounts)
    {
        if (std::find(categoryOrder.begin(), categoryOrder.end(), category) == categoryOrder.end())
        {
            out << " " << category << "=" << count;
        }
    }
    out << "\n";

    if (championFiles != content.championCount() ||
        abilityFiles != content.abilityCount() ||
        traitFiles != content.traitCount() ||
        itemFiles != content.itemCount())
    {
        report.warning("Content fidelity: loaded content count differs from normalized JSON file count");
    }
    if (placeholderAbilities > 0)
    {
        report.warning("Content fidelity: placeholder single-target magic abilities present");
    }
    if (traitsWithoutEffects > 0)
    {
        report.warning("Content fidelity: traits with empty/no executable effects present");
    }
    if (!traitMappingProjection.cacheFound)
    {
        report.warning("Content fidelity: raw TFT cache missing for trait stat mapping projection");
    }
    if (passiveOnlyItems > 0)
    {
        report.warning("Content fidelity: passive-stat-only items present");
    }
    if (noRuntimeEffectItems > 0)
    {
        report.warning("Content fidelity: items with no runtime effects present");
    }

    report.pass("Content fidelity summary generated");
}

static void critStatisticsTest(ValidationReport& report, std::ostream& out)
{
    const float expected = ValidationConstants::CritExpectedChance;
    const int trials = ValidationConstants::CritConsistencyTrials;
    RandomManager::global().setSeed(ValidationConstants::CritTestSeed);

    int crits = 0;
    for (int i = 0; i < trials; ++i)
    {
        if (DamageSystem::rollChance(expected))
        {
            crits += 1;
        }
    }
    const float actual = static_cast<float>(crits) / static_cast<float>(trials);
    out << "Expected Crit: " << std::lround(expected * AIConstants::PercentScale) << "%\n";
    out << "Actual Crit: " << std::fixed << std::setprecision(1) << (actual * AIConstants::PercentScale) << "%\n";
    const float diff = std::fabs(actual - expected);
    if (diff <= ValidationConstants::CritDiffPass)
    {
        report.pass("Crit consistency");
    }
    else if (diff <= ValidationConstants::CritDiffWarn)
    {
        report.warning("Crit consistency");
    }
    else
    {
        report.fail("Crit consistency");
    }
}

static void armorMrFormulaTest(ValidationReport& report, std::ostream& out)
{
    const int raw = ValidationConstants::ArmorMrRawDamage;
    std::vector<int> finals;
    for (int armor = 0; armor <= ValidationConstants::ArmorMrMax; armor += ValidationConstants::ArmorMrStep)
    {
        Unit dummy("Dummy",
                   ValidationConstants::DummyHp,
                   0,
                   armor,
                   armor,
                   ValidationConstants::DummyAttackCooldownMs,
                   ValidationConstants::DummyRange,
                   DamageType::Physical,
                   Position{ 0, 0 },
                   TeamId::TeamB);
        dummy.setArmor(armor);
        dummy.setMagicResist(armor);
        DamageDebugResult phys = DamageSystem::calculateDamageDebug(raw, DamageType::Physical, dummy);
        DamageDebugResult mag = DamageSystem::calculateDamageDebug(raw, DamageType::Magic, dummy);
        finals.push_back(phys.finalDamage);
        out << "Raw: " << raw << " Armor: " << armor
            << " Reduction: " << std::fixed << std::setprecision(1) << (reductionPercent(phys) * AIConstants::PercentScale) << "% Final: " << phys.finalDamage << "\n";
        out << "Raw: " << raw << " MR: " << armor
            << " Reduction: " << std::fixed << std::setprecision(1) << (reductionPercent(mag) * AIConstants::PercentScale) << "% Final: " << mag.finalDamage << "\n";
    }

    bool monotonic = true;
    for (std::size_t i = 1; i < finals.size(); ++i)
    {
        if (finals[i] > finals[i - 1])
        {
            monotonic = false;
            break;
        }
    }

    if (monotonic)
    {
        report.pass("Armor/MR formula stability");
    }
    else
    {
        report.fail("Armor/MR formula stability");
    }
}

static void areaShapeTest(ValidationReport& report, std::ostream& out)
{
    constexpr int OriginX = 5;
    constexpr int OriginY = 5;
    constexpr int DirX = 8;
    constexpr int DirY = 5;

    constexpr int CircleRadius = 1;
    constexpr int CircleMinCells = 7;
    constexpr int LineRadius = 3;
    constexpr int LineMinCells = 4;
    constexpr int CrossRadius = 2;
    constexpr int CrossMinCells = 1;
    constexpr int GridRadius = 1;
    constexpr int GridMinCells = 9;
    constexpr int ConeRadius = 2;
    constexpr int ConeMinCells = 1;

    Board b(GameConstants::BoardWidth, GameConstants::BoardHeight);
    const Position origin{ OriginX, OriginY };
    const Position dir{ DirX, DirY };

    struct Case { AreaShape shape; int radius; int expectedMin; };
    const Case cases[] = {
        { AreaShape::CircleRadius, CircleRadius, CircleMinCells },
        { AreaShape::Line, LineRadius, LineMinCells },
        { AreaShape::Cross, CrossRadius, CrossMinCells },
        { AreaShape::Grid, GridRadius, GridMinCells },
        { AreaShape::Cone, ConeRadius, ConeMinCells }
    };

    bool ok = true;
    for (const Case& c : cases)
    {
        const std::vector<Position> cells = getPositionsInArea(b, origin, c.shape, c.radius, dir);
        out << "Shape " << static_cast<int>(c.shape) << " radius " << c.radius << " count " << cells.size() << "\n";
        for (const Position& p : cells)
        {
            out << "(" << p.x << "," << p.y << ")\n";
        }
        std::unordered_map<std::int64_t, int> seen;
        for (const Position& p : cells)
        {
            const std::int64_t key = (static_cast<std::int64_t>(p.x) << 32) ^ static_cast<std::int64_t>(p.y);
            seen[key] += 1;
            if (seen[key] > 1)
            {
                ok = false;
            }
        }
        if (static_cast<int>(cells.size()) < c.expectedMin)
        {
            ok = false;
        }
    }

    if (ok)
    {
        report.pass("AoE targeting");
    }
    else
    {
        report.fail("AoE targeting");
    }
}

static void delayedEventTest(const ContentManager& content, ValidationReport& report)
{
    constexpr std::int32_t Event1Ms = 3200;
    constexpr std::int32_t Event3Ms = 3400;
    constexpr int TickCount = 40;
    constexpr std::size_t ExpectedEvents = 3;

    RandomManager::global().setSeed(ValidationConstants::TargetSelectionSeed);
    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Normal);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(content.createUnit(pickChampionByIndex(content, 0), Position{ 4, 7 }, TeamId::TeamA));
    units.push_back(content.createUnit(pickChampionByIndex(content, 1), Position{ 4, 2 }, TeamId::TeamB));

    GameState state(std::move(board), std::move(units), std::move(logger), content);
    state.setDtMs(ValidationConstants::DefaultDtMs);

    std::vector<std::string> executed;
    std::vector<std::int32_t> executedAt;

    state.scheduleCombatEvent(Event1Ms, [&executed, &executedAt, &state]() { executed.push_back("E1"); executedAt.push_back(state.timeMs()); }, "E1");
    state.scheduleCombatEvent(Event1Ms, [&executed, &executedAt, &state]() { executed.push_back("E2"); executedAt.push_back(state.timeMs()); }, "E2");
    state.scheduleCombatEvent(Event3Ms, [&executed, &executedAt, &state]() { executed.push_back("E3"); executedAt.push_back(state.timeMs()); }, "E3");

    for (int i = 0; i < TickCount; ++i)
    {
        state.advanceTick();
        state.processCombatEvents();
    }

    const bool onceEach = executed.size() == ExpectedEvents &&
                          executedAt.size() == ExpectedEvents &&
                          executed[0] == "E1" &&
                          executed[1] == "E2" &&
                          executed[2] == "E3" &&
                          executedAt[0] == Event1Ms &&
                          executedAt[1] == Event1Ms &&
                          executedAt[2] == Event3Ms;
    if (onceEach)
    {
        report.pass("Delayed events");
    }
    else
    {
        report.fail("Delayed events");
    }
}

static void replayConsistencyTest(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    constexpr std::int32_t DtMs = ValidationConstants::DefaultDtMs;
    constexpr int ReplayCount = ValidationConstants::ReplayIterations;
    std::vector<Unit> a;
    std::vector<Unit> b;
    a.push_back(content.createUnit(pickChampionByIndex(content, 0), Position{ 4, 7 }, TeamId::TeamA));
    a.push_back(content.createUnit(pickChampionByIndex(content, 1), Position{ 5, 7 }, TeamId::TeamA));
    b.push_back(content.createUnit(pickChampionByIndex(content, 2), Position{ 4, 2 }, TeamId::TeamB));
    b.push_back(content.createUnit(pickChampionByIndex(content, 3), Position{ 5, 2 }, TeamId::TeamB));

    a[0].addStatusEffect(makePercentBuff("Validation AS", StatusEffectType::BonusAttackSpeed, StatType::AttackSpeed, ValidationConstants::BuffAttackSpeedNeutral, ValidationConstants::LongDurationMs));
    a[0].addStatusEffect(makeFlatBuff("Validation Crit", StatusEffectType::CritChanceBonus, StatType::CritChance, ValidationConstants::BuffCritChanceLow, ValidationConstants::LongDurationMs));
    a[0].addStatusEffect(makeFlatBuff("Validation CritDmg", StatusEffectType::CritDamageBonus, StatType::CritDamage, ValidationConstants::BuffCritDamageLow, ValidationConstants::LongDurationMs));

    const std::uint32_t seed = ValidationConstants::DefaultSeed;
    std::int32_t winner0 = 0;
    const std::string log0 = runCombatAndCaptureLog(
        content,
        seed,
        a,
        b,
        DtMs,
        LogMode::Normal,
        true,
        true,
        &winner0
    );

    for (int i = 1; i < ReplayCount; ++i)
    {
        std::int32_t winner = 0;
        const std::string logN = runCombatAndCaptureLog(
            content,
            seed,
            a,
            b,
            DtMs,
            LogMode::Normal,
            true,
            true,
            &winner
        );
        if (winner != winner0 || logN != log0)
        {
            report.fail("Deterministic replay");
            out << "Replay mismatch at iteration " << i << "\n";
            divergenceReport(log0, logN, out);
            return;
        }
    }

    report.pass("Deterministic replay");
}

static void targetingDeterminismTest(const ContentManager& content, ValidationReport& report)
{
    std::vector<Unit> units;
    units.push_back(content.createUnit(pickChampionByIndex(content, 0), Position{ 4, 7 }, TeamId::TeamA));
    units.push_back(content.createUnit(pickChampionByIndex(content, 1), Position{ 3, 2 }, TeamId::TeamB));
    units.push_back(content.createUnit(pickChampionByIndex(content, 2), Position{ 5, 2 }, TeamId::TeamB));

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    GameState state(std::move(board), std::move(units), std::move(logger), content);
    state.setDtMs(ValidationConstants::DefaultDtMs);

    RandomManager::global().setSeed(ValidationConstants::TargetingDeterminismSeed);
    Unit& attacker = state.units()[0];
    Unit* t0 = TargetingSystem::findNearestEnemy(attacker, state.units());
    RandomManager::global().setSeed(ValidationConstants::TargetingDeterminismSeed);
    Unit* t1 = TargetingSystem::findNearestEnemy(attacker, state.units());

    if (t0 && t1 && t0->getName() == t1->getName())
    {
        report.pass("Target selection determinism");
    }
    else
    {
        report.fail("Target selection determinism");
    }
}

struct BenchResult
{
    long long totalMs = 0;
    std::int64_t ticks = 0;
    std::int64_t scheduled = 0;
    std::int64_t executed = 0;
    std::size_t maxQueue = 0;
};

static BenchResult runCombatBenchmark(const ContentManager& content,
                                      std::uint32_t seed,
                                      const std::vector<Unit>& teamA,
                                      const std::vector<Unit>& teamB,
                                      std::int32_t dtMs)
{
    RandomManager::global().setSeed(seed);
    DamageSystem::setSeed(seed);

    Logger logger(std::cout);
    logger.setMode(LogMode::Silent);

    Board board(10, 10);
    std::vector<Unit> all;
    all.reserve(teamA.size() + teamB.size());
    for (const Unit& u : teamA) all.push_back(u);
    for (const Unit& u : teamB) all.push_back(u);

    GameState state(std::move(board), std::move(all), std::move(logger), content);
    state.setDtMs(dtMs);

    CombatValidation::setEnabled(false);
    CombatValidation::setDetailedLogs(false);

    const auto t0 = std::chrono::high_resolution_clock::now();
    Combat combat;
    combat.run(state);
    const auto t1 = std::chrono::high_resolution_clock::now();

    BenchResult r{};
    r.totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    r.ticks = state.tickCount();
    r.scheduled = state.scheduledEventCount();
    r.executed = state.executedEventCount();
    r.maxQueue = state.maxScheduledQueueSize();
    return r;
}

static void benchmarkTest(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    struct Case { int n; const char* label; };
    static constexpr int CaseSmall = 1;
    static constexpr int CaseMedium = 4;
    static constexpr int CaseLarge = 8;
    const Case cases[] = { { CaseSmall, "1v1" }, { CaseMedium, "4v4" }, { CaseLarge, "8v8" } };

    const std::string aName = pickChampionByIndex(content, 0);
    const std::string bName = pickChampionByIndex(content, 1);

    bool ok = true;
    for (const Case& c : cases)
    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        for (int i = 0; i < c.n; ++i)
        {
            a.push_back(content.createUnit(aName, Position{ 4 + (i % 3), 7 + (i / 3) }, TeamId::TeamA));
            b.push_back(content.createUnit(bName, Position{ 4 + (i % 3), 2 - (i / 3) }, TeamId::TeamB));
        }

        static constexpr std::uint32_t BenchmarkSeedBase = 101u;
        static constexpr double MicrosecondsPerMillisecond = 1000.0;
        const BenchResult r =
            runCombatBenchmark(content, BenchmarkSeedBase + static_cast<std::uint32_t>(c.n), a, b, ValidationConstants::DefaultDtMs);
        const double avgUs =
            r.ticks > 0 ? (static_cast<double>(r.totalMs) * MicrosecondsPerMillisecond) / static_cast<double>(r.ticks) : 0.0;

        out << "Benchmark " << c.label
            << " | total " << r.totalMs << "ms"
            << " | ticks " << r.ticks
            << " | avg tick " << std::fixed << std::setprecision(2) << avgUs << "us"
            << " | scheduled " << r.scheduled
            << " | executed " << r.executed
            << " | maxQueue " << r.maxQueue
            << "\n";

        if (r.ticks <= 0)
        {
            ok = false;
        }
    }

    if (ok)
    {
        report.pass("Benchmark");
    }
    else
    {
        report.warning("Benchmark");
    }
}

static void attackSpeedTimerTest(ValidationReport& report, std::ostream& out)
{
    struct Case { float as; };
    const Case cases[] = { { 0.80f }, { 1.00f }, { 1.25f }, { 2.00f } };
    const std::int32_t dt = ValidationConstants::DefaultDtMs;
    bool ok = true;

    std::int32_t lastInterval = std::numeric_limits<std::int32_t>::max();
    for (const Case& c : cases)
    {
        Unit u("ASDummy",
               ValidationConstants::DummyHp,
               0,
               0,
               0,
               ValidationConstants::DummyAttackCooldownMs,
               ValidationConstants::DummyRange,
               DamageType::Physical,
               Position{ 0, 0 },
               TeamId::TeamA);
        u.setAttackSpeed(c.as);

        const std::int32_t interval =
            c.as > 0.0f
                ? std::max<std::int32_t>(
                      1,
                      static_cast<std::int32_t>(std::lround(static_cast<float>(CombatConstants::MsPerSecond) / c.as)))
                : 0;

        out << "AttackSpeed: " << std::fixed << std::setprecision(2) << c.as << "\n";
        out << "AttackCooldown: " << interval << "ms\n";

        if (interval >= lastInterval)
        {
            ok = false;
        }
        lastInterval = interval;

        for (int i = 0; i < ValidationConstants::AttackSpeedInnerIters; ++i)
        {
            std::int32_t elapsed = 0;
            int guard = 0;
            while (!u.canAttack() && guard < ValidationConstants::AttackSpeedAttackReadyGuard)
            {
                u.tick(dt);
                elapsed += dt;
                guard += 1;
                if (elapsed > ValidationConstants::AttackSpeedMaxElapsedMs)
                {
                    ok = false;
                    break;
                }
            }
            if (guard >= ValidationConstants::AttackSpeedAttackReadyGuard)
            {
                report.fail("Attack speed timing hung: canAttack never became true");
                return;
            }
            if (elapsed < interval)
            {
                ok = false;
            }
            if (elapsed > interval + dt)
            {
                ok = false;
            }
            u.resetAttackTimer();
        }
    }

    if (ok)
    {
        report.pass("Attack speed timing");
    }
    else
    {
        report.fail("Attack speed timing");
    }
}

static void manaSystemTest(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    AbilityEffect effect{};
    effect.name = "ManaTestSpell";
    effect.trigger = AbilityTrigger::OnCast;
    effect.areaShape = AreaShape::SingleTarget;
    effect.radius = 0;

    Ability ability{};
    ability.name = "ManaTestAbility";
    ability.manaCost = ValidationConstants::ManaTestDummyManaCost;
    ability.targetType = TargetType::CurrentEnemy;
    ability.effects.push_back(effect);

    Unit a("ManaTestCaster",
           ValidationConstants::DummyHp,
           0,
           0,
           0,
           ValidationConstants::DummyAttackCooldownMs,
           ValidationConstants::DummyRange,
           DamageType::Physical,
           Position{ 4, 7 },
           TeamId::TeamA,
           ValidationConstants::ManaTestDummyMaxMana,
           0,
           ability);

    Unit b("ManaTestTarget",
           ValidationConstants::DummyHp,
           0,
           0,
           0,
           ValidationConstants::DummyAttackCooldownMs,
           ValidationConstants::DummyRange,
           DamageType::Physical,
           Position{ 4, 2 },
           TeamId::TeamB);

    a.gainMana(ValidationConstants::VeryLargeValue);
    const bool clamped = a.getMana() == a.getMaxMana();
    if (!clamped)
    {
        report.fail("Mana overflow clamp");
        return;
    }
    report.pass("Mana overflow clamp");

    RandomManager::global().setSeed(ValidationConstants::ManaTestSeed);
    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Normal);
    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> all;
    all.push_back(a);
    all.push_back(b);
    GameState state(std::move(board), std::move(all), std::move(logger), content);

    CombatValidation::setEnabled(true);
    CombatValidation::setDetailedLogs(true);

    Unit& caster = state.units()[0];
    Unit& target = state.units()[1];

    caster.gainMana(caster.getMaxMana());
    const bool couldCastBefore = caster.canCastAbility();
    const bool began = SpellResolver::beginCast(state, caster, target);
    if (!began)
    {
        report.fail("Mana lock during cast");
        report.fail("Cast mana reset");
        return;
    }

    const std::int32_t manaAtBegin = caster.getMana();
    caster.gainMana(ValidationConstants::ManaLockTestGainAttempt);
    const std::int32_t manaAfterGainAttempt = caster.getMana();

    ManaSystem::applyDamageTakenMana(state, caster, ValidationConstants::ManaLockTestDamageTaken);
    const std::int32_t manaAfterDamageTakenAttempt = caster.getMana();

    const bool locked =
        caster.isCasting() &&
        manaAfterGainAttempt == manaAtBegin &&
        manaAfterDamageTakenAttempt == manaAtBegin;

    if (locked)
    {
        report.pass("Mana lock during cast");
    }
    else
    {
        report.fail("Mana lock during cast");
    }

    const std::int32_t dt = state.dtMs();
    int guard = 0;
    bool sawReleaseReset = false;
    while (guard < ValidationConstants::ManaLifecycleLoopMaxTicks)
    {
        state.advanceTick();
        state.processCombatEvents();
        for (Unit& u : state.units())
        {
            u.updateStatusEffects(dt);
            u.tick(dt);
        }

        if (state.timeMs() >= ValidationConstants::ManaResetReleaseMs && caster.getMana() == 0)
        {
            sawReleaseReset = true;
            break;
        }
        guard += 1;
    }

    if (!couldCastBefore)
    {
        report.fail("Cast mana reset");
        return;
    }

    if (!sawReleaseReset)
    {
        report.fail("Cast mana reset");
        return;
    }
    report.pass("Cast mana reset");

    const std::int32_t manaAfterRelease = caster.getMana();
    caster.gainMana(ValidationConstants::ManaLockTestGainAfterRelease);
    const bool stillLockedAfterRelease = caster.isManaLocked() && caster.getMana() == manaAfterRelease;

    int guardEnd = 0;
    while (caster.isCasting() && guardEnd < ValidationConstants::ManaLifecycleLoopMaxTicks)
    {
        state.advanceTick();
        state.processCombatEvents();
        for (Unit& u : state.units())
        {
            u.updateStatusEffects(dt);
            u.tick(dt);
        }
        guardEnd += 1;
    }

    if (guardEnd >= ValidationConstants::ManaLifecycleLoopMaxTicks)
    {
        report.fail("Cast recovery hung");
        return;
    }

    const std::int32_t manaBeforeUnlockGain = caster.getMana();
    caster.gainMana(ValidationConstants::ManaLockTestGainAfterUnlock);
    const bool canGainAfter = caster.getMana() > manaBeforeUnlockGain;

    if (stillLockedAfterRelease && canGainAfter)
    {
        report.pass("Mana lock lifecycle");
    }
    else
    {
        report.fail("Mana lock lifecycle");
    }
}

static void projectileTimingTest(const ContentManager& content, ValidationReport& report)
{
    RandomManager::global().setSeed(ValidationConstants::ReplaySeed);
    DamageSystem::setSeed(ValidationConstants::ReplaySeed);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Normal);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.push_back(content.createUnit(pickChampionByIndex(content, 0), Position{ 4, 7 }, TeamId::TeamA));
    units.push_back(content.createUnit(pickChampionByIndex(content, 1), Position{ 4, 2 }, TeamId::TeamB));

    const int attackerRange = units[0].getAttackRange();
    units[0].addStatusEffect(makePercentBuff("High AS",
                                             StatusEffectType::BonusAttackSpeed,
                                             StatType::AttackSpeed,
                                             ValidationConstants::BuffAttackSpeedHigh,
                                             ValidationConstants::LongDurationMs));

    GameState state(std::move(board), std::move(units), std::move(logger), content);
    state.setDtMs(ValidationConstants::ProjectileTimingDtMs);

    CombatValidation::setEnabled(true);
    CombatValidation::setDetailedLogs(true);

    Combat combat;
    for (int i = 0; i < ValidationConstants::ProjectileTimingMaxTicks; ++i)
    {
        if (!state.hasAlive(TeamId::TeamA) || !state.hasAlive(TeamId::TeamB))
        {
            break;
        }
        combat.run(state);
        break;
    }

    const std::string s = oss.str();
    const std::size_t aPos = s.find("ATTACK_START ");
    const std::size_t pPos = s.find("PROJECTILE_HIT ");

    if (aPos == std::string::npos || pPos == std::string::npos || pPos <= aPos)
    {
        report.warning("Projectile timing");
        return;
    }

    auto parseTimeMsAt = [&](std::size_t pos) -> std::int32_t
    {
        const std::size_t start = pos;
        const std::size_t msPos = s.find("ms", start);
        if (msPos == std::string::npos)
        {
            return -1;
        }
        std::size_t numStart = start;
        while (numStart < msPos && !std::isdigit(static_cast<unsigned char>(s[numStart])))
        {
            numStart += 1;
        }
        std::string num;
        for (std::size_t i = numStart; i < msPos; ++i)
        {
            if (std::isdigit(static_cast<unsigned char>(s[i])))
            {
                num.push_back(s[i]);
            }
            else
            {
                break;
            }
        }
        if (num.empty())
        {
            return -1;
        }
        return static_cast<std::int32_t>(std::stoi(num));
    };

    const std::int32_t attackStart = parseTimeMsAt(aPos);
    const std::int32_t projHit = parseTimeMsAt(pPos);

    if (attackStart < 0 || projHit < 0)
    {
        report.warning("Projectile timing");
        return;
    }

    const std::int32_t windup =
        attackerRange <= ValidationConstants::DummyRange ? CombatConstants::AttackWindupMeleeMs : CombatConstants::AttackWindupRangedMs;
    const bool ok = (projHit - attackStart) >= windup;

    if (ok) report.pass("Projectile timing");
    else report.fail("Projectile timing");
}

static void castTimingTest(const ContentManager& content, ValidationReport& report)
{
    RandomManager::global().setSeed(ValidationConstants::CastTimingSeed);
    DamageSystem::setSeed(ValidationConstants::CastTimingSeed);

    std::ostringstream oss;
    Logger logger(oss);
    logger.setMode(LogMode::Normal);

    AbilityEffect effect{};
    effect.name = "CastTimingSpell";
    effect.trigger = AbilityTrigger::OnCast;
    effect.areaShape = AreaShape::SingleTarget;
    effect.radius = 0;

    Ability ability{};
    ability.name = "CastTimingAbility";
    ability.manaCost = ValidationConstants::ManaTestDummyManaCost;
    ability.targetType = TargetType::CurrentEnemy;
    ability.effects.push_back(effect);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> units;
    units.emplace_back("CastTimingCaster",
                       ValidationConstants::DummyHp,
                       0,
                       0,
                       0,
                       ValidationConstants::DummyAttackCooldownMs,
                       ValidationConstants::DummyRange,
                       DamageType::Physical,
                       Position{ 4, 7 },
                       TeamId::TeamA,
                       ValidationConstants::ManaTestDummyMaxMana,
                       0,
                       ability);
    units.emplace_back("CastTimingTarget",
                       ValidationConstants::DummyHp,
                       0,
                       0,
                       0,
                       ValidationConstants::DummyAttackCooldownMs,
                       ValidationConstants::DummyRange,
                       DamageType::Physical,
                       Position{ 4, 2 },
                       TeamId::TeamB);

    GameState state(std::move(board), std::move(units), std::move(logger), content);
    state.setDtMs(ValidationConstants::DefaultDtMs);

    CombatValidation::setEnabled(true);
    CombatValidation::setDetailedLogs(true);

    Unit& caster = state.units()[0];
    Unit& target = state.units()[1];

    caster.gainMana(caster.getMaxMana());
    if (!caster.canCastAbility())
    {
        report.fail("Cast timing");
        return;
    }

    const bool started = SpellResolver::beginCast(state, caster, target);
    if (!started || !caster.isCasting() || caster.getMana() != caster.getMaxMana())
    {
        report.fail("Cast timing");
        return;
    }

    bool sawRelease = false;
    bool sawEnd = false;
    for (int i = 0; i < ValidationConstants::CastTimingTicksToProcess; ++i)
    {
        state.advanceTick();
        state.processCombatEvents();

        if (state.timeMs() == CombatConstants::SpellWindupMs)
        {
            sawRelease = caster.isCasting() && caster.isManaLocked() && caster.getMana() == 0;
        }
        if (state.timeMs() == (CombatConstants::SpellWindupMs + CombatConstants::SpellRecoveryMs))
        {
            sawEnd = !caster.isCasting() && !caster.isManaLocked();
        }

        for (Unit& u : state.units())
        {
            u.tick(state.dtMs());
        }
    }

    const bool ok = sawRelease && sawEnd && !caster.isCasting() && !caster.isManaLocked() && caster.getMana() == 0;
    if (ok)
    {
        report.pass("Cast timing");
    }
    else
    {
        report.fail("Cast timing");
    }
}

static bool runScenario(const ContentManager& content,
                        std::string_view name,
                        std::uint32_t seed,
                        std::vector<Unit> teamA,
                        std::vector<Unit> teamB,
                        std::ostream& out)
{
    RandomManager::global().setSeed(seed);
    DamageSystem::setSeed(seed);

    Logger logger(out);
    logger.setMode(LogMode::Silent);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    std::vector<Unit> all;
    all.reserve(teamA.size() + teamB.size());
    for (Unit& u : teamA) all.push_back(std::move(u));
    for (Unit& u : teamB) all.push_back(std::move(u));

    GameState state(std::move(board), std::move(all), std::move(logger), content);
    state.setDtMs(ValidationConstants::DefaultDtMs);

    CombatValidation::setEnabled(false);
    CombatValidation::setDetailedLogs(false);

    Combat combat;
    combat.run(state);

    const bool aAlive = state.hasAlive(TeamId::TeamA);
    const bool bAlive = state.hasAlive(TeamId::TeamB);
    const int winner = aAlive && !bAlive ? 1 : bAlive && !aAlive ? 2 : 0;

    out << "Scenario: " << name
        << " | winner " << (winner == 1 ? "TeamA" : winner == 2 ? "TeamB" : "None")
        << " | ticks " << state.tickCount()
        << " | scheduled " << state.scheduledEventCount()
        << " | executed " << state.executedEventCount()
        << "\n";

    return winner != 0;
}

static void scenarioSuite(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    bool ok = true;

    static constexpr std::uint32_t HighAttackSpeedScenarioSeed = 1001u;
    static constexpr std::uint32_t CritCarryScenarioSeed = 1002u;
    static constexpr std::uint32_t TankVsBurstScenarioSeed = 1003u;
    static constexpr std::uint32_t DelayedAoeScenarioSeed = 1004u;
    static constexpr std::uint32_t PassiveTriggerScenarioSeed = 1005u;
    static constexpr std::uint32_t DurabilityStackingScenarioSeed = 1006u;
    static constexpr std::uint32_t MultiTargetAoeScenarioSeed = 1007u;
    static constexpr std::uint32_t FiveVFiveScenarioSeed = 2001u;

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 0), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 1), Position{ 4, 2 }, TeamId::TeamB));
        a[0].addStatusEffect(makePercentBuff("High AS",
                                             StatusEffectType::BonusAttackSpeed,
                                             StatType::AttackSpeed,
                                             ValidationConstants::BuffAttackSpeedHigh2,
                                             ValidationConstants::LongDurationMs));
        ok = runScenario(content, "high attack speed carry", HighAttackSpeedScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 2), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 3), Position{ 4, 2 }, TeamId::TeamB));
        a[0].addStatusEffect(makeFlatBuff("Crit+",
                                          StatusEffectType::CritChanceBonus,
                                          StatType::CritChance,
                                          ValidationConstants::BuffCritChanceHigh,
                                          ValidationConstants::LongDurationMs));
        a[0].addStatusEffect(makeFlatBuff("CritDmg+",
                                          StatusEffectType::CritDamageBonus,
                                          StatType::CritDamage,
                                          ValidationConstants::BuffCritDamageHigh,
                                          ValidationConstants::LongDurationMs));
        ok = runScenario(content, "crit carry", CritCarryScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 4), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 5), Position{ 4, 2 }, TeamId::TeamB));
        static constexpr std::int32_t TankVsBurstHp = 3500;
        a[0].setMaxHp(TankVsBurstHp);
        a[0].setHp(TankVsBurstHp);
        ok = runScenario(content, "tank vs burst mage", TankVsBurstScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 6), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 7), Position{ 4, 2 }, TeamId::TeamB));
        b.push_back(content.createUnit(pickChampionByIndex(content, 8), Position{ 5, 2 }, TeamId::TeamB));
        ok = runScenario(content, "delayed AoE caster", DelayedAoeScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 9), Position{ 4, 7 }, TeamId::TeamA));
        a.push_back(content.createUnit(pickChampionByIndex(content, 10), Position{ 5, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 11), Position{ 4, 2 }, TeamId::TeamB));
        b.push_back(content.createUnit(pickChampionByIndex(content, 12), Position{ 5, 2 }, TeamId::TeamB));
        ok = runScenario(content, "passive trigger spam", PassiveTriggerScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 13), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 14), Position{ 4, 2 }, TeamId::TeamB));
        a[0].addStatusEffect(makeFlatBuff("Durability",
                                          StatusEffectType::DamageReduction,
                                          StatType::DamageReduction,
                                          ValidationConstants::BuffDamageReduction,
                                          ValidationConstants::LongDurationMs));
        ok = runScenario(content, "durability stacking", DurabilityStackingScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        std::vector<Unit> a;
        std::vector<Unit> b;
        a.push_back(content.createUnit(pickChampionByIndex(content, 15), Position{ 4, 7 }, TeamId::TeamA));
        b.push_back(content.createUnit(pickChampionByIndex(content, 16), Position{ 4, 2 }, TeamId::TeamB));
        b.push_back(content.createUnit(pickChampionByIndex(content, 17), Position{ 5, 2 }, TeamId::TeamB));
        b.push_back(content.createUnit(pickChampionByIndex(content, 18), Position{ 6, 2 }, TeamId::TeamB));
        ok = runScenario(content, "multi-target AoE", MultiTargetAoeScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    {
        const std::vector<Position> aPos = {
            Position{ 3, 7 }, Position{ 4, 7 }, Position{ 5, 7 }, Position{ 6, 7 }, Position{ 7, 7 }
        };
        const std::vector<Position> bPos = {
            Position{ 3, 2 }, Position{ 4, 2 }, Position{ 5, 2 }, Position{ 6, 2 }, Position{ 7, 2 }
        };
        std::vector<Unit> a = makeDynamicTeam(content, TeamId::TeamA, 5, 0, aPos);
        std::vector<Unit> b = makeDynamicTeam(content, TeamId::TeamB, 5, 5, bPos);
        ok = runScenario(content, "5v5", FiveVFiveScenarioSeed, std::move(a), std::move(b), out) && ok;
    }

    if (ok)
    {
        report.pass("Validation scenarios");
    }
    else
    {
        report.warning("Validation scenarios");
    }
}

static void macroSystemValidationTest(const ContentManager& content, ValidationReport& report, std::ostream& out)
{
    Random rng(123u);
    SharedUnitPool pool(content);
    ShopSystem shop(content, pool);

    {
        bool ok = true;
        std::string firstMismatch;
        for (const auto& [name, champ] : content.champions())
        {
            const int avail = pool.availableCount(name);
            if (isPlayableChampion(champ))
            {
                if (avail <= 0)
                {
                    ok = false;
                    if (firstMismatch.empty()) firstMismatch = name;
                }
            }
            else
            {
                if (avail != 0)
                {
                    ok = false;
                    if (firstMismatch.empty()) firstMismatch = name;
                }
            }
        }
        if (ok)
        {
            report.pass("Macro: Playable champion filtering");
        }
        else
        {
            out << "Playable filtering mismatch example: " << firstMismatch << "\n";
            report.fail("Macro: Playable champion filtering");
        }
    }

    PlayerState p("MacroTest");
    p.addGold(100);
    shop.reroll(p, rng, false);

    bool shopOk = true;
    bool shopPlayableOk = true;
    for (const ShopOffer& o : p.shop())
    {
        if (o.championName.empty())
        {
            continue;
        }
        const ChampionDefinition* def = content.getChampion(o.championName);
        if (!def)
        {
            shopOk = false;
            break;
        }
        if (pool.availableCount(o.championName) <= 0)
        {
            shopOk = false;
            break;
        }
        if (o.cost != def->cost)
        {
            shopOk = false;
            break;
        }
        if (!isPlayableChampion(*def))
        {
            shopPlayableOk = false;
        }
    }
    if (shopOk) report.pass("Macro: Shop offers valid champions");
    else report.fail("Macro: Shop offers valid champions");
    if (shopPlayableOk) report.pass("Macro: Shop offers playable champions");
    else report.fail("Macro: Shop offers playable champions");

    int firstOffer = -1;
    for (std::size_t i = 0; i < p.shop().size(); ++i)
    {
        if (!p.shop()[i].championName.empty())
        {
            firstOffer = static_cast<int>(i);
            break;
        }
    }

    if (firstOffer >= 0)
    {
        const std::string champ = p.shop()[static_cast<std::size_t>(firstOffer)].championName;
        const int cost = p.shop()[static_cast<std::size_t>(firstOffer)].cost;
        const int goldBefore = static_cast<int>(p.gold());
        const int poolBefore = pool.availableCount(champ);

        MacroAction buy;
        buy.type = MacroActionType::BuyUnit;
        buy.shopIndex = firstOffer;
        buy.goldCost = cost;
        buy.debugName = "BuyUnit";
        const bool okBuy = MacroExecutor::apply(buy, p, shop, rng, out);

        const int goldAfter = static_cast<int>(p.gold());
        const int poolAfter = pool.availableCount(champ);

        if (okBuy && goldAfter == goldBefore - cost) report.pass("Macro: Buying reduces gold");
        else report.fail("Macro: Buying reduces gold");

        if (okBuy && poolAfter == poolBefore - 1) report.pass("Macro: Buying removes from pool");
        else report.fail("Macro: Buying removes from pool");
    }
    else
    {
        report.fail("Macro: Buying reduces gold");
        report.fail("Macro: Buying removes from pool");
    }

    {
        const int goldBefore = static_cast<int>(p.gold());
        MacroAction r;
        r.type = MacroActionType::RerollShop;
        r.goldCost = ShopSystem::rerollCost();
        r.debugName = "RerollShop";
        const bool ok = MacroExecutor::apply(r, p, shop, rng, out);
        const int goldAfter = static_cast<int>(p.gold());
        if (ok && goldAfter == goldBefore - ShopSystem::rerollCost()) report.pass("Macro: Reroll costs 2 gold");
        else report.fail("Macro: Reroll costs 2 gold");
    }

    {
        const int goldBefore = static_cast<int>(p.gold());
        MacroAction xp;
        xp.type = MacroActionType::BuyXp;
        xp.goldCost = ShopSystem::xpForBuy();
        xp.debugName = "BuyXp";
        const bool ok = MacroExecutor::apply(xp, p, shop, rng, out);
        const int goldAfter = static_cast<int>(p.gold());
        if (ok && goldAfter == goldBefore - ShopSystem::xpForBuy()) report.pass("Macro: Buy XP costs 4 gold");
        else report.fail("Macro: Buy XP costs 4 gold");
    }

    {
        bool ok = false;
        if (!p.bench().empty())
        {
            const OwnedUnit sold = p.bench()[0];
            const int before = pool.availableCount(sold.championName);
            ok = shop.sellBench(p, 0);
            const int after = pool.availableCount(sold.championName);
            const int expectedDelta = sold.starLevel <= 1 ? 1 : sold.starLevel == 2 ? 3 : 9;
            ok = ok && (after == before + expectedDelta);
        }
        if (ok) report.pass("Macro: Selling returns to pool");
        else report.fail("Macro: Selling returns to pool");
    }

    {
        PlayerState up("UpgradeTest");
        const std::string champ = pickChampionByIndex(content, 0);
        const ChampionDefinition* def = content.getChampion(champ);
        const int baseCost = def ? def->cost : 1;

        for (int i = 0; i < 3; ++i)
        {
            OwnedUnit u;
            u.championName = champ;
            u.starLevel = 1;
            u.cost = baseCost;
            up.addToBench(u);
        }
        const bool ok =
            up.bench().size() == 1 &&
            up.bench()[0].championName == champ &&
            up.bench()[0].starLevel == 2;

        if (ok) report.pass("Macro: 3 copies upgrade to 2-star");
        else report.fail("Macro: 3 copies upgrade to 2-star");
    }

    {
        PlayerState up("UpgradeTest3");
        const std::string champ = pickChampionByIndex(content, 0);
        const ChampionDefinition* def = content.getChampion(champ);
        const int baseCost = def ? def->cost : 1;

        for (int i = 0; i < 9; ++i)
        {
            OwnedUnit u;
            u.championName = champ;
            u.starLevel = 1;
            u.cost = baseCost;
            up.addToBench(u);
        }
        const bool ok =
            up.bench().size() == 1 &&
            up.bench()[0].championName == champ &&
            up.bench()[0].starLevel == 3;

        if (ok) report.pass("Macro: 3 two-stars upgrade to 3-star");
        else report.fail("Macro: 3 two-stars upgrade to 3-star");
    }

    {
        PlayerState dead("DeadTest");
        dead.takeDamage(ValidationConstants::VeryLargeValue);
        if (dead.health() == 0) report.pass("Macro: Player dies at 0 HP");
        else report.fail("Macro: Player dies at 0 HP");
    }

    {
        PlayerState ppos("RepositionIllegal");
        const std::string champ = pickChampionByIndex(content, 0);
        const ChampionDefinition* def = content.getChampion(champ);
        OwnedUnit u{};
        u.championName = champ;
        u.starLevel = 1;
        u.cost = def ? def->cost : 1;
        u.hasFormation = true;
        u.formation = Position{ 0, 0 };
        ppos.addToBench(u);
        ppos.moveBenchToBoard(0);

        MacroAction a{};
        a.type = MacroActionType::RepositionUnit;
        a.boardIndex = 0;
        a.targetPosition = Position{ 0, 0 };
        a.debugName = "Reposition";

        const bool ok = MacroExecutor::apply(a, ppos, shop, rng, out);
        if (!ok) report.pass("Macro: Reposition to same position is illegal");
        else report.fail("Macro: Reposition to same position is illegal");
    }

    {
        PlayerState pturn("RepositionTurn");
        const std::string champ = pickChampionByIndex(content, 0);
        const ChampionDefinition* def = content.getChampion(champ);
        OwnedUnit u{};
        u.championName = champ;
        u.starLevel = 1;
        u.cost = def ? def->cost : 1;
        u.hasFormation = true;
        u.formation = Position{ 0, 0 };
        pturn.addToBench(u);
        pturn.moveBenchToBoard(0);

        SimpleMacroAI ai(777u);
        Random rngTurn(777u);
        MacroTurnStats stats{};

        MacroSimulation::takeTurnForValidation(pturn, ai, shop, rngTurn, content, nullptr, &pool, out, stats);

        if (stats.repositionActionsExecuted <= MacroConstants::MaxRepositionActionsPerTurn)
        {
            report.pass("Macro: Reposition per-turn cap");
        }
        else
        {
            report.fail("Macro: Reposition per-turn cap");
        }

        std::unordered_set<std::string> seen;
        bool dup = false;
        for (const std::string& k : stats.executedActionKeys)
        {
            if (k.rfind("POS|", 0) != 0)
            {
                continue;
            }
            if (!seen.insert(k).second)
            {
                dup = true;
                break;
            }
        }
        if (!dup) report.pass("Macro: Repeated identical Reposition blocked");
        else report.fail("Macro: Repeated identical Reposition blocked");
    }

    {
        PlayerState churn("MacroChurnTurn");
        churn.addGold(90);
        shop.reroll(churn, rng, false);

        const int benchTarget = static_cast<int>(std::max<std::size_t>(1, churn.benchLimit() - 1));
        for (int i = 0; i < benchTarget; ++i)
        {
            const std::string champ = pickChampionByIndex(content, static_cast<std::size_t>(i));
            const ChampionDefinition* def = content.getChampion(champ);
            OwnedUnit u{};
            u.championName = champ;
            u.starLevel = 1;
            u.cost = def ? def->cost : 1;
            churn.addToBench(u);
        }
        churn.moveBenchToBoard(0);

        SimpleMacroAI ai(4242u);
        Random rngTurn(4242u);
        MacroTurnStats stats{};
        MacroSimulation::takeTurnForValidation(churn, ai, shop, rngTurn, content, nullptr, &pool, out, stats);

        int transactions = 0;
        std::unordered_set<std::string> buyNames;
        std::unordered_set<std::string> sellNames;
        std::unordered_set<std::string> movedToBoardNames;
        std::unordered_set<std::string> movedToBenchNames;
        std::unordered_set<std::string> soldBoardNames;
        std::unordered_set<std::string> soldBenchNames;

        auto nameAfterPrefix = [](const std::string& k, const char* prefix) -> std::string
        {
            const std::size_t pos = k.find(prefix);
            if (pos == std::string::npos)
            {
                return {};
            }
            std::string tail = k.substr(pos + std::strlen(prefix));
            const std::size_t bar = tail.find('|');
            if (bar == std::string::npos)
            {
                return tail;
            }
            return tail.substr(0, bar);
        };

        for (const std::string& k : stats.executedActionKeys)
        {
            if (k.find("|BUY|") != std::string::npos || k.find("|SELL") != std::string::npos ||
                k.find("|B2F|") != std::string::npos || k.find("|F2B|") != std::string::npos)
            {
                transactions += 1;
            }
            const std::string buy = nameAfterPrefix(k, "BUY|");
            if (!buy.empty()) buyNames.insert(buy);

            const std::string sb = nameAfterPrefix(k, "SELLB|");
            if (!sb.empty()) { sellNames.insert(sb); soldBenchNames.insert(sb); }
            const std::string sf = nameAfterPrefix(k, "SELLF|");
            if (!sf.empty()) { sellNames.insert(sf); soldBoardNames.insert(sf); }

            const std::string b2f = nameAfterPrefix(k, "B2F|");
            if (!b2f.empty()) movedToBoardNames.insert(b2f);
            const std::string f2b = nameAfterPrefix(k, "F2B|");
            if (!f2b.empty()) movedToBenchNames.insert(f2b);
        }

        if (transactions <= MacroConstants::MaxTransactionsPerTurn) report.pass("Macro: MaxTransactionsPerTurn");
        else report.fail("Macro: MaxTransactionsPerTurn");

        bool buySellSame = false;
        for (const std::string& n : buyNames)
        {
            if (sellNames.find(n) != sellNames.end())
            {
                buySellSame = true;
                break;
            }
        }
        if (!buySellSame) report.pass("Macro: no buy-sell same unit same turn");
        else report.fail("Macro: no buy-sell same unit same turn");

        bool deploySellLoop = false;
        for (const std::string& n : movedToBoardNames)
        {
            if (soldBoardNames.find(n) != soldBoardNames.end())
            {
                deploySellLoop = true;
                break;
            }
        }
        for (const std::string& n : movedToBenchNames)
        {
            if (soldBenchNames.find(n) != soldBenchNames.end())
            {
                deploySellLoop = true;
                break;
            }
        }
        if (!deploySellLoop) report.pass("Macro: no deploy-sell loop");
        else report.fail("Macro: no deploy-sell loop");
    }

    {
        bool ok = true;
        for (int t = 0; t < 10; ++t)
        {
            PlayerState p("MacroStress");
            p.addGold(50);
            shop.reroll(p, rng, false);
            p.moveBenchToBoard(0);

            SimpleMacroAI ai(static_cast<std::uint32_t>(9000 + t));
            Random rngTurn(static_cast<std::uint32_t>(9000 + t));
            MacroTurnStats stats{};
            MacroSimulation::takeTurnForValidation(p, ai, shop, rngTurn, content, nullptr, &pool, out, stats);

            int tx = 0;
            int sells = 0;
            int boardSells = 0;
            for (const std::string& k : stats.executedActionKeys)
            {
                if (k.find("|BUY|") != std::string::npos || k.find("|SELL") != std::string::npos ||
                    k.find("|B2F|") != std::string::npos || k.find("|F2B|") != std::string::npos)
                {
                    tx += 1;
                }
                if (k.find("|SELLB|") != std::string::npos || k.find("|SELLF|") != std::string::npos)
                {
                    sells += 1;
                }
                if (k.find("|SELLF|") != std::string::npos)
                {
                    boardSells += 1;
                }
            }

            ok = ok &&
                 tx <= MacroConstants::MaxTransactionsPerTurn &&
                 sells <= MacroConstants::MaxSellsPerTurn &&
                 boardSells <= MacroConstants::MaxBoardSellsPerTurn &&
                 stats.executedActionKeys.size() <= static_cast<std::size_t>(12);
        }

        if (ok) report.pass("Macro: stress turn action churn bounded");
        else report.fail("Macro: stress turn action churn bounded");
    }

    {
        PlayerState me("RolloutOpponentEvalMe");
        const std::string myChamp = pickChampionByIndex(content, 0);
        const ChampionDefinition* myDef = content.getChampion(myChamp);
        OwnedUnit my{};
        my.championName = myChamp;
        my.starLevel = 1;
        my.cost = myDef ? myDef->cost : 1;
        my.hasFormation = true;
        my.formation = Position{ 0, 0 };
        me.boardMutable().push_back(my);

        PlayerState weakOpp("WeakOpp");
        const std::string weakChamp = pickChampionByIndex(content, 1);
        const ChampionDefinition* weakDef = content.getChampion(weakChamp);
        OwnedUnit wu{};
        wu.championName = weakChamp;
        wu.starLevel = 1;
        wu.cost = weakDef ? weakDef->cost : 1;
        wu.hasFormation = true;
        wu.formation = Position{ 0, 0 };
        weakOpp.boardMutable().push_back(wu);

        PlayerState strongOpp("StrongOpp");
        const std::string strongChampA = pickChampionByIndex(content, 2);
        const std::string strongChampB = pickChampionByIndex(content, 3);
        const ChampionDefinition* sDefA = content.getChampion(strongChampA);
        const ChampionDefinition* sDefB = content.getChampion(strongChampB);
        OwnedUnit sa{};
        sa.championName = strongChampA;
        sa.starLevel = 2;
        sa.cost = sDefA ? sDefA->cost : 1;
        sa.hasFormation = true;
        sa.formation = Position{ 0, 0 };
        OwnedUnit sb{};
        sb.championName = strongChampB;
        sb.starLevel = 1;
        sb.cost = sDefB ? sDefB->cost : 1;
        sb.hasFormation = true;
        sb.formation = Position{ 1, 0 };
        strongOpp.boardMutable().push_back(sa);
        strongOpp.boardMutable().push_back(sb);

        const FutureEval weakEval = FutureStateEvaluator::evaluate(me, content, nullptr, &pool, 3, 10, &weakOpp);
        const FutureEval strongEval = FutureStateEvaluator::evaluate(me, content, nullptr, &pool, 3, 10, &strongOpp);

        if (strongEval.ev < weakEval.ev) report.pass("Rollout: stronger opponent lowers EV");
        else report.fail("Rollout: stronger opponent lowers EV");
    }

    {
        struct NullBuffer final : std::streambuf { int overflow(int c) override { return c; } };
        NullBuffer nb;
        std::ostream nullOut(&nb);

        PlayerState base("OppRolloutDeterminism");
        base.addGold(50);
        shop.reroll(base, rng, false);
        const std::vector<MacroAction> legal = LegalActionGenerator::generate(base);

        PlayerState enemyP("EnemyBase");
        enemyP.addGold(50);
        enemyP.setLevel(6);
        const std::string champA = pickChampionByIndex(content, 0);
        const ChampionDefinition* defA = content.getChampion(champA);
        OwnedUnit e{};
        e.championName = champA;
        e.starLevel = 2;
        e.cost = defA ? defA->cost : 1;
        e.hasFormation = true;
        e.formation = Position{ 0, 0 };
        enemyP.boardMutable().push_back(e);
        const EnemySnapshot snap = ScoutSystem::snapshot(enemyP, content);
        const EnemySnapshot snapBefore = snap;

        RolloutPlannerConfig cfg{};
        cfg.depthRounds = 2;
        cfg.branchesPerAction = 3;
        cfg.topKActions = 6;
        cfg.maxActionsPerTurn = 10;
        cfg.debug = false;

        RolloutPlanner planner(777u, cfg);
        Random rrng(555u);
        const MacroAction a1 = planner.chooseAction(base, content, legal, &snap, &pool, 3, 10, rrng, nullOut);
        const MacroAction a2 = planner.chooseAction(base, content, legal, &snap, &pool, 3, 10, rrng, nullOut);

        const bool same =
            a1.type == a2.type &&
            a1.shopIndex == a2.shopIndex &&
            a1.boardIndex == a2.boardIndex &&
            a1.benchIndex == a2.benchIndex &&
            a1.itemIndex == a2.itemIndex &&
            a1.targetPosition.x == a2.targetPosition.x &&
            a1.targetPosition.y == a2.targetPosition.y;

        if (same) report.pass("Rollout: same seed gives same opponent rollout");
        else report.fail("Rollout: same seed gives same opponent rollout");

        auto snapEq = [](const EnemySnapshot& a, const EnemySnapshot& b) -> bool
        {
            if (a.boardStrength != b.boardStrength) return false;
            if (a.activeTraits != b.activeTraits) return false;
            if (a.carryUnit != b.carryUnit) return false;
            if (a.frontlineUnits != b.frontlineUnits) return false;
            if (a.itemStrength != b.itemStrength) return false;
            if (a.positioning != b.positioning) return false;
            if (a.level != b.level) return false;
            if (a.gold != b.gold) return false;
            if (a.xp != b.xp) return false;
            if (a.winStreak != b.winStreak) return false;
            if (a.loseStreak != b.loseStreak) return false;
            if (a.hp != b.hp) return false;
            if (a.units.size() != b.units.size()) return false;
            for (std::size_t i = 0; i < a.units.size(); ++i)
            {
                const auto& ua = a.units[i];
                const auto& ub = b.units[i];
                if (ua.championName != ub.championName) return false;
                if (ua.cost != ub.cost) return false;
                if (ua.starLevel != ub.starLevel) return false;
                if (ua.range != ub.range) return false;
                if (ua.threat != ub.threat) return false;
                if (ua.aoeThreat != ub.aoeThreat) return false;
                if (ua.itemOffense != ub.itemOffense) return false;
                if (ua.itemDefense != ub.itemDefense) return false;
                if (ua.itemCaster != ub.itemCaster) return false;
                if (ua.hasJumpThreat != ub.hasJumpThreat) return false;
                if (ua.position.x != ub.position.x || ua.position.y != ub.position.y) return false;
            }
            return true;
        };

        if (snapEq(snap, snapBefore)) report.pass("Rollout: opponent rollout does not mutate enemy snapshot");
        else report.fail("Rollout: opponent rollout does not mutate enemy snapshot");
    }

    {
        struct NullBuffer final : std::streambuf { int overflow(int c) override { return c; } };
        NullBuffer nb;
        std::ostream nullOut(&nb);

        SharedUnitPool poolSim(content);
        ShopSystem shopSim(content, poolSim);
        RoundSystem roundsSim(content, poolSim);

        PlayerState a("TempoA");
        PlayerState b("TempoB");
        a.addGold(MacroConstants::StartingGold);
        b.addGold(MacroConstants::StartingGold);

        SimpleMacroAI aiA(1111u);
        SimpleMacroAI aiB(2222u);
        Random rngA(111u);
        Random rngB(222u);

        bool ok = true;
        for (int roundIndex = 0; roundIndex <= 17; ++roundIndex)
        {
            const RoundInfo info = RoundSchedule::get(roundIndex);

            const EnemySnapshot snapB = ScoutSystem::snapshot(b, content);
            shopSim.reroll(a, rngA, false);
            MacroTurnStats sA{};
            MacroSimulation::takeTurnForValidationAt(a, aiA, shopSim, rngA, content, &snapB, &poolSim, info.stage, roundIndex, nullOut, sA);

            const EnemySnapshot snapA = ScoutSystem::snapshot(a, content);
            shopSim.reroll(b, rngB, false);
            MacroTurnStats sB{};
            MacroSimulation::takeTurnForValidationAt(b, aiB, shopSim, rngB, content, &snapA, &poolSim, info.stage, roundIndex, nullOut, sB);

            if (info.isPve)
            {
                const RoundResult rA = roundsSim.runPvE(a, roundIndex, rngA.nextU32());
                const RoundResult rB = roundsSim.runPvE(b, roundIndex, rngB.nextU32());
                a.takeDamage(rA.damageToA);
                b.takeDamage(rB.damageToA);
                (void)EconomySystem::applyRoundEnd(a, rA.playerAWon);
                (void)EconomySystem::applyRoundEnd(b, rB.playerAWon);
            }
            else
            {
                const RoundResult r = roundsSim.runPvP(a, b, roundIndex, static_cast<std::uint32_t>(7777u + roundIndex));
                a.takeDamage(r.damageToA);
                b.takeDamage(r.damageToB);
                (void)EconomySystem::applyRoundEnd(a, r.playerAWon);
                (void)EconomySystem::applyRoundEnd(b, r.playerBWon);
            }

            if (info.stage == 3 && info.round == 2)
            {
                ok = ok && !(a.level() == 4 && a.gold() >= 80);
                ok = ok && !(b.level() == 4 && b.gold() >= 80);
            }
            if (info.stage == 4 && info.round == 1)
            {
                ok = ok && !((a.level() <= 5) && (a.gold() >= 100));
                ok = ok && !((b.level() <= 5) && (b.gold() >= 100));
            }
        }

        if (ok) report.pass("Macro: stage level pressure prevents extreme hoarding");
        else report.fail("Macro: stage level pressure prevents extreme hoarding");
    }

    {
        PlayerState p("LowHpHighGoldTempo");
        p.addGold(90);
        p.takeDamage(80);

        struct NullBuffer final : std::streambuf { int overflow(int c) override { return c; } };
        NullBuffer nb;
        std::ostream nullOut(&nb);

        SharedUnitPool poolSim(content);
        ShopSystem shopSim(content, poolSim);
        Random rngTurn(333u);
        shopSim.reroll(p, rngTurn, false);
        const std::vector<MacroAction> legal = LegalActionGenerator::generate(p);
        SimpleMacroAI ai(333u);
        const MacroAction a = ai.chooseAction(p, content, legal, nullptr, &poolSim, 4, 17, rngTurn, true, false, nullOut);

        if (a.type != MacroActionType::EndTurn) report.pass("Macro: low HP + high gold forces spending");
        else report.fail("Macro: low HP + high gold forces spending");
    }

    {
        PlayerState occ("FormationOverlap");
        const std::string champA = pickChampionByIndex(content, 0);
        const std::string champB = pickChampionByIndex(content, 1);
        const ChampionDefinition* defA = content.getChampion(champA);
        const ChampionDefinition* defB = content.getChampion(champB);
        OwnedUnit a{};
        a.championName = champA;
        a.starLevel = 1;
        a.cost = defA ? defA->cost : 1;
        a.hasFormation = true;
        a.formation = Position{ 0, 0 };
        OwnedUnit b{};
        b.championName = champB;
        b.starLevel = 1;
        b.cost = defB ? defB->cost : 1;
        b.hasFormation = true;
        b.formation = Position{ 1, 0 };
        occ.boardMutable().push_back(a);
        occ.boardMutable().push_back(b);

        MacroAction toOccupied{};
        toOccupied.type = MacroActionType::RepositionUnit;
        toOccupied.boardIndex = 1;
        toOccupied.targetPosition = Position{ 0, 0 };
        toOccupied.debugName = "Reposition";

        const bool ok = MacroExecutor::apply(toOccupied, occ, shop, rng, out);
        if (!ok) report.pass("Macro: Reposition to occupied tile is illegal");
        else report.fail("Macro: Reposition to occupied tile is illegal");

        const std::vector<MacroAction> legal = LegalActionGenerator::generate(occ);
        bool found = false;
        for (const MacroAction& la : legal)
        {
            if (la.type == MacroActionType::RepositionUnit &&
                la.boardIndex == 1 &&
                la.targetPosition.x == 0 &&
                la.targetPosition.y == 0)
            {
                found = true;
                break;
            }
        }
        if (!found) report.pass("Macro: LegalActionGenerator avoids occupied tiles");
        else report.fail("Macro: LegalActionGenerator avoids occupied tiles");
    }

    {
        std::ostringstream nullOut;

        PlayerState base("RolloutDeterminism");
        base.addGold(50);
        shop.reroll(base, rng, false);
        const std::vector<MacroAction> legal = LegalActionGenerator::generate(base);

        RolloutPlannerConfig cfg{};
        cfg.depthRounds = 1;
        cfg.branchesPerAction = 4;
        cfg.topKActions = 6;
        cfg.maxActionsPerTurn = 8;
        cfg.debug = false;

        const PlayerState baseBefore = base;
        const SharedUnitPool poolBefore = pool;

        RolloutPlanner planner(999u, cfg);
        const MacroAction a1 = planner.chooseAction(base, content, legal, nullptr, &pool, 1, 0, rng, nullOut);
        const MacroAction a2 = planner.chooseAction(base, content, legal, nullptr, &pool, 1, 0, rng, nullOut);

        const bool same =
            a1.type == a2.type &&
            a1.shopIndex == a2.shopIndex &&
            a1.boardIndex == a2.boardIndex &&
            a1.benchIndex == a2.benchIndex &&
            a1.itemIndex == a2.itemIndex &&
            a1.targetPosition.x == a2.targetPosition.x &&
            a1.targetPosition.y == a2.targetPosition.y;

        if (same) report.pass("Rollout: deterministic replay");
        else report.fail("Rollout: deterministic replay");

        const bool unchanged =
            base.gold() == baseBefore.gold() &&
            base.health() == baseBefore.health() &&
            base.level() == baseBefore.level() &&
            base.xp() == baseBefore.xp() &&
            base.bench().size() == baseBefore.bench().size() &&
            base.board().size() == baseBefore.board().size() &&
            base.shop().size() == baseBefore.shop().size();
        const bool poolUnchanged = pool.availableCount(pickChampionByIndex(content, 0)) == poolBefore.availableCount(pickChampionByIndex(content, 0));

        if (unchanged && poolUnchanged) report.pass("Rollout: cloned state isolation");
        else report.fail("Rollout: cloned state isolation");

        BranchPrunerConfig pcfg{};
        pcfg.topKActions = 6;
        const auto c1 = BranchPruner::buildCandidates(base, content, legal, nullptr, &pool, 1, 0, pcfg);
        const auto c2 = BranchPruner::buildCandidates(base, content, legal, nullptr, &pool, 1, 0, pcfg);
        bool samePrune = c1.size() == c2.size();
        if (samePrune)
        {
            for (std::size_t i = 0; i < c1.size(); ++i)
            {
                const MacroAction& x = c1[i].action;
                const MacroAction& y = c2[i].action;
                if (x.type != y.type || x.shopIndex != y.shopIndex || x.boardIndex != y.boardIndex ||
                    x.benchIndex != y.benchIndex || x.itemIndex != y.itemIndex ||
                    x.targetPosition.x != y.targetPosition.x || x.targetPosition.y != y.targetPosition.y)
                {
                    samePrune = false;
                    break;
                }
            }
        }
        if (samePrune) report.pass("Rollout: branch pruning deterministic");
        else report.fail("Rollout: branch pruning deterministic");
    }

    {
        std::ostringstream nullOut;
        PlayerState base("RolloutTermination");
        base.addGold(40);
        shop.reroll(base, rng, false);
        const std::vector<MacroAction> legal = LegalActionGenerator::generate(base);

        RolloutPlannerConfig cfg{};
        cfg.depthRounds = 3;
        cfg.branchesPerAction = 8;
        cfg.topKActions = 8;
        cfg.maxActionsPerTurn = 12;
        cfg.debug = false;

        RolloutPlanner planner(1234u, cfg);
        const MacroAction a = planner.chooseAction(base, content, legal, nullptr, &pool, 2, 3, rng, nullOut);
        bool isLegal = false;
        for (const MacroAction& la : legal)
        {
            if (la.type != a.type)
            {
                continue;
            }
            if (la.shopIndex != a.shopIndex || la.boardIndex != a.boardIndex || la.benchIndex != a.benchIndex ||
                la.itemIndex != a.itemIndex || la.targetPosition.x != a.targetPosition.x || la.targetPosition.y != a.targetPosition.y)
            {
                continue;
            }
            isLegal = true;
            break;
        }
        if (isLegal) report.pass("Rollout: terminates safely");
        else report.fail("Rollout: terminates safely");
    }

    {
        PlayerState base("RolloutDebugDeterminism");
        base.addGold(50);
        shop.reroll(base, rng, false);
        const std::vector<MacroAction> legal = LegalActionGenerator::generate(base);

        RolloutPlannerConfig cfg{};
        cfg.depthRounds = 1;
        cfg.branchesPerAction = 3;
        cfg.topKActions = 5;
        cfg.maxActionsPerTurn = 6;
        cfg.debug = true;

        RolloutPlanner planner(999u, cfg);
        std::ostringstream out1;
        std::ostringstream out2;
        (void)planner.chooseAction(base, content, legal, nullptr, &pool, 2, 3, rng, out1);
        (void)planner.chooseAction(base, content, legal, nullptr, &pool, 2, 3, rng, out2);

        const std::string s1 = out1.str();
        const std::string s2 = out2.str();
        const bool hasLabels =
            s1.find("Action:") != std::string::npos &&
            s1.find("Rollouts:") != std::string::npos &&
            s1.find("AvgEV:") != std::string::npos &&
            s1.find("AvgHP:") != std::string::npos &&
            s1.find("AvgBoard:") != std::string::npos &&
            s1.find("Top4Prob:") != std::string::npos &&
            s1.find("DeathRisk:") != std::string::npos &&
            s1.find("Chosen:") != std::string::npos;

        if (hasLabels && s1 == s2) report.pass("Rollout: debug output deterministic");
        else report.fail("Rollout: debug output deterministic");
    }

    {
        PlayerState weakA("FutureEvalWeakA");
        weakA.addGold(150);
        weakA.setLevel(4);

        PlayerState weakB("FutureEvalWeakB");
        weakB.addGold(50);
        weakB.setLevel(4);

        const FutureEval a = FutureStateEvaluator::evaluate(weakA, content, nullptr, &pool, 4, 10, nullptr);
        const FutureEval b = FutureStateEvaluator::evaluate(weakB, content, nullptr, &pool, 4, 10, nullptr);
        if (a.ev < b.ev) report.pass("Rollout: strong econ but weak board is penalized");
        else report.fail("Rollout: strong econ but weak board is penalized");
    }

    {
        PlayerState highHp("FutureEvalHpHigh");
        highHp.addGold(50);
        highHp.setLevel(4);

        PlayerState lowHp("FutureEvalHpLow");
        lowHp.addGold(50);
        lowHp.setLevel(4);
        lowHp.takeDamage(80);

        const FutureEval a = FutureStateEvaluator::evaluate(highHp, content, nullptr, &pool, 4, 10, nullptr);
        const FutureEval b = FutureStateEvaluator::evaluate(lowHp, content, nullptr, &pool, 4, 10, nullptr);
        if (b.ev < a.ev && b.top4Prob < a.top4Prob) report.pass("Rollout: low HP increases tempo preference");
        else report.fail("Rollout: low HP increases tempo preference");
    }

    {
        PlayerState highHp50("FutureEvalGoldHighHp50");
        highHp50.addGold(50);
        highHp50.setLevel(4);

        PlayerState highHp100("FutureEvalGoldHighHp100");
        highHp100.addGold(100);
        highHp100.setLevel(4);

        PlayerState lowHp50("FutureEvalGoldLowHp50");
        lowHp50.addGold(50);
        lowHp50.setLevel(4);
        lowHp50.takeDamage(80);

        PlayerState lowHp100("FutureEvalGoldLowHp100");
        lowHp100.addGold(100);
        lowHp100.setLevel(4);
        lowHp100.takeDamage(80);

        const float deltaHigh =
            FutureStateEvaluator::evaluate(highHp100, content, nullptr, &pool, 4, 10, nullptr).ev -
            FutureStateEvaluator::evaluate(highHp50, content, nullptr, &pool, 4, 10, nullptr).ev;
        const float deltaLow =
            FutureStateEvaluator::evaluate(lowHp100, content, nullptr, &pool, 4, 10, nullptr).ev -
            FutureStateEvaluator::evaluate(lowHp50, content, nullptr, &pool, 4, 10, nullptr).ev;

        if (deltaLow < deltaHigh) report.pass("Rollout: HP risk affects decisions");
        else report.fail("Rollout: HP risk affects decisions");
    }
}

static bool findSharedTraitPair(const ContentManager& content,
                                std::string& aOut,
                                std::string& bOut,
                                std::string& traitOut)
{
    std::unordered_map<std::string, std::string> first;
    for (const auto& [name, def] : content.champions())
    {
        if (!def.isPlayable)
        {
            continue;
        }
        for (const std::string& t : def.traits)
        {
            if (t.empty())
            {
                continue;
            }
            auto it = first.find(t);
            if (it == first.end())
            {
                first.emplace(t, name);
                continue;
            }
            if (it->second != name)
            {
                aOut = it->second;
                bOut = name;
                traitOut = t;
                return true;
            }
        }
    }
    return false;
}

static bool readFileText(const std::filesystem::path& path, std::string& out)
{
    std::filesystem::path openPath = path;
    try
    {
        openPath = std::filesystem::weakly_canonical(path);
    }
    catch (...)
    {
        openPath = path;
    }

    std::ifstream f(openPath, std::ios::in | std::ios::binary);
    if (!f.is_open())
    {
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static void collectStringLiterals(const std::string& fileText, std::unordered_set<std::string>& literals)
{
    literals.clear();
    literals.reserve(256);

    auto addNormalString = [&](std::size_t& i)
    {
        std::string s;
        i += 1;
        for (; i < fileText.size(); ++i)
        {
            const char c = fileText[i];
            if (c == '\\')
            {
                if (i + 1 < fileText.size())
                {
                    s.push_back(fileText[i + 1]);
                    i += 1;
                }
                continue;
            }
            if (c == '"')
            {
                break;
            }
            s.push_back(c);
        }
        literals.insert(std::move(s));
    };

    auto addRawString = [&](std::size_t& i)
    {
        if (i + 2 >= fileText.size() || fileText[i] != 'R' || fileText[i + 1] != '"')
        {
            return false;
        }
        std::size_t p = i + 2;
        std::string delim;
        while (p < fileText.size() && fileText[p] != '(')
        {
            delim.push_back(fileText[p]);
            p += 1;
            if (delim.size() > 16)
            {
                return false;
            }
        }
        if (p >= fileText.size() || fileText[p] != '(')
        {
            return false;
        }
        const std::string end = ")" + delim + "\"";
        const std::size_t start = p + 1;
        const std::size_t close = fileText.find(end, start);
        if (close == std::string::npos)
        {
            return false;
        }
        literals.insert(fileText.substr(start, close - start));
        i = close + end.size() - 1;
        return true;
    };

    for (std::size_t i = 0; i < fileText.size(); ++i)
    {
        if (fileText[i] == 'R' && i + 1 < fileText.size() && fileText[i + 1] == '"')
        {
            if (addRawString(i))
            {
                continue;
            }
        }
        if (fileText[i] == '"')
        {
            addNormalString(i);
        }
    }
}

static void noHardcodedNamesTest(const ContentManager& content, ValidationReport& report)
{
    std::vector<std::filesystem::path> files = {
        std::filesystem::path("..") / "include" / "ai" / "BoardStrengthEvaluator.hpp",
        std::filesystem::path("..") / "src" / "ai" / "BoardStrengthEvaluator.cpp",
        std::filesystem::path("..") / "include" / "ai" / "UnitValueEvaluator.hpp",
        std::filesystem::path("..") / "src" / "ai" / "UnitValueEvaluator.cpp",
        std::filesystem::path("..") / "include" / "ai" / "TraitSynergyEvaluator.hpp",
        std::filesystem::path("..") / "src" / "ai" / "TraitSynergyEvaluator.cpp",
        std::filesystem::path("..") / "include" / "ai" / "ItemValueEvaluator.hpp",
        std::filesystem::path("..") / "src" / "ai" / "ItemValueEvaluator.cpp",
        std::filesystem::path("..") / "include" / "ai" / "UpgradePotentialEvaluator.hpp",
        std::filesystem::path("..") / "src" / "ai" / "UpgradePotentialEvaluator.cpp",
        std::filesystem::path("..") / "include" / "ai" / "CompDirectionPlanner.hpp",
        std::filesystem::path("..") / "src" / "ai" / "CompDirectionPlanner.cpp",
        std::filesystem::path("..") / "include" / "ai" / "MacroActionScorer.hpp",
        std::filesystem::path("..") / "src" / "ai" / "MacroActionScorer.cpp",
        std::filesystem::path("..") / "include" / "ai" / "PositioningOptimizer.hpp",
        std::filesystem::path("..") / "src" / "ai" / "PositioningOptimizer.cpp",
        std::filesystem::path("..") / "include" / "ai" / "SimpleMacroAI.hpp",
        std::filesystem::path("..") / "src" / "ai" / "SimpleMacroAI.cpp",
        std::filesystem::path("..") / "include" / "ai" / "ScoutSystem.hpp",
        std::filesystem::path("..") / "src" / "ai" / "ScoutSystem.cpp"
    };

    std::vector<std::string> tokens;
    tokens.reserve(content.championCount() + content.traitCount() + content.itemCount());
    for (const auto& [name, _] : content.champions()) tokens.push_back(name);
    for (const auto& [name, _] : content.traits()) tokens.push_back(name);
    for (const auto& [name, _] : content.items()) tokens.push_back(name);

    bool missingAnyFile = false;
    for (const std::filesystem::path& rel : files)
    {
        const std::filesystem::path abs = std::filesystem::absolute(rel);
        std::string text;
        if (!readFileText(abs, text))
        {
            missingAnyFile = true;
            continue;
        }
        std::unordered_set<std::string> literals;
        collectStringLiterals(text, literals);
        for (const std::string& token : tokens)
        {
            if (literals.find(token) != literals.end())
            {
                report.fail("AI: No hardcoded champion/trait/item names");
                return;
            }
        }
    }

    if (missingAnyFile)
    {
        report.warning("AI: No hardcoded champion/trait/item names");
    }
    else
    {
        report.pass("AI: No hardcoded champion/trait/item names");
    }
}

static void strategicAiValidationTest(const ContentManager& content, ValidationReport& report)
{
    {
        PlayerState p("BoardScoreDeterminism");
        const std::string c0 = pickChampionByIndex(content, 0);
        const ChampionDefinition* def0 = content.getChampion(c0);
        OwnedUnit u{};
        u.championName = c0;
        u.starLevel = 1;
        u.cost = def0 ? def0->cost : 1;
        p.addToBench(u);
        p.moveBenchToBoard(0);

        const float s1 = BoardStrengthEvaluator::evaluate(p, content).total;
        const float s2 = BoardStrengthEvaluator::evaluate(p, content).total;

        if (std::abs(s1 - s2) < 0.001f) report.pass("StrategicAI PASS: BoardStrengthEvaluator deterministic");
        else report.fail("StrategicAI FAIL: BoardStrengthEvaluator deterministic");
    }

    {
        const std::string c0 = pickChampionByIndex(content, 0);
        const ChampionDefinition* def0 = content.getChampion(c0);

        PlayerState p1("Star1");
        OwnedUnit u1{};
        u1.championName = c0;
        u1.starLevel = 1;
        u1.cost = def0 ? def0->cost : 1;
        p1.addToBench(u1);
        p1.moveBenchToBoard(0);

        PlayerState p2("Star2");
        OwnedUnit u2 = u1;
        u2.starLevel = 2;
        p2.addToBench(u2);
        p2.moveBenchToBoard(0);

        const float s1 = BoardStrengthEvaluator::evaluate(p1, content).total;
        const float s2 = BoardStrengthEvaluator::evaluate(p2, content).total;

        if (s2 > s1) report.pass("StrategicAI PASS: higher star score");
        else report.fail("StrategicAI FAIL: higher star score");
    }

    {
        std::string chosenTrait;
        int breakpoint = 0;
        std::vector<std::string> traitChamps;
        traitChamps.reserve(9);

        for (const auto& [traitName, td] : content.traits())
        {
            if (td.trait.breakpoints.empty())
            {
                continue;
            }
            int bp = 0;
            for (int b : td.trait.breakpoints)
            {
                if (b > 0)
                {
                    bp = b;
                    break;
                }
            }
            if (bp <= 0 || bp > 6)
            {
                continue;
            }
            std::vector<std::string> champs;
            champs.reserve(static_cast<std::size_t>(bp));
            for (const auto& [name, def] : content.champions())
            {
                if (!def.isPlayable)
                {
                    continue;
                }
                bool has = false;
                for (const std::string& t : def.traits)
                {
                    if (t == traitName)
                    {
                        has = true;
                        break;
                    }
                }
                if (has)
                {
                    champs.push_back(name);
                    if (static_cast<int>(champs.size()) >= bp)
                    {
                        break;
                    }
                }
            }
            if (static_cast<int>(champs.size()) < bp)
            {
                continue;
            }
            chosenTrait = traitName;
            breakpoint = bp;
            traitChamps = std::move(champs);
            break;
        }

        if (chosenTrait.empty() || breakpoint <= 1)
        {
            report.warning("StrategicAI FAIL: active trait score (no suitable trait found)");
        }
        else
        {
            PlayerState inactive("TraitInactive");
            PlayerState active("TraitActive");
            inactive.setLevel(std::max(1, breakpoint));
            active.setLevel(std::max(1, breakpoint));

            for (int i = 0; i < breakpoint - 1; ++i)
            {
                const ChampionDefinition* d = content.getChampion(traitChamps[static_cast<std::size_t>(i)]);
                OwnedUnit u{};
                u.championName = traitChamps[static_cast<std::size_t>(i)];
                u.starLevel = 1;
                u.cost = d ? d->cost : 1;
                inactive.addToBench(u);
                active.addToBench(u);
            }
            {
                const ChampionDefinition* d = content.getChampion(traitChamps[static_cast<std::size_t>(breakpoint - 1)]);
                OwnedUnit u{};
                u.championName = traitChamps[static_cast<std::size_t>(breakpoint - 1)];
                u.starLevel = 1;
                u.cost = d ? d->cost : 1;
                active.addToBench(u);
            }

            int guardA = 0;
            while (!inactive.bench().empty() && guardA < 20)
            {
                if (!inactive.moveBenchToBoard(0))
                {
                    break;
                }
                guardA += 1;
            }
            int guardB = 0;
            while (!active.bench().empty() && guardB < 20)
            {
                if (!active.moveBenchToBoard(0))
                {
                    break;
                }
                guardB += 1;
            }

            const float sInactive = TraitSynergyEvaluator::evaluate(inactive, content).total;
            const float sActive = TraitSynergyEvaluator::evaluate(active, content).total;
            if (sActive > sInactive)
            {
                report.pass("StrategicAI PASS: active trait score");
            }
            else
            {
                report.fail("StrategicAI FAIL: active trait score");
            }
        }
    }

    {
        auto similarity = [](const ChampionDefinition& a, const ChampionDefinition& b) -> float
        {
            const float hp = std::abs(static_cast<float>(a.hp - b.hp)) / static_cast<float>(std::max(1, a.hp + b.hp));
            const float ad = std::abs(static_cast<float>(a.ad - b.ad)) / static_cast<float>(std::max(1, a.ad + b.ad));
            const float armor = std::abs(static_cast<float>(a.armor - b.armor)) / static_cast<float>(std::max(1, a.armor + b.armor));
            const float mr = std::abs(static_cast<float>(a.magicResist - b.magicResist)) / static_cast<float>(std::max(1, a.magicResist + b.magicResist));
            const float as = std::abs(a.attackSpeed - b.attackSpeed) / std::max(0.01f, (a.attackSpeed + b.attackSpeed));
            const float ap = std::abs(a.abilityPower - b.abilityPower) / std::max(1.0f, (a.abilityPower + b.abilityPower));
            const float range = std::abs(static_cast<float>(a.range - b.range)) / static_cast<float>(std::max(1, a.range + b.range));
            return hp + ad + armor + mr + as + ap + range;
        };

        const ChampionDefinition* low = nullptr;
        const ChampionDefinition* high = nullptr;

        float best = 1e9f;
        for (const auto& [an, a] : content.champions())
        {
            if (!a.isPlayable) continue;
            for (const auto& [bn, b] : content.champions())
            {
                if (!b.isPlayable) continue;
                if (b.cost <= a.cost) continue;
                if (b.cost - a.cost < 2) continue;
                const float s = similarity(a, b);
                if (s < best)
                {
                    best = s;
                    low = &a;
                    high = &b;
                }
            }
        }

        constexpr float SimilarityMax = 1.35f;
        if (!low || !high || best > SimilarityMax)
        {
            report.warning("StrategicAI FAIL: higher cost score (no similar-stats pair found)");
        }
        else
        {
            PlayerState cheap("CostLow");
            PlayerState pricey("CostHigh");

            OwnedUnit uLow{};
            uLow.championName = low->name;
            uLow.starLevel = 1;
            uLow.cost = low->cost;
            cheap.addToBench(uLow);
            cheap.moveBenchToBoard(0);

            OwnedUnit uHigh{};
            uHigh.championName = high->name;
            uHigh.starLevel = 1;
            uHigh.cost = high->cost;
            pricey.addToBench(uHigh);
            pricey.moveBenchToBoard(0);

            const float sCheap = BoardStrengthEvaluator::evaluate(cheap, content).total;
            const float sPricey = BoardStrengthEvaluator::evaluate(pricey, content).total;
            if (sPricey > sCheap)
            {
                report.pass("StrategicAI PASS: higher cost score");
            }
            else
            {
                report.fail("StrategicAI FAIL: higher cost score");
            }
        }
    }

    {
        const std::string c0 = pickChampionByIndex(content, 0);
        const std::string c1 = pickChampionByIndex(content, 1);

        const ChampionDefinition* def0 = content.getChampion(c0);
        const ChampionDefinition* def1 = content.getChampion(c1);

        PlayerState p("ActionScoreDup");
        p.addGold(100);

        OwnedUnit base{};
        base.championName = c0;
        base.starLevel = 1;
        base.cost = def0 ? def0->cost : 1;
        p.addToBench(base);
        p.addToBench(base);

        p.shopMutable().clear();
        p.shopMutable().push_back(ShopOffer{ c0, def0 ? def0->cost : 1 });
        p.shopMutable().push_back(ShopOffer{ c1, def1 ? def1->cost : 1 });

        MacroAction buy0{};
        buy0.type = MacroActionType::BuyUnit;
        buy0.shopIndex = 0;
        buy0.goldCost = def0 ? def0->cost : 1;
        buy0.debugName = "BuyUnit #0";

        MacroAction buy1{};
        buy1.type = MacroActionType::BuyUnit;
        buy1.shopIndex = 1;
        buy1.goldCost = def1 ? def1->cost : 1;
        buy1.debugName = "BuyUnit #1";

        MacroAction end{};
        end.type = MacroActionType::EndTurn;
        end.debugName = "EndTurn";

        const std::vector<MacroAction> legal = { buy0, buy1, end };
        const std::vector<ActionScore> scores = MacroActionScorer::scoreActions(p, content, legal, nullptr, nullptr, 2, 1);

        float sDup = -1e9f;
        float sOther = -1e9f;
        for (const ActionScore& s : scores)
        {
            if (s.action.shopIndex == 0) sDup = s.score;
            if (s.action.shopIndex == 1) sOther = s.score;
        }

        if (sDup > sOther) report.pass("StrategicAI PASS: duplicate action score");
        else report.fail("StrategicAI FAIL: duplicate action score");
    }

    {
        PlayerState p("LowHpUrgency");
        p.addGold(20);
        p.takeDamage(90);

        MacroAction rr{};
        rr.type = MacroActionType::RerollShop;
        rr.goldCost = 2;
        rr.debugName = "RerollShop";

        MacroAction xp{};
        xp.type = MacroActionType::BuyXp;
        xp.goldCost = 4;
        xp.debugName = "BuyXp";

        MacroAction end{};
        end.type = MacroActionType::EndTurn;
        end.debugName = "EndTurn";

        const std::vector<MacroAction> legal = { rr, xp, end };
        const std::vector<ActionScore> scores = MacroActionScorer::scoreActions(p, content, legal, nullptr, nullptr, 2, 1);

        float sEnd = -1e9f;
        float sRR = -1e9f;
        for (const ActionScore& s : scores)
        {
            if (s.action.type == MacroActionType::EndTurn) sEnd = s.score;
            if (s.action.type == MacroActionType::RerollShop) sRR = s.score;
        }

        if (sRR > sEnd) report.pass("StrategicAI PASS: low HP urgency");
        else report.fail("StrategicAI FAIL: low HP urgency");
    }

    {
        PlayerState p("HighGoldEconomy");
        p.addGold(50);

        MacroAction rr{};
        rr.type = MacroActionType::RerollShop;
        rr.goldCost = 2;
        rr.debugName = "RerollShop";

        MacroAction end{};
        end.type = MacroActionType::EndTurn;
        end.debugName = "EndTurn";

        const std::vector<MacroAction> legal = { rr, end };
        const std::vector<ActionScore> scores = MacroActionScorer::scoreActions(p, content, legal, nullptr, nullptr, 2, 1);

        float sEnd = -1e9f;
        float sRR = -1e9f;
        for (const ActionScore& s : scores)
        {
            if (s.action.type == MacroActionType::EndTurn) sEnd = s.score;
            if (s.action.type == MacroActionType::RerollShop) sRR = s.score;
        }
        if (sEnd >= sRR)
        {
            report.pass("StrategicAI PASS: high gold preserves economy");
        }
        else
        {
            report.fail("StrategicAI FAIL: high gold preserves economy");
        }
    }

    {
        PlayerState p("ActionOrderStability");
        p.addGold(50);
        MacroAction rr{};
        rr.type = MacroActionType::RerollShop;
        rr.goldCost = 2;
        rr.debugName = "RerollShop";
        MacroAction end{};
        end.type = MacroActionType::EndTurn;
        end.debugName = "EndTurn";
        const std::vector<MacroAction> legal = { rr, end };

        const std::vector<ActionScore> s1 = MacroActionScorer::scoreActions(p, content, legal, nullptr, nullptr, 1, 0);
        const std::vector<ActionScore> s2 = MacroActionScorer::scoreActions(p, content, legal, nullptr, nullptr, 1, 0);

        const bool ok =
            s1.size() == s2.size() &&
            (s1.empty() || s1[0].action.debugName == s2[0].action.debugName) &&
            (s1.size() < 2 || s1[1].action.debugName == s2[1].action.debugName);
        if (ok) report.pass("StrategicAI PASS: action score stable ordering");
        else report.fail("StrategicAI FAIL: action score stable ordering");
    }

    noHardcodedNamesTest(content, report);
}

ValidationReport CombatValidation::runAll(const ContentManager& content, std::ostream& out)
{
    ValidationReport report{};

    out << "CombatValidation mode ON\n\n";

    auto step = [&](const std::string& name, auto&& fn)
    {
        logValidationStart(out, name);
        const std::size_t beforeFails = failCount(report);
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        const auto t1 = std::chrono::steady_clock::now();
        const long long ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        const bool passed = failCount(report) == beforeFails;
        logValidationEnd(out, name, passed, ms);
    };

    step("Content fidelity", [&]() { contentFidelityValidationTest(content, report, out); });
    step("Attack speed timing", [&]() { attackSpeedTimerTest(report, out); });
    step("Mana system", [&]() { manaSystemTest(content, report, out); });
    step("Projectile timing", [&]() { projectileTimingTest(content, report); });
    step("Cast timing", [&]() { castTimingTest(content, report); });
    step("Crit consistency", [&]() { critStatisticsTest(report, out); });
    step("Armor/MR formula", [&]() { armorMrFormulaTest(report, out); });
    step("Target selection determinism", [&]() { targetingDeterminismTest(content, report); });
    step("AoE targeting", [&]() { areaShapeTest(report, out); });
    step("Delayed events", [&]() { delayedEventTest(content, report); });
    step("Replay consistency", [&]() { replayConsistencyTest(content, report, out); });
    step("Benchmark", [&]() { benchmarkTest(content, report, out); });
    step("Trait effect vocabulary", [&]() { traitEffectVocabularyValidationTest(report); });
    step("Validation scenarios", [&]() { scenarioSuite(content, report, out); });
    step("Macro validations", [&]() { macroSystemValidationTest(content, report, out); });
    step("Strategic AI validations", [&]() { strategicAiValidationTest(content, report); });

    return report;
}
