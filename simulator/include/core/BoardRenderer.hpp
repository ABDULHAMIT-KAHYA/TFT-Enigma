#pragma once

#include <vector>
#include "core/Board.hpp"
#include "core/Unit.hpp"
class BoardRenderer
{
public:
    static void print(const Board& board,
                      const std::vector<Unit>& units,
                      const std::vector<Position>& highlightedCells = {});
};
