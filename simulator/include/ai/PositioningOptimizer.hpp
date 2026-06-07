#pragma once
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "ai/ScoutSystem.hpp"
class PositioningOptimizer
{
public:
    static void optimize(PlayerState& player,
                         const ContentManager& content,
                         const EnemySnapshot* enemy);

    static float score(const PlayerState& player,
                       const ContentManager& content,
                       const EnemySnapshot* enemy);
};
