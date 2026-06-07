#include "content/UnitFactory.hpp"
#include "constants/CombatConstants.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

UnitFactory::UnitFactory(const ContentManager& content)
    : content_(&content)
{
}

Unit UnitFactory::createFromChampion(std::string_view championName,
                                    const Position& position,
                                    TeamId team) const
{
    const ChampionDefinition* c = content_->getChampion(championName);
    if (!c)
    {
        throw std::runtime_error("Unknown champion: " + std::string(championName));
    }

    const std::int32_t cdMs =
        c->attackSpeed > 0.0f
            ? std::max<std::int32_t>(
                  1,
                  static_cast<std::int32_t>(
                      std::lround(static_cast<float>(CombatConstants::MsPerSecond) / c->attackSpeed)))
            : CombatConstants::MsPerSecond;

    Ability ability{};
    if (!c->abilityId.empty())
    {
        if (const Ability* a = content_->getAbility(c->abilityId))
        {
            ability = *a;
        }
    }

    Unit unit(
        c->name,
        c->hp,
        c->ad,
        c->armor,
        c->magicResist,
        cdMs,
        c->range,
        c->autoAttackDamageType,
        position,
        team,
        c->maxMana,
        c->manaGainOnAttack,
        ability
    );

    unit.setAbilityPower(c->abilityPower);
    unit.setAttackSpeed(c->attackSpeed);
    unit.setCritChance(c->critChance);
    unit.setCritDamage(c->critDamage);
    unit.setDurability(c->durability);
    unit.setTraits(c->traits);

    if (c->startMana > 0)
    {
        unit.gainMana(c->startMana);
    }

    return unit;
}
