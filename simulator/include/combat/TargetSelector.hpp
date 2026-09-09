#pragma once

#include <cstdint>
#include <vector>
#include "core/TeamId.hpp"
#include "core/Unit.hpp"
#include "core/UnitId.hpp"

enum class TargetPriority
{
    Nearest,
    FrontlineFirst
};

struct CombatTargetContext
{
    UnitId currentTargetId{};
    std::int32_t retargetLockedUntilMs = 0;
    UnitId castLockedTargetId{};
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
