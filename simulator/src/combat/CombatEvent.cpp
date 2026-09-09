#include "combat/CombatEvent.hpp"

const char* toString(CombatEventType type)
{
    switch (type)
    {
        case CombatEventType::DebugMarker:       return "DebugMarker";
        case CombatEventType::AutoAttackRelease: return "AutoAttackRelease";
        case CombatEventType::ProjectileHit:     return "ProjectileHit";
        case CombatEventType::SpellResolve:      return "SpellResolve";
        case CombatEventType::SpellEnd:          return "SpellEnd";
        case CombatEventType::AbilityEffect:     return "AbilityEffect";
    }
    return "Unknown";
}
