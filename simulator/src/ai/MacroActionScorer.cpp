#include "ai/MacroActionScorer.hpp"
#include "constants/AIConstants.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include "ai/PositioningOptimizer.hpp"
#include "macro/ShopSystem.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include "macro/RoundSchedule.hpp"

static int keepGoldFloor(int gold)
{
    if (gold >= MacroConstants::MaxInterestGold) return MacroConstants::MaxInterestGold;
    if (gold >= (MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold)) return MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold;
    if (gold >= (MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold * 2)) return MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold * 2;
    if (gold >= (MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold * 3)) return MacroConstants::MaxInterestGold - MacroConstants::InterestStepGold * 3;
    if (gold >= MacroConstants::InterestStepGold) return MacroConstants::InterestStepGold;
    return 0;
}

static int countCopies(const PlayerState& player, const std::string& name, int starLevel)
{
    int count = 0;
    for (const OwnedUnit& u : player.board())
    {
        if (u.championName == name && u.starLevel == starLevel) count += 1;
    }
    for (const OwnedUnit& u : player.bench())
    {
        if (u.championName == name && u.starLevel == starLevel) count += 1;
    }
    return count;
}

static float hpPressure(const PlayerState& player, const EnemySnapshot* enemy)
{
    const float hp = static_cast<float>(player.health());
    float p = 0.0f;
    if (hp <= AIConstants::HpPressureAllInThreshold) p += AIConstants::HpPressureAllInValue;
    else if (hp <= AIConstants::HpPressureStabilizeThreshold) p += AIConstants::HpPressureStabilizeValue;
    else if (hp <= AIConstants::HpPressureTempoThreshold) p += AIConstants::HpPressureTempoValue;

    if (enemy)
    {
        const float enemyScore = enemy->boardStrength;
        p += std::clamp((enemyScore - AIConstants::EnemyScorePressureBaseline) / AIConstants::EnemyScorePressureScale,
                        0.0f,
                        AIConstants::EnemyScorePressureMaxAdd);
    }
    return p;
}

static void autoLevelFromXp(PlayerState& player)
{
    while (player.level() < GameConstants::MaxLevel)
    {
        const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
        if (need <= 0)
        {
            break;
        }
        if (player.xp() < need)
        {
            break;
        }
        player.addXp(-need);
        player.setLevel(player.level() + 1);
    }
}

static bool applyDryRun(const MacroAction& a, PlayerState& p)
{
    switch (a.type)
    {
        case MacroActionType::BuyUnit:
        {
            if (a.shopIndex < 0 || static_cast<std::size_t>(a.shopIndex) >= p.shop().size())
            {
                return false;
            }
            const ShopOffer offer = p.shop()[static_cast<std::size_t>(a.shopIndex)];
            if (offer.championName.empty())
            {
                return false;
            }
            const int cost = std::max(0, offer.cost);
            if (!p.spendGold(cost))
            {
                return false;
            }
            OwnedUnit u{};
            u.championName = offer.championName;
            u.starLevel = 1;
            u.cost = cost;
            if (!p.addToBench(std::move(u)))
            {
                p.addGold(cost);
                return false;
            }
            p.shopMutable()[static_cast<std::size_t>(a.shopIndex)] = ShopOffer{ "", 0 };
            return true;
        }
        case MacroActionType::SellUnit:
        {
            if (a.benchIndex >= 0)
            {
                return p.sellBenchUnit(static_cast<std::size_t>(a.benchIndex));
            }
            if (a.boardIndex >= 0)
            {
                return p.sellBoardUnit(static_cast<std::size_t>(a.boardIndex));
            }
            return false;
        }
        case MacroActionType::RerollShop:
        {
            return false;
        }
        case MacroActionType::BuyXp:
        {
            if (!p.spendGold(ShopSystem::xpForBuy()))
            {
                return false;
            }
            p.addXp(ShopSystem::xpForBuy());
            autoLevelFromXp(p);
            return true;
        }
        case MacroActionType::MoveBenchToBoard:
        {
            if (a.benchIndex < 0)
            {
                return false;
            }
            return p.moveBenchToBoard(static_cast<std::size_t>(a.benchIndex));
        }
        case MacroActionType::MoveBoardToBench:
        {
            if (a.boardIndex < 0)
            {
                return false;
            }
            return p.moveBoardToBench(static_cast<std::size_t>(a.boardIndex));
        }
        case MacroActionType::RepositionUnit:
        {
            if (a.boardIndex < 0)
            {
                return false;
            }
            return p.repositionBoardUnit(static_cast<std::size_t>(a.boardIndex), a.targetPosition);
        }
        case MacroActionType::EquipItem:
        {
            if (a.boardIndex < 0 || a.itemIndex < 0)
            {
                return false;
            }
            return p.equipItem(static_cast<std::size_t>(a.boardIndex), static_cast<std::size_t>(a.itemIndex));
        }
        case MacroActionType::EndTurn:
        {
            return true;
        }
    }
    return false;
}

