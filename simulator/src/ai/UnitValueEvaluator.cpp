#include "ai/UnitValueEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

static float starMultiplier(int starLevel)
{
    const int s = std::clamp(starLevel, 1, 3);
    return s == 1 ? AIConstants::OneStarMultiplier : s == 2 ? AIConstants::TwoStarMultiplier : AIConstants::ThreeStarMultiplier;
}

static float statCarryScore(const ChampionDefinition& def)
{
    const float ad = static_cast<float>(std::max(0, def.ad));
    const float as = std::max(0.0f, def.attackSpeed);
    const float range = static_cast<float>(std::max(1, def.range));
    const float ap = std::max(0.0f, def.abilityPower);
    const float crit = std::clamp(def.critChance, 0.0f, 1.0f);

    return ad * as * (AIConstants::CarryRangeBase + AIConstants::CarryRangeScale * range) +
           ap * AIConstants::CarryApWeight +
           crit * AIConstants::CarryCritWeight;
}

static float statFrontlineScore(const ChampionDefinition& def)
{
    const float hp = static_cast<float>(std::max(1, def.hp));
    const float armor = static_cast<float>(std::max(0, def.armor));
    const float mr = static_cast<float>(std::max(0, def.magicResist));

    return hp * (1.0f + (armor + mr) * AIConstants::FrontlineDefenseScale);
}

static std::unordered_map<std::string, int> traitCountsFromBoard(const PlayerState& player,
                                                                 const ContentManager& content)
{
    std::unordered_map<std::string, int> counts;

    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }

        for (const std::string& t : def->traits)
        {
            if (!t.empty())
            {
                counts[t] += 1;
            }
        }
    }

    return counts;
}

static float traitUnitBonus(const ChampionDefinition& def,
                            const std::unordered_map<std::string, int>& currentCounts,
                            const ContentManager& content)
{
    float bonus = 0.0f;

    for (const std::string& t : def.traits)
    {
        if (t.empty())
        {
            continue;
        }

        const auto it = currentCounts.find(t);
        const int c = it == currentCounts.end() ? 0 : it->second;

        const TraitDefinition* td = content.getTrait(t);
        if (!td)
        {
            bonus += AIConstants::TraitUnitMissingDefinitionBonus;
            continue;
        }

        int next = 0;

        for (int bp : td->trait.breakpoints)
        {
            if (bp > c)
            {
                next = bp;
                break;
            }
        }

        if (next > 0)
        {
            const int delta = next - c;

            if (delta == 1)
            {
                bonus += AIConstants::TraitUnitNear1Bonus;
            }
            else if (delta == 2)
            {
                bonus += AIConstants::TraitUnitNear2Bonus;
            }
            else
            {
                bonus += AIConstants::TraitUnitFarBonus;
            }
        }
        else
        {
            bonus += AIConstants::TraitUnitNoNextBonus;
        }
    }

    return bonus;
}

UnitValueBreakdown UnitValueEvaluator::evaluate(const OwnedUnit& unit,
                                                const ContentManager& content,
                                                const PlayerState* owner)
{
    UnitValueBreakdown s{};

    const ChampionDefinition* def = content.getChampion(unit.championName);
    if (!def)
    {
        return s;
    }

    const float costBase = static_cast<float>(std::max(1, def->cost));

    s.base = costBase * AIConstants::UnitCostBaseWeight;

    const float starMult = starMultiplier(unit.starLevel);
    s.star = (starMult - 1.0f) * AIConstants::UnitStarBonusWeight * costBase;

    const float carryRaw = statCarryScore(*def);
    const float tankRaw = statFrontlineScore(*def);

    // These are the new fields TraitSynergyEvaluator.cpp needs.
    s.carry = carryRaw * AIConstants::UnitCarryContributionWeight;
    s.frontline = tankRaw * AIConstants::UnitFrontlineContributionWeight;

    s.stats = s.carry + s.frontline;

    const ItemValueScore itemScore = ItemValueEvaluator::evaluateUnitItems(unit, content);
    s.items = itemScore.total * AIConstants::UnitItemsContributionWeight;

    if (owner)
    {
        const std::unordered_map<std::string, int> counts = traitCountsFromBoard(*owner, content);
        s.traits = traitUnitBonus(*def, counts, content);
    }

    s.total = (s.base + s.star + s.stats + s.items + s.traits) * starMult;

    return s;
}
