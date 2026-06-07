#include "combat/Targeting.hpp"
#include <cstdint>
#include <limits>

Unit* findNearestEnemy(Unit& attacker, std::vector<Unit>& enemies)
{
    Unit* best = nullptr;
    std::int32_t bestDistance = (std::numeric_limits<std::int32_t>::max)();

    for (Unit& enemy : enemies)
    {
        if (!enemy.isAlive())
        {
            continue;
        }

        const std::int32_t d = distanceBetween(attacker.getPosition(), enemy.getPosition());
        if (d < bestDistance)
        {
            bestDistance = d;
            best = &enemy;
        }
    }

    return best;
}
