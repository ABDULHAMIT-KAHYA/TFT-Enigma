#include "combat/TraitResolver.hpp"
#include "content/ContentManager.hpp"
#include <algorithm>
#include <unordered_map>

static int activeBreakpointForCount(const std::vector<int>& breakpoints, int unitCount)
{
    int active = 0;
    for (int bp : breakpoints)
    {
        if (unitCount >= bp)
        {
            active = bp;
        }
    }
    return active;
}

std::vector<ActiveTrait> TraitResolver::resolveTeamTraits(const GameState& state, TeamId team)
{
    std::unordered_map<std::string, int> counts;

    for (const Unit& unit : state.units())
    {
        if (!unit.isAlive() || unit.getTeamId() != team)
        {
            continue;
        }
        for (const std::string& t : unit.traits())
        {
            if (!t.empty())
            {
                counts[t] += 1;
            }
        }
    }

    std::vector<ActiveTrait> out;
    out.reserve(counts.size());

    const auto& defs = state.content().traits();
    for (const auto& [traitName, count] : counts)
    {
        auto it = defs.find(traitName);
        if (it == defs.end())
        {
            continue;
        }
        const TraitDefinition& def = it->second;
        std::vector<int> breakpoints;
        if (!def.tiers.empty())
        {
            breakpoints.reserve(def.tiers.size());
            for (const TraitTier& t : def.tiers)
            {
                if (t.breakpoint > 0)
                {
                    breakpoints.push_back(t.breakpoint);
                }
            }
        }
        else
        {
            breakpoints = def.trait.breakpoints;
        }
        std::sort(breakpoints.begin(), breakpoints.end());
        breakpoints.erase(std::unique(breakpoints.begin(), breakpoints.end()), breakpoints.end());

        ActiveTrait at{};
        at.traitName = traitName;
        at.activeCount = count;
        at.breakpoint = activeBreakpointForCount(breakpoints, count);
        out.push_back(std::move(at));
    }

    std::sort(out.begin(), out.end(), [](const ActiveTrait& a, const ActiveTrait& b) {
        return a.traitName < b.traitName;
    });
    return out;
}
