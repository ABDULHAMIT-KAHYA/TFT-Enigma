#pragma once

#include <cstdint>
#include <vector>
#include "core/TeamId.hpp"
#include "core/Unit.hpp"
enum class TargetPriority
{
    Nearest,
    FrontlineFirst
};

struct CombatTargetContext
{
    Unit* currentTarget = nullptr;
    std::int32_t retargetLockedUntilMs = 0;
    Unit* castLockedTarget = nullptr;
    std::int32_t castLockUntilMs = 0;
};

class TargetSelector
{
public:
    static Unit* selectTarget(const Unit& attacker,
                              std::vector<Unit>& allUnits,
                              const CombatTargetContext& ctx,
                              std::int32_t timeMs,
                              TargetPriority priority);
};

