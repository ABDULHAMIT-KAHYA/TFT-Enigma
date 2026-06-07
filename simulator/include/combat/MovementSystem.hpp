#pragma once
#include "core/Board.hpp"
#include "core/Unit.hpp"
namespace MovementSystem
{
    bool tryMoveToward(Unit& mover, const Unit& target, Board& board);
}
