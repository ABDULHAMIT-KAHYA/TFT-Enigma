#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>
#include "content/Ability.hpp"
#include "core/Board.hpp"
#include "content/ContentManager.hpp"
#include "combat/DamageSystem.hpp"
#include "core/GameState.hpp"
#include "core/TeamId.hpp"
#include "core/Unit.hpp"
enum class ValidationStatus
{
    Pass,
    Warning,
    Fail
};

struct ValidationEntry
{
    ValidationStatus status = ValidationStatus::Pass;
    std::string message{};
};

struct ValidationReport
{
    std::vector<ValidationEntry> entries{};

    void pass(std::string message);
    void warning(std::string message);
    void fail(std::string message);

    bool hasFail() const;
    void print(std::ostream& out) const;
};

class CombatValidation
{
public:
    static void setEnabled(bool enabled);
    static bool enabled();

    static void setDetailedLogs(bool detailed);
    static bool detailedLogs();

    static void logAutoAttack(GameState& state,
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
                              std::int32_t manaGainFromDamageTaken);

    static void logAbilityHit(GameState& state,
                              const Unit& caster,
                              const Unit& target,
                              std::string_view sourceName,
                              std::int32_t rawDamage,
                              const DamageDebugResult& dmg,
                              bool didCrit,
                              float critChanceUsed,
                              float critDamageUsed,
                              std::int32_t rawAfterCrit,
                              std::int32_t manaGainFromDamageTaken);

    static void logTargetChange(GameState& state,
                                const Unit& unit,
                                const Unit* previous,
                                const Unit* current);

    static void logArea(const Board& board,
                        const Position& origin,
                        AreaShape shape,
                        std::int32_t radius,
                        const Position& directionTarget,
                        std::ostream& out);

    static ValidationReport runAll(const ContentManager& content, std::ostream& out);
};
