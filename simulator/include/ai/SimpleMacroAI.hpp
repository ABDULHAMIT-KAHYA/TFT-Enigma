#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/MacroAction.hpp"
#include "macro/PlayerState.hpp"
#include "ai/ScoutSystem.hpp"
#include "core/Random.hpp"
class SharedUnitPool;

struct SimpleMacroAIConfig
{
    bool enableRollouts = false;
    bool rolloutDebug = false;
    int rolloutDepthRounds = 3;
    int rolloutBranchesPerAction = 8;
    int rolloutTopKActions = 10;
    int rolloutMaxActionsPerTurn = 12;
};

class SimpleMacroAI
{
public:
    explicit SimpleMacroAI(std::uint32_t seed, SimpleMacroAIConfig config = {});

    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions);

    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions,
                             const EnemySnapshot* enemy,
                             const SharedUnitPool* pool,
                             int stage,
                             int roundIndex,
                             const Random& rng,
                             bool isTurnStart,
                             bool verbose,
                             std::ostream& out);

private:
    std::uint32_t seed_ = 0;
    SimpleMacroAIConfig config_{};
};
