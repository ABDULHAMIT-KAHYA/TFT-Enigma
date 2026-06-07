#pragma once

#include <cstdint>
#include <vector>
#include "macro/MacroAction.hpp"
#include "macro/PlayerState.hpp"
#include "ai/ScoutSystem.hpp"

class ContentManager;
class SharedUnitPool;
struct EnemySnapshot;

class MonteCarloRolloutPlanner
{
public:
    explicit MonteCarloRolloutPlanner(std::uint32_t seed);

    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions,
                             const EnemySnapshot* enemy,
                             const SharedUnitPool* pool,
                             int candidates,
                             int rolloutsPerCandidate,
                             int maxActions) const;

private:
    std::uint32_t seed_ = 0;
};

