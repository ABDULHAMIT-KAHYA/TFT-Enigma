#include "macro/MacroSimulation.hpp"
#include "macro/EconomySystem.hpp"
#include "constants/GameConstants.hpp"
#include "macro/LegalActionGenerator.hpp"
#include "constants/MacroConstants.hpp"
#include "macro/MacroExecutor.hpp"
#include "macro/RoundSchedule.hpp"
#include "macro/RoundSystem.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/PositioningOptimizer.hpp"
#include "ai/ScoutSystem.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include "macro/ShopSystem.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

static std::uint32_t mixSeed(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t x = a ^ (b + GameConstants::SeedMixConstant + (a << GameConstants::SeedMixShiftA) + (a >> GameConstants::SeedMixShiftB));
    x ^= (x << GameConstants::SeedScrambleShift1);
    x ^= (x >> GameConstants::SeedScrambleShift2);
    x ^= (x << GameConstants::SeedScrambleShift3);
    return x;
}

static void printShop(const PlayerState& p, std::ostream& out)
{
    out << "Shop: ";
    for (std::size_t i = 0; i < p.shop().size(); ++i)
    {
        const ShopOffer& o = p.shop()[i];
        if (o.championName.empty())
        {
            out << "[ ] ";
        }
        else
        {
            out << "[" << i << ":" << o.championName << ":" << o.cost << "] ";
        }
    }
    out << "\n";
}

static void printUnitsLine(const char* label, const std::vector<OwnedUnit>& units, std::ostream& out)
{
    out << label << ": ";
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        const OwnedUnit& u = units[i];
        out << "[" << i << ":" << u.championName << "*" << u.starLevel << ":" << u.cost << "]";
        if (u.hasFormation)
        {
            out << "@" << u.formation.x << "," << u.formation.y;
        }
        if (!u.items.empty())
        {
            out << "{";
            for (std::size_t k = 0; k < u.items.size(); ++k)
            {
                if (k) out << ",";
                out << u.items[k];
            }
            out << "}";
        }
        out << " ";
    }
    out << "\n";
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

