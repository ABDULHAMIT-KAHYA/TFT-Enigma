#pragma once

#include <string>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
class SharedUnitPool;

struct UpgradePotentialScore
{
    float total = 0.0f;
    float nearTwoStar = 0.0f;
    float nearThreeStar = 0.0f;
    float shopPairs = 0.0f;
    std::string debug{};
};

class UpgradePotentialEvaluator
{
public:
    static UpgradePotentialScore evaluate(const PlayerState& player, const ContentManager& content);
    static UpgradePotentialScore evaluate(const PlayerState& player,
                                          const ContentManager& content,
                                          const SharedUnitPool* pool);
};
