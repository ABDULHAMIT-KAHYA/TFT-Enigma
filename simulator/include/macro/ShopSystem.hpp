#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "core/Random.hpp"
class SharedUnitPool
{
public:
    explicit SharedUnitPool(const ContentManager& content);

    bool takeChampion(const std::string& championName);
    void returnChampion(const std::string& championName, int copies = 1);
    int availableCount(const std::string& championName) const;
    std::string randomChampionByCost(int cost, Random& rng) const;

    bool canTake(const std::string& championName) const;
    bool take(const std::string& championName);
    void giveBack(const std::string& championName);

private:
    std::unordered_map<std::string, int> remaining_;
    std::unordered_map<std::string, int> cost_;
    std::unordered_map<int, std::vector<std::string>> names_by_cost_;
};

class ShopSystem
{
public:
    ShopSystem(const ContentManager& content, SharedUnitPool& pool);

    void reroll(PlayerState& player, Random& rng, bool paid);
    bool buy(PlayerState& player, std::size_t shopIndex);
    bool sellBench(PlayerState& player, std::size_t benchIndex);
    bool sellBoard(PlayerState& player, std::size_t boardIndex);

    static std::int32_t xpCost(std::int32_t xpAmount);
    static std::int32_t rerollCost();
    static std::int32_t xpForBuy();
    static std::int32_t xpToNextLevel(int currentLevel);

private:
    int rollCostForLevel(int level, Random& rng) const;

    const ContentManager* content_;
    SharedUnitPool* pool_;
};
