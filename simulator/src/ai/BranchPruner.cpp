#include "ai/BranchPruner.hpp"
#include "ai/MacroActionScorer.hpp"
#include <algorithm>
#include <unordered_set>

static std::string actionKey(const MacroAction& a)
{
    switch (a.type)
    {
        case MacroActionType::BuyUnit:
            return "BUY|" + std::to_string(a.shopIndex);
        case MacroActionType::SellUnit:
            return "SELL|" + std::to_string(a.benchIndex) + "|" + std::to_string(a.boardIndex);
        case MacroActionType::RerollShop:
            return "RR";
        case MacroActionType::BuyXp:
            return "XP";
        case MacroActionType::MoveBenchToBoard:
            return "B2F|" + std::to_string(a.benchIndex);
        case MacroActionType::MoveBoardToBench:
            return "F2B|" + std::to_string(a.boardIndex);
        case MacroActionType::RepositionUnit:
            return "POS|" + std::to_string(a.boardIndex) + "|" + std::to_string(a.targetPosition.x) + "|" + std::to_string(a.targetPosition.y);
        case MacroActionType::EquipItem:
            return "ITEM|" + std::to_string(a.boardIndex) + "|" + std::to_string(a.itemIndex);
        case MacroActionType::EndTurn:
            return "END";
    }
    return "";
}

std::vector<RolloutBranch> BranchPruner::buildCandidates(const PlayerState& player,
                                                         const ContentManager& content,
                                                         const std::vector<MacroAction>& legalActions,
                                                         const EnemySnapshot* enemy,
                                                         const SharedUnitPool* pool,
                                                         int stage,
                                                         int roundIndex,
                                                         const BranchPrunerConfig& cfg)
{
    std::vector<ActionScore> scored = MacroActionScorer::scoreActions(player, content, legalActions, enemy, pool, stage, roundIndex);
    std::stable_sort(scored.begin(), scored.end(), [](const ActionScore& a, const ActionScore& b)
    {
        return a.score > b.score;
    });

    const int topK = std::clamp(cfg.topKActions, 1, static_cast<int>(scored.size()));
    std::unordered_set<std::string> seen;

    std::vector<RolloutBranch> out;
    out.reserve(static_cast<std::size_t>(topK));
    for (int i = 0; i < topK; ++i)
    {
        const MacroAction& a = scored[static_cast<std::size_t>(i)].action;
        const std::string k = actionKey(a);
        if (!seen.insert(k).second)
        {
            continue;
        }
        RolloutBranch b{};
        b.action = a;
        b.heuristicScore = scored[static_cast<std::size_t>(i)].score;
        out.push_back(std::move(b));
    }

    bool hasEnd = false;
    for (const RolloutBranch& b : out)
    {
        if (b.action.type == MacroActionType::EndTurn)
        {
            hasEnd = true;
            break;
        }
    }
    if (!hasEnd)
    {
        RolloutBranch b{};
        b.action = MacroAction{ MacroActionType::EndTurn };
        b.action.debugName = "EndTurn";
        b.heuristicScore = -1e9f;
        out.push_back(std::move(b));
    }

    return out;
}
