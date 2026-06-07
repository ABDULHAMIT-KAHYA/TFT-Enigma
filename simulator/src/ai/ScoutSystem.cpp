#include "ai/ScoutSystem.hpp"
#include "constants/AIConstants.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "constants/GameConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include <algorithm>
#include <unordered_map>

static float unitStrength(const OwnedUnit& u)
{
    const int star = std::clamp(u.starLevel, 1, 3);
    const float starMult = star == 1 ? 1.0f : star == 2 ? AIConstants::ScoutStar2Multiplier : AIConstants::ScoutStar3Multiplier;
    return static_cast<float>(std::max(1, u.cost)) * starMult;
}

static float itemStrength(const Item& item)
{
    float s = 0.0f;
    for (const StatusEffect& e : item.passiveStats)
    {
        if (e.modifierType == ModifierType::Flat)
        {
            s += std::abs(e.value);
        }
        else
        {
            s += std::abs(e.value) * AIConstants::PercentScale;
        }
    }
    return s;
}

static bool hasThreatTag(const ChampionDefinition& def, const char* tag)
{
    for (const std::string& t : def.tags)
    {
        if (t == tag)
        {
            return true;
        }
    }
    return false;
}

EnemySnapshot ScoutSystem::snapshot(const PlayerState& enemy, const ContentManager& content)
{
    EnemySnapshot snap{};
    snap.level = enemy.level();
    snap.gold = enemy.gold();
    snap.xp = enemy.xp();
    snap.winStreak = enemy.winStreak();
    snap.loseStreak = enemy.loseStreak();
    snap.hp = enemy.health();

    snap.boardStrength = BoardStrengthEvaluator::evaluate(enemy, content).total;

    snap.units.reserve(enemy.board().size());
    for (const OwnedUnit& u : enemy.board())
    {
        const Position pos = u.hasFormation ? u.formation : Position{ GameConstants::DefaultFormationX, GameConstants::DefaultFormationY };
        snap.positioning.push_back(pos);

        EnemySnapshot::UnitInfo info{};
        info.championName = u.championName;
        info.cost = std::max(1, u.cost);
        info.starLevel = std::clamp(u.starLevel, 1, 3);
        info.position = pos;

        if (const ChampionDefinition* def = content.getChampion(u.championName))
        {
            info.range = std::max(1, static_cast<int>(def->range));
            info.hasJumpThreat = hasThreatTag(*def, "assassin") || hasThreatTag(*def, "jump") || hasThreatTag(*def, "jumper");

            const ItemValueScore items = ItemValueEvaluator::evaluateUnitItems(u, content);
            info.itemOffense = items.offense;
            info.itemDefense = items.defense;
            info.itemCaster = items.caster;

            const float base = unitStrength(u) * AIConstants::ScoutUnitBaseThreatScale;
            const float statThreat =
                static_cast<float>(std::max(0, def->ad)) * def->attackSpeed * AIConstants::ScoutAdAsThreatScale +
                def->abilityPower * AIConstants::ScoutApThreatScale +
                def->critChance * AIConstants::ScoutCritThreatScale;
            const float itemThreat =
                items.offense * AIConstants::ScoutItemOffenseThreatScale +
                items.caster * AIConstants::ScoutItemCasterThreatScale;
            info.threat = base + statThreat + itemThreat;

            info.aoeThreat = items.caster * AIConstants::ScoutAoeCasterThreatScale +
                             std::max(0.0f, def->abilityPower) * AIConstants::ScoutAoeApThreatScale;
            if (info.range > 1)
            {
                info.aoeThreat += AIConstants::ScoutAoeThreatBonus;
            }
        }
        else
        {
            info.threat = unitStrength(u) * AIConstants::ScoutUnitBaseThreatScale;
        }

        for (const std::string& itemName : u.items)
        {
            if (const Item* item = content.getItem(itemName))
            {
                snap.itemStrength += itemStrength(*item);
            }
        }

        snap.units.push_back(std::move(info));
    }

    int bestCarry = -1;
    float bestCarryScore = -1.0f;
    for (std::size_t i = 0; i < enemy.board().size(); ++i)
    {
        const OwnedUnit& u = enemy.board()[i];
        float score = unitStrength(u) + static_cast<float>(u.items.size()) * AIConstants::ScoutCarryItemCountScale;
        if (score > bestCarryScore)
        {
            bestCarryScore = score;
            bestCarry = static_cast<int>(i);
        }
    }
    snap.carryUnit = bestCarry;

    for (std::size_t i = 0; i < enemy.board().size(); ++i)
    {
        const OwnedUnit& u = enemy.board()[i];
        if (u.hasFormation && u.formation.y <= 0)
        {
            snap.frontlineUnits.push_back(static_cast<int>(i));
        }
    }

    std::unordered_map<std::string, int> traitCounts;
    for (const OwnedUnit& u : enemy.board())
    {
        if (const ChampionDefinition* def = content.getChampion(u.championName))
        {
            for (const std::string& t : def->traits)
            {
                if (!t.empty())
                {
                    traitCounts[t] += 1;
                }
            }
        }
    }

    for (const auto& [t, c] : traitCounts)
    {
        const TraitDefinition* td = content.getTrait(t);
        if (!td)
        {
            continue;
        }
        int bp = 0;
        for (int b : td->trait.breakpoints)
        {
            if (c >= b) bp = b;
        }
        if (bp > 0)
        {
            snap.activeTraits.push_back(t);
        }
    }
    std::sort(snap.activeTraits.begin(), snap.activeTraits.end());

    return snap;
}
