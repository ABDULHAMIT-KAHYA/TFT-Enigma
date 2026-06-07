#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct GameConstants
{
    static constexpr int MaxLevel = 9;
    static constexpr std::size_t BenchLimit = 9;
    static constexpr std::size_t MaxItemsPerUnit = 3;

    static constexpr std::int32_t BoardWidth = 10;
    static constexpr std::int32_t BoardHeight = 10;

    static constexpr std::int32_t FormationWidth = 7;
    static constexpr std::int32_t FormationHeight = 3;
    static constexpr std::int32_t DefaultFormationX = 3;
    static constexpr std::int32_t DefaultFormationY = 1;

    static constexpr std::int32_t DefaultDtMs = 100;

    static constexpr int ShopOffersPerRoll = 5;
    static constexpr int ShopRollAttempts = 40;
    static constexpr int ShopFallbackCostMin = 1;
    static constexpr int ShopFallbackCostMax = 5;

    static constexpr std::int32_t ShopRerollCostGold = 2;
    static constexpr std::int32_t XpPerBuy = 4;

    static constexpr int CopiesPerTwoStar = 3;
    static constexpr int CopiesPerThreeStar = 9;

    static constexpr int XpTableSize = 10;
    static constexpr std::array<std::int32_t, XpTableSize> XpToNextLevel = {
        0,
        2,
        4,
        6,
        10,
        20,
        36,
        56,
        80,
        0
    };

    // Shop roll odds by player level. Each entry is weights for costs [1..5].
    static constexpr std::array<std::array<int, 6>, 10> ShopCostOddsByLevel = { {
        { 0, 0, 0, 0, 0, 0 },               // 0 (unused)
        { 0, 100, 0, 0, 0, 0 },             // 1
        { 0, 100, 0, 0, 0, 0 },             // 2
        { 0, 75, 25, 0, 0, 0 },             // 3
        { 0, 55, 30, 15, 0, 0 },            // 4
        { 0, 45, 33, 20, 2, 0 },            // 5
        { 0, 30, 40, 25, 5, 0 },            // 6
        { 0, 19, 30, 40, 10, 1 },           // 7
        { 0, 18, 25, 32, 22, 3 },           // 8
        { 0, 15, 20, 25, 30, 10 }           // 9
    } };

    static constexpr std::uint32_t SeedMixConstant = 0x9e3779b9u;
    static constexpr int SeedMixShiftA = 6;
    static constexpr int SeedMixShiftB = 2;
    static constexpr int SeedScrambleShift1 = 13;
    static constexpr int SeedScrambleShift2 = 17;
    static constexpr int SeedScrambleShift3 = 5;
    static constexpr std::uint32_t AiSeedSaltA = 0xA11u;
    static constexpr std::uint32_t AiSeedSaltB = 0xB22u;
    static constexpr std::uint32_t RngSeedSaltA = 0x1234567u;
    static constexpr std::uint32_t RngSeedSaltB = 0x7654321u;

    static constexpr int SharedPoolSizeCost1 = 22;
    static constexpr int SharedPoolSizeCost2 = 20;
    static constexpr int SharedPoolSizeCost3 = 17;
    static constexpr int SharedPoolSizeCost4 = 10;
    static constexpr int SharedPoolSizeCost5 = 9;
};
