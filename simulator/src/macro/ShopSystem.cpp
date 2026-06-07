#include "macro/ShopSystem.hpp"
#include "content/ChampionFilter.hpp"
#include "constants/GameConstants.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>
#include <cctype>

static int rngInt(Random& rng, int maxExclusive)
{
    if (maxExclusive <= 1)
    {
        return 0;
    }
    return rng.nextInt(maxExclusive);
}

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

static int copiesForStarLevel(int starLevel)
{
    if (starLevel <= 1) return 1;
    if (starLevel == 2) return GameConstants::CopiesPerTwoStar;
    return GameConstants::CopiesPerThreeStar;
}

SharedUnitPool::SharedUnitPool(const ContentManager& content)
{
    static bool logged = false;
    int playable = 0;
    int filtered = 0;
    for (const auto& [name, champ] : content.champions())
    {
        if (!isPlayableChampion(champ))
        {
            filtered += 1;
            continue;
        }
        playable += 1;
        const int cost = champ.cost;
        if (cost < 1 || cost > 5)
        {
            continue;
        }
        remaining_[name] = poolSizeForCost(cost);
        cost_[name] = cost;
        names_by_cost_[cost].push_back(name);
    }
    for (auto& [_, names] : names_by_cost_)
    {
        std::sort(names.begin(), names.end());
    }

    if (!logged)
    {
        logged = true;
        std::cout << "Playable Champions: " << playable << "\n";
        std::cout << "Filtered Non-Playable Entities: " << filtered << "\n";
        std::cout << std::flush;
    }
}

int SharedUnitPool::availableCount(const std::string& championName) const
{
    auto it = remaining_.find(championName);
    if (it == remaining_.end())
    {
        return 0;
    }
    return std::max(0, it->second);
}

bool SharedUnitPool::takeChampion(const std::string& championName)
{
    auto it = remaining_.find(championName);
    if (it == remaining_.end() || it->second <= 0)
    {
        return false;
    }
    it->second -= 1;
    return true;
}

void SharedUnitPool::returnChampion(const std::string& championName, int copies)
{
    if (copies <= 0)
    {
        return;
    }
    auto it = remaining_.find(championName);
    if (it == remaining_.end())
    {
        return;
    }
    it->second += copies;
}

std::string SharedUnitPool::randomChampionByCost(int cost, Random& rng) const
{
    auto itNames = names_by_cost_.find(cost);
    if (itNames == names_by_cost_.end() || itNames->second.empty())
    {
        return "";
    }

    int total = 0;
    for (const std::string& name : itNames->second)
    {
        total += availableCount(name);
    }
    if (total <= 0)
    {
        return "";
    }

    int r = rngInt(rng, total);
    for (const std::string& name : itNames->second)
    {
        const int c = availableCount(name);
        if (c <= 0)
        {
            continue;
        }
        if (r < c)
        {
            return name;
        }
        r -= c;
    }
    return "";
}

bool SharedUnitPool::canTake(const std::string& championName) const
{
    return availableCount(championName) > 0;
}

bool SharedUnitPool::take(const std::string& championName)
{
    return takeChampion(championName);
}

void SharedUnitPool::giveBack(const std::string& championName)
{
    returnChampion(championName, 1);
}

ShopSystem::ShopSystem(const ContentManager& content, SharedUnitPool& pool)
    : content_(&content),
      pool_(&pool)
{
}

std::int32_t ShopSystem::rerollCost()
{
    return GameConstants::ShopRerollCostGold;
}

std::int32_t ShopSystem::xpForBuy()
{
    return GameConstants::XpPerBuy;
}

std::int32_t ShopSystem::xpCost(std::int32_t xpAmount)
{
    if (xpAmount <= 0)
    {
        return 0;
    }
    return xpAmount;
}

std::int32_t ShopSystem::xpToNextLevel(int currentLevel)
{
    currentLevel = std::clamp(currentLevel, 1, GameConstants::MaxLevel);
    if (currentLevel >= GameConstants::MaxLevel)
    {
        return 0;
    }
    if (currentLevel < 0 || currentLevel >= static_cast<int>(GameConstants::XpToNextLevel.size()))
    {
        return 0;
    }
    return GameConstants::XpToNextLevel[static_cast<std::size_t>(currentLevel)];
}

