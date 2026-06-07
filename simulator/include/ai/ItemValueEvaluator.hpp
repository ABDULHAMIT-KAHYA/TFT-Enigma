#pragma once

#include <string>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
struct ItemValueScore
{
    float total = 0.0f;
    float offense = 0.0f;
    float defense = 0.0f;
    float caster = 0.0f;
    std::string debug{};
};

class ItemValueEvaluator
{
public:
    static ItemValueScore evaluateItem(const Item& item);
    static ItemValueScore evaluateUnitItems(const OwnedUnit& unit, const ContentManager& content);
};

