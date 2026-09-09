#include "combat/SpellResolver.hpp"
#include "combat/AbilitySystem.hpp"
#include "validation/CombatValidation.hpp"
#include "combat/ItemSystem.hpp"
#include "combat/TraitSystem.hpp"
#include "constants/CombatConstants.hpp"
#include <algorithm>
#include <sstream>

static std::int32_t castWindupMs(const Unit&)
{
    return CombatConstants::SpellWindupMs;
}

static std::int32_t castRecoveryMs(const Unit&)
{
    return CombatConstants::SpellRecoveryMs;
}

bool SpellResolver::beginCast(GameState& state, Unit& caster, Unit& primaryTarget)
{
    if (!caster.isAlive())
    {
        return false;
    }
    if (caster.isCasting())
    {
        return false;
    }
    if (!caster.canCastNow())
    {
        return false;
    }
    if (!caster.canCastAbility())
    {
        return false;
    }

    const Ability& ability = caster.getAbility();

    const std::int32_t windup = castWindupMs(caster);
    const std::int32_t recovery = castRecoveryMs(caster);

    if (CombatValidation::enabled() && CombatValidation::detailedLogs())
    {
        std::ostringstream ss;
        ss << "CAST_BEGIN " << state.timeMs() << "ms "
           << caster.getName()
           << " | ability=" << ability.name
           << " | windup=" << windup
           << " | recovery=" << recovery;
        state.logger().combat(ss.str());
    }

    caster.beginCast(windup, recovery);
    TraitSystem::onCast(state, caster, ability, &primaryTarget);
    ItemSystem::onCast(state, caster, ability, &primaryTarget);

    const UnitId casterId = caster.id();
    const UnitId targetId = primaryTarget.id();
    const std::int32_t startMs = state.timeMs();

    CombatEvent resolve{};
    resolve.type = CombatEventType::SpellResolve;
    resolve.executeAtMs = startMs + windup;
    resolve.sourceId = casterId;
    resolve.targetId = targetId;
    resolve.policy = CombatEventTargetPolicy::RequireAliveSource;
    resolve.debugName = "SpellResolve";
    state.scheduleCombatEvent(resolve);

    CombatEvent end{};
    end.type = CombatEventType::SpellEnd;
    end.executeAtMs = startMs + windup + recovery;
    end.sourceId = casterId;
    end.targetId = targetId;
    end.policy = CombatEventTargetPolicy::RequireAliveSource;
    end.debugName = "SpellEnd";
    state.scheduleCombatEvent(end);

    return true;
}

