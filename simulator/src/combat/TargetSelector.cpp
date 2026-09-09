#include "combat/TargetSelector.hpp"
#include "core/Board.hpp"
#include <algorithm>
#include <limits>

static bool isValidTarget(const Unit& attacker, const Unit& candidate)
{
    if (!candidate.isAlive()) return false;
    if (!candidate.isEnemyOf(attacker)) return false;
    if (candidate.isUntargetable()) return false;
    return true;
}

static Unit* findById(std::vector<Unit>& allUnits, UnitId id)
{
    if (!isValid(id)) return nullptr;
    for (Unit& unit : allUnits)
    {
        if (unit.id() == id) return &unit;
    }
    return nullptr;
}

static int manhattanDistance(const Position& a, const Position& b)
{
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

static int frontlineScore(const Unit& candidate, int midY)
{
    return std::abs(candidate.getPosition().y - midY);
}

Unit* TargetSelector::selectTarget(const Unit& attacker,
                                  std::vector<Unit>& allUnits,
                                  const CombatTargetContext& ctx,
                                  std::int32_t timeMs,
                                  TargetPriority priority)
{
    Unit* castLocked = findById(allUnits, ctx.castLockedTargetId);
    if (castLocked && timeMs < ctx.castLockUntilMs && isValidTarget(attacker, *castLocked))
    {
        return castLocked;
    }

    Unit* current = findById(allUnits, ctx.currentTargetId);
    if (current && isValidTarget(attacker, *current))
    {
        return current;
    }

    if (timeMs < ctx.retargetLockedUntilMs)
    {
        return nullptr;
    }

    Unit* best = nullptr;
    int bestDist = std::numeric_limits<int>::max();
    int bestFront = std::numeric_limits<int>::max();

    int maxY = 0;
    for (const Unit& u : allUnits)
    {
        maxY = std::max(maxY, u.getPosition().y);
    }
    const int midY = maxY / 2;

    for (Unit& u : allUnits)
    {
        if (!isValidTarget(attacker, u)) continue;

        const int dist = manhattanDistance(attacker.getPosition(), u.getPosition());
        const int front = frontlineScore(u, midY);

        if (!best)
        {
            best = &u;
            bestDist = dist;
            bestFront = front;
            continue;
        }

        if (priority == TargetPriority::FrontlineFirst)
        {
            if (front < bestFront || (front == bestFront && dist < bestDist))
            {
                best = &u;
                bestDist = dist;
                bestFront = front;
            }
        }
        else if (dist < bestDist)
        {
            best = &u;
            bestDist = dist;
            bestFront = front;
        }
    }

    return best;
}
