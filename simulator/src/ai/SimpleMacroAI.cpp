#include "ai/SimpleMacroAI.hpp"
#include "constants/AIConstants.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/CompDirectionPlanner.hpp"
#include "ai/MacroActionScorer.hpp"
#include "ai/RolloutPlanner.hpp"
#include "macro/RoundSchedule.hpp"
#include "macro/ShopSystem.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

SimpleMacroAI::SimpleMacroAI(std::uint32_t seed, SimpleMacroAIConfig config)
    : seed_(seed), config_(config)
{
}

MacroAction SimpleMacroAI::chooseAction(const PlayerState& player,
                                        const ContentManager& content,
                                        const std::vector<MacroAction>& legalActions)
{
    std::ostringstream oss;
    Random rng(1u);
    return chooseAction(player, content, legalActions, nullptr, nullptr, 1, 0, rng, true, false, oss);
}

MacroAction SimpleMacroAI::chooseAction(const PlayerState& player,
                                        const ContentManager& content,
                                        const std::vector<MacroAction>& legalActions,
                                        const EnemySnapshot* enemy,
                                        const SharedUnitPool* pool,
                                        int stage,
                                        int roundIndex,
                                        const Random& rng,
                                        bool isTurnStart,
                                        bool verbose,
                                        std::ostream& out)
{
    if (config_.enableRollouts && isTurnStart)
    {
        RolloutPlannerConfig cfg{};
        cfg.depthRounds = config_.rolloutDepthRounds;
        cfg.branchesPerAction = config_.rolloutBranchesPerAction;
        cfg.topKActions = config_.rolloutTopKActions;
        cfg.maxActionsPerTurn = config_.rolloutMaxActionsPerTurn;
        cfg.debug = config_.rolloutDebug;

        RolloutPlanner planner(seed_, cfg);
        return planner.chooseAction(player, content, legalActions, enemy, pool, stage, roundIndex, rng, out);
    }

    const RoundInfo info = RoundSchedule::get(roundIndex);
    const int effStage = stage > 0 ? stage : info.stage;

    auto expectedLevelForStage = [&](int s) -> int
    {
        if (s <= 1) return 3;
        if (s == 2) return 4;
        if (s == 3) return 6;
        if (s == 4) return 7;
        return 8;
    };

    const int expLvl = expectedLevelForStage(std::clamp(effStage, 1, 9));
    if (effStage >= 3 && player.level() < expLvl && player.gold() >= 50)
    {
        for (const MacroAction& a : legalActions)
        {
            if (a.type == MacroActionType::BuyXp)
            {
                return a;
            }
        }
    }

    const BoardScore bs = BoardStrengthEvaluator::evaluate(player, content);
    const CompDirection dir = CompDirectionPlanner::infer(player, content);

    float enemyScore = 0.0f;
    if (enemy)
    {
        enemyScore = enemy->boardStrength;
    }

    std::string mode = "greed";
    if (player.health() <= static_cast<std::int32_t>(AIConstants::HpPressureAllInThreshold))
    {
        mode = "all-in";
    }
    else if (player.health() <= static_cast<std::int32_t>(AIConstants::SimpleMacroAIStabilizeHealthThreshold) ||
             enemyScore > bs.total + AIConstants::SimpleMacroAIEnemyStrengthGapThreshold)
    {
        mode = "stabilize";
    }
    else if (player.winStreak() <= 0 && player.loseStreak() >= AIConstants::SimpleMacroAITempoLoseStreakThreshold)
    {
        mode = "tempo";
    }

    if (mode == "greed" &&
        effStage >= 3 &&
        player.level() < expLvl &&
        player.gold() >= 50)
    {
        mode = "tempo";
    }

    const std::vector<ActionScore> scored =
        MacroActionScorer::scoreActions(player, content, legalActions, enemy, pool, effStage, roundIndex);

    MacroAction best{};
    std::string bestReason;
    float bestScore = AIConstants::SimpleMacroAIInitialBestScore;
    const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
    const std::int32_t remaining = need > 0 ? std::max<std::int32_t>(0, need - player.xp()) : 0;
    const bool xpLevelsNow = need > 0 && remaining <= ShopSystem::xpForBuy();

    for (const ActionScore& s : scored)
    {
        float adj = s.score;
        if (mode == "greed")
        {
            if (s.action.type == MacroActionType::RerollShop) adj -= AIConstants::SimpleMacroAIGreedRerollPenalty;
            if (s.action.type == MacroActionType::BuyXp && !xpLevelsNow)
            {
                if (!(stage >= 3 && player.level() < expLvl && player.gold() >= 50))
                {
                    adj -= AIConstants::SimpleMacroAIGreedBuyXpNotNowPenalty;
                }
            }
            if (s.action.type == MacroActionType::EndTurn)
            {
                if (!(stage >= 3 && player.level() < expLvl))
                {
                    adj += AIConstants::SimpleMacroAIGreedEndTurnBonus;
                }
            }
        }
        else if (mode == "tempo")
        {
            if (s.action.type == MacroActionType::BuyXp && (static_cast<int>(player.board().size()) >= player.unitCap()))
                adj += AIConstants::SimpleMacroAITempoBuyXpWhenBoardFullBonus;
            if (s.action.type == MacroActionType::RerollShop) adj -= AIConstants::SimpleMacroAITempoRerollPenalty;
        }
        else if (mode == "stabilize")
        {
            if (s.action.type == MacroActionType::RerollShop) adj += AIConstants::SimpleMacroAIStabilizeRerollBonus;
            if (s.action.type == MacroActionType::EndTurn) adj -= AIConstants::SimpleMacroAIStabilizeEndTurnPenalty;
        }
        else if (mode == "all-in")
        {
            if (s.action.type == MacroActionType::RerollShop) adj += AIConstants::SimpleMacroAIAllInRerollBonus;
            if (s.action.type == MacroActionType::BuyXp && xpLevelsNow) adj += AIConstants::SimpleMacroAIAllInBuyXpLevelsNowBonus;
            if (s.action.type == MacroActionType::EndTurn) adj -= AIConstants::SimpleMacroAIAllInEndTurnPenalty;
        }

        const int clampedStage = std::clamp(effStage, 1, 9);
        const int targetGold =
            clampedStage <= 2 ? 50 :
            clampedStage == 3 ? 40 :
            clampedStage == 4 ? 30 :
            20;
        if (s.action.type == MacroActionType::EndTurn)
        {
            const int excessGold = std::max(0, static_cast<int>(player.gold()) - targetGold);
            if (excessGold > 0)
            {
                adj -= static_cast<float>(excessGold) * AIConstants::SimpleMacroAIStagePressureGoldExcessPenaltyPer;
            }
            if (clampedStage >= 3 && player.level() <= 4 && player.gold() >= 80)
            {
                adj -= AIConstants::SimpleMacroAIStagePressureHighGoldLowLevelEndTurnPenalty;
            }
        }

        const int lvlDef = std::max(0, expLvl - player.level());
        if (lvlDef > 0 && effStage >= 3 && player.gold() >= 50)
        {
            if (s.action.type == MacroActionType::BuyXp)
            {
                adj += static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitBuyXpBonus * 2.0f;
            }
            if (s.action.type == MacroActionType::EndTurn)
            {
                adj -= static_cast<float>(lvlDef) * AIConstants::StageLevelDeficitEndTurnPenalty * 2.0f;
            }
        }

        if (adj > bestScore)
        {
            best = s.action;
            bestReason = s.reason;
            bestScore = adj;
        }
    }

    if (best.type != MacroActionType::EndTurn && bestScore < 0.0f)
    {
        best = MacroAction{ MacroActionType::EndTurn };
        best.debugName = "EndTurn";
        bestReason = "no-positive-action";
    }
    if ((best.type == MacroActionType::MoveBenchToBoard || best.type == MacroActionType::MoveBoardToBench) &&
        bestScore < AIConstants::SimpleMacroAIMoveMinScore)
    {
        best = MacroAction{ MacroActionType::EndTurn };
        best.debugName = "EndTurn";
        bestReason = "low-value-move";
    }
    if (best.type == MacroActionType::RepositionUnit && bestScore < AIConstants::SimpleMacroAIMoveMinScore)
    {
        best = MacroAction{ MacroActionType::EndTurn };
        best.debugName = "EndTurn";
        bestReason = "low-value-reposition";
    }
    if (best.type == MacroActionType::SellUnit &&
        player.bench().size() < player.benchLimit() &&
        bestScore < AIConstants::SimpleMacroAISellMinScore)
    {
        best = MacroAction{ MacroActionType::EndTurn };
        best.debugName = "EndTurn";
        bestReason = "low-value-sell";
    }

    if (verbose)
    {
        out << "BoardScore " << bs.debug << "\n";
        out << TraitSynergyEvaluator::evaluate(player, content).debug << "\n";
        out << "CompDirection traits=[";
        for (std::size_t i = 0; i < dir.coreTraits.size(); ++i)
        {
            if (i) out << ",";
            out << dir.coreTraits[i];
        }
        out << "] confidence=" << std::lround(dir.confidence * AIConstants::PercentScale) << "% " << dir.debug << "\n";
        if (enemy)
        {
            out << "Scout strongestEnemyScore=" << std::lround(enemyScore) << "\n";
        }
        out << "DecisionMode: " << mode << "\n";
        out << "Best action: " << best.debugName << " score=" << std::lround(bestScore) << " reason=" << bestReason << "\n";
    }

    return best.type == MacroActionType::EndTurn && best.debugName.empty()
        ? MacroAction{ MacroActionType::EndTurn }
        : best;
}
