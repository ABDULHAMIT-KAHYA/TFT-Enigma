#pragma once

#include <cstdint>
#include "core/GameState.hpp"
#include "core/Unit.hpp"
#include "core/UnitId.hpp"

enum class SpellExecutionPhase
{
    Windup,
    Resolve,
    Recovery
};

struct SpellCastContext
{
    UnitId casterId{};
    UnitId targetId{};
    std::int32_t castStartMs = 0;
    std::int32_t windupMs = 0;
    std::int32_t recoveryMs = 0;
};

class SpellResolver
{
public:
    static bool beginCast(GameState& state, Unit& caster, Unit& primaryTarget);
};
