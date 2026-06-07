#pragma once

#include <string>
#include <vector>
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/CompDirectionPlanner.hpp"
#include "macro/MacroAction.hpp"
#include "macro/PlayerState.hpp"
#include "ai/ScoutSystem.hpp"
class SharedUnitPool;

struct ActionScore
{
    MacroAction action{};
    float score = 0.0f;
    std::string reason{};
};

class MacroActionScorer
{
public:
    static std::vector<ActionScore> scoreActions(const PlayerState& player,
                                                 const ContentManager& content,
                                                 const std::vector<MacroAction>& legalActions,
                                                 const EnemySnapshot* enemy,
                                                 const SharedUnitPool* pool,
                                                 int stage,
                                                 int roundIndex);
};
