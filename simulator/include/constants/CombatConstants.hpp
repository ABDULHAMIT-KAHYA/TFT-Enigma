#pragma once

#include <cstdint>

struct CombatConstants
{
    static constexpr std::int32_t MsPerSecond = 1000;
    static constexpr std::int32_t MaxCombatDurationMs = 60000;

    static constexpr float LowHealthThresholdPct = 0.25f;

    // TFT-like stat scaling used by RoundSystem when instantiating 2* / 3* units.
    static constexpr float TwoStarStatMultiplier = 1.8f;
    static constexpr float ThreeStarStatMultiplier = 3.0f;

    static constexpr std::int32_t AttackWindupMeleeMs = 150;
    static constexpr std::int32_t AttackWindupRangedMs = 200;
    static constexpr std::int32_t AttackBackswingMeleeMs = 100;
    static constexpr std::int32_t AttackBackswingRangedMs = 150;

    static constexpr float ProjectileSpeedCellsPerSecond = 12.0f;

    static constexpr std::int32_t RetargetLockMs = 200;
    static constexpr std::int32_t CastTargetLockMs = 400;

    static constexpr std::int32_t VerboseBoardPrintIntervalMs = 1000;
    static constexpr std::int32_t MovementBlockedWarnIntervalMs = 1000;

    static constexpr std::int32_t SpellWindupMs = 300;
    static constexpr std::int32_t SpellRecoveryMs = 200;

    static constexpr float ManaPerHpLostDamageTaken = 0.06f;
    static constexpr float ManaDamageTakenCapFraction = 0.5f;

    static constexpr std::int32_t TraitPeriodicTickMs = 1000;
    static constexpr std::int32_t TraitAuraUpdateTickMs = 500;

    static constexpr float DefaultCritDamageMultiplier = 1.5f;
    static constexpr std::int32_t BlockedWarnInitialTimeMs = -1000000000;

    static constexpr float MaxDamageReduction = 0.95f;

    static constexpr float DefenseMitigationPercentScale = 100.0f;

    static constexpr std::int32_t DefaultPassiveStatusDurationMs = 12000;

    static constexpr std::int32_t TraitShieldOnCombatStartDurationMs = 8000;

    // Sentinel value in JSON/content to indicate "no override" for optional float fields.
    static constexpr float NoOverrideFloat = -1.0f;
};
