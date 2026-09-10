#include "macro/TrainingData.hpp"

#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/ScoutSystem.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include "constants/GameConstants.hpp"
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace
{
constexpr int FeatureSchemaVersionValue = 2;
constexpr int ActionSchemaVersionValue = 1;
constexpr int InvalidIndex = -1;
constexpr int MissingContentId = 0;
constexpr float PercentScale = 100.0f;

int actionTypeId(MacroActionType type)
{
    switch (type)
    {
        case MacroActionType::EndTurn: return 0;
        case MacroActionType::BuyUnit: return 1;
        case MacroActionType::SellUnit: return 2;
        case MacroActionType::RerollShop: return 3;
        case MacroActionType::BuyXp: return 4;
        case MacroActionType::MoveBenchToBoard: return 5;
        case MacroActionType::MoveBoardToBench: return 6;
        case MacroActionType::RepositionUnit: return 7;
        case MacroActionType::EquipItem: return 8;
    }
    return 0;
}

std::vector<std::string> sortedChampionNames(const ContentManager& content)
{
    std::vector<std::string> names;
    names.reserve(content.champions().size());
    for (const auto& [name, _] : content.champions())
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> sortedItemNames(const ContentManager& content)
{
    std::vector<std::string> names;
    names.reserve(content.items().size());
    for (const auto& [name, _] : content.items())
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

int stableChampionId(const ContentManager& content, const std::string& name)
{
    if (name.empty())
    {
        return MissingContentId;
    }

    const std::vector<std::string> names = sortedChampionNames(content);
    const auto it = std::lower_bound(names.begin(), names.end(), name);
    if (it == names.end() || *it != name)
    {
        return MissingContentId;
    }
    return static_cast<int>(std::distance(names.begin(), it)) + 1;
}

int stableItemId(const ContentManager& content, const std::string& name)
{
    if (name.empty())
    {
        return MissingContentId;
    }

    const std::vector<std::string> names = sortedItemNames(content);
    const auto it = std::lower_bound(names.begin(), names.end(), name);
    if (it == names.end() || *it != name)
    {
        return MissingContentId;
    }
    return static_cast<int>(std::distance(names.begin(), it)) + 1;
}

int equippedItemCount(const PlayerState& player)
{
    int total = 0;
    for (const OwnedUnit& unit : player.board())
    {
        total += static_cast<int>(unit.items.size());
    }
    return total;
}

int itemCategoryCount(const PlayerState& player, const ContentManager& content, const char* category)
{
    int total = 0;
    auto countItem = [&](const std::string& itemName)
    {
        const Item* item = content.getItem(itemName);
        if (item && item->metadata.itemCategory == category)
        {
            total += 1;
        }
    };

    for (const OwnedUnit& unit : player.board())
    {
        for (const std::string& itemName : unit.items)
        {
            countItem(itemName);
        }
    }
    for (const std::string& itemName : player.itemBench())
    {
        countItem(itemName);
    }
    return total;
}

struct UnitTotals
{
    int count = 0;
    int totalCost = 0;
    int totalStars = 0;
    int totalHp = 0;
    int totalAd = 0;
    int totalArmor = 0;
    int totalMr = 0;
    float totalAp = 0.0f;
    float totalAs = 0.0f;
    int duplicatePairs = 0;
};

UnitTotals summarizeUnits(const std::vector<OwnedUnit>& units, const ContentManager& content)
{
    UnitTotals totals{};
    std::unordered_map<std::string, int> copies;
    for (const OwnedUnit& unit : units)
    {
        const ChampionDefinition* def = content.getChampion(unit.championName);
        totals.count += 1;
        totals.totalCost += unit.cost;
        totals.totalStars += unit.starLevel;
        copies[unit.championName] += 1;
        if (!def)
        {
            continue;
        }
        totals.totalHp += def->hp;
        totals.totalAd += def->ad;
        totals.totalArmor += def->armor;
        totals.totalMr += def->magicResist;
        totals.totalAp += def->abilityPower;
        totals.totalAs += def->attackSpeed;
    }
    for (const auto& [_, count] : copies)
    {
        totals.duplicatePairs += count / GameConstants::CopiesPerTwoStar;
    }
    return totals;
}

struct ShopTotals
{
    int offers = 0;
    int totalCost = 0;
    int affordable = 0;
    int duplicateSignal = 0;
};

ShopTotals summarizeShop(const PlayerState& player)
{
    ShopTotals totals{};
    std::unordered_map<std::string, int> ownedCounts;
    for (const OwnedUnit& unit : player.board())
    {
        ownedCounts[unit.championName] += 1;
    }
    for (const OwnedUnit& unit : player.bench())
    {
        ownedCounts[unit.championName] += 1;
    }

    for (const ShopOffer& offer : player.shop())
    {
        if (offer.championName.empty())
        {
            continue;
        }
        totals.offers += 1;
        totals.totalCost += offer.cost;
        if (player.canAfford(offer.cost))
        {
            totals.affordable += 1;
        }
        if (ownedCounts.find(offer.championName) != ownedCounts.end())
        {
            totals.duplicateSignal += 1;
        }
    }
    return totals;
}

std::pair<int, int> traitCounts(const PlayerState& player, const ContentManager& content)
{
    std::unordered_map<std::string, int> counts;
    for (const OwnedUnit& unit : player.board())
    {
        const ChampionDefinition* champion = content.getChampion(unit.championName);
        if (!champion)
        {
            continue;
        }
        for (const std::string& trait : champion->traits)
        {
            counts[trait] += 1;
        }
    }

    int active = 0;
    int minNextDistance = 0;
    for (const auto& [traitName, count] : counts)
    {
        const TraitDefinition* trait = content.getTrait(traitName);
        if (!trait)
        {
            continue;
        }
        int localNextDistance = 0;
        for (int breakpoint : trait->trait.breakpoints)
        {
            if (count >= breakpoint)
            {
                active += 1;
            }
            else
            {
                localNextDistance = breakpoint - count;
                break;
            }
        }
        if (localNextDistance > 0 && (minNextDistance == 0 || localNextDistance < minNextDistance))
        {
            minNextDistance = localNextDistance;
        }
    }

    return { active, minNextDistance };
}


void appendActionFeature(std::vector<float>& out, const ActionEncoding& action)
{
    out.push_back(static_cast<float>(action.actionType));
    out.push_back(static_cast<float>(action.shopIndex));
    out.push_back(static_cast<float>(action.boardIndex));
    out.push_back(static_cast<float>(action.benchIndex));
    out.push_back(static_cast<float>(action.itemIndex));
    out.push_back(static_cast<float>(action.targetX));
    out.push_back(static_cast<float>(action.targetY));
    out.push_back(static_cast<float>(action.unitContentId));
    out.push_back(static_cast<float>(action.auxiliaryId));
    out.push_back(static_cast<float>(action.goldCost));
}
const OwnedUnit* referencedUnit(const MacroAction& action, const PlayerState& player)
{
    if (action.boardIndex >= 0 && static_cast<std::size_t>(action.boardIndex) < player.board().size())
    {
        return &player.board()[static_cast<std::size_t>(action.boardIndex)];
    }
    if (action.benchIndex >= 0 && static_cast<std::size_t>(action.benchIndex) < player.bench().size())
    {
        return &player.bench()[static_cast<std::size_t>(action.benchIndex)];
    }
    if (action.shopIndex >= 0 && static_cast<std::size_t>(action.shopIndex) < player.shop().size())
    {
        return nullptr;
    }
    return nullptr;
}
}

namespace TrainingData
{
int featureSchemaVersion()
{
    return FeatureSchemaVersionValue;
}

int actionSchemaVersion()
{
    return ActionSchemaVersionValue;
}

const std::vector<std::string>& stateFeatureNames()
{
    static const std::vector<std::string> names = {
        "roundIndex",
        "stage",
        "hp",
        "gold",
        "level",
        "xp",
        "winStreak",
        "loseStreak",
        "interest",
        "alivePlayers",
        "boardCount",
        "benchCount",
        "itemBenchCount",
        "equippedItemCount",
        "boardTotalCost",
        "boardAverageStar",
        "boardTotalHp",
        "boardTotalAd",
        "boardTotalAp",
        "boardTotalArmor",
        "boardTotalMr",
        "boardTotalAttackSpeedTimes100",
        "benchTotalCost",
        "benchDuplicateGroups",
        "activeTraitBreakpoints",
        "nearestTraitBreakpointDistance",
        "shopOffers",
        "shopTotalCost",
        "shopAffordableOffers",
        "shopDuplicateSignals",
        "boardStrength",
        "traitStrength",
        "upgradePotential",
        "combatItemCount",
        "supportItemCount",
        "artifactItemCount",
        "radiantItemCount",
        "enemyBoardStrength",
        "enemyHp",
        "enemyLevel",
        "enemyItemStrength"
    };
    return names;
}

StateFeatures encodeState(const PlayerState& player,
                          const ContentManager& content,
                          int roundIndex,
                          int stage,
                          int alivePlayers,
                          const EnemySnapshot* enemy)
{
    StateFeatures features{};
    features.schemaVersion = FeatureSchemaVersionValue;

    const UnitTotals board = summarizeUnits(player.board(), content);
    const UnitTotals bench = summarizeUnits(player.bench(), content);
    const ShopTotals shop = summarizeShop(player);
    const auto [activeTraits, nextTraitDistance] = traitCounts(player, content);
    const BoardScore boardScore = BoardStrengthEvaluator::evaluate(player, content);
    const TraitSynergyScore traitScore = TraitSynergyEvaluator::evaluate(player, content);
    const UpgradePotentialScore upgradeScore = UpgradePotentialEvaluator::evaluate(player, content);

    const float averageStar = board.count > 0
        ? static_cast<float>(board.totalStars) / static_cast<float>(board.count)
        : 0.0f;

    features.values = {
        static_cast<float>(roundIndex),
        static_cast<float>(stage),
        static_cast<float>(player.health()),
        static_cast<float>(player.gold()),
        static_cast<float>(player.level()),
        static_cast<float>(player.xp()),
        static_cast<float>(player.winStreak()),
        static_cast<float>(player.loseStreak()),
        static_cast<float>(player.interest()),
        static_cast<float>(alivePlayers),
        static_cast<float>(board.count),
        static_cast<float>(bench.count),
        static_cast<float>(player.itemBench().size()),
        static_cast<float>(equippedItemCount(player)),
        static_cast<float>(board.totalCost),
        averageStar,
        static_cast<float>(board.totalHp),
        static_cast<float>(board.totalAd),
        board.totalAp,
        static_cast<float>(board.totalArmor),
        static_cast<float>(board.totalMr),
        board.totalAs * PercentScale,
        static_cast<float>(bench.totalCost),
        static_cast<float>(bench.duplicatePairs),
        static_cast<float>(activeTraits),
        static_cast<float>(nextTraitDistance),
        static_cast<float>(shop.offers),
        static_cast<float>(shop.totalCost),
        static_cast<float>(shop.affordable),
        static_cast<float>(shop.duplicateSignal),
        boardScore.total,
        traitScore.total,
        upgradeScore.total,
        static_cast<float>(itemCategoryCount(player, content, "CombatItem")),
        static_cast<float>(itemCategoryCount(player, content, "SupportItem")),
        static_cast<float>(itemCategoryCount(player, content, "Artifact")),
        static_cast<float>(itemCategoryCount(player, content, "RadiantItem")),
        enemy ? enemy->boardStrength : 0.0f,
        enemy ? static_cast<float>(enemy->hp) : 0.0f,
        enemy ? static_cast<float>(enemy->level) : 0.0f,
        enemy ? enemy->itemStrength : 0.0f
    };

    return features;
}


std::vector<float> encodeStateActionFeatures(const StateFeatures& state,
                                             const ActionEncoding& action)
{
    std::vector<float> actionValues;
    actionValues.reserve(10);
    appendActionFeature(actionValues, action);

    std::vector<float> out;
    out.reserve(state.values.size() + actionValues.size() + state.values.size() * actionValues.size());
    for (float value : state.values)
    {
        out.push_back(value / 100.0f);
    }
    for (float value : actionValues)
    {
        out.push_back(value / 100.0f);
    }
    for (float stateValue : state.values)
    {
        const float s = stateValue / 100.0f;
        for (float actionValue : actionValues)
        {
            out.push_back(s * (actionValue / 100.0f));
        }
    }
    return out;
}
ActionEncoding encodeAction(const MacroAction& action,
                            const PlayerState& player,
                            const ContentManager& content)
{
    ActionEncoding encoded{};
    encoded.schemaVersion = ActionSchemaVersionValue;
    encoded.actionType = actionTypeId(action.type);
    encoded.shopIndex = action.shopIndex;
    encoded.boardIndex = action.boardIndex;
    encoded.benchIndex = action.benchIndex;
    encoded.itemIndex = action.itemIndex;
    encoded.targetX = action.targetPosition.x;
    encoded.targetY = action.targetPosition.y;
    encoded.goldCost = action.goldCost;

    if (action.type == MacroActionType::BuyUnit &&
        action.shopIndex >= 0 &&
        static_cast<std::size_t>(action.shopIndex) < player.shop().size())
    {
        encoded.unitContentId = stableChampionId(content, player.shop()[static_cast<std::size_t>(action.shopIndex)].championName);
    }
    else if (const OwnedUnit* unit = referencedUnit(action, player))
    {
        encoded.unitContentId = stableChampionId(content, unit->championName);
    }

    if (action.type == MacroActionType::EquipItem &&
        action.itemIndex >= 0 &&
        static_cast<std::size_t>(action.itemIndex) < player.itemBench().size())
    {
        encoded.auxiliaryId = stableItemId(content, player.itemBench()[static_cast<std::size_t>(action.itemIndex)]);
    }

    if (action.type != MacroActionType::RepositionUnit)
    {
        encoded.targetX = InvalidIndex;
        encoded.targetY = InvalidIndex;
    }

    return encoded;
}
}

