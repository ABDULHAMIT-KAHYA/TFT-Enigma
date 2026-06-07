#pragma once

#include <cstdint>
#include "macro/PlayerState.hpp"

class ContentManager;
struct EnemySnapshot;
class SharedUnitPool;

struct FutureEval
{
    float ev = 0.0f;
    float board = 0.0f;
    float enemyBoard = 0.0f;
    float top4Prob = 0.0f;
    float winProb = 0.0f;
    float placementEV = 0.0f;
};

class FutureStateEvaluator
{
public:
    static FutureEval evaluate(const PlayerState& player,
                               const ContentManager& content,
                               const EnemySnapshot* enemy,
                               const SharedUnitPool* pool,
                               int stage,
                               int roundIndex,
                               const PlayerState* opponent = nullptr);
};
