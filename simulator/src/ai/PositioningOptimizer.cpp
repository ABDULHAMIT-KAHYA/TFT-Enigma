#include "ai/PositioningOptimizer.hpp"
#include "constants/AIConstants.hpp"
#include "constants/GameConstants.hpp"
#include "ai/ItemValueEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_set>

struct OwnUnitInfo
{
    int index = -1;
    bool ranged = false;
    float carryScore = 0.0f;
    float tankScore = 0.0f;
    float overall = 0.0f;
    ItemValueScore items{};
};

static constexpr std::size_t FormationCellCount =
    static_cast<std::size_t>(GameConstants::FormationWidth) * static_cast<std::size_t>(GameConstants::FormationHeight);

static constexpr int MinStarLevel = 1;
static constexpr int TwoStarLevel = 2;
static constexpr int MaxStarLevel = 3;
static constexpr int NearbyDistance = 2;

static float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

static float statCarryScore(const ChampionDefinition& def)
{
    const float ad = static_cast<float>(std::max(0, def.ad));
    const float as = std::max(AIConstants::PositioningMinAttackSpeed, def.attackSpeed);
    const float ap = std::max(0.0f, def.abilityPower);
    const float crit = clamp01(def.critChance);
    const float rangeBonus = def.range > 1 ? AIConstants::PositioningRangeBonus : 0.0f;
    return ad * as * AIConstants::PositioningAdAsWeight +
           ap * AIConstants::PositioningApWeight +
           crit * AIConstants::PositioningCritWeight +
           rangeBonus;
}

static float statTankScore(const ChampionDefinition& def)
{
    const float hp = static_cast<float>(std::max(1, def.hp));
    const float armor = static_cast<float>(std::max(0, def.armor));
    const float mr = static_cast<float>(std::max(0, def.magicResist));
    return hp * (1.0f + AIConstants::PositioningTankDefenseScale * (armor + mr));
}

static int starLevelClamped(const OwnedUnit& u)
{
    return std::clamp(u.starLevel, MinStarLevel, MaxStarLevel);
}

static float starMultiplier(const OwnedUnit& u)
{
    const int star = starLevelClamped(u);
    if (star == 1) return AIConstants::OneStarMultiplier;
    if (star == TwoStarLevel) return AIConstants::TwoStarMultiplier;
    return AIConstants::ThreeStarMultiplier;
}

static OwnUnitInfo buildOwnInfo(const PlayerState& player, const ContentManager& content, int idx)
{
    OwnUnitInfo info{};
    info.index = idx;

    if (idx < 0 || static_cast<std::size_t>(idx) >= player.board().size())
    {
        return info;
    }

    const OwnedUnit& u = player.board()[static_cast<std::size_t>(idx)];
    const ChampionDefinition* def = content.getChampion(u.championName);
    if (!def)
    {
        return info;
    }

    info.ranged = def->range > 1;
    info.items = ItemValueEvaluator::evaluateUnitItems(u, content);

    const float star = starMultiplier(u);
    const float carry = statCarryScore(*def) * star +
                        info.items.offense * AIConstants::PositioningCarryItemOffenseWeight +
                        info.items.caster * AIConstants::PositioningCarryItemCasterWeight;
    const float tank = statTankScore(*def) * star + info.items.defense * AIConstants::PositioningTankItemDefenseWeight;

    info.carryScore = carry;
    info.tankScore = tank;

    const UnitValueBreakdown base = UnitValueEvaluator::evaluate(u, content, &player);
    info.overall = base.total +
                   carry * AIConstants::PositioningOverallCarryBonusWeight +
                   tank * AIConstants::PositioningOverallTankBonusWeight;
    return info;
}

static int enemyCarryIndexSafe(const EnemySnapshot* enemy)
{
    if (!enemy)
    {
        return -1;
    }
    if (enemy->carryUnit < 0)
    {
        return -1;
    }
    if (!enemy->units.empty())
    {
        if (static_cast<std::size_t>(enemy->carryUnit) >= enemy->units.size())
        {
            return -1;
        }
        return enemy->carryUnit;
    }
    if (static_cast<std::size_t>(enemy->carryUnit) >= enemy->positioning.size())
    {
        return -1;
    }
    return enemy->carryUnit;
}

