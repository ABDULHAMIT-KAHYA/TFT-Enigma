#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "macro/MacroAction.hpp"

class ContentManager;
class PlayerState;
struct EnemySnapshot;

struct StateFeatures
{
    int schemaVersion = 2;
    std::vector<float> values{};
};

struct ActionEncoding
{
    int schemaVersion = 1;
    int actionType = 0;
    int shopIndex = -1;
    int boardIndex = -1;
    int benchIndex = -1;
    int itemIndex = -1;
    int targetX = -1;
    int targetY = -1;
    int unitContentId = 0;
    int auxiliaryId = 0;
    int goldCost = 0;
};

namespace TrainingData
{
    int featureSchemaVersion();
    int actionSchemaVersion();

    const std::vector<std::string>& stateFeatureNames();

    StateFeatures encodeState(const PlayerState& player,
                              const ContentManager& content,
                              int roundIndex,
                              int stage,
                              int alivePlayers,
                              const EnemySnapshot* enemy);

    ActionEncoding encodeAction(const MacroAction& action,
                                const PlayerState& player,
                                const ContentManager& content);

    std::vector<float> encodeStateActionFeatures(const StateFeatures& state,
                                                const ActionEncoding& action);
}

