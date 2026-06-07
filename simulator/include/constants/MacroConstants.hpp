#pragma once

#include <cstdint>

struct MacroConstants
{
    static constexpr int MaxActionsPerTurn = 30;
    static constexpr int MaxRounds = 200;

    static constexpr int MaxTransactionsPerTurn = 8;
    static constexpr int MaxSellsPerTurn = 4;
    static constexpr int MaxBoardSellsPerTurn = 2;
    static constexpr float MaxSellBoardStrengthDropPct = 0.12f;
    static constexpr float MinSellBoardStrengthDropAbs = 40.0f;
    static constexpr float StrongerReplacementMinFactor = 1.15f;
    static constexpr float StrongerReplacementMinAbs = 30.0f;

    static constexpr int MaxMoveActionsSoftCap = 10;
    static constexpr int MaxMoveActionsHardCap = 14;
    static constexpr int MaxRerollsPerTurn = 4;
    static constexpr int MaxRerollsHardCap = 6;
    static constexpr int MaxXpBuysPerTurn = 2;
    static constexpr int MaxXpBuysHardCap = 3;
    static constexpr int MaxRepositionActionsPerTurn = 1;
    static constexpr int MaxRepeatedActionKey = 3;
    static constexpr std::size_t RecentMoveHistoryLimit = 6;

    static constexpr std::int32_t InterestStepGold = 10;
    static constexpr std::int32_t MaxInterestGold = 50;
    static constexpr std::int32_t MaxInterest = 5;

    static constexpr std::int32_t BaseRoundGold = 5;
    static constexpr std::int32_t WinBonusGold = 1;
    static constexpr std::int32_t StartingGold = 10;
    static constexpr std::int32_t StreakTier1 = 2;
    static constexpr std::int32_t StreakTier2 = 3;
    static constexpr std::int32_t StreakTier3 = 5;
    static constexpr std::int32_t StreakBonus1 = 1;
    static constexpr std::int32_t StreakBonus2 = 2;
    static constexpr std::int32_t StreakBonus3 = 3;
};