static Position enemyCarryPosition(const EnemySnapshot* enemy)
{
    if (!enemy)
    {
        return Position{ GameConstants::DefaultFormationX, GameConstants::DefaultFormationY };
    }
    const int idx = enemyCarryIndexSafe(enemy);
    if (idx < 0)
    {
        return Position{ GameConstants::DefaultFormationX, GameConstants::DefaultFormationY };
    }
    if (!enemy->units.empty())
    {
        return enemy->units[static_cast<std::size_t>(idx)].position;
    }
    return enemy->positioning[static_cast<std::size_t>(idx)];
}

static float enemyTotalAoeThreat(const EnemySnapshot* enemy)
{
    if (!enemy)
    {
        return 0.0f;
    }
    float t = 0.0f;
    for (const EnemySnapshot::UnitInfo& u : enemy->units)
    {
        t += std::max(0.0f, u.aoeThreat);
    }
    return t;
}

static Position currentFormationPos(const PlayerState& player, std::size_t boardIndex)
{
    if (boardIndex >= player.board().size())
    {
        return Position{ 0, 0 };
    }
    const OwnedUnit& u = player.board()[boardIndex];
    if (u.hasFormation)
    {
        return Position{
            std::clamp(u.formation.x, 0, GameConstants::FormationWidth - 1),
            std::clamp(u.formation.y, 0, GameConstants::FormationHeight - 1)
        };
    }
    const int x = static_cast<int>(boardIndex % static_cast<std::size_t>(GameConstants::FormationWidth));
    const int y = std::min(static_cast<int>(boardIndex / static_cast<std::size_t>(GameConstants::FormationWidth)),
                           GameConstants::FormationHeight - 1);
    return Position{ x, y };
}

