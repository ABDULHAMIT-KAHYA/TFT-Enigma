#include "combat/MovementSystem.hpp"
#include <cstdint>
#include <vector>

namespace MovementSystem
{
    static std::int32_t sign(std::int32_t v)
    {
        return (v > 0) - (v < 0);
    }

    static void pushUnique(std::vector<Position>& candidates, Position p)
    {
        for (const Position& existing : candidates)
        {
            if (existing.x == p.x && existing.y == p.y)
            {
                return;
            }
        }
        candidates.push_back(p);
    }

    bool tryMoveToward(Unit& mover, const Unit& target, Board& board)
    {
        if (mover.isInRange(target))
        {
            return false;
        }

        const Position current = mover.getPosition();
        const Position previous = mover.getLastPosition();
        const Position targetPos = target.getPosition();

        const std::int32_t dx = targetPos.x - current.x;
        const std::int32_t dy = targetPos.y - current.y;
        const std::int32_t stepX = sign(dx);
        const std::int32_t stepY = sign(dy);

        std::vector<Position> candidates;
        candidates.reserve(8);

        if (stepX != 0)
        {
            Position p = current;
            p.x += stepX;
            pushUnique(candidates, p);
        }

        if (stepY != 0)
        {
            Position p = current;
            p.y += stepY;
            pushUnique(candidates, p);
        }

        if (stepX != 0)
        {
            Position p1 = current;
            p1.y += 1;
            pushUnique(candidates, p1);

            Position p2 = current;
            p2.y -= 1;
            pushUnique(candidates, p2);

            Position p3 = current;
            p3.x -= stepX;
            pushUnique(candidates, p3);
        }
        else if (stepY != 0)
        {
            Position p1 = current;
            p1.x += 1;
            pushUnique(candidates, p1);

            Position p2 = current;
            p2.x -= 1;
            pushUnique(candidates, p2);

            Position p3 = current;
            p3.y -= stepY;
            pushUnique(candidates, p3);
        }

        const std::int32_t currentDistance = distanceBetween(current, targetPos);
        bool found = false;
        Position best = current;
        std::int32_t bestDistance = currentDistance;

        for (const Position& candidate : candidates)
        {
            if (candidate.x == previous.x && candidate.y == previous.y)
            {
                continue;
            }
            if (!board.canMoveTo(candidate))
            {
                continue;
            }

            const std::int32_t d = distanceBetween(candidate, targetPos);
            if (!found || d < bestDistance)
            {
                best = candidate;
                bestDistance = d;
                found = true;
            }
        }

        if (!found)
        {
            for (const Position& candidate : candidates)
            {
                if (!board.canMoveTo(candidate))
                {
                    continue;
                }

                const std::int32_t d = distanceBetween(candidate, targetPos);
                if (!found || d < bestDistance)
                {
                    best = candidate;
                    bestDistance = d;
                    found = true;
                }
            }
        }

        if (!found)
        {
            return false;
        }

        if (best.x == current.x && best.y == current.y)
        {
            return false;
        }

        mover.applyMove(best);
        return true;
    }
}