int ShopSystem::rollCostForLevel(int level, Random& rng) const
{
    struct Entry { int cost; int weight; };

    level = std::clamp(level, 1, GameConstants::MaxLevel);
    const auto& weights = GameConstants::ShopCostOddsByLevel[static_cast<std::size_t>(level)];

    int total = 0;
    for (int cost = GameConstants::ShopFallbackCostMin; cost <= GameConstants::ShopFallbackCostMax; ++cost)
    {
        total += std::max(0, weights[static_cast<std::size_t>(cost)]);
    }
    const int r = rngInt(rng, total);

    int acc = 0;
    for (int cost = GameConstants::ShopFallbackCostMin; cost <= GameConstants::ShopFallbackCostMax; ++cost)
    {
        acc += std::max(0, weights[static_cast<std::size_t>(cost)]);
        if (r < acc)
        {
            return cost;
        }
    }
    return GameConstants::ShopFallbackCostMin;
}

void ShopSystem::reroll(PlayerState& player, Random& rng, bool paid)
{
    if (paid)
    {
        if (!player.spendGold(rerollCost()))
        {
            return;
        }
    }

    std::vector<ShopOffer> offers;
    offers.reserve(static_cast<std::size_t>(GameConstants::ShopOffersPerRoll));

    for (int i = 0; i < GameConstants::ShopOffersPerRoll; ++i)
    {
        std::string champ;
        int finalCost = 0;
        for (int attempt = 0; attempt < GameConstants::ShopRollAttempts; ++attempt)
        {
            const int cost = rollCostForLevel(player.level(), rng);
            champ = pool_->randomChampionByCost(cost, rng);
            if (champ.empty())
            {
                for (int fallback = GameConstants::ShopFallbackCostMin;
                     fallback <= GameConstants::ShopFallbackCostMax && champ.empty();
                     ++fallback)
                {
                    champ = pool_->randomChampionByCost(fallback, rng);
                }
            }
            if (champ.empty())
            {
                continue;
            }
            if (const ChampionDefinition* c = content_->getChampion(champ))
            {
                if (!isPlayableChampion(*c))
                {
                    champ.clear();
                    continue;
                }
                finalCost = c->cost;
                break;
            }
            champ.clear();
        }

        offers.push_back(ShopOffer{ std::move(champ), finalCost > 0 ? finalCost : 0 });
    }

    player.shopMutable() = std::move(offers);
}

bool ShopSystem::buy(PlayerState& player, std::size_t shopIndex)
{
    if (shopIndex >= player.shop().size())
    {
        return false;
    }

    const ShopOffer offer = player.shop()[shopIndex];
    if (offer.championName.empty())
    {
        return false;
    }

    const ChampionDefinition* def = content_->getChampion(offer.championName);
    if (!def)
    {
        return false;
    }

    if (!player.spendGold(def->cost))
    {
        return false;
    }

    if (!pool_->takeChampion(def->name))
    {
        player.addGold(def->cost);
        return false;
    }

    OwnedUnit u;
    u.championName = def->name;
    u.starLevel = 1;
    u.cost = def->cost;

    if (!player.addToBench(u))
    {
        pool_->returnChampion(def->name, 1);
        player.addGold(def->cost);
        return false;
    }

    player.shopMutable()[shopIndex] = ShopOffer{ "", 0 };
    return true;
}

bool ShopSystem::sellBench(PlayerState& player, std::size_t benchIndex)
{
    if (benchIndex >= player.bench().size())
    {
        return false;
    }
    const OwnedUnit sold = player.bench()[benchIndex];
    if (!player.sellBenchUnit(benchIndex))
    {
        return false;
    }
    pool_->returnChampion(sold.championName, copiesForStarLevel(sold.starLevel));
    return true;
}

bool ShopSystem::sellBoard(PlayerState& player, std::size_t boardIndex)
{
    if (boardIndex >= player.board().size())
    {
        return false;
    }
    const OwnedUnit sold = player.board()[boardIndex];
    if (!player.sellBoardUnit(boardIndex))
    {
        return false;
    }
    pool_->returnChampion(sold.championName, copiesForStarLevel(sold.starLevel));
    return true;
}
