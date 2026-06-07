#pragma once

#include <cstdint>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "core/Random.hpp"
#include "macro/ShopSystem.hpp"
enum class RoundType
{
    PvE,
    PvP
};

struct RoundResult
{
    bool playerAWon = false;
    bool playerBWon = false;
    std::int32_t damageToA = 0;
    std::int32_t damageToB = 0;
    int survivingA = 0;
    int survivingB = 0;
};

class RoundSystem
{
public:
    RoundSystem(const ContentManager& content, SharedUnitPool& pool);

    void startRound(PlayerState& player, ShopSystem& shop, Random& rng, std::int32_t roundIndex);
    void buyXp(PlayerState& player, std::int32_t goldSpend);
    void autoLevel(PlayerState& player);

    RoundResult runPvP(PlayerState& a, PlayerState& b, std::int32_t roundIndex, std::uint32_t combatSeed);
    RoundResult runPvE(PlayerState& player, std::int32_t roundIndex, std::uint32_t combatSeed);

    void applyRoundEndEconomy(PlayerState& player, bool won);

private:
    void autoDeploy(PlayerState& player);
    std::int32_t computePlayerDamage(int survivingEnemyUnits, int roundIndex) const;

    const ContentManager* content_;
    SharedUnitPool* pool_;
};
