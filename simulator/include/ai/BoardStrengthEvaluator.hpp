#pragma once

#include <string>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
struct BoardScore
{
    float total = 0.0f;
    float unitPower = 0.0f;
    float traitPower = 0.0f;
    float itemPower = 0.0f;
    float frontlinePower = 0.0f;
    float carryPower = 0.0f;
    float economyPower = 0.0f;
    float upgradePotential = 0.0f;
    std::string debug{};
};

class BoardStrengthEvaluator
{
public:
    static BoardScore evaluate(const PlayerState& player, const ContentManager& content);
};

