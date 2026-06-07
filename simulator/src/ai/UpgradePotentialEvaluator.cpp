#include "ai/UpgradePotentialEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "macro/ShopSystem.hpp"
#include <array>
#include <algorithm>
#include <sstream>
#include <unordered_map>

static constexpr int CopiesPerStarUpgrade = 3;
static constexpr int OneStarIndex = 1;
static constexpr int TwoStarIndex = 2;
static constexpr int StarIndexCount = 4;

static int poolSizeForCost(int cost)
{
    switch (cost)
    {
        case 1: return GameConstants::SharedPoolSizeCost1;
        case 2: return GameConstants::SharedPoolSizeCost2;
        case 3: return GameConstants::SharedPoolSizeCost3;
        case 4: return GameConstants::SharedPoolSizeCost4;
        case 5: return GameConstants::SharedPoolSizeCost5;
        default: return GameConstants::SharedPoolSizeCost1;
    }
}

static void addCounts(std::unordered_map<std::string, std::array<int, StarIndexCount>>& counts, const OwnedUnit& u)
{
    const int s = std::clamp(u.starLevel, 1, 3);
    counts[u.championName][s] += 1;
}

static float availabilityMultiplier(const SharedUnitPool* pool, const ContentManager& content, const std::string& name)
{
    if (!pool)
    {
        return 1.0f;
    }
    const ChampionDefinition* def = content.getChampion(name);
    const int cost = def ? def->cost : 1;
    const int initial = std::max(1, poolSizeForCost(cost));
    const int avail = std::max(0, pool->availableCount(name));
    const float ratio = std::clamp(static_cast<float>(avail) / static_cast<float>(initial), 0.0f, 1.0f);
    return AIConstants::UpgradeAvailabilityMin + AIConstants::UpgradeAvailabilityScale * ratio;
}

static UpgradePotentialScore evaluateImpl(const PlayerState& player,
                                         const ContentManager& content,
                                         const SharedUnitPool* pool)
{
    UpgradePotentialScore out{};

    std::unordered_map<std::string, std::array<int, StarIndexCount>> counts;

    for (const OwnedUnit& u : player.board())
    {
        addCounts(counts, u);
    }
    for (const OwnedUnit& u : player.bench())
    {
        addCounts(counts, u);
    }

    float near2 = 0.0f;
    float near3 = 0.0f;
    for (const auto& [name, arr] : counts)
    {
        const int c1 = arr[OneStarIndex];
        const int c2 = arr[TwoStarIndex];
        const float avail = availabilityMultiplier(pool, content, name);
        if ((c1 % CopiesPerStarUpgrade) == TwoStarIndex) near2 += AIConstants::UpgradeNearTwoStarMod2Weight * avail;
        if ((c2 % CopiesPerStarUpgrade) == TwoStarIndex) near3 += AIConstants::UpgradeNearThreeStarMod2Weight * avail;
        if ((c1 % CopiesPerStarUpgrade) == OneStarIndex) near2 += AIConstants::UpgradeNearTwoStarMod1Weight * avail;
        if ((c2 % CopiesPerStarUpgrade) == OneStarIndex) near3 += AIConstants::UpgradeNearThreeStarMod1Weight * avail;
    }

    float shopPairs = 0.0f;
    for (const ShopOffer& o : player.shop())
    {
        if (o.championName.empty())
        {
            continue;
        }
        const auto it = counts.find(o.championName);
        if (it == counts.end())
        {
            continue;
        }
        const int c1 = it->second[OneStarIndex];
        const int c2 = it->second[TwoStarIndex];
        const float avail = availabilityMultiplier(pool, content, o.championName);
        if ((c1 % CopiesPerStarUpgrade) == TwoStarIndex) shopPairs += AIConstants::UpgradeShopPairTwoStarMod2Weight * avail;
        if ((c2 % CopiesPerStarUpgrade) == TwoStarIndex) shopPairs += AIConstants::UpgradeShopPairThreeStarMod2Weight * avail;
        if ((c1 % CopiesPerStarUpgrade) == OneStarIndex) shopPairs += AIConstants::UpgradeShopPairTwoStarMod1Weight * avail;
        if ((c2 % CopiesPerStarUpgrade) == OneStarIndex) shopPairs += AIConstants::UpgradeShopPairThreeStarMod1Weight * avail;
    }

    const float goldFactor =
        std::clamp(static_cast<float>(player.gold()) / static_cast<float>(MacroConstants::MaxInterestGold), 0.0f, 1.0f);

    out.nearTwoStar = near2;
    out.nearThreeStar = near3;
    out.shopPairs = shopPairs;
    out.total = (near2 * AIConstants::UpgradeTotalNearTwoStarWeight +
                 near3 +
                 shopPairs * AIConstants::UpgradeTotalShopPairsWeight) *
                (AIConstants::UpgradeTotalGoldBase + AIConstants::UpgradeTotalGoldScale * goldFactor);

    return out;
}

UpgradePotentialScore UpgradePotentialEvaluator::evaluate(const PlayerState& player, const ContentManager& content)
{
    return evaluateImpl(player, content, nullptr);
}

UpgradePotentialScore UpgradePotentialEvaluator::evaluate(const PlayerState& player,
                                                         const ContentManager& content,
                                                         const SharedUnitPool* pool)
{
    return evaluateImpl(player, content, pool);
}
