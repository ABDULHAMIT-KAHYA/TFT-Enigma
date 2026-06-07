#include "ai/BoardStrengthEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include "ai/TraitSynergyEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

static constexpr int MinStarLevel = 1;
static constexpr int TwoStarLevel = 2;
static constexpr int MaxStarLevel = 3;

static float economyScore(const PlayerState& player)
{
    const float gold = static_cast<float>(std::max<std::int32_t>(0, player.gold()));
    const float hp = static_cast<float>(std::max<std::int32_t>(0, player.health()));
    const float lvl = static_cast<float>(std::max(1, player.level()));
    const float streak = static_cast<float>(player.winStreak() - player.loseStreak());
    return gold * AIConstants::BoardStrengthEconomyGoldWeight +
           hp * AIConstants::BoardStrengthEconomyHpWeight +
           lvl * AIConstants::BoardStrengthEconomyLevelWeight +
           streak * AIConstants::BoardStrengthEconomyStreakWeight;
}

static float starScoreMultiplier(int starLevel)
{
    const int s = std::clamp(starLevel, MinStarLevel, MaxStarLevel);
    if (s == 1) return AIConstants::OneStarMultiplier;
    if (s == TwoStarLevel) return AIConstants::TwoStarMultiplier;
    return AIConstants::ThreeStarMultiplier;
}

static float frontlinePower(const PlayerState& player, const ContentManager& content)
{
    std::vector<float> scores;
    scores.reserve(player.board().size());
    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        const float hp = static_cast<float>(std::max(1, def->hp));
        const float armor = static_cast<float>(std::max(0, def->armor));
        const float mr = static_cast<float>(std::max(0, def->magicResist));
        const float star = starScoreMultiplier(u.starLevel);
        const float itemDef = ItemValueEvaluator::evaluateUnitItems(u, content).defense;
        scores.push_back((hp * (1.0f + AIConstants::BoardStrengthFrontlineDefenseScale * (armor + mr)) +
                          itemDef * AIConstants::BoardStrengthFrontlineItemDefenseWeight) *
                         star);
    }
    std::sort(scores.begin(), scores.end(), std::greater<float>());
    float total = 0.0f;
    for (std::size_t i = 0; i < scores.size() && i < static_cast<std::size_t>(AIConstants::BoardStrengthFrontlineTopUnits); ++i)
    {
        total += scores[i];
    }
    return total * AIConstants::BoardStrengthFrontlineTotalScale;
}

static float carryPower(const PlayerState& player, const ContentManager& content)
{
    float best = 0.0f;
    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        const float ad = static_cast<float>(std::max(0, def->ad));
        const float as = std::max(0.0f, def->attackSpeed);
        const float ap = std::max(0.0f, def->abilityPower);
        const float crit = std::clamp(def->critChance, 0.0f, 1.0f);
        const float range = static_cast<float>(std::max(1, def->range));
        const ItemValueScore items = ItemValueEvaluator::evaluateUnitItems(u, content);
        const float star = starScoreMultiplier(u.starLevel);

        const float dmg = ad * as * (AIConstants::CarryRangeBase + AIConstants::CarryRangeScale * range) +
                          ap * AIConstants::CarryApWeight +
                          crit * AIConstants::CarryCritWeight;
        const float score = (dmg +
                             items.offense * AIConstants::BoardStrengthCarryItemOffenseWeight +
                             items.caster * AIConstants::BoardStrengthCarryItemCasterWeight) *
                            star;
        best = std::max(best, score);
    }
    return best * AIConstants::BoardStrengthCarryTotalScale;
}

BoardScore BoardStrengthEvaluator::evaluate(const PlayerState& player, const ContentManager& content)
{
    BoardScore out{};

    float unitPower = 0.0f;
    float itemPower = 0.0f;

    for (const OwnedUnit& u : player.board())
    {
        unitPower += UnitValueEvaluator::evaluate(u, content, &player).total;
        itemPower += ItemValueEvaluator::evaluateUnitItems(u, content).total;
    }

    for (const std::string& itemName : player.itemBench())
    {
        const Item* item = content.getItem(itemName);
        if (!item)
        {
            continue;
        }
        itemPower += ItemValueEvaluator::evaluateItem(*item).total * AIConstants::BoardStrengthBenchItemDiscount;
    }

    const TraitSynergyScore traits = TraitSynergyEvaluator::evaluate(player, content);
    const UpgradePotentialScore upgrades = UpgradePotentialEvaluator::evaluate(player, content);

    out.unitPower = unitPower * AIConstants::BoardStrengthUnitPowerScale;
    out.itemPower = itemPower * AIConstants::BoardStrengthItemPowerScale;
    out.traitPower = traits.total * AIConstants::BoardStrengthTraitPowerScale;
    out.frontlinePower = frontlinePower(player, content);
    out.carryPower = carryPower(player, content);
    out.economyPower = economyScore(player);
    out.upgradePotential = upgrades.total * AIConstants::BoardStrengthUpgradePotentialScale;

    out.total =
        out.unitPower +
        out.traitPower +
        out.itemPower +
        out.frontlinePower +
        out.carryPower +
        out.economyPower +
        out.upgradePotential;

    std::ostringstream ss;
    ss << "total=" << std::lround(out.total)
       << " unit=" << std::lround(out.unitPower)
       << " trait=" << std::lround(out.traitPower)
       << " item=" << std::lround(out.itemPower)
       << " front=" << std::lround(out.frontlinePower)
       << " carry=" << std::lround(out.carryPower)
       << " econ=" << std::lround(out.economyPower)
       << " upg=" << std::lround(out.upgradePotential);
    out.debug = ss.str();

    return out;
}
