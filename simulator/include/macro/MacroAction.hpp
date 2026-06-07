#pragma once

#include <cstdint>
#include <string>
#include "macro/MacroActionType.hpp"
#include "core/Position.hpp"
struct MacroAction
{
    MacroActionType type = MacroActionType::EndTurn;

    int shopIndex = -1;
    int benchIndex = -1;
    int boardIndex = -1;
    int itemIndex = -1;

    Position targetPosition{ 0, 0 };

    std::int32_t goldCost = 0;
    std::string debugName{};
};

