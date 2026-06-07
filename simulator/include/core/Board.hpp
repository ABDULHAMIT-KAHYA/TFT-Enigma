#pragma once
#include "core/Position.hpp"
#include "core/TeamId.hpp"
#include "content/Ability.hpp"
#include <cstdint>
#include <vector>

class Unit;

class Board {
public:
    Board(std::int32_t width, std::int32_t height);

    bool isInside(const Position& position) const;
    bool isSamePosition(const Position& a, const Position& b) const;
    bool isOccupied(const Position& position) const;
    bool canMoveTo(const Position& nextPosition) const;
    void clearOccupancy();
    void setOccupied(const Position& position, bool occupied);
    void rebuildOccupancy(const std::vector<Position>& occupiedPositions);

    std::int32_t getWidth() const;
    std::int32_t getHeight() const;

private:
    std::size_t indexOf(const Position& position) const;

    std::int32_t width_;
    std::int32_t height_;
    std::vector<std::uint8_t> occupied_;
};

std::vector<Position> getPositionsInArea(const Board& board,
                                        const Position& origin,
                                        AreaShape shape,
                                        std::int32_t radius,
                                        const Position& directionTarget);

std::vector<Unit*> getUnitsInArea(const Board& board,
                                 std::vector<Unit>& units,
                                 const Position& origin,
                                 AreaShape shape,
                                 std::int32_t radius,
                                 TeamId teamFilter,
                                 const Position& directionTarget);
