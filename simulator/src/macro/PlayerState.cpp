#include "macro/PlayerState.hpp"
#include "constants/GameConstants.hpp"
#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <utility>

static int copiesForStarLevel(int starLevel)
{
    if (starLevel <= 1) return 1;
    if (starLevel == 2) return 3;
    return 9;
}

PlayerState::PlayerState(std::string name)
    : name_(std::move(name)),
      gold_(0),
      health_(100),
      level_(1),
      xp_(0),
      win_streak_(0),
      lose_streak_(0),
      bench_{},
      board_{},
      shop_{},
      item_bench_{} {}

const std::string& PlayerState::name() const { return name_; }
std::int32_t PlayerState::gold() const { return gold_; }
std::int32_t PlayerState::health() const { return health_; }
int PlayerState::level() const { return level_; }
std::int32_t PlayerState::xp() const { return xp_; }

bool PlayerState::canAfford(std::int32_t cost) const
{
    return cost <= 0 ? true : gold_ >= cost;
}

void PlayerState::addGold(std::int32_t amount)
{
    gold_ = std::max<std::int32_t>(0, gold_ + amount);
}

bool PlayerState::spendGold(std::int32_t amount)
{
    if (amount <= 0)
    {
        return true;
    }
    if (gold_ < amount)
    {
        return false;
    }
    gold_ -= amount;
    return true;
}

void PlayerState::takeDamage(std::int32_t amount)
{
    if (amount <= 0)
    {
        return;
    }
    health_ = std::max<std::int32_t>(0, health_ - amount);
}

void PlayerState::addXp(std::int32_t amount)
{
    xp_ = std::max<std::int32_t>(0, xp_ + amount);
}

void PlayerState::setLevel(int level)
{
    level_ = std::clamp(level, 1, GameConstants::MaxLevel);
}

int PlayerState::unitCap() const
{
    return level_;
}

std::size_t PlayerState::benchLimit() const
{
    return GameConstants::BenchLimit;
}

std::int32_t PlayerState::winStreak() const { return win_streak_; }
std::int32_t PlayerState::loseStreak() const { return lose_streak_; }

void PlayerState::recordWin()
{
    win_streak_ += 1;
    lose_streak_ = 0;
}

void PlayerState::recordLoss()
{
    lose_streak_ += 1;
    win_streak_ = 0;
}

std::int32_t PlayerState::interest() const
{
    return std::min<std::int32_t>(5, gold_ / 10);
}

const std::vector<OwnedUnit>& PlayerState::bench() const { return bench_; }
const std::vector<OwnedUnit>& PlayerState::board() const { return board_; }
std::vector<OwnedUnit>& PlayerState::benchMutable() { return bench_; }
std::vector<OwnedUnit>& PlayerState::boardMutable() { return board_; }

const std::vector<std::string>& PlayerState::itemBench() const { return item_bench_; }
std::vector<std::string>& PlayerState::itemBenchMutable() { return item_bench_; }

bool PlayerState::addToBench(OwnedUnit unit)
{
    if (bench_.size() >= benchLimit())
    {
        return false;
    }
    bench_.push_back(std::move(unit));
    applyAutoUpgrades();
    return true;
}

bool PlayerState::moveBenchToBoard(std::size_t benchIndex)
{
    if (benchIndex >= bench_.size())
    {
        return false;
    }
    if (static_cast<int>(board_.size()) >= unitCap())
    {
        return false;
    }

    board_.push_back(bench_[benchIndex]);
    bench_.erase(bench_.begin() + static_cast<std::ptrdiff_t>(benchIndex));
    applyAutoUpgrades();
    return true;
}

bool PlayerState::moveBoardToBench(std::size_t boardIndex)
{
    if (boardIndex >= board_.size())
    {
        return false;
    }
    if (bench_.size() >= GameConstants::BenchLimit)
    {
        return false;
    }

    bench_.push_back(board_[boardIndex]);
    board_.erase(board_.begin() + static_cast<std::ptrdiff_t>(boardIndex));
    applyAutoUpgrades();
    return true;
}

