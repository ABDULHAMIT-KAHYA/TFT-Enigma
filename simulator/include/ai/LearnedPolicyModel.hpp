#pragma once

#include <string>
#include <vector>

#include "macro/MacroAction.hpp"
#include "macro/TrainingData.hpp"

class ContentManager;
class PlayerState;
struct EnemySnapshot;

class LearnedPolicyModel
{
public:
    bool load(const std::string& path, std::string& error);
    bool isLoaded() const;
    const std::string& path() const;

    float score(const StateFeatures& state, const ActionEncoding& action) const;
    MacroAction chooseAction(const PlayerState& player,
                             const ContentManager& content,
                             const std::vector<MacroAction>& legalActions,
                             const EnemySnapshot* enemy,
                             int stage,
                             int roundIndex) const;

private:
    std::string path_{};
    std::vector<float> weights_{};
    float bias_ = 0.0f;
};
