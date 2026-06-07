#include "ai/TraitSynergyEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_map>

static constexpr int MinStarLevel = 1;
static constexpr int MaxStarLevel = 3;
static constexpr int NearSoonNeedUnits = 2;

static int activeBreakpointForCount(const TraitDefinition& def, int count)
{
    int active = 0;
    if (!def.tiers.empty())
    {
        for (const TraitTier& t : def.tiers)
        {
            if (t.breakpoint > 0 && count >= t.breakpoint)
            {
                active = std::max(active, t.breakpoint);
            }
        }
        return active;
    }

    for (int bp : def.trait.breakpoints)
    {
        if (count >= bp)
        {
            active = std::max(active, bp);
        }
    }
    return active;
}

static int nextBreakpointForCount(const TraitDefinition& def, int count)
{
    int next = 0;
    std::vector<int> bps;
    if (!def.tiers.empty())
    {
        for (const TraitTier& t : def.tiers)
        {
            if (t.breakpoint > 0)
            {
                bps.push_back(t.breakpoint);
            }
        }
    }
    else
    {
        bps = def.trait.breakpoints;
    }

    std::sort(bps.begin(), bps.end());
    bps.erase(std::unique(bps.begin(), bps.end()), bps.end());
    for (int bp : bps)
    {
        if (bp > count)
        {
            next = bp;
            break;
        }
    }
    return next;
}