bool PlayerState::repositionBoardUnit(std::size_t boardIndex, const Position& formationPosition)
{
    if (boardIndex >= board_.size())
    {
        return false;
    }
    if (board_[boardIndex].hasFormation &&
        board_[boardIndex].formation.x == formationPosition.x &&
        board_[boardIndex].formation.y == formationPosition.y)
    {
        return false;
    }

    for (std::size_t i = 0; i < board_.size(); ++i)
    {
        if (i == boardIndex)
        {
            continue;
        }
        if (!board_[i].hasFormation)
        {
            continue;
        }
        if (board_[i].formation.x == formationPosition.x && board_[i].formation.y == formationPosition.y)
        {
            return false;
        }
    }

    board_[boardIndex].hasFormation = true;
    board_[boardIndex].formation = formationPosition;
    return true;
}

bool PlayerState::sellBenchUnit(std::size_t benchIndex)
{
    if (benchIndex >= bench_.size())
    {
        return false;
    }
    for (const std::string& item : bench_[benchIndex].items)
    {
        if (!item.empty())
        {
            item_bench_.push_back(item);
        }
    }
    addGold(bench_[benchIndex].cost * copiesForStarLevel(bench_[benchIndex].starLevel));
    bench_.erase(bench_.begin() + static_cast<std::ptrdiff_t>(benchIndex));
    return true;
}

bool PlayerState::sellBoardUnit(std::size_t boardIndex)
{
    if (boardIndex >= board_.size())
    {
        return false;
    }
    for (const std::string& item : board_[boardIndex].items)
    {
        if (!item.empty())
        {
            item_bench_.push_back(item);
        }
    }
    addGold(board_[boardIndex].cost * copiesForStarLevel(board_[boardIndex].starLevel));
    board_.erase(board_.begin() + static_cast<std::ptrdiff_t>(boardIndex));
    return true;
}

bool PlayerState::addItemToBench(std::string itemName)
{
    if (itemName.empty())
    {
        return false;
    }
    item_bench_.push_back(std::move(itemName));
    return true;
}

bool PlayerState::addItemToInventory(std::string itemName)
{
    return addItemToBench(std::move(itemName));
}

bool PlayerState::equipItemToBoardUnit(std::size_t boardIndex, std::size_t itemBenchIndex)
{
    if (boardIndex >= board_.size())
    {
        return false;
    }
    if (itemBenchIndex >= item_bench_.size())
    {
        return false;
    }
    if (board_[boardIndex].items.size() >= GameConstants::MaxItemsPerUnit)
    {
        return false;
    }

    std::string itemName = std::move(item_bench_[itemBenchIndex]);
    item_bench_.erase(item_bench_.begin() + static_cast<std::ptrdiff_t>(itemBenchIndex));
    if (itemName.empty())
    {
        return false;
    }

    board_[boardIndex].items.push_back(std::move(itemName));
    return true;
}

bool PlayerState::equipItem(std::size_t boardIndex, std::size_t itemIndex)
{
    return equipItemToBoardUnit(boardIndex, itemIndex);
}

bool PlayerState::unequipItemFromBoardUnit(std::size_t boardIndex, std::size_t itemIndex)
{
    if (boardIndex >= board_.size())
    {
        return false;
    }
    if (itemIndex >= board_[boardIndex].items.size())
    {
        return false;
    }

    std::string itemName = std::move(board_[boardIndex].items[itemIndex]);
    board_[boardIndex].items.erase(board_[boardIndex].items.begin() + static_cast<std::ptrdiff_t>(itemIndex));
    if (!itemName.empty())
    {
        item_bench_.push_back(std::move(itemName));
    }
    return true;
}

const std::vector<ShopOffer>& PlayerState::shop() const { return shop_; }
std::vector<ShopOffer>& PlayerState::shopMutable() { return shop_; }

