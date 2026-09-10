#include "macro/LobbySimulation.hpp"
#include "ai/ScoutSystem.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "content/ChampionFilter.hpp"
#include "content/ContentManager.hpp"
#include "core/Random.hpp"
#include "macro/EconomySystem.hpp"
#include "macro/MacroSimulation.hpp"
#include "macro/PlayerState.hpp"
#include "macro/RoundSchedule.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/ShopSystem.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <sstream>
#include <streambuf>

namespace
{
constexpr int LobbyPlayerCount = 8;
constexpr int LobbyOpeningGold = 12;
constexpr int LobbyMaxOpeningBuys = 3;
constexpr int LobbyOpeningItemRewards = 2;
constexpr int LobbyPveItemRewards = 1;

std::uint32_t mixLobbySeed(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t x = a ^ (b + GameConstants::SeedMixConstant + (a << GameConstants::SeedMixShiftA) + (a >> GameConstants::SeedMixShiftB));
    x ^= (x << GameConstants::SeedScrambleShift1);
    x ^= (x >> GameConstants::SeedScrambleShift2);
    x ^= (x << GameConstants::SeedScrambleShift3);
    return x;
}

struct NullBuffer final : std::streambuf
{
    int overflow(int c) override { return c; }
};

struct LobbyEconomyLedger
{
    std::array<int, LobbyPlayerCount> expectedGold{};
    int maxObservedGold = 0;
    int maxEconomyEventsPerPlayerRound = 0;
    int totalEconomyIncome = 0;
    int totalTurnGoldDelta = 0;
    bool balanced = true;

    void initialize(const std::vector<PlayerState>& players)
    {
        for (std::size_t i = 0; i < players.size() && i < expectedGold.size(); ++i)
        {
            expectedGold[i] = static_cast<int>(players[i].gold());
            maxObservedGold = std::max(maxObservedGold, expectedGold[i]);
        }
    }

    void recordTurnDelta(std::size_t playerIndex, int beforeGold, int afterGold)
    {
        const int delta = afterGold - beforeGold;
        totalTurnGoldDelta += delta;
        expectedGold[playerIndex] += delta;
        if (expectedGold[playerIndex] != afterGold)
        {
            balanced = false;
        }
        maxObservedGold = std::max(maxObservedGold, afterGold);
    }

    void recordRoundIncome(std::size_t playerIndex,
                           PlayerState& player,
                           bool won,
                           std::array<int, LobbyPlayerCount>& economyEventsThisRound)
    {
        const int beforeGold = static_cast<int>(player.gold());
        const EconomyResult economy = EconomySystem::applyRoundEnd(player, won);
        economyEventsThisRound[playerIndex] += 1;
        totalEconomyIncome += economy.total;
        expectedGold[playerIndex] += economy.total;
        if (expectedGold[playerIndex] != player.gold() || player.gold() != beforeGold + economy.total)
        {
            balanced = false;
        }
        maxObservedGold = std::max(maxObservedGold, static_cast<int>(player.gold()));
    }

