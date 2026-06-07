#include "core/Board.hpp"
#include "core/Unit.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

Board::Board(std::int32_t width, std::int32_t height)
    : width_(width),
      height_(height),
      occupied_(static_cast<std::size_t>(width > 0 ? width : 0) *
                    static_cast<std::size_t>(height > 0 ? height : 0),
                0)
{
}

bool Board::isInside(const Position& position) const
{
    return position.x >= 0 &&
           position.y >= 0 &&
           position.x < width_ &&
           position.y < height_;
}

bool Board::isSamePosition(const Position& a, const Position& b) const
{
    return a.x == b.x && a.y == b.y;
}

std::size_t Board::indexOf(const Position& position) const
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(position.x);
}

bool Board::isOccupied(const Position& position) const
{
    if (!isInside(position))
    {
        return false;
    }

    return occupied_[indexOf(position)] != 0;
}

bool Board::canMoveTo(const Position& nextPosition) const
{
    if (!isInside(nextPosition))
    {
        return false;
    }

    return !isOccupied(nextPosition);
}

void Board::clearOccupancy()
{
    std::fill(occupied_.begin(), occupied_.end(), 0);
}

void Board::setOccupied(const Position& position, bool occupied)
{
    if (!isInside(position))
    {
        return;
    }

    occupied_[indexOf(position)] = occupied ? 1 : 0;
}

void Board::rebuildOccupancy(const std::vector<Position>& occupiedPositions)
{
    clearOccupancy();
    for (const Position& p : occupiedPositions)
    {
        setOccupied(p, true);
    }
}

std::int32_t Board::getWidth() const
{
    return width_;
}

std::int32_t Board::getHeight() const
{
    return height_;
}

static Position pickHexDirection(const Position& from, const Position& to)
{
    const std::int32_t dx = to.x - from.x;
    const std::int32_t dy = to.y - from.y;

    if (dx == 0 && dy == 0)
    {
        return Position{ 1, 0 };
    }

    const std::array<Position, 6> dirs = {
        Position{ 1, 0 },
        Position{ 0, 1 },
        Position{ -1, 1 },
        Position{ -1, 0 },
        Position{ 0, -1 },
        Position{ 1, -1 }
    };

    std::int32_t bestScore = (std::numeric_limits<std::int32_t>::min)();
    Position best = dirs[0];

    for (const Position& d : dirs)
    {
        const std::int32_t score = d.x * dx + d.y * dy;
        if (score > bestScore)
        {
            bestScore = score;
            best = d;
        }
    }

    return best;
}

std::vector<Position> getPositionsInArea(const Board& board,
                                        const Position& origin,
                                        AreaShape shape,
                                        std::int32_t radius,
                                        const Position& directionTarget)
{
    std::vector<Position> out;
    const std::int32_t r = std::max<std::int32_t>(0, radius);

    if (shape == AreaShape::SingleTarget || shape == AreaShape::Self)
    {
        if (board.isInside(origin))
        {
            out.push_back(origin);
        }
        return out;
    }

    if (shape == AreaShape::Line)
    {
        const Position dir = pickHexDirection(origin, directionTarget);
        Position p = origin;
        for (std::int32_t i = 0; i <= r; ++i)
        {
            if (board.isInside(p))
            {
                out.push_back(p);
            }
            p = Position{ p.x + dir.x, p.y + dir.y };
        }
        return out;
    }

    for (std::int32_t y = 0; y < board.getHeight(); ++y)
    {
        for (std::int32_t x = 0; x < board.getWidth(); ++x)
        {
            const Position p{ x, y };
            if (!board.isInside(p))
            {
                continue;
            }

            const std::int32_t dist = distanceBetween(p, origin);

            if (shape == AreaShape::CircleRadius)
            {
                if (dist <= r)
                {
                    out.push_back(p);
                }
                continue;
            }

            if (shape == AreaShape::Cross)
            {
                if (dist <= r)
                {
                    const std::int32_t dx = p.x - origin.x;
                    const std::int32_t dy = p.y - origin.y;
                    if (dx == 0 || dy == 0 || (dx + dy) == 0)
                    {
                        out.push_back(p);
                    }
                }
                continue;
            }

            if (shape == AreaShape::Grid)
            {
                const std::int32_t dx = std::abs(p.x - origin.x);
                const std::int32_t dy = std::abs(p.y - origin.y);
                if (dx <= r && dy <= r)
                {
                    out.push_back(p);
                }
                continue;
            }

            if (shape == AreaShape::Cone)
            {
                if (dist <= r)
                {
                    const Position dir = pickHexDirection(origin, directionTarget);
                    const std::int32_t dx = p.x - origin.x;
                    const std::int32_t dy = p.y - origin.y;
                    const std::int32_t forward = dir.x * dx + dir.y * dy;
                    if (forward > 0)
                    {
                        out.push_back(p);
                    }
                }
                continue;
            }
        }
    }

    return out;
}

std::vector<Unit*> getUnitsInArea(const Board& board,
                                 std::vector<Unit>& units,
                                 const Position& origin,
                                 AreaShape shape,
                                 std::int32_t radius,
                                 TeamId teamFilter,
                                 const Position& directionTarget)
{
    std::vector<Unit*> out;
    const std::vector<Position> cells =
        getPositionsInArea(board, origin, shape, radius, directionTarget);

    for (Unit& unit : units)
    {
        if (!unit.isAlive())
        {
            continue;
        }
        if (unit.getTeamId() != teamFilter)
        {
            continue;
        }

        for (const Position& p : cells)
        {
            if (unit.getPosition().x == p.x && unit.getPosition().y == p.y)
            {
                out.push_back(&unit);
                break;
            }
        }
    }

    return out;
}
