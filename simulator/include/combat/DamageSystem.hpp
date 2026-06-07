#pragma once

#include <cstdint>
#include "combat/DamageType.hpp"
#include "core/RandomManager.hpp"
#include "core/Unit.hpp"
struct DamageDebugResult
{
    std::int32_t rawDamage;
    std::int32_t defenseUsed;
    float durabilityUsed;
    std::int32_t preDurabilityDamage;
    std::int32_t finalDamage;
    DamageType damageType;
};

class DamageSystem
{
public:
    static std::int32_t calculateDamage(std::int32_t rawDamage,
                                        DamageType damageType,
                                        const Unit& target);

    static DamageDebugResult calculateDamageDebug(std::int32_t rawDamage,
                                                  DamageType damageType,
                                                  const Unit& target);

    static void setSeed(std::uint32_t seed);
    static bool rollChance(float chance01);
};
