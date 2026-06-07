#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "content/Ability.hpp"
#include "combat/DamageType.hpp"
#include "content/Item.hpp"
#include "core/Position.hpp"
#include "core/TeamId.hpp"
#include "content/TraitData.hpp"
#include "core/Unit.hpp"
struct ChampionDefinition
{
    std::string name;
    int cost = 1;
    std::string spritePath{};

    std::int32_t hp = 1;
    std::int32_t ad = 0;
    std::int32_t armor = 0;
    std::int32_t magicResist = 0;

    float abilityPower = 0.0f;
    float attackSpeed = 1.0f;
    float critChance = 0.0f;
    float critDamage = 1.5f;
    float durability = 0.0f;

    std::int32_t range = 1;
    DamageType autoAttackDamageType = DamageType::Physical;

    std::int32_t maxMana = 0;
    std::int32_t manaGainOnAttack = 0;
    std::int32_t startMana = 0;

    std::vector<std::string> traits{};
    std::string abilityId{};

    bool isPlayable = true;
    std::vector<std::string> tags{};
};

using ChampionData = ChampionDefinition;
using AbilityData = Ability;

class ContentManager
{
public:
    bool loadAll(const std::string& dataRootDir);

    std::size_t championCount() const;
    std::size_t traitCount() const;
    std::size_t itemCount() const;
    std::size_t abilityCount() const;

    const ChampionDefinition* getChampion(std::string_view name) const;
    const Ability* getAbility(std::string_view id) const;
    const TraitDefinition* getTrait(std::string_view name) const;
    const Item* getItem(std::string_view name) const;

    Unit createUnit(std::string_view championName,
                    const Position& position,
                    TeamId team) const;

    const std::unordered_map<std::string, TraitDefinition>& traits() const;
    const std::unordered_map<std::string, Item>& items() const;
    const std::unordered_map<std::string, Ability>& abilities() const;
    const std::unordered_map<std::string, ChampionDefinition>& champions() const;

private:
    std::unordered_map<std::string, Ability> abilities_;
    std::unordered_map<std::string, TraitDefinition> traits_;
    std::unordered_map<std::string, Item> items_;
    std::unordered_map<std::string, ChampionDefinition> champions_;
};

std::vector<std::string> getLoadedChampionNames(const ContentManager& content);
std::vector<std::string> getPlayableChampionNames(const ContentManager& content);
std::string pickChampionByIndex(const ContentManager& content, std::size_t index);

std::vector<Unit> makeDynamicTeam(const ContentManager& content,
                                 TeamId team,
                                 std::size_t count,
                                 std::size_t startIndex,
                                 const std::vector<Position>& positions = {});