float PositioningOptimizer::score(const PlayerState& player,
                                  const ContentManager& content,
                                  const EnemySnapshot* enemy)
{
    if (player.board().empty())
    {
        return 0.0f;
    }

    std::vector<OwnUnitInfo> infos;
    infos.reserve(player.board().size());
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        infos.push_back(buildOwnInfo(player, content, static_cast<int>(i)));
    }

    int carry = -1;
    int tank = -1;
    float bestCarry = -1.0f;
    float bestTank = -1.0f;

    for (const OwnUnitInfo& info : infos)
    {
        const float score = info.carryScore +
                            info.items.offense * AIConstants::PositioningCarrySelectionOffenseWeight +
                            info.items.caster * AIConstants::PositioningCarrySelectionCasterWeight;
        if (score > bestCarry)
        {
            bestCarry = score;
            carry = info.index;
        }
    }
    for (const OwnUnitInfo& info : infos)
    {
        if (info.index == carry)
        {
            continue;
        }
        const float score = info.tankScore;
        if (score > bestTank)
        {
            bestTank = score;
            tank = info.index;
        }
    }

    const Position eCarryPos = enemyCarryPosition(enemy);
    const int eCarrySide = eCarryPos.x <= GameConstants::DefaultFormationX ? -1 : 1;

    std::array<float, FormationCellCount> heat{};
    heat.fill(0.0f);
    if (enemy && !enemy->units.empty())
    {
        for (const EnemySnapshot::UnitInfo& eu : enemy->units)
        {
            const int ex = std::clamp(eu.position.x, 0, GameConstants::FormationWidth - 1);
            const int ey = std::clamp(eu.position.y, 0, GameConstants::FormationHeight - 1);
            const int eyOpp = AIConstants::PositioningEnemyMirrorYBase - ey;
            const float baseThreat = std::max(0.0f, eu.threat);
            for (int y = 0; y < GameConstants::FormationHeight; ++y)
            {
                for (int x = 0; x < GameConstants::FormationWidth; ++x)
                {
                    const int dy = std::abs(y - eyOpp);
                    const int dx = std::abs(x - ex);
                    const int dist = dx + dy;
                    float t = baseThreat / (1.0f + static_cast<float>(dist));
                    if (eu.hasJumpThreat && y >= (GameConstants::FormationHeight - 1))
                    {
                        t *= AIConstants::PositioningJumpThreatMultiplier;
                    }
                    heat[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] += t;
                }
            }
        }
    }

    const float aoeThreat = enemyTotalAoeThreat(enemy);
    const float aoeFactor = clamp01(aoeThreat / AIConstants::PositioningAoeThreatDivisor);

    std::array<int, FormationCellCount> occupied{};
    occupied.fill(0);

    std::vector<Position> pos;
    pos.reserve(player.board().size());
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        pos.push_back(currentFormationPos(player, i));
    }

    for (const Position& p : pos)
    {
        const int x = std::clamp(p.x, 0, GameConstants::FormationWidth - 1);
        const int y = std::clamp(p.y, 0, GameConstants::FormationHeight - 1);
        occupied[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] = 1;
    }

    auto isOcc = [&](int x, int y) -> bool
    {
        return occupied[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] != 0;
    };

    auto neighborCount = [&](int x, int y) -> int
    {
        int c = 0;
        for (int yy = 0; yy < GameConstants::FormationHeight; ++yy)
        {
            for (int xx = 0; xx < GameConstants::FormationWidth; ++xx)
            {
                if (!isOcc(xx, yy))
                {
                    continue;
                }
                if (xx == x && yy == y)
                {
                    continue;
                }
                const int dx = std::abs(xx - x);
                const int dy = std::abs(yy - y);
                const int d = dx + dy;
                if (d <= NearbyDistance)
                {
                    c += 1;
                }
            }
        }
        return c;
    };

    auto scoreFor = [&](const OwnUnitInfo& u, int x, int y, const Position* carryPos) -> float
    {
        const float h = heat[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)];
        const int neigh = neighborCount(x, y);
        const float spreadMultiplier =
            (y >= GameConstants::DefaultFormationY) ? 1.0f : AIConstants::PositioningSpreadPenaltyFrontRowMultiplier;
        const float spreadPenalty =
            static_cast<float>(neigh) *
            (AIConstants::PositioningSpreadPenaltyBase +
             AIConstants::PositioningSpreadPenaltyScale * aoeFactor * spreadMultiplier);
        float s = 0.0f;
        s -= spreadPenalty;

        if (u.index == carry)
        {
            s += y == (GameConstants::FormationHeight - 1) ? AIConstants::PositioningCarryBackRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningCarryMidRowBonus
                                                        : AIConstants::PositioningCarryFrontRowPenalty;
            s -= h * AIConstants::PositioningCarryHeatPenaltyWeight;
            const int edgeDistance = std::min(x, (GameConstants::FormationWidth - 1) - x);
            const float edgeBonus = static_cast<float>(edgeDistance) * AIConstants::PositioningCarryEdgeBonusScale;
            s += edgeBonus;
            if (enemy)
            {
                const int awayX = eCarrySide < 0 ? (GameConstants::FormationWidth - 1) : 0;
                s += AIConstants::PositioningCarryAwayFromEnemyBase -
                     static_cast<float>(std::abs(x - awayX)) * AIConstants::PositioningCarryAwayFromEnemyDistancePenalty;
            }
            return s;
        }

        if (u.index == tank)
        {
            s += y == 0 ? AIConstants::PositioningTankFrontRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningTankMidRowBonus
                                                        : AIConstants::PositioningTankBackRowPenalty;
            s -= h * AIConstants::PositioningTankHeatPenaltyWeight;
            s += AIConstants::PositioningTankVsEnemyCarryBase -
                 static_cast<float>(std::abs(x - eCarryPos.x)) * AIConstants::PositioningTankVsEnemyCarryDistancePenalty;
            return s;
        }

        if (u.ranged)
        {
            s += y == (GameConstants::FormationHeight - 1) ? AIConstants::PositioningRangedBackRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningRangedMidRowBonus
                                                        : AIConstants::PositioningRangedFrontRowPenalty;
            s -= h * AIConstants::PositioningRangedHeatPenaltyWeight;
        }
        else
        {
            s += y == 0 ? AIConstants::PositioningMeleeFrontRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningMeleeMidRowBonus
                                                        : AIConstants::PositioningMeleeBackRowPenalty;
            s -= h * AIConstants::PositioningMeleeHeatPenaltyWeight;
        }

        if (carryPos)
        {
            const float carryHeat =
                heat[static_cast<std::size_t>(carryPos->y * GameConstants::FormationWidth + carryPos->x)];
            const bool carryThreatened =
                carryHeat > AIConstants::PositioningCarryThreatenedHeatThreshold ||
                (aoeFactor > AIConstants::PositioningCarryThreatenedAoeFactorThreshold &&
                 carryHeat > AIConstants::PositioningCarryThreatenedHeatThresholdLow);
            if (carryThreatened)
            {
                const int d = distanceBetween(Position{ x, y }, *carryPos);
                if (d == 1)
                {
                    s += (u.tankScore > u.carryScore ? AIConstants::PositioningBodyguardAdjacencyTankBonusD1
                                                     : AIConstants::PositioningBodyguardAdjacencyCarryBonusD1);
                }
                else if (d == NearbyDistance)
                {
                    s += (u.tankScore > u.carryScore ? AIConstants::PositioningBodyguardAdjacencyTankBonusD2
                                                     : AIConstants::PositioningBodyguardAdjacencyCarryBonusD2);
                }
            }
        }

        s += u.overall * AIConstants::PositioningOverallTotalWeight;
        return s;
    };

    Position carryPos{ GameConstants::DefaultFormationX, GameConstants::DefaultFormationY };
    bool haveCarryPos = false;
    if (carry >= 0 && static_cast<std::size_t>(carry) < pos.size())
    {
        carryPos = pos[static_cast<std::size_t>(carry)];
        haveCarryPos = true;
    }

    float total = 0.0f;
    for (const OwnUnitInfo& u : infos)
    {
        if (u.index < 0 || static_cast<std::size_t>(u.index) >= pos.size())
        {
            continue;
        }
        const Position p = pos[static_cast<std::size_t>(u.index)];
        total += scoreFor(u,
                          std::clamp(p.x, 0, GameConstants::FormationWidth - 1),
                          std::clamp(p.y, 0, GameConstants::FormationHeight - 1),
                          haveCarryPos ? &carryPos : nullptr);
    }
    return total;
}

