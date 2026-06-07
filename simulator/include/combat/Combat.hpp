#pragma once

#include <vector>
#include "core/Unit.hpp"
#include "core/Board.hpp"
class GameState;

class Combat {
public:
    void run(std::vector<Unit>& teamA,
             std::vector<Unit>& teamB,
             Board& board);

    void run(GameState& state);
};