TraitSynergyScore TraitSynergyEvaluator::evaluate(const PlayerState& player, const ContentManager& content)
{
    TraitSynergyScore out{};

    struct Accum
    {
        int boardCount = 0;
        int benchCount = 0;
        int shopCount = 0;
        float quality = 0.0f;
        float carryQuality = 0.0f;
        float tankQuality = 0.0f;
    };

    std::unordered_map<std::string, Accum> acc;
    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
        for (const std::string& t : def->traits)
        {
            if (!t.empty())
            {
                Accum& a = acc[t];
                a.boardCount += 1;
                a.quality += uv.total * AIConstants::TraitSynergyBoardUnitValueWeight +
                             static_cast<float>(std::max(1, u.cost)) * AIConstants::TraitSynergyBoardCostWeight +
                             static_cast<float>(std::clamp(u.starLevel, MinStarLevel, MaxStarLevel) - 1) *
                                 AIConstants::TraitSynergyBoardStarWeight;
                a.carryQuality += uv.carry * AIConstants::TraitSynergyBoardUnitValueWeight;
                a.tankQuality += uv.frontline * AIConstants::TraitSynergyBoardUnitValueWeight;
            }
        }
    }
    for (const OwnedUnit& u : player.bench())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        const UnitValueBreakdown uv = UnitValueEvaluator::evaluate(u, content, &player);
        for (const std::string& t : def->traits)
        {
            if (!t.empty())
            {
                Accum& a = acc[t];
                a.benchCount += 1;
                a.quality += uv.total * AIConstants::TraitSynergyBenchUnitValueWeight +
                             static_cast<float>(std::max(1, u.cost)) * AIConstants::TraitSynergyBenchCostWeight +
                             static_cast<float>(std::clamp(u.starLevel, MinStarLevel, MaxStarLevel) - 1) *
                                 AIConstants::TraitSynergyBenchStarWeight;
                a.carryQuality += uv.carry * AIConstants::TraitSynergyBenchUnitValueWeight;
                a.tankQuality += uv.frontline * AIConstants::TraitSynergyBenchUnitValueWeight;
            }
        }
    }
    for (const ShopOffer& o : player.shop())
    {
        if (o.championName.empty())
        {
            continue;
        }
        const ChampionDefinition* def = content.getChampion(o.championName);
        if (!def)
        {
            continue;
        }
        for (const std::string& t : def->traits)
        {
            if (!t.empty())
            {
                Accum& a = acc[t];
                a.shopCount += 1;
                a.quality += static_cast<float>(std::max(1, o.cost)) * AIConstants::TraitSynergyShopCostWeight;
            }
        }
    }

    std::vector<ActiveTrait> activeTraits;
    float activeScore = 0.0f;
    float nearScore = 0.0f;
    std::vector<std::string> nearDebug;
    std::vector<std::string> ignoredDebug;

    for (const auto& [t, a] : acc)
    {
        const TraitDefinition* td = content.getTrait(t);
        if (!td)
        {
            continue;
        }
        const int boardCount = a.boardCount;
        const int effectiveCount = a.boardCount + a.benchCount + (a.shopCount > 0 ? 1 : 0);
        const int activeBp = activeBreakpointForCount(*td, boardCount);
        const int nextBp = nextBreakpointForCount(*td, boardCount);

        const float qBase = a.quality;
        const float qCarry = a.carryQuality;
        const float qTank = a.tankQuality;
        const float profileBoost =
            1.0f +
            std::clamp((qCarry + qTank) / AIConstants::TraitSynergyProfileBoostDivisor, 0.0f, AIConstants::TraitSynergyProfileBoostMaxAdd);

        if (activeBp > 0)
        {
            ActiveTrait at{};
            at.traitName = t;
            at.activeCount = boardCount;
            at.breakpoint = activeBp;
            activeTraits.push_back(std::move(at));

            const float tierBase = AIConstants::TraitSynergyActiveTierBase +
                                   static_cast<float>(activeBp) * AIConstants::TraitSynergyActiveTierPerBreakpoint;
            const float overflow = static_cast<float>(std::max(0, boardCount - activeBp));
            const float overflowBoost = 1.0f + AIConstants::TraitSynergyOverflowBoostPerUnit * overflow;
            const float qualityBoost =
                1.0f +
                std::clamp(qBase / AIConstants::TraitSynergyQualityBoostDivisor, 0.0f, AIConstants::TraitSynergyQualityBoostMaxAdd);
            activeScore += tierBase * overflowBoost * qualityBoost * profileBoost;
        }

        if (nextBp > 0)
        {
            const int needUnits = nextBp - boardCount;
            const bool nearNow = needUnits == 1;
            const bool nearSoon = needUnits == NearSoonNeedUnits && effectiveCount >= nextBp - 1;
            const bool hasBenchHelp = a.benchCount > 0;
            const bool hasShopHelp = a.shopCount > 0;

            if (nearNow)
            {
                float s = AIConstants::TraitSynergyNearNowBase +
                          static_cast<float>(nextBp) * AIConstants::TraitSynergyNearNowPerBreakpoint;
                if (hasBenchHelp) s *= AIConstants::TraitSynergyNearNowBenchMultiplier;
                if (hasShopHelp) s *= AIConstants::TraitSynergyNearNowShopMultiplier;
                s *= profileBoost;
                nearScore += s;
                std::ostringstream ss;
                ss << t << " needs 1";
                nearDebug.push_back(ss.str());
            }
            else if (nearSoon)
            {
                float s = AIConstants::TraitSynergyNearSoonBase +
                          static_cast<float>(nextBp) * AIConstants::TraitSynergyNearSoonPerBreakpoint;
                if (hasBenchHelp) s *= AIConstants::TraitSynergyNearSoonBenchMultiplier;
                if (hasShopHelp) s *= AIConstants::TraitSynergyNearSoonShopMultiplier;
                s *= profileBoost;
                nearScore += s;
                std::ostringstream ss;
                ss << t << " needs 2";
                nearDebug.push_back(ss.str());
            }
            else
            {
                if (boardCount <= 1 && (effectiveCount < nextBp - 1))
                {
                    std::ostringstream ss;
                    ss << t << " one-off";
                    ignoredDebug.push_back(ss.str());
                }
                else
                {
                    nearScore += AIConstants::TraitSynergyNearFallbackBonus;
                }
            }
        }
    }

    std::sort(activeTraits.begin(), activeTraits.end(), [](const ActiveTrait& a, const ActiveTrait& b) {
        return a.traitName < b.traitName;
    });

    out.activeTraits = activeTraits;
    out.active = activeScore;
    out.near = nearScore;
    out.total = activeScore + nearScore * AIConstants::TraitSynergyTotalNearWeight;

    std::ostringstream dbg;
    dbg << "TraitScore: active=[";
    for (std::size_t i = 0; i < activeTraits.size(); ++i)
    {
        if (i) dbg << ", ";
        dbg << activeTraits[i].traitName << "(" << activeTraits[i].activeCount << ")";
    }
    dbg << "] near=[";
    for (std::size_t i = 0; i < nearDebug.size(); ++i)
    {
        if (i) dbg << ", ";
        dbg << nearDebug[i];
    }
    dbg << "] ignored=[";
    for (std::size_t i = 0; i < ignoredDebug.size(); ++i)
    {
        if (i) dbg << ", ";
        dbg << ignoredDebug[i];
    }
    dbg << "]";
    out.debug = dbg.str();

    return out;
}