    void finishRound(const std::array<int, LobbyPlayerCount>& economyEventsThisRound)
    {
        for (int events : economyEventsThisRound)
        {
            maxEconomyEventsPerPlayerRound = std::max(maxEconomyEventsPerPlayerRound, events);
            if (events > 1)
            {
                balanced = false;
            }
        }
    }
};

bool alive(const PlayerState& p)
{
    return p.health() > 0;
}

void buyOpeningBoard(PlayerState& player, ShopSystem& shop, Random& rng)
{
    shop.reroll(player, rng, false);
    for (int bought = 0; bought < LobbyMaxOpeningBuys; ++bought)
    {
        bool didBuy = false;
        for (std::size_t i = 0; i < player.shop().size(); ++i)
        {
            const ShopOffer& offer = player.shop()[i];
            if (!offer.championName.empty() && player.canAfford(offer.cost) && player.bench().size() < player.benchLimit())
            {
                didBuy = shop.buy(player, i);
                if (didBuy)
                {
                    break;
                }
            }
        }
        if (!didBuy)
        {
            break;
        }
    }
    while (static_cast<int>(player.board().size()) < player.unitCap() && !player.bench().empty())
    {
        player.moveBenchToBoard(0);
    }
}

void takeAliveTurn(PlayerState& player,
                   SimpleMacroAI& ai,
                   ShopSystem& shop,
                   Random& rng,
                   const ContentManager& content,
                   const EnemySnapshot* enemy,
                   const SharedUnitPool* pool,
                   int stage,
                   int roundIndex,
                   std::ostream& out,
                   MacroTurnStats& stats)
{
    MacroSimulation::takeTurnForValidationAt(player, ai, shop, rng, content, enemy, pool, stage, roundIndex, out, stats);
}

std::vector<int> activeIndices(const std::vector<PlayerState>& players)
{
    std::vector<int> indices;
    indices.reserve(players.size());
    for (std::size_t i = 0; i < players.size(); ++i)
    {
        if (alive(players[i]))
        {
            indices.push_back(static_cast<int>(i));
        }
    }
    return indices;
}

std::vector<std::pair<int, int>> deterministicPairs(std::vector<int> active, int roundIndex, int& bye)
{
    bye = -1;
    if (active.empty())
    {
        return {};
    }
    const int rotateBy = roundIndex % static_cast<int>(active.size());
    std::rotate(active.begin(), active.begin() + rotateBy, active.end());

    if ((active.size() % 2u) != 0u)
    {
        bye = active.back();
        active.pop_back();
    }

    std::vector<std::pair<int, int>> pairs;
    for (std::size_t i = 0; i + 1 < active.size(); i += 2)
    {
        pairs.push_back({ active[i], active[i + 1] });
    }
    return pairs;
}

std::vector<std::string> collectLobbyCombatItemNames(const ContentManager& content)
{
    std::vector<std::string> names;
    for (const auto& [name, item] : content.items())
    {
        if (item.metadata.itemCategory != "CombatItem")
        {
            continue;
        }
        if (item.passiveStats.empty() && item.triggeredEffects.empty() && item.genericEffects.empty())
        {
            continue;
        }
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

int grantLobbyCombatItems(PlayerState& player,
                          const std::vector<std::string>& itemPool,
                          Random& rng,
                          int count)
{
    if (itemPool.empty() || count <= 0)
    {
        return 0;
    }

    int granted = 0;
    for (int i = 0; i < count; ++i)
    {
        const int itemIndex = rng.nextInt(static_cast<int>(itemPool.size()));
        if (player.addItemToBench(itemPool[static_cast<std::size_t>(itemIndex)]))
        {
            granted += 1;
        }
    }
    return granted;
}

struct LobbyItemTotals
{
    int equipped = 0;
    int bench = 0;
    int maxEquippedOnUnit = 0;
};

LobbyItemTotals countLobbyItems(const std::vector<PlayerState>& players)
{
    LobbyItemTotals totals{};
    for (const PlayerState& player : players)
    {
        totals.bench += static_cast<int>(player.itemBench().size());
        for (const OwnedUnit& unit : player.board())
        {
            const int itemCount = static_cast<int>(unit.items.size());
            totals.equipped += itemCount;
            totals.maxEquippedOnUnit = std::max(totals.maxEquippedOnUnit, itemCount);
        }
        for (const OwnedUnit& unit : player.bench())
        {
            const int itemCount = static_cast<int>(unit.items.size());
            totals.equipped += itemCount;
            totals.maxEquippedOnUnit = std::max(totals.maxEquippedOnUnit, itemCount);
        }
    }
    return totals;
}
std::string unitCollectionSummary(const std::vector<OwnedUnit>& units)
{
    int totalStars = 0;
    int totalCost = 0;
    int totalItems = 0;
    for (const OwnedUnit& unit : units)
    {
        totalStars += std::clamp(unit.starLevel, 1, 3);
        totalCost += std::max(1, unit.cost);
        totalItems += static_cast<int>(unit.items.size());
    }

    std::ostringstream ss;
    ss << "units=" << units.size()
       << ";stars=" << totalStars
       << ";cost=" << totalCost
       << ";items=" << totalItems;
    return ss.str();
}

std::string traitFeatureSummary(const PlayerState& player, const ContentManager& content)
{
    std::vector<std::pair<std::string, int>> counts;
    auto addTrait = [&](const std::string& trait)
    {
        for (auto& entry : counts)
        {
            if (entry.first == trait)
            {
                entry.second += 1;
                return;
            }
        }
        counts.push_back({ trait, 1 });
    };

    for (const OwnedUnit& unit : player.board())
    {
        const ChampionDefinition* champion = content.getChampion(unit.championName);
        if (!champion)
        {
            continue;
        }
        for (const std::string& trait : champion->traits)
        {
            if (!trait.empty())
            {
                addTrait(trait);
            }
        }
    }

    std::sort(counts.begin(), counts.end(), [](const auto& a, const auto& b)
    {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::ostringstream ss;
    ss << "traits=" << counts.size();
    const std::size_t limit = std::min<std::size_t>(3, counts.size());
    for (std::size_t i = 0; i < limit; ++i)
    {
        ss << ";" << counts[i].first << "=" << counts[i].second;
    }
    return ss.str();
}

std::string itemFeatureSummary(const PlayerState& player)
{
    int equipped = 0;
    int unitsWithItems = 0;
    int maxItems = 0;
    for (const OwnedUnit& unit : player.board())
    {
        const int count = static_cast<int>(unit.items.size());
        equipped += count;
        if (count > 0)
        {
            unitsWithItems += 1;
        }
        maxItems = std::max(maxItems, count);
    }

    std::ostringstream ss;
    ss << "bench=" << player.itemBench().size()
       << ";equipped=" << equipped
       << ";units=" << unitsWithItems
       << ";max=" << maxItems;
    return ss.str();
}

std::string shopFeatureSummary(const PlayerState& player)
{
    int offers = 0;
    int totalCost = 0;
    int affordable = 0;
    for (const ShopOffer& offer : player.shop())
    {
        if (offer.championName.empty())
        {
            continue;
        }
        offers += 1;
        totalCost += offer.cost;
        if (player.canAfford(offer.cost))
        {
            affordable += 1;
        }
    }

    std::ostringstream ss;
    ss << "offers=" << offers
       << ";cost=" << totalCost
       << ";affordable=" << affordable;
    return ss.str();
}

float placementReward(int placement)
{
    switch (placement)
    {
        case 1: return 1.00f;
        case 2: return 0.70f;
        case 3: return 0.45f;
        case 4: return 0.20f;
        case 5: return -0.10f;
        case 6: return -0.35f;
        case 7: return -0.65f;
        case 8: return -1.00f;
        default: return 0.0f;
    }
}

LobbyDecisionRecord makeDecisionRecord(std::uint32_t seed,
                                       int roundIndex,
                                       int stage,
                                       int playerId,
                                       int alivePlayers,
                                       const PlayerState& player,
                                       const ContentManager& content,
                                       const EnemySnapshot* enemy)
{
    LobbyDecisionRecord record{};
    record.gameSeed = seed;
    record.round = roundIndex;
    record.playerId = playerId;
    record.hp = player.health();
    record.gold = player.gold();
    record.level = player.level();
    record.boardSummary = unitCollectionSummary(player.board());
    record.benchSummary = unitCollectionSummary(player.bench());
    record.traitSummary = traitFeatureSummary(player, content);
    record.itemSummary = itemFeatureSummary(player);
    record.shopSummary = shopFeatureSummary(player);
    record.stateFeatures = TrainingData::encodeState(player, content, roundIndex, stage, alivePlayers, enemy);
    return record;
}
std::string inferCompStyle(const PlayerState& player, const ContentManager& content)
{
    std::vector<std::pair<std::string, int>> counts;

    auto addTrait = [&](const std::string& trait)
    {
        for (auto& entry : counts)
        {
            if (entry.first == trait)
            {
                entry.second += 1;
                return;
            }
        }
        counts.push_back({ trait, 1 });
    };

    for (const OwnedUnit& unit : player.board())
    {
        const ChampionDefinition* champion = content.getChampion(unit.championName);
        if (!champion)
        {
            continue;
        }
        for (const std::string& trait : champion->traits)
        {
            if (!trait.empty())
            {
                addTrait(trait);
            }
        }
    }

    if (counts.empty())
    {
        return "No board";
    }

    std::sort(counts.begin(), counts.end(), [](const auto& a, const auto& b)
    {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::ostringstream ss;
    const std::size_t limit = std::min<std::size_t>(2, counts.size());
    for (std::size_t i = 0; i < limit; ++i)
    {
        if (i > 0)
        {
            ss << "/";
        }
        ss << counts[i].first << "(" << counts[i].second << ")";
    }
    return ss.str();
}
bool sharedPoolCountsValid(const ContentManager& content, const SharedUnitPool& pool)
{
    for (const auto& [name, champ] : content.champions())
    {
        if (!isPlayableChampion(champ))
        {
            continue;
        }
        if (pool.availableCount(name) < 0)
        {
            return false;
        }
    }
    return true;
}

std::string summarize(const std::vector<LobbyPlayerResult>& players,
                      int roundsPlayed,
                      int actionsAfterElimination,
                      int maxFinalGold,
                      int maxObservedGold,
                      int maxEconomyEventsPerPlayerRound,
                      int totalEconomyIncome,
                      int totalTurnGoldDelta,
                      int totalItemsGranted,
                      int totalItemsEquipped,
                      int totalItemsOnBench,
                      int maxEquippedItemsOnUnit,
                      int combatTimeoutCount,
                      bool economyAccountingBalanced,
                      bool sharedPoolValid,
                      bool completed)
{
    std::vector<LobbyPlayerResult> sorted = players;
    std::sort(sorted.begin(), sorted.end(), [](const LobbyPlayerResult& a, const LobbyPlayerResult& b)
    {
        if (a.placement != b.placement) return a.placement < b.placement;
        return a.name < b.name;
    });

    std::ostringstream ss;
    ss << "rounds=" << roundsPlayed
       << "|afterElim=" << actionsAfterElimination
       << "|maxFinalGold=" << maxFinalGold
       << "|maxObservedGold=" << maxObservedGold
       << "|maxEconEvents=" << maxEconomyEventsPerPlayerRound
       << "|econIncome=" << totalEconomyIncome
       << "|turnGoldDelta=" << totalTurnGoldDelta
       << "|itemsGranted=" << totalItemsGranted
       << "|itemsEquipped=" << totalItemsEquipped
       << "|itemsBench=" << totalItemsOnBench
       << "|maxUnitItems=" << maxEquippedItemsOnUnit
       << "|combatTimeouts=" << combatTimeoutCount
       << "|accounting=" << (economyAccountingBalanced ? 1 : 0)
       << "|pool=" << (sharedPoolValid ? 1 : 0)
       << "|done=" << (completed ? 1 : 0);
    for (const LobbyPlayerResult& p : sorted)
    {
        ss << "|" << p.placement << ":" << p.name << ":" << p.health << ":" << p.gold << ":" << p.level << ":" << (p.eliminated ? 1 : 0) << ":" << p.compStyle;
    }
    return ss.str();
}
}

LobbySimulationResult LobbySimulation::simulate(const ContentManager& content,
                                                std::uint32_t seed,
                                                bool verbose,
                                                std::ostream* out,
                                                const std::vector<SimpleMacroAIConfig>* aiConfigs)
{
    NullBuffer nb;
    std::ostream nullOut(&nb);
    std::ostream& log = (verbose && out) ? *out : nullOut;

    SharedUnitPool pool(content);
    ShopSystem shop(content, pool);
    RoundSystem rounds(content, pool);
    const std::vector<std::string> lobbyItemPool = collectLobbyCombatItemNames(content);
    int totalItemsGranted = 0;

    std::vector<PlayerState> players;
    players.reserve(LobbyPlayerCount);
    std::array<Random, LobbyPlayerCount> rngs = {
        Random(seed ^ 0x1001u), Random(seed ^ 0x1002u), Random(seed ^ 0x1003u), Random(seed ^ 0x1004u),
        Random(seed ^ 0x1005u), Random(seed ^ 0x1006u), Random(seed ^ 0x1007u), Random(seed ^ 0x1008u)
    };
    std::vector<SimpleMacroAI> ais;
    ais.reserve(LobbyPlayerCount);
    for (int i = 0; i < LobbyPlayerCount; ++i)
    {
        SimpleMacroAIConfig cfg{};
        if (aiConfigs && static_cast<std::size_t>(i) < aiConfigs->size())
        {
            cfg = (*aiConfigs)[static_cast<std::size_t>(i)];
        }
        ais.emplace_back(seed ^ static_cast<std::uint32_t>(0xA001u + i), cfg);
    }

    for (int i = 0; i < LobbyPlayerCount; ++i)
    {
        std::ostringstream name;
        name << "Lobby Player " << (i + 1);
        players.emplace_back(name.str());
        players.back().addGold(MacroConstants::StartingGold + LobbyOpeningGold);
        if (i >= 2)
        {
            players.back().setLevel(2);
        }
        if (i >= 5)
        {
            players.back().setLevel(3);
        }
        buyOpeningBoard(players.back(), shop, rngs[static_cast<std::size_t>(i)]);
        totalItemsGranted += grantLobbyCombatItems(players.back(),
                                                  lobbyItemPool,
                                                  rngs[static_cast<std::size_t>(i)],
                                                  LobbyOpeningItemRewards);
    }

    LobbyEconomyLedger ledger{};
    ledger.initialize(players);

    std::array<int, LobbyPlayerCount> placements{};
    int nextPlacement = LobbyPlayerCount;
    int roundsPlayed = 0;
    int actionsAfterElimination = 0;
    int combatTimeoutCount = 0;
    std::vector<LobbyDecisionRecord> decisionRecords;

    for (int roundIndex = 0; roundIndex < MacroConstants::MaxRounds; ++roundIndex)
    {
        const std::vector<int> activeBefore = activeIndices(players);
        if (activeBefore.size() <= 1)
        {
            break;
        }

        const RoundInfo info = RoundSchedule::get(roundIndex);
        log << "\n=== LOBBY ROUND " << info.label << (info.isPve ? " PvE" : " PvP") << " ===\n";

        for (std::size_t i = 0; i < players.size(); ++i)
        {
            PlayerState& player = players[i];
            if (!alive(player))
            {
                if (!player.shop().empty())
                {
                    actionsAfterElimination += 1;
                }
                continue;
            }

            EnemySnapshot enemy{};
            const std::vector<int> currentActive = activeIndices(players);
            for (int idx : currentActive)
            {
                if (idx != static_cast<int>(i))
                {
                    enemy = ScoutSystem::snapshot(players[static_cast<std::size_t>(idx)], content);
                    break;
                }
            }

            shop.reroll(player, rngs[i], false);
            LobbyDecisionRecord decision = makeDecisionRecord(seed,
                                                              roundIndex,
                                                              info.stage,
                                                              static_cast<int>(i) + 1,
                                                              static_cast<int>(currentActive.size()),
                                                              player,
                                                              content,
                                                              &enemy);
            const PlayerState decisionPlayerSnapshot = player;
            const int beforeTurnGold = static_cast<int>(player.gold());
            MacroTurnStats stats{};
            takeAliveTurn(player,
                          ais[i],
                          shop,
                          rngs[i],
                          content,
                          &enemy,
                          &pool,
                          info.stage,
                          roundIndex,
                          log,
                          stats);
            decision.legalActionIds = stats.legalActionKeys;
            decision.chosenAction = stats.chosenActionKey.empty() ? "EndTurn" : stats.chosenActionKey;
            decision.legalActionEncodings.reserve(stats.legalActions.size());
            for (const MacroAction& action : stats.legalActions)
            {
                decision.legalActionEncodings.push_back(TrainingData::encodeAction(action, decisionPlayerSnapshot, content));
            }
            MacroAction chosenForEncoding = stats.chosenAction;
            if (stats.chosenActionKey.empty())
            {
                chosenForEncoding.type = MacroActionType::EndTurn;
            }
            decision.chosenActionEncoding = TrainingData::encodeAction(chosenForEncoding, decisionPlayerSnapshot, content);
            decisionRecords.push_back(std::move(decision));
            ledger.recordTurnDelta(i, beforeTurnGold, static_cast<int>(player.gold()));
        }

        std::vector<int> eliminatedThisRound;
        std::array<int, LobbyPlayerCount> economyEventsThisRound{};
        if (info.isPve)
        {
            for (int idx : activeBefore)
            {
                PlayerState& player = players[static_cast<std::size_t>(idx)];
                const std::uint32_t fightSeed = mixLobbySeed(seed, static_cast<std::uint32_t>(roundIndex * LobbyPlayerCount + idx));
                const RoundResult r = rounds.runPvE(player, roundIndex, fightSeed);
                if (r.combatTimedOut) combatTimeoutCount += 1;
                player.takeDamage(r.damageToA);
                ledger.recordRoundIncome(static_cast<std::size_t>(idx), player, r.playerAWon, economyEventsThisRound);
                if (alive(player))
                {
                    totalItemsGranted += grantLobbyCombatItems(player,
                                                              lobbyItemPool,
                                                              rngs[static_cast<std::size_t>(idx)],
                                                              LobbyPveItemRewards);
                }
                if (!alive(player))
                {
                    eliminatedThisRound.push_back(idx);
                }
            }
        }
        else
        {
            int bye = -1;
            const std::vector<std::pair<int, int>> pairs = deterministicPairs(activeBefore, roundIndex, bye);
            for (std::size_t pairIndex = 0; pairIndex < pairs.size(); ++pairIndex)
            {
                const int ai = pairs[pairIndex].first;
                const int bi = pairs[pairIndex].second;
                PlayerState& a = players[static_cast<std::size_t>(ai)];
                PlayerState& b = players[static_cast<std::size_t>(bi)];
                const std::uint32_t fightSeed = mixLobbySeed(seed, static_cast<std::uint32_t>(roundIndex * LobbyPlayerCount + static_cast<int>(pairIndex)));
                const RoundResult r = rounds.runPvP(a, b, roundIndex, fightSeed);
                if (r.combatTimedOut) combatTimeoutCount += 1;
                a.takeDamage(r.damageToA);
                b.takeDamage(r.damageToB);
                ledger.recordRoundIncome(static_cast<std::size_t>(ai), a, r.playerAWon, economyEventsThisRound);
                ledger.recordRoundIncome(static_cast<std::size_t>(bi), b, r.playerBWon, economyEventsThisRound);
                if (!alive(a)) eliminatedThisRound.push_back(ai);
                if (!alive(b)) eliminatedThisRound.push_back(bi);
            }
            if (bye >= 0)
            {
                PlayerState& player = players[static_cast<std::size_t>(bye)];
                ledger.recordRoundIncome(static_cast<std::size_t>(bye), player, true, economyEventsThisRound);
            }
        }

        ledger.finishRound(economyEventsThisRound);

        std::sort(eliminatedThisRound.begin(), eliminatedThisRound.end(), [&](int a, int b)
        {
            const PlayerState& pa = players[static_cast<std::size_t>(a)];
            const PlayerState& pb = players[static_cast<std::size_t>(b)];
            if (pa.health() != pb.health()) return pa.health() < pb.health();
            return a < b;
        });
        eliminatedThisRound.erase(std::unique(eliminatedThisRound.begin(), eliminatedThisRound.end()), eliminatedThisRound.end());
        for (int idx : eliminatedThisRound)
        {
            if (placements[static_cast<std::size_t>(idx)] == 0)
            {
                placements[static_cast<std::size_t>(idx)] = nextPlacement--;
                players[static_cast<std::size_t>(idx)].shopMutable().clear();
            }
        }

        roundsPlayed = roundIndex + 1;
    }

    std::vector<int> survivors = activeIndices(players);
    std::sort(survivors.begin(), survivors.end(), [&](int a, int b)
    {
        const PlayerState& pa = players[static_cast<std::size_t>(a)];
        const PlayerState& pb = players[static_cast<std::size_t>(b)];
        if (pa.health() != pb.health()) return pa.health() > pb.health();
        if (pa.level() != pb.level()) return pa.level() > pb.level();
        if (pa.gold() != pb.gold()) return pa.gold() > pb.gold();
        return a < b;
    });

    for (int idx : survivors)
    {
        if (placements[static_cast<std::size_t>(idx)] == 0)
        {
            placements[static_cast<std::size_t>(idx)] = nextPlacement--;
        }
    }

    const LobbyItemTotals itemTotals = countLobbyItems(players);

    LobbySimulationResult result{};
    result.seed = seed;
    result.initialPlayers = LobbyPlayerCount;
    result.roundsPlayed = roundsPlayed;
    result.actionsAfterElimination = actionsAfterElimination;
    result.maxObservedGold = ledger.maxObservedGold;
    result.maxEconomyEventsPerPlayerRound = ledger.maxEconomyEventsPerPlayerRound;
    result.totalEconomyIncome = ledger.totalEconomyIncome;
    result.totalTurnGoldDelta = ledger.totalTurnGoldDelta;
    result.totalItemsGranted = totalItemsGranted;
    result.totalItemsEquipped = itemTotals.equipped;
    result.totalItemsOnBench = itemTotals.bench;
    result.maxEquippedItemsOnUnit = itemTotals.maxEquippedOnUnit;
    result.combatTimeoutCount = combatTimeoutCount;
    result.economyAccountingBalanced = ledger.balanced;
    result.sharedPoolValid = sharedPoolCountsValid(content, pool);
    result.completed = !survivors.empty();
    result.players.reserve(players.size());

    for (std::size_t i = 0; i < players.size(); ++i)
    {
        LobbyPlayerResult p{};
        p.name = players[i].name();
        p.health = players[i].health();
        p.gold = players[i].gold();
        p.level = players[i].level();
        p.placement = placements[i];
        p.eliminated = p.health <= 0;
        p.compStyle = inferCompStyle(players[i], content);
        result.maxFinalGold = std::max(result.maxFinalGold, static_cast<int>(p.gold));
        if (p.placement == 1)
        {
            result.winner = p.name;
        }
        result.players.push_back(std::move(p));
    }

    for (LobbyDecisionRecord& record : decisionRecords)
    {
        const std::size_t playerIndex = record.playerId > 0 ? static_cast<std::size_t>(record.playerId - 1) : result.players.size();
        if (playerIndex < result.players.size())
        {
            record.eventualPlacement = result.players[playerIndex].placement;
            record.terminalReward = placementReward(record.eventualPlacement);
        }
    }
    result.decisionRecords = std::move(decisionRecords);

    result.summary = summarize(result.players,
                               result.roundsPlayed,
                               result.actionsAfterElimination,
                               result.maxFinalGold,
                               result.maxObservedGold,
                               result.maxEconomyEventsPerPlayerRound,
                               result.totalEconomyIncome,
                               result.totalTurnGoldDelta,
                               result.totalItemsGranted,
                               result.totalItemsEquipped,
                               result.totalItemsOnBench,
                               result.maxEquippedItemsOnUnit,
                               result.combatTimeoutCount,
                               result.economyAccountingBalanced,
                               result.sharedPoolValid,
                               result.completed);
    return result;
}

int LobbySimulation::run(const ContentManager& content, std::uint32_t seed, std::ostream& out)
{
    const LobbySimulationResult result = simulate(content, seed, true, &out);
    out << "\n=== LOBBY RESULT ===\n";
    out << "Seed: " << result.seed << "\n";
    out << "Rounds played: " << result.roundsPlayed << "\n";
    out << "Winner: " << result.winner << "\n";
    std::vector<LobbyPlayerResult> players = result.players;
    std::sort(players.begin(), players.end(), [](const LobbyPlayerResult& a, const LobbyPlayerResult& b)
    {
        return a.placement < b.placement;
    });
    for (const LobbyPlayerResult& p : players)
    {
        out << "#" << p.placement << " " << p.name
            << " HP=" << p.health
            << " Gold=" << p.gold
            << " Level=" << p.level
            << " Comp=" << p.compStyle
            << (p.eliminated ? " eliminated" : " alive")
            << "\n";
    }
    out << "Economy accounting: " << (result.economyAccountingBalanced ? "balanced" : "failed")
        << " | maxFinalGold=" << result.maxFinalGold
        << " maxObservedGold=" << result.maxObservedGold
        << " maxEconomyEventsPerPlayerRound=" << result.maxEconomyEventsPerPlayerRound
        << " totalEconomyIncome=" << result.totalEconomyIncome
        << " totalTurnGoldDelta=" << result.totalTurnGoldDelta
        << " combatTimeoutCount=" << result.combatTimeoutCount
        << "\n";
    out << "Items: granted=" << result.totalItemsGranted
        << " equipped=" << result.totalItemsEquipped
        << " bench=" << result.totalItemsOnBench
        << " maxEquippedOnUnit=" << result.maxEquippedItemsOnUnit
        << "\n";
    out << "Shared pool valid: " << (result.sharedPoolValid ? "yes" : "no") << "\n";
    return result.completed && result.sharedPoolValid && result.economyAccountingBalanced ? 0 : 1;
}





