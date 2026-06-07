#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "core/Position.hpp"
struct ShopOffer
{
    std::string championName;
    int cost = 1;
};

struct OwnedUnit
{
    std::string championName;
    int starLevel = 1;
    int cost = 1;
    std::vector<std::string> items{};
    bool hasFormation = false;
    Position formation{ 0, 0 };
};

class PlayerState
{
public:
    explicit PlayerState(std::string name);

    const std::string& name() const;

    std::int32_t gold() const;
    std::int32_t health() const;
    int level() const;
    std::int32_t xp() const;

    bool canAfford(std::int32_t cost) const;
    void addGold(std::int32_t amount);
    bool spendGold(std::int32_t amount);

    void takeDamage(std::int32_t amount);

    void addXp(std::int32_t amount);
    void setLevel(int level);

    int unitCap() const;
    std::size_t benchLimit() const;

    std::int32_t winStreak() const;
    std::int32_t loseStreak() const;
    void recordWin();
    void recordLoss();

    std::int32_t interest() const;

    const std::vector<OwnedUnit>& bench() const;
    const std::vector<OwnedUnit>& board() const;
    std::vector<OwnedUnit>& benchMutable();
    std::vector<OwnedUnit>& boardMutable();

    const std::vector<std::string>& itemBench() const;
    std::vector<std::string>& itemBenchMutable();

    bool addToBench(OwnedUnit unit);
    bool moveBenchToBoard(std::size_t benchIndex);
    bool moveBoardToBench(std::size_t boardIndex);
    bool repositionBoardUnit(std::size_t boardIndex, const Position& formationPosition);
    bool sellBenchUnit(std::size_t benchIndex);
    bool sellBoardUnit(std::size_t boardIndex);

    bool addItemToBench(std::string itemName);
    bool addItemToInventory(std::string itemName);
    bool equipItemToBoardUnit(std::size_t boardIndex, std::size_t itemBenchIndex);
    bool equipItem(std::size_t boardIndex, std::size_t itemIndex);
    bool unequipItemFromBoardUnit(std::size_t boardIndex, std::size_t itemIndex);

    const std::vector<ShopOffer>& shop() const;
    std::vector<ShopOffer>& shopMutable();

    void applyAutoUpgrades();

private:
    std::string name_;

    std::int32_t gold_;
    std::int32_t health_;
    int level_;
    std::int32_t xp_;

    std::int32_t win_streak_;
    std::int32_t lose_streak_;

    std::vector<OwnedUnit> bench_;
    std::vector<OwnedUnit> board_;
    std::vector<ShopOffer> shop_;
    std::vector<std::string> item_bench_;
};
