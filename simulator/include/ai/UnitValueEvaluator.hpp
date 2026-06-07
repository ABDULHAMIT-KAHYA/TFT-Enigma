#pragma once

#include <string>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
struct UnitValueBreakdown
{
    float total = 0.0f;

    float base = 0.0f;
    float star = 0.0f;
    float stats = 0.0f;
    float items = 0.0f;
    float traits = 0.0f;

    // ADD THESE
    float carry = 0.0f;
    float frontline = 0.0f;

    std::string debug{};
};

class UnitValueEvaluator
{
public:
    static UnitValueBreakdown evaluate(const OwnedUnit& unit,
                                       const ContentManager& content,
                                       const PlayerState* owner);
};