static void takeTurn(PlayerState& player,
                     SimpleMacroAI& ai,
                     ShopSystem& shop,
                     Random& rng,
                     const ContentManager& content,
                     const EnemySnapshot* enemy,
                     const SharedUnitPool* pool,
                     int stage,
                     int roundIndex,
                     std::ostream& out,
                     MacroTurnStats* turnStats,
                     const MacroAction* forcedFirstAction,
                     int maxActionsOverride,
                     bool verboseFirstStep)
{
    struct UnitKey
    {
        std::string name;
        int star = 1;
        int cost = 1;
        int items = 0;
    };

    auto keyOf = [](const OwnedUnit& u) -> UnitKey
    {
        UnitKey k{};
        k.name = u.championName;
        k.star = std::clamp(u.starLevel, 1, 3);
        k.cost = std::max(1, u.cost);
        k.items = static_cast<int>(u.items.size());
        return k;
    };

    auto keyEquals = [](const UnitKey& a, const UnitKey& b) -> bool
    {
        return a.star == b.star && a.cost == b.cost && a.items == b.items && a.name == b.name;
    };

    auto keyString = [](const UnitKey& k) -> std::string
    {
        std::ostringstream ss;
        ss << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
        return ss.str();
    };

    auto boardOnly = [](const BoardScore& s) -> float
    {
        return s.unitPower + s.traitPower + s.itemPower + s.frontlinePower + s.carryPower;
    };

    auto unitScore = [&](const OwnedUnit& u) -> float
    {
        return UnitValueEvaluator::evaluate(u, content, &player).total;
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

    auto actionKey = [&](const MacroAction& a) -> std::string
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
        if (a.type == MacroActionType::MoveBenchToBoard && a.benchIndex >= 0 &&
            static_cast<std::size_t>(a.benchIndex) < player.bench().size())
        {
            const UnitKey k = keyOf(player.bench()[static_cast<std::size_t>(a.benchIndex)]);
            ss << "B2F|" << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
            return ss.str();
        }
        if (a.type == MacroActionType::MoveBoardToBench && a.boardIndex >= 0 &&
            static_cast<std::size_t>(a.boardIndex) < player.board().size())
        {
            const UnitKey k = keyOf(player.board()[static_cast<std::size_t>(a.boardIndex)]);
            ss << "F2B|" << k.name << "|" << k.star << "|" << k.cost << "|" << k.items;
            return ss.str();
        }
        if (a.type == MacroActionType::BuyXp)
        {
            ss << "XP|" << player.level() << "|" << player.xp();
            return ss.str();
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
        return "";
    };

    std::deque<std::pair<MacroActionType, UnitKey>> recentMoves;
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

    auto metricsOf = [&](const PlayerState& p) -> TurnMetrics
    {
        TurnMetrics m{};
        const BoardScore bs = BoardStrengthEvaluator::evaluate(p, content);
        m.board = boardOnly(bs);
        m.upgrades = UpgradePotentialEvaluator::evaluate(p, content, pool).total;
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

    TurnMetrics lastMetrics = metricsOf(player);
    int staleActions = 0;

    auto expectedLevelForStage = [](int s) -> int
    {
        if (s <= 1) return 3;
        if (s == 2) return 4;
        if (s == 3) return 6;
        if (s == 4) return 7;
        return 8;
    };

    if (turnStats)
    {
        turnStats->repositionActionsExecuted = 0;
        turnStats->executedActionKeys.clear();
    }

    const int maxActions = maxActionsOverride > 0 ? maxActionsOverride : MacroConstants::MaxActionsPerTurn;
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

            bool undo = false;
            if (!recentMoves.empty())
            {
                const auto& [lastType, lastKey] = recentMoves.back();
                if (a.type == MacroActionType::MoveBenchToBoard && lastType == MacroActionType::MoveBoardToBench &&
                    a.benchIndex >= 0 && static_cast<std::size_t>(a.benchIndex) < player.bench().size())
                {
                    undo = keyEquals(keyOf(player.bench()[static_cast<std::size_t>(a.benchIndex)]), lastKey);
                }
                else if (a.type == MacroActionType::MoveBoardToBench && lastType == MacroActionType::MoveBenchToBoard &&
                         a.boardIndex >= 0 && static_cast<std::size_t>(a.boardIndex) < player.board().size())
                {
                    undo = keyEquals(keyOf(player.board()[static_cast<std::size_t>(a.boardIndex)]), lastKey);
                }
            }
            if (undo)
            {
                continue;
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

                        const TurnMetrics afterM = metricsOf(tmp);
                        const TurnMetrics beforeM = lastMetrics;
                        const bool econUp = afterM.interestTier > beforeM.interestTier;
                        const bool boardNotWorse = afterM.board >= beforeM.board - 1.0f;
                        const bool upgradesUp = afterM.upgrades > beforeM.upgrades + 1.0f;
                        const bool traitsUp = afterM.traits > beforeM.traits + 1.0f;
                        if (!(econUp || (boardNotWorse && (upgradesUp || traitsUp))))
                        {
                            continue;
                        }
                    }
                }
            }

            const std::string k = actionKey(a);
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
            filtered.push_back(a);
        }

        MacroAction chosen{};
        if (forcedFirstAction && step == 0)
        {
            chosen = *forcedFirstAction;
        }
        else
        {
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
                : ai.chooseAction(player, content, filtered, enemy, pool, stage, roundIndex, rng, step == 0, step == 0 && verboseFirstStep, out);
        }

        const std::string chosenKey = actionKey(chosen);
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

        if (chosen.type == MacroActionType::RerollShop)
        {
            rerollsExecuted += 1;
        }
        if (chosen.type == MacroActionType::BuyXp)
        {
            xpBuysExecuted += 1;
        }
        if (chosen.type == MacroActionType::RepositionUnit)
        {
            repositionActionsExecuted += 1;
        }

        if (turnStats)
        {
            if (!chosenKey.empty())
            {
                turnStats->executedActionKeys.push_back(chosenKey);
            }
            turnStats->repositionActionsExecuted = repositionActionsExecuted;
        }

        if (chosen.type == MacroActionType::MoveBenchToBoard &&
            chosen.benchIndex >= 0 &&
            static_cast<std::size_t>(chosen.benchIndex) < player.bench().size())
        {
            const UnitKey k = keyOf(player.bench()[static_cast<std::size_t>(chosen.benchIndex)]);
            recentMoves.push_back({ MacroActionType::MoveBenchToBoard, k });
            movedToBoardKeys.insert(keyString(k));
            if (recentMoves.size() > MacroConstants::RecentMoveHistoryLimit) recentMoves.pop_front();
            moveActionsExecuted += 1;
        }
        else if (chosen.type == MacroActionType::MoveBoardToBench &&
                 chosen.boardIndex >= 0 &&
                 static_cast<std::size_t>(chosen.boardIndex) < player.board().size())
        {
            recentMoves.push_back({ MacroActionType::MoveBoardToBench, keyOf(player.board()[static_cast<std::size_t>(chosen.boardIndex)]) });
            if (recentMoves.size() > MacroConstants::RecentMoveHistoryLimit) recentMoves.pop_front();
            moveActionsExecuted += 1;
        }
        else if (chosen.type == MacroActionType::BuyUnit &&
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

        if (!MacroExecutor::apply(chosen, player, shop, rng, out))
        {
            break;
        }

        const TurnMetrics now = metricsOf(player);
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
            MacroAction end{};
            end.type = MacroActionType::EndTurn;
            end.debugName = "EndTurn";
            const std::string k = actionKey(end);
            if (turnStats && !k.empty())
            {
                turnStats->executedActionKeys.push_back(k);
            }
            (void)MacroExecutor::apply(end, player, shop, rng, out);
            break;
        }

        if (chosen.type == MacroActionType::EndTurn)
        {
            break;
        }
        if (moveActionsExecuted >= MacroConstants::MaxMoveActionsHardCap ||
            rerollsExecuted >= MacroConstants::MaxRerollsHardCap ||
            xpBuysExecuted >= MacroConstants::MaxXpBuysHardCap ||
            transactionsExecuted >= MacroConstants::MaxTransactionsPerTurn)
        {
            break;
        }
    }

    ensureBoardFilled(player);
    resolveFormationOverlaps(player);
    if (repositionActionsExecuted <= 0)
    {
        PositioningOptimizer::optimize(player, content, enemy);
    }

    out << "ActionBudget tx=" << transactionsExecuted
        << " sells=" << sellsExecuted
        << " boardSells=" << boardSellsExecuted
        << "\n";
}

