#include "ai/RolloutPlanner.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/FutureStateEvaluator.hpp"
#include "ai/RolloutPolicy.hpp"
#include "ai/ScoutSystem.hpp"
#include "ai/StateCloner.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "macro/EconomySystem.hpp"
#include "macro/LegalActionGenerator.hpp"
#include "macro/MacroExecutor.hpp"
#include "macro/RoundSchedule.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include "ai/TraitSynergyEvaluator.hpp"




#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <ostream>
#include <sstream>
#include <streambuf>
namespace
{
    struct NullBuffer final : std::streambuf
    {
        int overflow(int c) override { return c; }
    };

    static std::uint32_t mixSeed(std::uint32_t a, std::uint32_t b)
    {
        std::uint32_t x = a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
        x ^= (x >> 16);
        x *= 0x7feb352du;
        x ^= (x >> 15);
        x *= 0x846ca68bu;
        x ^= (x >> 16);
        return x;
    }

    static std::uint64_t fnv1a64(const void* data, std::size_t size)
    {
        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        std::uint64_t h = 1469598103934665603ull;
        for (std::size_t i = 0; i < size; ++i)
        {
            h ^= static_cast<std::uint64_t>(p[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    static void hashMix(std::uint64_t& h, std::uint64_t v)
    {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    static void hashString(std::uint64_t& h, const std::string& s)
    {
        const std::uint64_t sh = fnv1a64(s.data(), s.size());
        hashMix(h, sh);
        hashMix(h, static_cast<std::uint64_t>(s.size()));
    }

    static std::uint64_t hashPlayerState(const PlayerState& p)
    {
        std::uint64_t h = 0xcbf29ce484222325ull;

        hashMix(h, static_cast<std::uint64_t>(std::max<std::int32_t>(0, p.health())));
        hashMix(h, static_cast<std::uint64_t>(std::max<std::int32_t>(0, p.gold())));
        hashMix(h, static_cast<std::uint64_t>(std::max(1, p.level())));
        hashMix(h, static_cast<std::uint64_t>(std::max<std::int32_t>(0, p.xp())));
        hashMix(h, static_cast<std::uint64_t>(std::max<std::int32_t>(0, p.winStreak())));
        hashMix(h, static_cast<std::uint64_t>(std::max<std::int32_t>(0, p.loseStreak())));

        hashMix(h, static_cast<std::uint64_t>(p.board().size()));
        for (const OwnedUnit& u : p.board())
        {
            hashString(h, u.championName);
            hashMix(h, static_cast<std::uint64_t>(u.starLevel));
            hashMix(h, static_cast<std::uint64_t>(u.cost));
            hashMix(h, static_cast<std::uint64_t>(u.hasFormation ? 1 : 0));
            hashMix(h, static_cast<std::uint64_t>(u.formation.x));
            hashMix(h, static_cast<std::uint64_t>(u.formation.y));
            hashMix(h, static_cast<std::uint64_t>(u.items.size()));
            for (const std::string& it : u.items)
            {
                hashString(h, it);
            }
        }

        hashMix(h, static_cast<std::uint64_t>(p.bench().size()));
        for (const OwnedUnit& u : p.bench())
        {
            hashString(h, u.championName);
            hashMix(h, static_cast<std::uint64_t>(u.starLevel));
            hashMix(h, static_cast<std::uint64_t>(u.cost));
            hashMix(h, static_cast<std::uint64_t>(u.hasFormation ? 1 : 0));
            hashMix(h, static_cast<std::uint64_t>(u.formation.x));
            hashMix(h, static_cast<std::uint64_t>(u.formation.y));
            hashMix(h, static_cast<std::uint64_t>(u.items.size()));
            for (const std::string& it : u.items)
            {
                hashString(h, it);
            }
        }

        hashMix(h, static_cast<std::uint64_t>(p.itemBench().size()));
        for (const std::string& it : p.itemBench())
        {
            hashString(h, it);
        }

        hashMix(h, static_cast<std::uint64_t>(p.shop().size()));
        for (const ShopOffer& o : p.shop())
        {
            hashString(h, o.championName);
            hashMix(h, static_cast<std::uint64_t>(o.cost));
        }

        return h;
    }

    static void ensureBoardFilled(PlayerState& player)
    {
        while (static_cast<int>(player.board().size()) < player.unitCap() && !player.bench().empty())
        {
            player.moveBenchToBoard(0);
        }
        while (static_cast<int>(player.board().size()) > player.unitCap())
        {
            player.moveBoardToBench(player.board().size() - 1);
        }
    }

    static void resolveFormationOverlaps(PlayerState& player)
    {
        bool occupied[GameConstants::FormationWidth * GameConstants::FormationHeight] = {};
        for (OwnedUnit& u : player.boardMutable())
        {
            if (!u.hasFormation)
            {
                continue;
            }
            const int x = std::clamp(u.formation.x, 0, GameConstants::FormationWidth - 1);
            const int y = std::clamp(u.formation.y, 0, GameConstants::FormationHeight - 1);
            const int idx = y * GameConstants::FormationWidth + x;
            if (occupied[idx])
            {
                u.hasFormation = false;
                u.formation = Position{ 0, 0 };
                continue;
            }
            u.formation = Position{ x, y };
            occupied[idx] = true;
        }
    }

    struct UnitKey
    {
        std::string name;
        int star = 1;
        int cost = 1;
        int items = 0;
    };

    static UnitKey keyOf(const OwnedUnit& u)
    {
        UnitKey k{};
        k.name = u.championName;
        k.star = std::clamp(u.starLevel, 1, 3);
        k.cost = std::max(1, u.cost);
        k.items = static_cast<int>(u.items.size());
        return k;
    }

    static std::string keyString(const UnitKey& k)
    {
        std::ostringstream ss;
        ss << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
        return ss.str();
    }

    static std::string actionKeyForRollout(const PlayerState& player, const MacroAction& a)
    {
        std::ostringstream ss;
        ss << static_cast<int>(a.type) << "|";
        if (a.type == MacroActionType::BuyUnit && a.shopIndex >= 0 &&
            static_cast<std::size_t>(a.shopIndex) < player.shop().size())
        {
            const ShopOffer& o = player.shop()[static_cast<std::size_t>(a.shopIndex)];
            ss << "BUY|" << a.shopIndex << "|" << o.championName;
            return ss.str();
        }
        if (a.type == MacroActionType::SellUnit)
        {
            if (a.benchIndex >= 0 && static_cast<std::size_t>(a.benchIndex) < player.bench().size())
            {
                const UnitKey k = keyOf(player.bench()[static_cast<std::size_t>(a.benchIndex)]);
                ss << "SELLB|" << k.name << "|" << k.star;
                return ss.str();
            }
            if (a.boardIndex >= 0 && static_cast<std::size_t>(a.boardIndex) < player.board().size())
            {
                const UnitKey k = keyOf(player.board()[static_cast<std::size_t>(a.boardIndex)]);
                ss << "SELLF|" << k.name << "|" << k.star;
                return ss.str();
            }
        }
        if (a.type == MacroActionType::RerollShop)
        {
            ss << "RR";
            return ss.str();
        }
        if (a.type == MacroActionType::BuyXp)
        {
            ss << "XP|" << player.level() << "|" << player.xp();
            return ss.str();
        }
        if (a.type == MacroActionType::MoveBenchToBoard)
        {
            if (a.benchIndex >= 0 && static_cast<std::size_t>(a.benchIndex) < player.bench().size())
            {
                const UnitKey k = keyOf(player.bench()[static_cast<std::size_t>(a.benchIndex)]);
                ss << "B2F|" << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
                return ss.str();
            }
        }
        if (a.type == MacroActionType::MoveBoardToBench)
        {
            if (a.boardIndex >= 0 && static_cast<std::size_t>(a.boardIndex) < player.board().size())
            {
                const UnitKey k = keyOf(player.board()[static_cast<std::size_t>(a.boardIndex)]);
                ss << "F2B|" << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
                return ss.str();
            }
        }
        if (a.type == MacroActionType::RepositionUnit)
        {
            ss << "POS|" << a.boardIndex << "|" << a.targetPosition.x << "|" << a.targetPosition.y;
            return ss.str();
        }
        if (a.type == MacroActionType::EquipItem)
        {
            ss << "ITEM|" << a.boardIndex << "|" << a.itemIndex;
            return ss.str();
        }
        if (a.type == MacroActionType::EndTurn)
        {
            ss << "END";
            return ss.str();
        }
        return "";
    }

    static void simulateTurn(PlayerState& player,
                             ShopSystem& shop,
                             Random& rng,
                             const ContentManager& content,
                             const EnemySnapshot* enemy,
                             const SharedUnitPool* pool,
                             int stage,
                             int roundIndex,
                             const RolloutPolicy& policy,
                             const MacroAction* forcedFirstAction,
                             int maxActions,
                             std::ostream& out)
    {
        auto boardOnly = [](const BoardScore& s) -> float
        {
            return s.unitPower + s.traitPower + s.itemPower + s.frontlinePower + s.carryPower;
        };

        auto strongerReplacementExists = [&](const OwnedUnit& soldUnit) -> bool
        {
            const float sold = UnitValueEvaluator::evaluate(soldUnit, content, &player).total;
            float best = 0.0f;
            for (const OwnedUnit& b : player.bench())
            {
                if (b.championName == soldUnit.championName && b.starLevel == soldUnit.starLevel && b.cost == soldUnit.cost &&
                    static_cast<int>(b.items.size()) == static_cast<int>(soldUnit.items.size()))
                {
                    continue;
                }
                best = std::max(best, UnitValueEvaluator::evaluate(b, content, &player).total);
            }
            return best >= sold * MacroConstants::StrongerReplacementMinFactor ||
                   (best - sold) >= MacroConstants::StrongerReplacementMinAbs;
        };

        auto isTransaction = [](MacroActionType t) -> bool
        {
            return t == MacroActionType::BuyUnit ||
                   t == MacroActionType::SellUnit ||
                   t == MacroActionType::MoveBenchToBoard ||
                   t == MacroActionType::MoveBoardToBench;
        };

        std::deque<std::pair<MacroActionType, std::string>> recentMoves;
        std::unordered_map<std::string, int> actionCounts;
        std::unordered_set<std::string> boughtNames;
        std::unordered_set<std::string> soldNames;
        std::unordered_set<std::string> movedToBoardKeys;
        int transactionsExecuted = 0;
        int sellsExecuted = 0;
        int boardSellsExecuted = 0;
        int moveActionsExecuted = 0;
        int rerollsExecuted = 0;
        int xpBuysExecuted = 0;
        int repositionActionsExecuted = 0;

        struct TurnMetrics
        {
            float board = 0.0f;
            float upgrades = 0.0f;
            float traits = 0.0f;
            int interestTier = 0;
        };

        auto metricsOf = [&](const PlayerState& p, const SharedUnitPool* poolPtr) -> TurnMetrics
        {
            TurnMetrics m{};
            const BoardScore bs = BoardStrengthEvaluator::evaluate(p, content);
            m.board = boardOnly(bs);
            m.upgrades = UpgradePotentialEvaluator::evaluate(p, content, poolPtr).total;
            m.traits = TraitSynergyEvaluator::evaluate(p, content).total;
            m.interestTier = std::clamp(static_cast<int>(std::max<std::int32_t>(0, p.gold()) / MacroConstants::InterestStepGold), 0, MacroConstants::MaxInterest);
            return m;
        };

        auto improved = [&](const TurnMetrics& after, const TurnMetrics& before) -> bool
        {
            if (after.board > before.board + 1.0f) return true;
            if (after.upgrades > before.upgrades + 1.0f) return true;
            if (after.traits > before.traits + 1.0f) return true;
            if (after.interestTier > before.interestTier) return true;
            return false;
        };

        TurnMetrics lastMetrics = metricsOf(player, pool);
        int staleActions = 0;

        maxActions = std::clamp(maxActions, 1, MacroConstants::MaxActionsPerTurn);
        for (int step = 0; step < maxActions; ++step)
        {
            const std::vector<MacroAction> legal = LegalActionGenerator::generate(player);
            std::vector<MacroAction> filtered;
            filtered.reserve(legal.size());

            const float boardBefore = boardOnly(BoardStrengthEvaluator::evaluate(player, content));
            const float maxSellDrop = std::max(MacroConstants::MinSellBoardStrengthDropAbs, boardBefore * MacroConstants::MaxSellBoardStrengthDropPct);
            const bool benchFull = player.bench().size() >= player.benchLimit();

            for (const MacroAction& a : legal)
            {
                if (isTransaction(a.type) && transactionsExecuted >= MacroConstants::MaxTransactionsPerTurn)
                {
                    continue;
                }
                if (a.type == MacroActionType::SellUnit && sellsExecuted >= MacroConstants::MaxSellsPerTurn)
                {
                    continue;
                }
                if (a.type == MacroActionType::SellUnit && a.boardIndex >= 0 && boardSellsExecuted >= MacroConstants::MaxBoardSellsPerTurn)
                {
                    continue;
                }
                if ((a.type == MacroActionType::MoveBenchToBoard || a.type == MacroActionType::MoveBoardToBench) &&
                    moveActionsExecuted >= MacroConstants::MaxMoveActionsSoftCap)
                {
                    continue;
                }
                if (a.type == MacroActionType::RerollShop && rerollsExecuted >= MacroConstants::MaxRerollsPerTurn)
                {
                    continue;
                }
                if (a.type == MacroActionType::BuyXp && xpBuysExecuted >= MacroConstants::MaxXpBuysPerTurn)
                {
                    continue;
                }
                if (a.type == MacroActionType::RepositionUnit && repositionActionsExecuted >= MacroConstants::MaxRepositionActionsPerTurn)
                {
                    continue;
                }

                const std::string k = actionKeyForRollout(player, a);
                if (!k.empty())
                {
                    const auto it = actionCounts.find(k);
                    if (a.type == MacroActionType::RepositionUnit)
                    {
                        if (it != actionCounts.end() && it->second >= 1)
                        {
                            continue;
                        }
                    }
                    else if (it != actionCounts.end() && it->second >= MacroConstants::MaxRepeatedActionKey)
                    {
                        continue;
                    }
                }

                if (a.type == MacroActionType::BuyUnit &&
                    a.shopIndex >= 0 &&
                    static_cast<std::size_t>(a.shopIndex) < player.shop().size())
                {
                    const std::string& name = player.shop()[static_cast<std::size_t>(a.shopIndex)].championName;
                    if (!name.empty() && soldNames.find(name) != soldNames.end())
                    {
                        continue;
                    }
                }

                if (a.type == MacroActionType::SellUnit)
                {
                    bool isBoard = false;
                    OwnedUnit sold{};
                    UnitKey soldK{};
                    if (a.benchIndex >= 0 && static_cast<std::size_t>(a.benchIndex) < player.bench().size())
                    {
                        sold = player.bench()[static_cast<std::size_t>(a.benchIndex)];
                        soldK = keyOf(sold);
                    }
                    else if (a.boardIndex >= 0 && static_cast<std::size_t>(a.boardIndex) < player.board().size())
                    {
                        isBoard = true;
                        sold = player.board()[static_cast<std::size_t>(a.boardIndex)];
                        soldK = keyOf(sold);
                    }

                    if (!sold.championName.empty())
                    {
                        if (boughtNames.find(sold.championName) != boughtNames.end())
                        {
                            if (!benchFull && !(isBoard && strongerReplacementExists(sold)))
                            {
                                continue;
                            }
                        }
                        if (isBoard && movedToBoardKeys.find(keyString(soldK)) != movedToBoardKeys.end())
                        {
                            if (!benchFull && !strongerReplacementExists(sold))
                            {
                                continue;
                            }
                        }
                        if (isBoard)
                        {
                            PlayerState tmp = player;
                            (void)tmp.sellBoardUnit(static_cast<std::size_t>(a.boardIndex));
                            const float after = boardOnly(BoardStrengthEvaluator::evaluate(tmp, content));
                            const float drop = boardBefore - after;
                            if (drop > maxSellDrop)
                            {
                                continue;
                            }
                        }
                    }
                }

                if (a.type == MacroActionType::BuyUnit &&
                    a.shopIndex >= 0 &&
                    static_cast<std::size_t>(a.shopIndex) < player.shop().size())
                {
                    const std::string& name = player.shop()[static_cast<std::size_t>(a.shopIndex)].championName;
                    if (!name.empty() && soldNames.find(name) != soldNames.end())
                    {
                        continue;
                    }

                    const ChampionDefinition* def = content.getChampion(name);
                    OwnedUnit cand{};
                    cand.championName = name;
                    cand.starLevel = 1;
                    cand.cost = def ? def->cost : std::max(1, player.shop()[static_cast<std::size_t>(a.shopIndex)].cost);
                    const float candValue = UnitValueEvaluator::evaluate(cand, content, &player).total;
                    float worstBench = 1e30f;
                    for (const OwnedUnit& u : player.bench())
                    {
                        worstBench = std::min(worstBench, UnitValueEvaluator::evaluate(u, content, &player).total);
                    }
                    const bool benchNearFull = player.bench().size() + 1 >= player.benchLimit();
                    bool enablesImmediateUpgrade = false;
                    int copies = 0;
                    for (const OwnedUnit& u : player.board()) if (u.championName == name) copies += 1;
                    for (const OwnedUnit& u : player.bench()) if (u.championName == name) copies += 1;
                    enablesImmediateUpgrade = ((copies % 3) == 2);

                    if (benchNearFull && !enablesImmediateUpgrade && candValue + 5.0f < worstBench)
                    {
                        continue;
                    }
                }

                if (a.type == MacroActionType::SellUnit && a.boardIndex >= 0 &&
                    static_cast<std::size_t>(a.boardIndex) < player.board().size())
                {
                    const OwnedUnit sold = player.board()[static_cast<std::size_t>(a.boardIndex)];
                    PlayerState tmp = player;
                    (void)tmp.sellBoardUnit(static_cast<std::size_t>(a.boardIndex));
                    const float after = boardOnly(BoardStrengthEvaluator::evaluate(tmp, content));
                    const float drop = boardBefore - after;
                    if (drop > maxSellDrop)
                    {
                        continue;
                    }

                    const TurnMetrics afterM = metricsOf(tmp, pool);
                    const TurnMetrics beforeM = lastMetrics;
                    const bool econUp = afterM.interestTier > beforeM.interestTier;
                    const bool boardNotWorse = afterM.board >= beforeM.board - 1.0f;
                    const bool upgradesUp = afterM.upgrades > beforeM.upgrades + 1.0f;
                    const bool traitsUp = afterM.traits > beforeM.traits + 1.0f;
                    if (boughtNames.find(sold.championName) != boughtNames.end() && !benchFull && !strongerReplacementExists(sold))
                    {
                        continue;
                    }
                    if (movedToBoardKeys.find(keyString(keyOf(sold))) != movedToBoardKeys.end() && !benchFull && !strongerReplacementExists(sold))
                    {
                        continue;
                    }
                    if (!(econUp || (boardNotWorse && (upgradesUp || traitsUp))))
                    {
                        continue;
                    }
                }
                filtered.push_back(a);
            }

            MacroAction chosen{};
            if (forcedFirstAction && step == 0)
            {
                chosen = *forcedFirstAction;
            }
            else
            {
                auto expectedLevelForStage = [](int s) -> int
                {
                    if (s <= 1) return 3;
                    if (s == 2) return 4;
                    if (s == 3) return 6;
                    if (s == 4) return 7;
                    return 8;
                };

                MacroAction forced{};
                bool hasForced = false;
                if (step == 0 && stage >= 3 && pool && player.gold() >= 50 && player.level() < expectedLevelForStage(stage))
                {
                    for (const MacroAction& a : filtered)
                    {
                        if (a.type == MacroActionType::BuyXp)
                        {
                            forced = a;
                            hasForced = true;
                            break;
                        }
                    }
                }

                chosen = hasForced
                    ? forced
                    : policy.chooseAction(player, content, filtered, enemy, pool, stage, roundIndex, rng, false, out);
            }

            const std::string chosenKey = actionKeyForRollout(player, chosen);
            if (!chosenKey.empty())
            {
                actionCounts[chosenKey] += 1;
            }

            if (isTransaction(chosen.type))
            {
                transactionsExecuted += 1;
            }
            if (chosen.type == MacroActionType::SellUnit)
            {
                sellsExecuted += 1;
                if (chosen.boardIndex >= 0) boardSellsExecuted += 1;
            }

            if (chosen.type == MacroActionType::RerollShop) rerollsExecuted += 1;
            if (chosen.type == MacroActionType::BuyXp) xpBuysExecuted += 1;
            if (chosen.type == MacroActionType::RepositionUnit) repositionActionsExecuted += 1;
            if (chosen.type == MacroActionType::MoveBenchToBoard || chosen.type == MacroActionType::MoveBoardToBench)
            {
                moveActionsExecuted += 1;
            }

            if (chosen.type == MacroActionType::BuyUnit &&
                chosen.shopIndex >= 0 &&
                static_cast<std::size_t>(chosen.shopIndex) < player.shop().size())
            {
                const std::string name = player.shop()[static_cast<std::size_t>(chosen.shopIndex)].championName;
                if (!name.empty())
                {
                    boughtNames.insert(name);
                }
            }
            else if (chosen.type == MacroActionType::SellUnit)
            {
                if (chosen.benchIndex >= 0 && static_cast<std::size_t>(chosen.benchIndex) < player.bench().size())
                {
                    const std::string name = player.bench()[static_cast<std::size_t>(chosen.benchIndex)].championName;
                    if (!name.empty()) soldNames.insert(name);
                }
                else if (chosen.boardIndex >= 0 && static_cast<std::size_t>(chosen.boardIndex) < player.board().size())
                {
                    const std::string name = player.board()[static_cast<std::size_t>(chosen.boardIndex)].championName;
                    if (!name.empty()) soldNames.insert(name);
                }
            }
            else if (chosen.type == MacroActionType::MoveBenchToBoard &&
                     chosen.benchIndex >= 0 &&
                     static_cast<std::size_t>(chosen.benchIndex) < player.bench().size())
            {
                const UnitKey k = keyOf(player.bench()[static_cast<std::size_t>(chosen.benchIndex)]);
                movedToBoardKeys.insert(keyString(k));
            }

            if (!MacroExecutor::apply(chosen, player, shop, rng, out))
            {
                break;
            }

            const TurnMetrics now = metricsOf(player, pool);
            if (improved(now, lastMetrics))
            {
                staleActions = 0;
            }
            else
            {
                staleActions += 1;
            }
            lastMetrics = now;
            if (staleActions >= 3)
            {
                break;
            }
            if (chosen.type == MacroActionType::EndTurn)
            {
                break;
            }
            if (moveActionsExecuted >= MacroConstants::MaxMoveActionsHardCap ||
                rerollsExecuted >= MacroConstants::MaxRerollsHardCap ||
                xpBuysExecuted >= MacroConstants::MaxXpBuysHardCap)
            {
                break;
            }
            if (transactionsExecuted >= MacroConstants::MaxTransactionsPerTurn)
            {
                break;
            }
            if (sellsExecuted >= MacroConstants::MaxSellsPerTurn)
            {
                break;
            }
        }

        ensureBoardFilled(player);
        resolveFormationOverlaps(player);
    }
}

void RolloutResult::addSample(float ev, float hp, float board, bool died, float top4)
{
    rollouts += 1;
    sumEV += ev;
    sumHP += hp;
    sumBoard += board;
    sumTop4 += top4;
    deaths += died ? 1 : 0;
}

void RolloutResult::finalize()
{
    if (rollouts <= 0)
    {
        avgEV = 0.0f;
        avgHP = 0.0f;
        avgBoard = 0.0f;
        top4Prob = 0.0f;
        deathRisk = 0.0f;
        return;
    }
    const float inv = 1.0f / static_cast<float>(rollouts);
    avgEV = sumEV * inv;
    avgHP = sumHP * inv;
    avgBoard = sumBoard * inv;
    top4Prob = sumTop4 * inv;
    deathRisk = static_cast<float>(deaths) * inv;
}

RolloutPlanner::RolloutPlanner(std::uint32_t seed, RolloutPlannerConfig cfg)
    : seed_(seed), cfg_(cfg)
{
}

MacroAction RolloutPlanner::chooseAction(const PlayerState& player,
                                        const ContentManager& content,
                                        const std::vector<MacroAction>& legalActions,
                                        const EnemySnapshot* enemy,
                                        const SharedUnitPool* pool,
                                        int stage,
                                        int roundIndex,
                                        const Random& rng,
                                        std::ostream& out) const
{
    RolloutPlannerConfig cfg = cfg_;
    cfg.depthRounds = std::clamp(cfg.depthRounds, 1, 6);
    cfg.branchesPerAction = std::clamp(cfg.branchesPerAction, 1, 32);
    cfg.topKActions = std::clamp(cfg.topKActions, 1, static_cast<int>(legalActions.size()));
    cfg.maxActionsPerTurn = std::clamp(cfg.maxActionsPerTurn, 1, MacroConstants::MaxActionsPerTurn);

    BranchPrunerConfig prunerCfg{};
    prunerCfg.topKActions = cfg.topKActions;

    std::vector<RolloutBranch> candidates =
        BranchPruner::buildCandidates(player, content, legalActions, enemy, pool, stage, roundIndex, prunerCfg);
    if (candidates.empty())
    {
        return MacroAction{ MacroActionType::EndTurn };
    }

    NullBuffer nullBuf;
    std::ostream nullOut(&nullBuf);

    const std::uint32_t policySeed = mixSeed(seed_, 0xA11A11A1u);
    const RolloutPolicy policy(policySeed);

    float bestEV = -1e30f;
    float bestHeuristic = -1e30f;
    MacroAction best = candidates.front().action;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        RolloutBranch& b = candidates[i];
        std::unordered_map<std::uint64_t, std::pair<FutureEval, bool>> terminalCache;
        terminalCache.reserve(static_cast<std::size_t>(cfg.branchesPerAction) * 2u);

        float runningEvSum = 0.0f;
        int runningSamples = 0;
        for (int r = 0; r < cfg.branchesPerAction; ++r)
        {
            if (!pool)
            {
                break;
            }
            const std::uint32_t rSeed = mixSeed(seed_, mixSeed(static_cast<std::uint32_t>(i + 1), static_cast<std::uint32_t>(r + 1)));
            Random rrng = rng;
            rrng.setSeed(mixSeed(rrng.nextU32(), rSeed));

            ClonedMacroState state = StateCloner::clone(content, player, *pool, rrng);

            std::ostream& sink = cfg.debug ? out : nullOut;
            PlayerState opponent("Opponent");
            Random oppRng(mixSeed(rSeed, 0x0FF0A11u));
            ShopSystem oppShop(content, state.pool);
            if (enemy)
            {
                opponent = StateCloner::enemyFromSnapshot(*enemy);
                oppShop.reroll(opponent, oppRng, false);
            }

            bool died = false;
            int simStage = stage;
            for (int d = 0; d < cfg.depthRounds; ++d)
            {
                const int ri = roundIndex + d;
                const RoundInfo info = RoundSchedule::get(ri);
                simStage = info.stage;

                if (d > 0)
                {
                    state.shop.reroll(state.player, state.rng, false);
                    if (enemy)
                    {
                        oppShop.reroll(opponent, oppRng, false);
                    }
                }

                EnemySnapshot oppSnap{};
                EnemySnapshot mySnap{};
                const EnemySnapshot* oppSnapPtr = nullptr;
                const EnemySnapshot* mySnapPtr = nullptr;
                if (enemy)
                {
                    oppSnap = ScoutSystem::snapshot(opponent, content);
                    mySnap = ScoutSystem::snapshot(state.player, content);
                    oppSnapPtr = &oppSnap;
                    mySnapPtr = &mySnap;
                }

                simulateTurn(state.player,
                             state.shop,
                             state.rng,
                             content,
                             oppSnapPtr,
                             &state.pool,
                             info.stage,
                             ri,
                             policy,
                             d == 0 ? &b.action : nullptr,
                             cfg.maxActionsPerTurn,
                             sink);

                if (enemy)
                {
                    simulateTurn(opponent,
                                 oppShop,
                                 oppRng,
                                 content,
                                 mySnapPtr,
                                 &state.pool,
                                 info.stage,
                                 ri,
                                 policy,
                                 nullptr,
                                 cfg.maxActionsPerTurn,
                                 sink);
                }

                RoundResult combat{};
                if (info.isPve)
                {
                    combat = state.rounds.runPvE(state.player, ri, state.rng.nextU32());
                    if (!combat.playerAWon)
                    {
                        state.player.takeDamage(combat.damageToA);
                        EconomySystem::applyRoundEnd(state.player, false);
                    }
                    else
                    {
                        EconomySystem::applyRoundEnd(state.player, true);
                    }

                    if (enemy)
                    {
                        const RoundResult oppCombat = state.rounds.runPvE(opponent, ri, oppRng.nextU32());
                        if (!oppCombat.playerAWon)
                        {
                            opponent.takeDamage(oppCombat.damageToA);
                            EconomySystem::applyRoundEnd(opponent, false);
                        }
                        else
                        {
                            EconomySystem::applyRoundEnd(opponent, true);
                        }
                    }
                }
                else if (!enemy)
                {
                    combat = state.rounds.runPvE(state.player, ri, state.rng.nextU32());
                    if (!combat.playerAWon)
                    {
                        state.player.takeDamage(combat.damageToA);
                        EconomySystem::applyRoundEnd(state.player, false);
                    }
                    else
                    {
                        EconomySystem::applyRoundEnd(state.player, true);
                    }
                }
                else
                {
                    const std::uint32_t combatSeed = mixSeed(state.rng.nextU32(), oppRng.nextU32());
                    combat = state.rounds.runPvP(state.player, opponent, ri, combatSeed);
                    if (combat.playerAWon)
                    {
                        opponent.takeDamage(combat.damageToB);
                    }
                    else if (combat.playerBWon)
                    {
                        state.player.takeDamage(combat.damageToA);
                    }
                    else
                    {
                        state.player.takeDamage(combat.damageToA);
                        opponent.takeDamage(combat.damageToB);
                    }

                    EconomySystem::applyRoundEnd(state.player, combat.playerAWon);
                    EconomySystem::applyRoundEnd(opponent, combat.playerBWon);
                }

                if (state.player.health() <= 0)
                {
                    died = true;
                    break;
                }
                if (enemy && opponent.health() <= 0)
                {
                    break;
                }
            }

            const std::uint64_t terminalKey =
                hashPlayerState(state.player) ^
                (enemy ? (hashPlayerState(opponent) * 0x27d4eb2f165667c5ull) : 0ull) ^
                (static_cast<std::uint64_t>(simStage) * 0x9e3779b97f4a7c15ull) ^
                (static_cast<std::uint64_t>(roundIndex) * 0xbf58476d1ce4e5b9ull) ^
                (static_cast<std::uint64_t>(died ? 1 : 0) * 0x94d049bb133111ebull);

            FutureEval eval{};
            bool diedUsed = died;
            auto it = terminalCache.find(terminalKey);
            if (it != terminalCache.end())
            {
                eval = it->second.first;
                diedUsed = it->second.second;
            }
            else
            {
                eval = FutureStateEvaluator::evaluate(state.player, content, nullptr, &state.pool, simStage, roundIndex, enemy ? &opponent : nullptr);
                terminalCache.emplace(terminalKey, std::make_pair(eval, died));
            }

            b.result.addSample(eval.ev, static_cast<float>(state.player.health()), eval.board, diedUsed, eval.top4Prob);
            runningEvSum += eval.ev;
            runningSamples += 1;

            if (runningSamples >= 2)
            {
                const float avg = runningEvSum / static_cast<float>(runningSamples);
                if (avg + 120.0f < bestEV)
                {
                    break;
                }
            }
        }

        b.result.finalize();

        if (cfg.debug)
        {
            out << "Action: " << (b.action.debugName.empty() ? "?" : b.action.debugName) << "\n";
            out << "Rollouts: " << b.result.rollouts << "\n";
            out << "AvgEV: " << std::lround(b.result.avgEV) << "\n";
            out << "AvgHP: " << std::lround(b.result.avgHP) << "\n";
            out << "AvgBoard: " << std::lround(b.result.avgBoard) << "\n";
            out << "Top4Prob: " << std::lround(b.result.top4Prob * AIConstants::PercentScale) << "%\n";
            out << "DeathRisk: " << std::lround(b.result.deathRisk * AIConstants::PercentScale) << "%\n\n";
        }

        if (b.result.avgEV > bestEV || (b.result.avgEV == bestEV && b.heuristicScore > bestHeuristic))
        {
            bestEV = b.result.avgEV;
            bestHeuristic = b.heuristicScore;
            best = b.action;
        }
    }

    if (cfg.debug)
    {
        out << "Chosen: " << (best.debugName.empty() ? "?" : best.debugName) << "\n";
    }

    return best.type == MacroActionType::EndTurn && best.debugName.empty()
        ? MacroAction{ MacroActionType::EndTurn }
        : best;
}
