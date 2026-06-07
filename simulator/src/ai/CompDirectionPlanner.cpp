#include "ai/CompDirectionPlanner.hpp"
#include "constants/AIConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

static void addTraitWeights(std::unordered_map<std::string, float>& w,
                            const ChampionDefinition& def,
                            float amount)
{
    for (const std::string& t : def.traits)
    {
        if (!t.empty())
        {
            w[t] += amount;
        }
    }
}

CompDirection CompDirectionPlanner::infer(const PlayerState& player, const ContentManager& content)
{
    CompDirection out{};

    std::unordered_map<std::string, float> traitW;
    std::unordered_map<std::string, float> unitW;

    float totalW = 0.0f;
    float offense = 0.0f;
    float caster = 0.0f;
    float defense = 0.0f;

    auto consumeUnit = [&](const OwnedUnit& u, float mult)
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            return;
        }
        const float v = UnitValueEvaluator::evaluate(u, content, &player).total * mult;
        totalW += v;
        unitW[u.championName] += v;
        addTraitWeights(traitW, *def, v);

        const ItemValueScore items = ItemValueEvaluator::evaluateUnitItems(u, content);
        offense += items.offense;
        caster += items.caster;
        defense += items.defense;
    };

    for (const OwnedUnit& u : player.board())
    {
        consumeUnit(u, 1.0f);
    }
    for (const OwnedUnit& u : player.bench())
    {
        consumeUnit(u, AIConstants::CompPlannerBenchUnitWeight);
    }

    for (const ShopOffer& o : player.shop())
    {
        if (o.championName.empty())
        {
            continue;
        }
        OwnedUnit temp{};
        temp.championName = o.championName;
        temp.starLevel = 1;
        temp.cost = o.cost;
        consumeUnit(temp, AIConstants::CompPlannerShopUnitWeight);
    }

    std::vector<std::pair<std::string, float>> traitsSorted(traitW.begin(), traitW.end());
    std::sort(traitsSorted.begin(), traitsSorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second || (a.second == b.second && a.first < b.first); });

    std::vector<std::pair<std::string, float>> unitsSorted(unitW.begin(), unitW.end());
    std::sort(unitsSorted.begin(), unitsSorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second || (a.second == b.second && a.first < b.first); });

    constexpr std::size_t MaxCoreTraits = 3;
    for (std::size_t i = 0; i < traitsSorted.size() && i < MaxCoreTraits; ++i)
    {
        out.coreTraits.push_back(traitsSorted[i].first);
    }
    constexpr std::size_t MaxCoreUnits = 2;
    for (std::size_t i = 0; i < unitsSorted.size() && i < MaxCoreUnits; ++i)
    {
        out.coreUnits.push_back(unitsSorted[i].first);
    }

    float topTraitW = traitsSorted.empty() ? 0.0f : traitsSorted[0].second;
    float topTwo = topTraitW + (traitsSorted.size() > 1 ? traitsSorted[1].second : 0.0f);
    out.confidence = totalW <= 0.0f ? 0.0f : std::clamp(topTwo / totalW, 0.0f, 1.0f);

    std::ostringstream ss;
    const float styleSum = std::max(1.0f, offense + caster + defense);
    ss << "style(off=" << std::lround(AIConstants::PercentScale * offense / styleSum)
       << "% ap=" << std::lround(AIConstants::PercentScale * caster / styleSum)
       << "% def=" << std::lround(AIConstants::PercentScale * defense / styleSum)
       << "%)";
    out.debug = ss.str();

    return out;
}
