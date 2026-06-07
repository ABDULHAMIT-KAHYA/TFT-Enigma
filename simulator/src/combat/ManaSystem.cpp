#include "combat/ManaSystem.hpp"
#include "validation/CombatValidation.hpp"
#include "constants/CombatConstants.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace ManaSystem
{
    std::int32_t gainFromDamageTaken(std::int32_t hpLost, std::int32_t maxMana)
    {
        if (hpLost <= 0 || maxMana <= 0)
        {
            return 0;
        }
        const float manaPerDamage = CombatConstants::ManaPerHpLostDamageTaken;
        const std::int32_t raw =
            static_cast<std::int32_t>(std::lround(static_cast<float>(hpLost) * manaPerDamage));
        const std::int32_t cap =
            std::max<std::int32_t>(1, static_cast<std::int32_t>(std::lround(static_cast<float>(maxMana) * CombatConstants::ManaDamageTakenCapFraction)));
        return std::clamp<std::int32_t>(raw, 1, cap);
    }

    std::int32_t applyDamageTakenMana(GameState& state, Unit& unit, std::int32_t hpLost)
    {
        const std::int32_t gain = gainFromDamageTaken(hpLost, unit.getMaxMana());
        if (gain <= 0)
        {
            return 0;
        }

        const std::int32_t before = unit.getMana();
        unit.gainMana(gain);
        const std::int32_t after = unit.getMana();
        const std::int32_t applied = std::max<std::int32_t>(0, after - before);

        if (CombatValidation::enabled() && CombatValidation::detailedLogs() && applied > 0)
        {
            std::ostringstream ss;
            ss << state.timeMs() << "ms " << unit.getName() << " gains +" << applied << " mana from damage taken";
            state.logger().combat(ss.str());
        }

        return applied;
    }
}
