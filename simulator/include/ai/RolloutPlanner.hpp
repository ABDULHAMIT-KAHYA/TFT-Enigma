#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>
#include "ai/RolloutBranch.hpp"
#include "ai/BranchPruner.hpp"
#include "core/Random.hpp"

class ContentManager;
class PlayerState;
class SharedUnitPool;
struct EnemySnapshot;

struct RolloutPlannerConfig
{
    int depthRounds = 3;
    int branchesPerAction = 8;
    int topKActions = 10;
    int maxActionsPerTurn = 12;
    bool debug = false;
};

class RolloutPlanner
{
public:
    explicit RolloutPlanner(std::uint32_t seed, RolloutPlannerConfig cfg);

    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions,
                             const EnemySnapshot* enemy,
                             const SharedUnitPool* pool,
                             int stage,
                             int roundIndex,
                             const Random& rng,
                             std::ostream& out) const;

private:
    std::uint32_t seed_ = 0;
    RolloutPlannerConfig cfg_{};
};