void MacroSimulation::takeTurnForValidation(PlayerState& player,
                                           SimpleMacroAI& ai,
                                           ShopSystem& shop,
                                           Random& rng,
                                           const ContentManager& content,
                                           const EnemySnapshot* enemy,
                                           const SharedUnitPool* pool,
                                           std::ostream& out,
                                           MacroTurnStats& stats)
{
    takeTurn(player, ai, shop, rng, content, enemy, pool, 1, 0, out, &stats, nullptr, 0, true);
}

void MacroSimulation::takeTurnForValidationAt(PlayerState& player,
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
    takeTurn(player, ai, shop, rng, content, enemy, pool, stage, roundIndex, out, &stats, nullptr, 0, true);
}

void MacroSimulation::takeTurnWithForcedFirstAction(PlayerState& player,
                                                    SimpleMacroAI& ai,
                                                    ShopSystem& shop,
                                                    Random& rng,
                                                    const ContentManager& content,
                                                    const EnemySnapshot* enemy,
                                                    const SharedUnitPool* pool,
                                                    const MacroAction& forcedFirstAction,
                                                    int stage,
                                                    int roundIndex,
                                                    int maxActions,
                                                    std::ostream& out)
{
    takeTurn(player, ai, shop, rng, content, enemy, pool, stage, roundIndex, out, nullptr, &forcedFirstAction, maxActions, false);
}

