#pragma once

#include <cstdint>

struct ValidationConstants
{
    static constexpr std::uint32_t DefaultSeed = 999u;
    static constexpr std::uint32_t ManaTestSeed = 7u;
    static constexpr std::uint32_t TargetSelectionSeed = 77u;
    static constexpr std::uint32_t ReplaySeed = 17u;
    static constexpr std::uint32_t ScenarioSeed = 1007u;
    static constexpr std::uint32_t TargetingDeterminismSeed = 42u;

    static constexpr int ReplayIterations = 100;

    static constexpr std::int32_t DefaultDtMs = 100;

    static constexpr int MaxWaitTicks = 2000;
    static constexpr int AttackSpeedInnerIters = 10;
    static constexpr int AttackSpeedAttackReadyGuard = 10000;
    static constexpr std::int32_t AttackSpeedMaxElapsedMs = 20000;

    static constexpr int CritConsistencyTrials = 10000;
    static constexpr std::uint32_t CritTestSeed = 123u;
    static constexpr float CritExpectedChance = 0.25f;
    static constexpr float CritDiffPass = 0.01f;
    static constexpr float CritDiffWarn = 0.02f;

    static constexpr int ArmorMrRawDamage = 200;
    static constexpr int ArmorMrMax = 200;
    static constexpr int ArmorMrStep = 20;

    static constexpr std::int32_t DummyHp = 1000;
    static constexpr std::int32_t DummyAttackCooldownMs = 1000;
    static constexpr std::int32_t DummyRange = 1;
    static constexpr std::int32_t LongDurationMs = 60000;

    static constexpr float BuffAttackSpeedNeutral = 1.0f;
    static constexpr float BuffAttackSpeedHigh = 3.0f;
    static constexpr float BuffAttackSpeedHigh2 = 1.5f;
    static constexpr float BuffCritChanceLow = 0.25f;
    static constexpr float BuffCritChanceHigh = 0.5f;
    static constexpr float BuffCritDamageLow = 0.5f;
    static constexpr float BuffCritDamageHigh = 0.75f;
    static constexpr float BuffDamageReduction = 0.4f;

    static constexpr int ManaResetReleaseMs = 300;
    static constexpr int ManaLifecycleLoopMaxTicks = 2000;
    static constexpr std::int32_t ManaLockTestGainAttempt = 50;
    static constexpr std::int32_t ManaLockTestDamageTaken = 200;
    static constexpr std::int32_t ManaLockTestGainAfterRelease = 20;
    static constexpr std::int32_t ManaLockTestGainAfterUnlock = 20;

    static constexpr std::uint32_t CastTimingSeed = 19u;
    static constexpr std::int32_t CastTimingWindupMs = 300;
    static constexpr std::int32_t CastTimingRecoveryMs = 200;
    static constexpr std::int32_t CastTimingEndMs = 500;
    static constexpr std::int32_t CastTimingTicksToProcess = 6;

    static constexpr std::int32_t ManaTestDummyMaxMana = 50;
    static constexpr std::int32_t ManaTestDummyManaCost = 10;
    static constexpr int AttackSpeedQuickTickCount = 300;
    static constexpr std::int32_t ProjectileTimingDtMs = 50;
    static constexpr int ProjectileTimingMaxTicks = 300;


    static constexpr std::int32_t VeryLargeValue = 999999;
};