void PlayerState::applyAutoUpgrades()
{
    struct Ref
    {
        bool onBoard = false;
        std::size_t index = 0;
    };

    bool changed = true;
    while (changed)
    {
        changed = false;
        std::unordered_map<std::string, std::vector<Ref>> benchRefs;
        std::unordered_map<std::string, std::vector<Ref>> boardRefs;

        for (std::size_t i = 0; i < bench_.size(); ++i)
        {
            const OwnedUnit& u = bench_[i];
            const std::string key = u.championName + "#" + std::to_string(u.starLevel);
            benchRefs[key].push_back(Ref{ false, i });
        }
        for (std::size_t i = 0; i < board_.size(); ++i)
        {
            const OwnedUnit& u = board_[i];
            const std::string key = u.championName + "#" + std::to_string(u.starLevel);
            boardRefs[key].push_back(Ref{ true, i });
        }

        std::vector<std::string> keys;
        keys.reserve(benchRefs.size() + boardRefs.size());
        for (const auto& [k, _] : benchRefs) keys.push_back(k);
        for (const auto& [k, _] : boardRefs)
        {
            if (benchRefs.find(k) == benchRefs.end())
            {
                keys.push_back(k);
            }
        }
        std::sort(keys.begin(), keys.end());

        for (const std::string& key : keys)
        {
            std::vector<Ref> refs;
            if (auto itB = boardRefs.find(key); itB != boardRefs.end())
            {
                for (const Ref& r : itB->second) refs.push_back(r);
            }
            if (auto it = benchRefs.find(key); it != benchRefs.end())
            {
                for (const Ref& r : it->second) refs.push_back(r);
            }

            if (refs.size() < 3)
            {
                continue;
            }

            const std::size_t hashPos = key.find('#');
            const std::string champName = hashPos == std::string::npos ? key : key.substr(0, hashPos);
            const int star = hashPos == std::string::npos ? 1 : std::max(1, std::stoi(key.substr(hashPos + 1)));
            if (star >= 3)
            {
                continue;
            }

            Ref primary = refs[0];
            for (const Ref& r : refs)
            {
                if (r.onBoard)
                {
                    primary = r;
                    break;
                }
            }

            std::vector<Ref> consume;
            consume.reserve(2);
            for (const Ref& r : refs)
            {
                if (consume.size() >= 2)
                {
                    break;
                }
                if (r.onBoard == primary.onBoard && r.index == primary.index)
                {
                    continue;
                }
                consume.push_back(r);
            }
            if (consume.size() < 2)
            {
                continue;
            }

            OwnedUnit* primaryUnit = primary.onBoard ? &board_[primary.index] : &bench_[primary.index];
            if (!primaryUnit || primaryUnit->championName != champName || primaryUnit->starLevel != star)
            {
                continue;
            }

            for (const Ref& r : consume)
            {
                OwnedUnit* u = r.onBoard ? &board_[r.index] : &bench_[r.index];
                if (!u)
                {
                    continue;
                }
                for (const std::string& item : u->items)
                {
                    if (!item.empty())
                    {
                        primaryUnit->items.push_back(item);
                    }
                }
            }

            primaryUnit->starLevel = star + 1;

            std::vector<std::size_t> boardErase;
            std::vector<std::size_t> benchErase;
            for (const Ref& r : consume)
            {
                if (r.onBoard) boardErase.push_back(r.index);
                else benchErase.push_back(r.index);
            }
            std::sort(boardErase.begin(), boardErase.end(), std::greater<std::size_t>());
            std::sort(benchErase.begin(), benchErase.end(), std::greater<std::size_t>());

            for (std::size_t idx : boardErase)
            {
                if (idx < board_.size())
                {
                    board_.erase(board_.begin() + static_cast<std::ptrdiff_t>(idx));
                }
            }
            for (std::size_t idx : benchErase)
            {
                if (idx < bench_.size())
                {
                    bench_.erase(bench_.begin() + static_cast<std::ptrdiff_t>(idx));
                }
            }

            changed = true;
            break;
        }
    }
}
