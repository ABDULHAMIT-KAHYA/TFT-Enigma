#include "combat/DamageSystem.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/StatSystem.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

static float mitigationMultiplier(float defense)
{
    const float d = std::max(0.0f, defense);
    return CombatConstants::DefenseMitigationPercentScale /
           (CombatConstants::DefenseMitigationPercentScale + d);
}

std::int32_t DamageSystem::calculateDamage(std::int32_t rawDamage,
                                           DamageType damageType,
                                           const Unit& target)
{
    if (rawDamage <= 0)
    {
        return 0;
    }

    const float durability = StatSystem::getFinalStat(target, StatType::DamageReduction);
    const float durabilityMultiplier = 1.0f - std::clamp(durability, 0.0f, CombatConstants::MaxDamageReduction);

    switch (damageType)
    {
        case DamageType::Physical:
        {
            const float armor = static_cast<float>(StatSystem::getFinalStatInt(target, StatType::Armor));
            const float pre = static_cast<float>(rawDamage) * mitigationMultiplier(armor);
            return static_cast<std::int32_t>(std::lround(std::max(0.0f, pre) * durabilityMultiplier));
        }

        case DamageType::Magic:
        {
            const float mr = static_cast<float>(StatSystem::getFinalStatInt(target, StatType::MagicResist));
            const float pre = static_cast<float>(rawDamage) * mitigationMultiplier(mr);
            return static_cast<std::int32_t>(std::lround(std::max(0.0f, pre) * durabilityMultiplier));
        }

        case DamageType::TrueDamage:
            return static_cast<std::int32_t>(std::lround(static_cast<float>(rawDamage) * durabilityMultiplier));
    }

    return 0;
}


DamageDebugResult DamageSystem::calculateDamageDebug(std::int32_t rawDamage,
                                                     DamageType damageType,
                                                     const Unit& target)
{
    DamageDebugResult result{};
    result.rawDamage = rawDamage;
    result.damageType = damageType;
    result.durabilityUsed =
        std::clamp(StatSystem::getFinalStat(target, StatType::DamageReduction), 0.0f, CombatConstants::MaxDamageReduction);
    result.preDurabilityDamage = 0;

    if (rawDamage <= 0)
    {
        result.defenseUsed = 0;
        result.finalDamage = 0;
        return result;
    }

    switch (damageType)
    {
        case DamageType::Physical:
            result.defenseUsed = StatSystem::getFinalStatInt(target, StatType::Armor);
            result.preDurabilityDamage =
                static_cast<std::int32_t>(
                    std::lround(static_cast<float>(rawDamage) * mitigationMultiplier(static_cast<float>(result.defenseUsed)))
                );
            result.finalDamage =
                static_cast<std::int32_t>(
                    std::lround(static_cast<float>(result.preDurabilityDamage) * (1.0f - result.durabilityUsed))
                );
            return result;

        case DamageType::Magic:
            result.defenseUsed = StatSystem::getFinalStatInt(target, StatType::MagicResist);
            result.preDurabilityDamage =
                static_cast<std::int32_t>(
                    std::lround(static_cast<float>(rawDamage) * mitigationMultiplier(static_cast<float>(result.defenseUsed)))
                );
            result.finalDamage =
                static_cast<std::int32_t>(
                    std::lround(static_cast<float>(result.preDurabilityDamage) * (1.0f - result.durabilityUsed))
                );
            return result;

        case DamageType::TrueDamage:
            result.defenseUsed = 0;
            result.preDurabilityDamage = rawDamage;
            result.finalDamage =
                static_cast<std::int32_t>(
                    std::lround(static_cast<float>(rawDamage) * (1.0f - result.durabilityUsed))
                );
            return result;
    }

    result.defenseUsed = 0;
    result.finalDamage = 0;
    return result;
}

void DamageSystem::setSeed(std::uint32_t seed)
{
    RandomManager::global().setSeed(seed);
}

bool DamageSystem::rollChance(float chance01)
{
    const float c = std::clamp(chance01, 0.0f, 1.0f);
    const float r = RandomManager::global().randomFloat01();
    return r < c;
}
