#include "ai/RolloutPolicy.hpp"
#include "ai/MacroActionScorer.hpp"
#include "constants/AIConstants.hpp"
#include "macro/ShopSystem.hpp"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <sstream>
#include <streambuf>
RolloutPolicy::RolloutPolicy(std::uint32_t seed) : seed_(seed) {}

MacroAction RolloutPolicy::chooseAction(const PlayerState& player,
                                        const ContentManager& content,
                                        const std::vector<MacroAction>& legalActions,
                                        const EnemySnapshot* enemy,
                                        const SharedUnitPool* pool,
                                        int stage,
                                        int roundIndex,
                                        const Random&,
                                        bool verbose,
                                        std::ostream& out) const
{
    (void)seed_;

    auto expectedLevelForStage = [&](int s) -> int
    {
        if (s <= 1) return 3;
        if (s == 2) return 4;
        if (s == 3) return 6;
        if (s == 4) return 7;
        return 8;
    };

    const int expLvl = expectedLevelForStage(std::clamp(stage, 1, 9));
    if (stage >= 3 && player.level() < expLvl && player.gold() >= 50)
    {
        for (const MacroAction& a : legalActions)
        {
            if (a.type == MacroActionType::BuyXp)
            {
                return a;
            }
        }
    }

    const std::vector<ActionScore> scored =
        MacroActionScorer::scoreActions(player, content, legalActions, enemy, pool, stage, roundIndex);

    MacroAction best{};
    float bestScore = AIConstants::SimpleMacroAIInitialBestScore;
    std::string bestReason;

    const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
    const std::int32_t remaining = need > 0 ? std::max<std::int32_t>(0, need - player.xp()) : 0;
    const bool xpLevelsNow = need > 0 && remaining <= ShopSystem::xpForBuy();

    for (const ActionScore& s : scored)
    {
        float adj = s.score;

        const int clampedStage = std::clamp(stage, 1, 9);
        const int targetGold =
            clampedStage <= 2 ? 50 :
            clampedStage == 3 ? 40 :
            clampedStage == 4 ? 30 :
            20;

        if (s.action.type == MacroActionType::BuyXp && xpLevelsNow)
        {
            adj += AIConstants::SimpleMacroAIAllInBuyXpLevelsNowBonus * 0.25f;
        }

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

        if (adj > bestScore)
        {
            best = s.action;
            bestScore = adj;
            bestReason = s.reason;
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
        out << "RolloutPolicy best=" << best.debugName
            << " score=" << std::lround(bestScore)
            << " reason=" << bestReason
            << "\n";
    }

    return best.type == MacroActionType::EndTurn && best.debugName.empty()
        ? MacroAction{ MacroActionType::EndTurn }
        : best;
}
