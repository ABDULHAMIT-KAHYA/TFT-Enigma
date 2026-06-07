#include "combat/TargetingSystem.hpp"
#include "core/RandomManager.hpp"
#include <cstdint>
#include <limits>

namespace TargetingSystem
{
    Unit* findNearestEnemy(Unit& attacker, std::vector<Unit>& candidates)
    {
        std::int32_t bestDistance = (std::numeric_limits<std::int32_t>::max)();
        std::vector<Unit*> best;

        for (Unit& unit : candidates)
        {
            if (!unit.isAlive())
            {
                continue;
            }

            if (!attacker.isEnemyOf(unit))
            {
                continue;
            }

            const std::int32_t d = distanceBetween(attacker.getPosition(), unit.getPosition());
            if (d < bestDistance)
            {
                bestDistance = d;
                best.clear();
                best.push_back(&unit);
            }
            else if (d == bestDistance)
            {
                best.push_back(&unit);
            }
        }

        if (best.empty())
        {
            return nullptr;
        }
        if (best.size() == 1)
        {
            return best.front();
        }
        const int idx = RandomManager::global().randomInt(static_cast<int>(best.size()));
        return best[static_cast<std::size_t>(idx)];
    }
}
