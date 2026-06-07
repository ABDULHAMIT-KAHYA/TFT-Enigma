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

    Unit* casterPtr = &caster;
    Unit* targetPtr = &primaryTarget;
    const std::int32_t startMs = state.timeMs();

    state.scheduleCombatEvent(
        startMs + windup,
        [&state, casterPtr, targetPtr]()
        {
            if (!casterPtr || !casterPtr->isAlive())
            {
                return;
            }
            if (!casterPtr->isCasting())
            {
                return;
            }
            casterPtr->resetManaAfterCast();
            AbilitySystem::executeTrigger(state, *casterPtr, targetPtr, AbilityTrigger::OnCast);

            if (CombatValidation::enabled() && CombatValidation::detailedLogs())
            {
                std::ostringstream ss;
                ss << "CAST_RELEASE " << state.timeMs() << "ms " << casterPtr->getName();
                state.logger().combat(ss.str());
            }
        },
        "SpellResolve"
    );

    state.scheduleCombatEvent(
        startMs + windup + recovery,
        [&state, casterPtr]()
        {
            if (!casterPtr || !casterPtr->isAlive())
            {
                return;
            }
            if (!casterPtr->isCasting())
            {
                return;
            }
            casterPtr->endCast();
            if (CombatValidation::enabled() && CombatValidation::detailedLogs())
            {
                std::ostringstream ss;
                ss << "CAST_END " << state.timeMs() << "ms " << casterPtr->getName();
                state.logger().combat(ss.str());
            }
        },
        "SpellEnd"
    );

    return true;
}
