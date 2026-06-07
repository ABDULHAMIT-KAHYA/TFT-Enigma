#pragma once

#include <iosfwd>
#include <vector>
#include "core/Random.hpp"
#include "macro/MacroAction.hpp"

class ContentManager;
class PlayerState;
class SharedUnitPool;
struct EnemySnapshot;

class RolloutPolicy
{
public:
    explicit RolloutPolicy(std::uint32_t seed);

    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions,
                             const EnemySnapshot* enemy,
                             const SharedUnitPool* pool,
                             int stage,
                             int roundIndex,
                             const Random& rng,
                             bool verbose,
                             std::ostream& out) const;

private:
    std::uint32_t seed_ = 0;
};

