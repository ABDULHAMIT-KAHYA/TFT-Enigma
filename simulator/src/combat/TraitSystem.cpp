#include "combat/TraitSystem.hpp"
#include "combat/TraitEffectExecutor.hpp"
#include "combat/TraitResolver.hpp"
#include "constants/CombatConstants.hpp"
namespace TraitSystem
{
    static const std::vector<ActiveTrait>& activeTraitsForTeam(const GameState& state, TeamId team)
    {
        const TraitRuntimeState& rt = state.traitRuntime();
        return team == TeamId::TeamA ? rt.activeTraitsA : rt.activeTraitsB;
    }

    void onCombatStart(GameState& state)
    {
        TraitRuntimeState& rt = state.traitRuntime();
        rt.activeTraitsA = TraitResolver::resolveTeamTraits(state, TeamId::TeamA);
        rt.activeTraitsB = TraitResolver::resolveTeamTraits(state, TeamId::TeamB);
        rt.nextPeriodicMsA = state.timeMs() + CombatConstants::TraitPeriodicTickMs;
        rt.nextPeriodicMsB = state.timeMs() + CombatConstants::TraitPeriodicTickMs;
        rt.nextAuraUpdateMsA = state.timeMs() + CombatConstants::TraitAuraUpdateTickMs;
        rt.nextAuraUpdateMsB = state.timeMs() + CombatConstants::TraitAuraUpdateTickMs;

        TraitEffectExecutor::apply(state, TeamId::TeamA, rt.activeTraitsA, TraitHook::OnCombatStart, nullptr, nullptr);
        TraitEffectExecutor::apply(state, TeamId::TeamB, rt.activeTraitsB, TraitHook::OnCombatStart, nullptr, nullptr);
    }

    void tick(GameState& state)
    {
        TraitRuntimeState& rt = state.traitRuntime();
        const std::int32_t now = state.timeMs();

        if (now >= rt.nextPeriodicMsA)
        {
            TraitEffectExecutor::apply(state, TeamId::TeamA, rt.activeTraitsA, TraitHook::Periodic, nullptr, nullptr);
            rt.nextPeriodicMsA += CombatConstants::TraitPeriodicTickMs;
        }
        if (now >= rt.nextPeriodicMsB)
        {
            TraitEffectExecutor::apply(state, TeamId::TeamB, rt.activeTraitsB, TraitHook::Periodic, nullptr, nullptr);
            rt.nextPeriodicMsB += CombatConstants::TraitPeriodicTickMs;
        }

        if (now >= rt.nextAuraUpdateMsA)
        {
            TraitEffectExecutor::apply(state, TeamId::TeamA, rt.activeTraitsA, TraitHook::AuraUpdate, nullptr, nullptr);
            rt.nextAuraUpdateMsA += CombatConstants::TraitAuraUpdateTickMs;
        }
        if (now >= rt.nextAuraUpdateMsB)
        {
            TraitEffectExecutor::apply(state, TeamId::TeamB, rt.activeTraitsB, TraitHook::AuraUpdate, nullptr, nullptr);
            rt.nextAuraUpdateMsB += CombatConstants::TraitAuraUpdateTickMs;
        }
    }

    void onAttack(GameState& state, Unit& attacker, Unit& target)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, attacker.getTeamId());
        TraitEffectExecutor::apply(state, attacker.getTeamId(), active, TraitHook::OnAttack, &attacker, &target);
    }

    void onHit(GameState& state, Unit& attacker, Unit& target)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, attacker.getTeamId());
        TraitEffectExecutor::apply(state, attacker.getTeamId(), active, TraitHook::OnHit, &attacker, &target);
    }

    void onCast(GameState& state, Unit& caster, const Ability&, Unit* target)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, caster.getTeamId());
        TraitEffectExecutor::apply(state, caster.getTeamId(), active, TraitHook::OnCast, &caster, target);
    }

    void onCrit(GameState& state, Unit& attacker, Unit& target)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, attacker.getTeamId());
        TraitEffectExecutor::apply(state, attacker.getTeamId(), active, TraitHook::OnCrit, &attacker, &target);
    }

    void onKill(GameState& state, Unit& killer, Unit& victim)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, killer.getTeamId());
        TraitEffectExecutor::apply(state, killer.getTeamId(), active, TraitHook::OnKill, &killer, &victim);
    }

    void onLowHealth(GameState& state, Unit& unit)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, unit.getTeamId());
        TraitEffectExecutor::apply(state, unit.getTeamId(), active, TraitHook::OnLowHealth, &unit, nullptr);
    }

    void afterDamage(GameState& state, Unit& source, Unit& target)
    {
        const std::vector<ActiveTrait>& active = activeTraitsForTeam(state, source.getTeamId());
        TraitEffectExecutor::apply(state, source.getTeamId(), active, TraitHook::AfterDamage, &source, &target);
    }
}
