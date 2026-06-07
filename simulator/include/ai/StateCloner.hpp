#pragma once

#include <cstdint>
#include "ai/ScoutSystem.hpp"
#include "core/Random.hpp"
#include "macro/PlayerState.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/ShopSystem.hpp"

class ContentManager;

struct ClonedMacroState
{
    PlayerState player;
    SharedUnitPool pool;
    Random rng;
    ShopSystem shop;
    RoundSystem rounds;

    ClonedMacroState(const ContentManager& content,
                     const PlayerState& p,
                     const SharedUnitPool& sharedPool,
                     const Random& r);
};

class StateCloner
{
public:
    static ClonedMacroState clone(const ContentManager& content,
                                  const PlayerState& player,
                                  const SharedUnitPool& pool,
                                  const Random& rng);

    static PlayerState enemyFromSnapshot(const EnemySnapshot& snap);
};

