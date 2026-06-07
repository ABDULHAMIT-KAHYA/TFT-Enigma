#pragma once

#include <vector>
#include "ai/RolloutBranch.hpp"
#include "core/Random.hpp"

class ContentManager;
class PlayerState;
class SharedUnitPool;
struct EnemySnapshot;

struct BranchPrunerConfig
{
    int topKActions = 10;
};

class BranchPruner
{
public:
    static std::vector<RolloutBranch> buildCandidates(const PlayerState& player,
                                                      const ContentManager& content,
                                                      const std::vector<MacroAction>& legalActions,
                                                      const EnemySnapshot* enemy,
                                                      const SharedUnitPool* pool,
                                                      int stage,
                                                      int roundIndex,
                                                      const BranchPrunerConfig& cfg);
};
