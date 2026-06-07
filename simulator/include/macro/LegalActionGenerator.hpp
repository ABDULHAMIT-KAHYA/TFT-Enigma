#pragma once

#include <vector>
#include "macro/MacroAction.hpp"
#include "macro/PlayerState.hpp"
class LegalActionGenerator
{
public:
    static std::vector<MacroAction> generate(const PlayerState& player);
};