int MacroSimulation::run(const ContentManager& content, std::uint32_t seed, bool useMonteCarlo, bool mcDebug, std::ostream& out)
{
    SharedUnitPool pool(content);
    ShopSystem shop(content, pool);
    RoundSystem rounds(content, pool);

    PlayerState playerA("Player A");
    PlayerState playerB("Player B");

    playerA.addGold(MacroConstants::StartingGold);
    playerB.addGold(MacroConstants::StartingGold);

    SimpleMacroAIConfig aiCfg{};
    aiCfg.enableRollouts = useMonteCarlo;
    aiCfg.rolloutDebug = mcDebug;
    SimpleMacroAI aiA(seed ^ GameConstants::AiSeedSaltA, aiCfg);
    SimpleMacroAI aiB(seed ^ GameConstants::AiSeedSaltB, aiCfg);

    Random rngA(seed ^ GameConstants::RngSeedSaltA);
    Random rngB(seed ^ GameConstants::RngSeedSaltB);

    const int maxRounds = MacroConstants::MaxRounds;
    for (int roundIndex = 0; roundIndex < maxRounds; ++roundIndex)
    {
        const RoundInfo info = RoundSchedule::get(roundIndex);

        out << "\n=== ROUND " << info.label << (info.isPve ? " PvE" : " PvP") << " ===\n";

        out << playerA.name() << " HP=" << playerA.health()
            << " Gold=" << playerA.gold()
            << " Lvl=" << playerA.level()
            << " XP=" << playerA.xp()
            << " WS=" << playerA.winStreak()
            << " LS=" << playerA.loseStreak()
            << "\n";
        const EnemySnapshot snapB = ScoutSystem::snapshot(playerB, content);
        shop.reroll(playerA, rngA, false);
        printShop(playerA, out);
        takeTurn(playerA, aiA, shop, rngA, content, &snapB, &pool, info.stage, roundIndex, out, nullptr, nullptr, 0, true);
        printUnitsLine("Board", playerA.board(), out);
        printUnitsLine("Bench", playerA.bench(), out);

        out << playerB.name() << " HP=" << playerB.health()
            << " Gold=" << playerB.gold()
            << " Lvl=" << playerB.level()
            << " XP=" << playerB.xp()
            << " WS=" << playerB.winStreak()
            << " LS=" << playerB.loseStreak()
            << "\n";
        const EnemySnapshot snapA = ScoutSystem::snapshot(playerA, content);
        shop.reroll(playerB, rngB, false);
        printShop(playerB, out);
        takeTurn(playerB, aiB, shop, rngB, content, &snapA, &pool, info.stage, roundIndex, out, nullptr, nullptr, 0, true);
        printUnitsLine("Board", playerB.board(), out);
        printUnitsLine("Bench", playerB.bench(), out);

        if (info.isPve)
        {
            const std::uint32_t seedA = mixSeed(seed, static_cast<std::uint32_t>(roundIndex * 2 + 0));
            const std::uint32_t seedB = mixSeed(seed, static_cast<std::uint32_t>(roundIndex * 2 + 1));

            const RoundResult rA = rounds.runPvE(playerA, roundIndex, seedA);
            const RoundResult rB = rounds.runPvE(playerB, roundIndex, seedB);

            out << "Combat: " << playerA.name() << (rA.playerAWon ? " won" : " lost")
                << " | dmgTaken=" << rA.damageToA << "\n";
            out << "Combat: " << playerB.name() << (rB.playerAWon ? " won" : " lost")
                << " | dmgTaken=" << rB.damageToA << "\n";

            playerA.takeDamage(rA.damageToA);
            playerB.takeDamage(rB.damageToA);

            const EconomyResult eA = EconomySystem::applyRoundEnd(playerA, rA.playerAWon);
            const EconomyResult eB = EconomySystem::applyRoundEnd(playerB, rB.playerAWon);

            out << playerA.name() << " economy +" << eA.total << " (base " << eA.baseGold
                << " interest " << eA.interest << " win " << eA.winBonus << " streak " << eA.streakBonus << ")\n";
            out << playerB.name() << " economy +" << eB.total << " (base " << eB.baseGold
                << " interest " << eB.interest << " win " << eB.winBonus << " streak " << eB.streakBonus << ")\n";
        }
        else
        {
            const std::uint32_t seedFight = mixSeed(seed, static_cast<std::uint32_t>(roundIndex));
            const RoundResult r = rounds.runPvP(playerA, playerB, roundIndex, seedFight);

            out << "Combat: "
                << (r.playerAWon ? playerA.name() : r.playerBWon ? playerB.name() : "Tie")
                << " | survivors A=" << r.survivingA << " B=" << r.survivingB
                << " | damageToA=" << r.damageToA << " damageToB=" << r.damageToB
                << "\n";

            playerA.takeDamage(r.damageToA);
            playerB.takeDamage(r.damageToB);

            const bool aWon = r.playerAWon;
            const bool bWon = r.playerBWon;

            const EconomyResult eA = EconomySystem::applyRoundEnd(playerA, aWon);
            const EconomyResult eB = EconomySystem::applyRoundEnd(playerB, bWon);

            out << playerA.name() << " economy +" << eA.total << " (base " << eA.baseGold
                << " interest " << eA.interest << " win " << eA.winBonus << " streak " << eA.streakBonus << ")\n";
            out << playerB.name() << " economy +" << eB.total << " (base " << eB.baseGold
                << " interest " << eB.interest << " win " << eB.winBonus << " streak " << eB.streakBonus << ")\n";
        }

        out << playerA.name() << " HP=" << playerA.health() << " Gold=" << playerA.gold()
            << " Lvl=" << playerA.level() << " XP=" << playerA.xp() << "\n";
        out << playerB.name() << " HP=" << playerB.health() << " Gold=" << playerB.gold()
            << " Lvl=" << playerB.level() << " XP=" << playerB.xp() << "\n";

        if (playerA.health() <= 0 || playerB.health() <= 0)
        {
            out << "Game over\n";
            return 0;
        }
    }

    return 0;
}