static float traitAlignmentBonus(const OwnedUnit& unit,
                                 const ContentManager& content,
                                 const CompDirection& dir)
{
    const ChampionDefinition* def = content.getChampion(unit.championName);
    if (!def)
    {
        return 0.0f;
    }

    float b = 0.0f;
    for (const std::string& t : def->traits)
    {
        if (t.empty())
        {
            continue;
        }
        for (const std::string& core : dir.coreTraits)
        {
            if (t == core)
            {
                b += AIConstants::TraitAlignmentUnitBonus;
                break;
            }
        }
    }
    return b;
}

static int expectedLevelForStage(int stage, int roundIndex)
{
    if (stage <= 1) return 3;
    if (stage == 2) return 4;
    if (stage == 3) return 6;
    if (stage == 4) return 7;
    return 8;
}

std::vector<ActionScore> MacroActionScorer::scoreActions(const PlayerState& player,
                                                         const ContentManager& content,
                                                         const std::vector<MacroAction>& legalActions,
                                                         const EnemySnapshot* enemy,
                                                         const SharedUnitPool* pool,
                                                         int stage,
                                                         int roundIndex)
{
    std::vector<ActionScore> scored;
    scored.reserve(legalActions.size());

    const BoardScore boardScore = BoardStrengthEvaluator::evaluate(player, content);
    const TraitSynergyScore traitScore = TraitSynergyEvaluator::evaluate(player, content);
    const UpgradePotentialScore upgradeScore = UpgradePotentialEvaluator::evaluate(player, content, pool);
    const CompDirection dir = CompDirectionPlanner::infer(player, content);

    const int gold = static_cast<int>(player.gold());
    const int floor = keepGoldFloor(gold);
    const float pressure = hpPressure(player, enemy);
    const int expLvl = expectedLevelForStage(stage, roundIndex);
    const int lvlDef = std::max(0, expLvl - player.level());
    const bool underLeveled = lvlDef > 0;

    auto add = [&](const MacroAction& a, float s, std::string reason)
    {
        ActionScore as{};
        as.action = a;
        as.score = s;
        as.reason = std::move(reason);
        scored.push_back(std::move(as));
    };

    for (const MacroAction& a : legalActions)
    {
        float s = 0.0f;
        std::ostringstream why;

        float deltaBoard = 0.0f;
        if (a.type != MacroActionType::RerollShop && a.type != MacroActionType::RepositionUnit)
        {
            PlayerState sim = player;
            if (applyDryRun(a, sim))
            {
                const BoardScore after = BoardStrengthEvaluator::evaluate(sim, content);
                deltaBoard = after.total - boardScore.total;
            }
        }

        if (a.type == MacroActionType::BuyUnit)
        {
            if (a.shopIndex < 0 || static_cast<std::size_t>(a.shopIndex) >= player.shop().size())
            {
                continue;
            }
            const ShopOffer& offer = player.shop()[static_cast<std::size_t>(a.shopIndex)];
            if (offer.championName.empty())
            {
                continue;
            }

            OwnedUnit u{};
            u.championName = offer.championName;
            u.starLevel = 1;
            u.cost = offer.cost;

            const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
            s += uv.total * AIConstants::BuyUnitUnitValueWeight;
            s += deltaBoard * AIConstants::BuyUnitDeltaBoardWeight;

            const int same1 = countCopies(player, offer.championName, 1);
            const int same2 = countCopies(player, offer.championName, 2);
            if ((same1 % 3) == 2) { s += AIConstants::DuplicateToTwoStarBonus; why << "duplicate->2* "; }
            else if ((same1 % 3) == 1) { s += AIConstants::PairBonus; why << "pair "; }
            if ((same2 % 3) == 2) { s += AIConstants::DuplicateToThreeStarBonus; why << "duplicate->3* "; }
            else if ((same2 % 3) == 1) { s += AIConstants::TwoStarPairBonus; why << "2* pair "; }

            s += traitAlignmentBonus(u, content, dir);
            if (traitAlignmentBonus(u, content, dir) > 0.0f) why << "trait-align ";

            if (pool)
            {
                const int avail = pool->availableCount(offer.championName);
                if (avail <= 0) { s -= AIConstants::PoolEmptyPenalty; why << "pool-empty "; }
                else if (avail <= 2) { s -= AIConstants::PoolLowPenalty; why << "pool-low "; }
            }
            if (enemy && !enemy->units.empty())
            {
                int contested = 0;
                for (const EnemySnapshot::UnitInfo& eu : enemy->units)
                {
                    if (eu.championName == offer.championName)
                    {
                        contested += 1;
                    }
                }
                if (contested > 0)
                {
                    s -= static_cast<float>(contested) * AIConstants::ContestPenaltyPerUnit;
                    why << "contested ";
                }
            }

            const float upgradeBias = std::min(1.0f, upgradeScore.total / AIConstants::UpgradeBiasDivisor);
            s += upgradeBias * AIConstants::UpgradeBiasBonus;

            const float econPenalty =
                static_cast<float>(a.goldCost) *
                (gold - a.goldCost < floor ? AIConstants::BuyUnitEconPenaltyLowEcon : AIConstants::BuyUnitEconPenaltyHighEcon);
            s -= econPenalty * (1.0f - AIConstants::BuyUnitPressureEconRelief * pressure);

            if (underLeveled && gold >= 50)
            {
                s -= static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitHighGoldPenalty;
                why << "lvl-pressure ";
            }

            why << "buy";
        }
        else if (a.type == MacroActionType::SellUnit)
        {
            const OwnedUnit* u = nullptr;
            if (a.benchIndex >= 0 && static_cast<std::size_t>(a.benchIndex) < player.bench().size())
            {
                u = &player.bench()[static_cast<std::size_t>(a.benchIndex)];
            }
            if (a.boardIndex >= 0 && static_cast<std::size_t>(a.boardIndex) < player.board().size())
            {
                u = &player.board()[static_cast<std::size_t>(a.boardIndex)];
            }
            if (u)
            {
                const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(*u, content, &player);
                s += deltaBoard * AIConstants::SellUnitDeltaBoardWeight;
                s -= uv.total * AIConstants::SellUnitUnitValuePenaltyWeight;
                s += static_cast<float>(std::max(1, u->cost)) * AIConstants::SellUnitGoldBackWeight;
                if (player.bench().size() >= player.benchLimit()) { s += AIConstants::SellUnitFreeBenchBonus; why << "free-space "; }
                if (underLeveled && a.boardIndex >= 0)
                {
                    s -= static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitEndTurnPenalty * 0.25f;
                    why << "avoid-board-sell ";
                }
                why << "sell";
            }
        }
        else if (a.type == MacroActionType::RerollShop)
        {
            s += AIConstants::RerollBasePressureWeight * pressure;
            s += upgradeScore.total * AIConstants::RerollUpgradePotentialWeight;
            s -= static_cast<float>(a.goldCost) *
                 (gold - a.goldCost < floor ? AIConstants::RerollPenaltyLowEcon : AIConstants::RerollPenaltyHighEcon) *
                 (1.0f - AIConstants::RerollPressurePenaltyScale * pressure);
            if (pressure > AIConstants::RerollStabilizeTagThreshold) why << "stabilize ";
            if (upgradeScore.total > AIConstants::RerollUpgradeHuntTagThreshold) why << "upgrade-hunt ";
            why << "reroll";
        }
        else if (a.type == MacroActionType::BuyXp)
        {
            const bool boardFull = static_cast<int>(player.board().size()) >= player.unitCap();
            s += boardFull ? AIConstants::BuyXpBoardFullBase : AIConstants::BuyXpBoardNotFullBase;
            s += deltaBoard * AIConstants::BuyXpDeltaBoardWeight;
            s += pressure * AIConstants::BuyXpPressureWeight;
            s -= static_cast<float>(a.goldCost) *
                 (gold - a.goldCost < floor ? AIConstants::BuyXpGoldPenaltyLowEcon : AIConstants::BuyXpGoldPenaltyHighEcon);
            const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
            const int goldAfter = gold - a.goldCost;
            const int floorAfter = keepGoldFloor(goldAfter);
            if (floorAfter < floor)
            {
                s -= static_cast<float>(floor - floorAfter) * AIConstants::BuyXpBreakInterestPenaltyPerGold;
                why << "break-interest ";
            }
            if (need <= 0)
            {
                s -= AIConstants::BuyXpMaxLevelPenalty;
                why << "max-level ";
            }
            else
            {
                const std::int32_t remaining = std::max<std::int32_t>(0, need - player.xp());
                const std::int32_t buy = ShopSystem::xpForBuy();
                const bool levelsNow = remaining <= buy;
                const bool levelsSoon = remaining <= buy * AIConstants::BuyXpLevelsSoonBuys;

                float bestBench = 0.0f;
                float worstBoard = 1e9f;
                for (const OwnedUnit& u : player.bench())
                {
                    bestBench = std::max(bestBench, UnitValueEvaluator::evaluate(u, content, &player).total);
                }
                for (const OwnedUnit& u : player.board())
                {
                    worstBoard = std::min(worstBoard, UnitValueEvaluator::evaluate(u, content, &player).total);
                }
                if (worstBoard > 1e8f) worstBoard = 0.0f;
                const bool benchDeployableStrength = bestBench > worstBoard + AIConstants::BuyXpBenchDeployableDelta;
                if (underLeveled)
                {
                    s += static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitBuyXpBonus;
                    why << "lvl-pressure ";
                    if (gold >= 50)
                    {
                        s += static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitBuyXpHighGoldBonus;
                        why << "high-gold ";
                    }
                    if (boardFull && benchDeployableStrength)
                    {
                        s += static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitBuyXpBenchCapBonus;
                        why << "cap ";
                    }
                    const float buysAway = static_cast<float>(remaining) / static_cast<float>(std::max<std::int32_t>(1, buy));
                    s += buysAway * static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitBuyXpFarBonusPerBuy;
                    why << "far-ok ";
                }

                if (levelsNow)
                {
                    s += AIConstants::BuyXpLevelsNowBonus;
                    why << "level-now ";
                    if (boardFull && benchDeployableStrength)
                    {
                        s += AIConstants::BuyXpDeployImmediateBonus;
                        why << "deploy ";
                    }
                }
                else if (levelsSoon)
                {
                    s += AIConstants::BuyXpLevelsSoonBonus;
                    why << "level-soon ";
                    if (boardFull && benchDeployableStrength)
                    {
                        s += AIConstants::BuyXpDeploySoonBonus;
                        why << "deploy-soon ";
                    }
                }
                else
                {
                    s -= AIConstants::BuyXpTooFarBasePenalty;
                    const float r =
                        std::clamp(static_cast<float>(remaining) / AIConstants::BuyXpTooFarRemainingScale,
                                   0.0f,
                                   AIConstants::BuyXpTooFarRemainingMax);
                    s -= r * AIConstants::BuyXpTooFarRemainingPenaltyPer;
                    why << "far ";
                }

                if (!levelsSoon)
                {
                    if (!boardFull)
                    {
                        s -= AIConstants::BuyXpNotCappedPenalty;
                        why << "no-cap ";
                    }
                    else if (!benchDeployableStrength)
                    {
                        s -= AIConstants::BuyXpNoDeployPenalty;
                        why << "no-deploy ";
                    }
                }

                if (upgradeScore.total > AIConstants::BuyXpUpgradeOverXpThreshold &&
                    pressure < AIConstants::BuyXpUpgradeOverXpPressureMax &&
                    !levelsNow)
                {
                    s -= AIConstants::BuyXpUpgradeOverXpPenalty;
                    why << "upg>xp ";
                }

                if (pressure > AIConstants::BuyXpTempoPressureThreshold &&
                    boardFull &&
                    levelsSoon &&
                    goldAfter >= AIConstants::BuyXpMinGoldAfterTempo)
                {
                    s += AIConstants::BuyXpTempoBonus;
                    why << "tempo ";
                }
            }
            why << "level";
        }
        else if (a.type == MacroActionType::MoveBenchToBoard)
        {
            if (a.benchIndex < 0 || static_cast<std::size_t>(a.benchIndex) >= player.bench().size())
            {
                continue;
            }
            const OwnedUnit& u = player.bench()[static_cast<std::size_t>(a.benchIndex)];
            const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
            s += uv.total * AIConstants::MoveBenchToBoardUnitValueWeight;
            s += traitScore.total * AIConstants::MoveBenchToBoardTraitWeight;
            s += deltaBoard * AIConstants::MoveBenchToBoardDeltaBoardWeight;
            why << "deploy";
        }
        else if (a.type == MacroActionType::MoveBoardToBench)
        {
            if (a.boardIndex < 0 || static_cast<std::size_t>(a.boardIndex) >= player.board().size())
            {
                continue;
            }
            const OwnedUnit& u = player.board()[static_cast<std::size_t>(a.boardIndex)];
            const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
            s += deltaBoard * AIConstants::MoveBoardToBenchDeltaBoardWeight;
            s -= uv.total * AIConstants::MoveBoardToBenchUnitValuePenaltyWeight;
            if (player.bench().size() < player.benchLimit()) { s += AIConstants::MoveBoardToBenchIfSpaceBonus; }
            why << "bench";
        }
        else if (a.type == MacroActionType::EquipItem)
        {
            if (a.itemIndex < 0 || static_cast<std::size_t>(a.itemIndex) >= player.itemBench().size())
            {
                continue;
            }
            if (a.boardIndex < 0 || static_cast<std::size_t>(a.boardIndex) >= player.board().size())
            {
                continue;
            }

            const std::string& itemName = player.itemBench()[static_cast<std::size_t>(a.itemIndex)];
            const Item* item = content.getItem(itemName);
            if (!item)
            {
                continue;
            }
            const OwnedUnit& u = player.board()[static_cast<std::size_t>(a.boardIndex)];
            const ItemValueScore iv = ItemValueEvaluator::evaluateItem(*item);
            const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
            s += iv.total * AIConstants::EquipItemItemValueWeight;
            s += uv.total * AIConstants::EquipItemUnitValueWeight;
            s += deltaBoard * AIConstants::EquipItemDeltaBoardWeight;
            why << "equip";
        }
        else if (a.type == MacroActionType::RepositionUnit)
        {
            const float before = PositioningOptimizer::score(player, content, enemy);
            PlayerState sim = player;
            if (!applyDryRun(a, sim))
            {
                continue;
            }
            const float after = PositioningOptimizer::score(sim, content, enemy);
            const float delta = after - before;

            if (delta > 0.0f)
            {
                s += enemy && enemy->carryUnit >= 0 ? AIConstants::RepositionBaseWithEnemyCarry : AIConstants::RepositionBaseNoEnemyCarry;
            }
            s += delta * AIConstants::RepositionDeltaWeight;
            why << "pos";
        }
        else if (a.type == MacroActionType::EndTurn)
        {
            s += 0.0f;
            if (gold >= floor && pressure <= AIConstants::EndTurnLowPressureThreshold)
            {
                s += AIConstants::EndTurnGreedBonus;
                if (gold >= MacroConstants::MaxInterestGold)
                {
                    s += AIConstants::EndTurnMaxInterestPreservationBonus;
                    why << "max-interest ";
                }
            }
            if (underLeveled)
            {
                s -= static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitEndTurnPenalty;
                const int excessGold = std::max(0, gold - 50);
                s -= static_cast<float>(excessGold) * AIConstants::StageLevelDeficitHighGoldPenalty * (1.0f + pressure);
                why << "lvl-pressure ";
            }
            why << "end";
        }

        s += boardScore.total * AIConstants::ScoreBoardScale;
        s += dir.confidence * AIConstants::CompConfidenceScale;

        add(a, s, why.str());
    }

    std::stable_sort(scored.begin(), scored.end(), [](const ActionScore& a, const ActionScore& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.action.debugName < b.action.debugName;
    });

    return scored;
}