void PositioningOptimizer::optimize(PlayerState& player,
                                   const ContentManager& content,
                                   const EnemySnapshot* enemy)
{
    if (player.board().empty())
    {
        return;
    }

    std::vector<OwnUnitInfo> infos;
    infos.reserve(player.board().size());
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        infos.push_back(buildOwnInfo(player, content, static_cast<int>(i)));
    }

    int carry = -1;
    int tank = -1;
    float bestCarry = -1.0f;
    float bestTank = -1.0f;

    for (const OwnUnitInfo& info : infos)
    {
        const float score = info.carryScore +
                            info.items.offense * AIConstants::PositioningCarrySelectionOffenseWeight +
                            info.items.caster * AIConstants::PositioningCarrySelectionCasterWeight;
        if (score > bestCarry)
        {
            bestCarry = score;
            carry = info.index;
        }
    }
    for (const OwnUnitInfo& info : infos)
    {
        if (info.index == carry)
        {
            continue;
        }
        const float score = info.tankScore;
        if (score > bestTank)
        {
            bestTank = score;
            tank = info.index;
        }
    }

    const Position eCarryPos = enemyCarryPosition(enemy);
    const int eCarrySide = eCarryPos.x <= GameConstants::DefaultFormationX ? -1 : 1;

    std::array<float, FormationCellCount> heat{};
    heat.fill(0.0f);
    if (enemy && !enemy->units.empty())
    {
        for (const EnemySnapshot::UnitInfo& eu : enemy->units)
        {
            const int ex = std::clamp(eu.position.x, 0, GameConstants::FormationWidth - 1);
            const int ey = std::clamp(eu.position.y, 0, GameConstants::FormationHeight - 1);
            const int eyOpp = AIConstants::PositioningEnemyMirrorYBase - ey;
            const float baseThreat = std::max(0.0f, eu.threat);
            for (int y = 0; y < GameConstants::FormationHeight; ++y)
            {
                for (int x = 0; x < GameConstants::FormationWidth; ++x)
                {
                    const int dy = std::abs(y - eyOpp);
                    const int dx = std::abs(x - ex);
                    const int dist = dx + dy;
                    float t = baseThreat / (1.0f + static_cast<float>(dist));
                    if (eu.hasJumpThreat && y >= (GameConstants::FormationHeight - 1))
                    {
                        t *= AIConstants::PositioningJumpThreatMultiplier;
                    }
                    heat[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] += t;
                }
            }
        }
    }

    const float aoeThreat = enemyTotalAoeThreat(enemy);
    const float aoeFactor = clamp01(aoeThreat / AIConstants::PositioningAoeThreatDivisor);

    std::array<int, FormationCellCount> occupied{};
    occupied.fill(0);

    auto isOcc = [&](int x, int y) -> bool
    {
        return occupied[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] != 0;
    };

    auto markOcc = [&](int x, int y)
    {
        occupied[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)] = 1;
    };

    auto neighborCount = [&](int x, int y) -> int
    {
        int c = 0;
        for (int yy = 0; yy < GameConstants::FormationHeight; ++yy)
        {
            for (int xx = 0; xx < GameConstants::FormationWidth; ++xx)
            {
                if (!isOcc(xx, yy))
                {
                    continue;
                }
                if (distanceBetween(Position{ x, y }, Position{ xx, yy }) <= 1)
                {
                    c += 1;
                }
            }
        }
        return c;
    };

    auto place = [&](int idx, int x, int y)
    {
        if (idx < 0 || static_cast<std::size_t>(idx) >= player.board().size())
        {
            return;
        }
        x = std::clamp(x, 0, GameConstants::FormationWidth - 1);
        y = std::clamp(y, 0, GameConstants::FormationHeight - 1);
        if (isOcc(x, y))
        {
            return;
        }
        player.repositionBoardUnit(static_cast<std::size_t>(idx), Position{ x, y });
        markOcc(x, y);
    };

    auto scoreFor = [&](const OwnUnitInfo& u, int x, int y, const Position* carryPos) -> float
    {
        const float h = heat[static_cast<std::size_t>(y * GameConstants::FormationWidth + x)];
        const int neigh = neighborCount(x, y);
        const float spreadMultiplier = (y >= GameConstants::DefaultFormationY) ? 1.0f : AIConstants::PositioningSpreadPenaltyFrontRowMultiplier;
        const float spreadPenalty =
            static_cast<float>(neigh) *
            (AIConstants::PositioningSpreadPenaltyBase +
             AIConstants::PositioningSpreadPenaltyScale * aoeFactor * spreadMultiplier);
        float s = 0.0f;
        s -= spreadPenalty;

        if (u.index == carry)
        {
            s += y == (GameConstants::FormationHeight - 1) ? AIConstants::PositioningCarryBackRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningCarryMidRowBonus
                                                        : AIConstants::PositioningCarryFrontRowPenalty;
            s -= h * AIConstants::PositioningCarryHeatPenaltyWeight;
            const int edgeDistance = std::min(x, (GameConstants::FormationWidth - 1) - x);
            const float edgeBonus = static_cast<float>(edgeDistance) * AIConstants::PositioningCarryEdgeBonusScale;
            s += edgeBonus;
            if (enemy)
            {
                const int awayX = eCarrySide < 0 ? (GameConstants::FormationWidth - 1) : 0;
                s += AIConstants::PositioningCarryAwayFromEnemyBase -
                     static_cast<float>(std::abs(x - awayX)) * AIConstants::PositioningCarryAwayFromEnemyDistancePenalty;
            }
            return s;
        }

        if (u.index == tank)
        {
            s += y == 0 ? AIConstants::PositioningTankFrontRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningTankMidRowBonus
                                                        : AIConstants::PositioningTankBackRowPenalty;
            s -= h * AIConstants::PositioningTankHeatPenaltyWeight;
            s += AIConstants::PositioningTankVsEnemyCarryBase -
                 static_cast<float>(std::abs(x - eCarryPos.x)) * AIConstants::PositioningTankVsEnemyCarryDistancePenalty;
            return s;
        }

        if (u.ranged)
        {
            s += y == (GameConstants::FormationHeight - 1) ? AIConstants::PositioningRangedBackRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningRangedMidRowBonus
                                                        : AIConstants::PositioningRangedFrontRowPenalty;
            s -= h * AIConstants::PositioningRangedHeatPenaltyWeight;
        }
        else
        {
            s += y == 0 ? AIConstants::PositioningMeleeFrontRowBonus
                 : y == GameConstants::DefaultFormationY ? AIConstants::PositioningMeleeMidRowBonus
                                                        : AIConstants::PositioningMeleeBackRowPenalty;
            s -= h * AIConstants::PositioningMeleeHeatPenaltyWeight;
        }

        if (carryPos)
        {
            const float carryHeat = heat[static_cast<std::size_t>(carryPos->y * GameConstants::FormationWidth + carryPos->x)];
            const bool carryThreatened =
                carryHeat > AIConstants::PositioningCarryThreatenedHeatThreshold ||
                (aoeFactor > AIConstants::PositioningCarryThreatenedAoeFactorThreshold &&
                 carryHeat > AIConstants::PositioningCarryThreatenedHeatThresholdLow);
            if (carryThreatened)
            {
                const int d = distanceBetween(Position{ x, y }, *carryPos);
                if (d == 1)
                {
                    s += (u.tankScore > u.carryScore ? AIConstants::PositioningBodyguardAdjacencyTankBonusD1
                                                     : AIConstants::PositioningBodyguardAdjacencyCarryBonusD1);
                }
                else if (d == NearbyDistance)
                {
                    s += (u.tankScore > u.carryScore ? AIConstants::PositioningBodyguardAdjacencyTankBonusD2
                                                     : AIConstants::PositioningBodyguardAdjacencyCarryBonusD2);
                }
            }
        }

        s += u.overall * AIConstants::PositioningOverallTotalWeight;
        return s;
    };

    const int backRowY = GameConstants::FormationHeight - 1;
    Position placedCarryPos{ GameConstants::DefaultFormationX, backRowY };
    bool haveCarryPos = false;

    if (carry >= 0)
    {
        float best = AIConstants::PositioningVeryNegativeScore;
        Position bestPos{ GameConstants::DefaultFormationX, backRowY };
        for (int y = backRowY; y >= GameConstants::DefaultFormationY; --y)
        {
            for (int x = 0; x < GameConstants::FormationWidth; ++x)
            {
                if (isOcc(x, y))
                {
                    continue;
                }
                const OwnUnitInfo& u = infos[static_cast<std::size_t>(carry)];
                const float s = scoreFor(u, x, y, nullptr);
                if (s > best)
                {
                    best = s;
                    bestPos = Position{ x, y };
                }
            }
            if (best > AIConstants::PositioningVeryNegativeScoreEarlyExit)
            {
                break;
            }
        }
        place(carry, bestPos.x, bestPos.y);
        placedCarryPos = bestPos;
        haveCarryPos = true;
    }

    if (tank >= 0)
    {
        float best = AIConstants::PositioningVeryNegativeScore;
        Position bestPos{ GameConstants::DefaultFormationX, 0 };
        for (int y = 0; y <= GameConstants::DefaultFormationY; ++y)
        {
            for (int x = 0; x < GameConstants::FormationWidth; ++x)
            {
                if (isOcc(x, y))
                {
                    continue;
                }
                const OwnUnitInfo& u = infos[static_cast<std::size_t>(tank)];
                const float s = scoreFor(u, x, y, haveCarryPos ? &placedCarryPos : nullptr);
                if (s > best)
                {
                    best = s;
                    bestPos = Position{ x, y };
                }
            }
        }
        place(tank, bestPos.x, bestPos.y);
    }

    std::vector<int> remaining;
    remaining.reserve(player.board().size());
    for (const OwnUnitInfo& u : infos)
    {
        if (u.index == carry || u.index == tank)
        {
            continue;
        }
        remaining.push_back(u.index);
    }

    std::stable_sort(remaining.begin(), remaining.end(), [&](int a, int b)
    {
        const OwnUnitInfo& ua = infos[static_cast<std::size_t>(a)];
        const OwnUnitInfo& ub = infos[static_cast<std::size_t>(b)];
        static constexpr float SortOverallWeight = 0.1f;
        const float sa = std::max(ua.carryScore, ua.tankScore) + ua.overall * SortOverallWeight;
        const float sb = std::max(ub.carryScore, ub.tankScore) + ub.overall * SortOverallWeight;
        if (sa != sb)
        {
            return sa > sb;
        }
        return a < b;
    });

    for (int idx : remaining)
    {
        const OwnUnitInfo& u = infos[static_cast<std::size_t>(idx)];
        float best = AIConstants::PositioningVeryNegativeScore;
        Position bestPos{ GameConstants::DefaultFormationX, GameConstants::DefaultFormationY };
        for (int y = 0; y < GameConstants::FormationHeight; ++y)
        {
            for (int x = 0; x < GameConstants::FormationWidth; ++x)
            {
                if (isOcc(x, y))
                {
                    continue;
                }
                const float s = scoreFor(u, x, y, haveCarryPos ? &placedCarryPos : nullptr);
                if (s > best)
                {
                    best = s;
                    bestPos = Position{ x, y };
                }
            }
        }
        place(idx, bestPos.x, bestPos.y);
    }
}
