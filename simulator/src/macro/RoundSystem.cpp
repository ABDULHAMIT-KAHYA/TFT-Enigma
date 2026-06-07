#include "macro/RoundSystem.hpp"
#include "core/Board.hpp"
#include "combat/Combat.hpp"
#include "constants/CombatConstants.hpp"
#include "combat/DamageSystem.hpp"
#include "core/GameState.hpp"
#include "constants/GameConstants.hpp"
#include "core/Logger.hpp"
#include "macro/RoundSchedule.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

static Position teamAPos(int index)
{
    return Position{ 4 + (index % 3), 6 + (index / 3) };
}

static Position teamBPos(int index)
{
    return Position{ 4 + (index % 3), 3 - (index / 3) };
}

static Position mapFormationPosition(const OwnedUnit& u, TeamId team, int fallbackIndex)
{
    if (!u.hasFormation)
    {
        return team == TeamId::TeamA ? teamAPos(fallbackIndex) : teamBPos(fallbackIndex);
    }

    const int col = std::clamp(static_cast<int>(u.formation.x), 0, static_cast<int>(GameConstants::FormationWidth) - 1);
    const int row = std::clamp(static_cast<int>(u.formation.y), 0, static_cast<int>(GameConstants::FormationHeight) - 1);

    const int x = 2 + col;
    const int teamAys[3] = { 7, 6, 5 };
    const int teamBys[3] = { 2, 3, 4 };
    const int y = team == TeamId::TeamA ? teamAys[row] : teamBys[row];

    return Position{ x, y };
}

RoundSystem::RoundSystem(const ContentManager& content, SharedUnitPool& pool)
    : content_(&content),
      pool_(&pool)
{
}

void RoundSystem::startRound(PlayerState& player, ShopSystem& shop, Random& rng, std::int32_t roundIndex)
{
    std::ostringstream ss;
    ss << "Round " << roundIndex << " start for " << player.name()
       << " | Gold: " << player.gold()
       << " | Level: " << player.level()
       << " | HP: " << player.health();
    std::cout << ss.str() << "\n";

    autoDeploy(player);
    shop.reroll(player, rng, false);

    std::cout << "Shop for " << player.name() << ": ";
    for (const ShopOffer& o : player.shop())
    {
        if (o.championName.empty())
        {
            continue;
        }
        std::cout << o.championName << "(" << o.cost << ") ";
    }
    std::cout << "\n";
}

void RoundSystem::buyXp(PlayerState& player, std::int32_t goldSpend)
{
    const std::int32_t spend = std::max<std::int32_t>(0, goldSpend);
    if (spend == 0)
    {
        return;
    }
    if (!player.spendGold(spend))
    {
        return;
    }
    player.addXp(spend);
    std::cout << player.name() << " bought " << spend << " XP\n";
}

void RoundSystem::autoLevel(PlayerState& player)
{
    while (player.level() < GameConstants::MaxLevel)
    {
        const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
        if (player.xp() < need)
        {
            break;
        }
        player.addXp(-need);
        player.setLevel(player.level() + 1);
        std::cout << player.name() << " reached Level " << player.level() << "\n";
    }
}

void RoundSystem::autoDeploy(PlayerState& player)
{
    while (static_cast<int>(player.board().size()) < player.unitCap() && !player.bench().empty())
    {
        player.moveBenchToBoard(0);
    }

    while (static_cast<int>(player.board().size()) > player.unitCap())
    {
        player.moveBoardToBench(player.board().size() - 1);
    }
}

static std::vector<Unit> instantiateTeam(const ContentManager& content,
                                        const PlayerState& player,
                                        TeamId team)
{
    std::vector<Unit> out;
    out.reserve(player.board().size());

    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        const OwnedUnit& u = player.board()[i];
        const Position p = mapFormationPosition(u, team, static_cast<int>(i));
        Unit unit = content.createUnit(u.championName, p, team);
        const int star = std::clamp(u.starLevel, 1, 3);
        const float mult = star == 1 ? 1.0f : star == 2 ? CombatConstants::TwoStarStatMultiplier : CombatConstants::ThreeStarStatMultiplier;
        unit.setMaxHp(static_cast<std::int32_t>(std::lround(static_cast<float>(unit.getMaxHp()) * mult)));
        unit.setHp(unit.getMaxHp());
        unit.setAd(static_cast<std::int32_t>(std::lround(static_cast<float>(unit.getAd()) * mult)));
        for (const std::string& itemName : u.items)
        {
            if (itemName.empty())
            {
                continue;
            }
            if (const Item* item = content.getItem(itemName))
            {
                unit.addItem(*item);
            }
        }
        out.push_back(std::move(unit));
    }
    return out;
}

static int countAlive(const std::vector<Unit>& units, TeamId team)
{
    int alive = 0;
    for (const Unit& u : units)
    {
        if (u.isAlive() && u.getTeamId() == team)
        {
            ++alive;
        }
    }
    return alive;
}

static int starDamageScore(const PlayerState& player)
{
    int score = 0;
    for (const OwnedUnit& u : player.board())
    {
        if (u.starLevel == 2) score += 1;
        else if (u.starLevel == 3) score += 3;
    }
    return score;
}

std::int32_t RoundSystem::computePlayerDamage(int survivingEnemyUnits, int roundIndex) const
{
    const RoundInfo info = RoundSchedule::get(roundIndex);
    int base = 2;
    if (info.stage <= 2) base = 2;
    else if (info.stage == 3) base = 4;
    else if (info.stage == 4) base = 6;
    else if (info.stage == 5) base = 8;
    else if (info.stage == 6) base = 10;
    else base = 12;
    return base + std::max(0, survivingEnemyUnits);
}

RoundResult RoundSystem::runPvP(PlayerState& a, PlayerState& b, std::int32_t roundIndex, std::uint32_t combatSeed)
{
    autoDeploy(a);
    autoDeploy(b);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    Logger logger(std::cout);
    logger.setMode(LogMode::Silent);

    DamageSystem::setSeed(combatSeed);

    std::vector<Unit> teamA = instantiateTeam(*content_, a, TeamId::TeamA);
    std::vector<Unit> teamB = instantiateTeam(*content_, b, TeamId::TeamB);

    std::vector<Unit> all;
    all.reserve(teamA.size() + teamB.size());
    for (const Unit& u : teamA) all.push_back(u);
    for (const Unit& u : teamB) all.push_back(u);

    GameState state(std::move(board), std::move(all), std::move(logger), *content_);
    Combat combat;
    combat.run(state);

    const int aliveA = countAlive(state.units(), TeamId::TeamA);
    const int aliveB = countAlive(state.units(), TeamId::TeamB);

    RoundResult result;
    result.survivingA = aliveA;
    result.survivingB = aliveB;
    result.playerAWon = aliveA > 0 && aliveB == 0;
    result.playerBWon = aliveB > 0 && aliveA == 0;

    if (result.playerAWon)
    {
        result.damageToB = computePlayerDamage(aliveA, roundIndex) + starDamageScore(a);
    }
    else if (result.playerBWon)
    {
        result.damageToA = computePlayerDamage(aliveB, roundIndex) + starDamageScore(b);
    }

    return result;
}

RoundResult RoundSystem::runPvE(PlayerState& player, std::int32_t roundIndex, std::uint32_t combatSeed)
{
    autoDeploy(player);

    Board board(GameConstants::BoardWidth, GameConstants::BoardHeight);
    Logger logger(std::cout);
    logger.setMode(LogMode::Silent);

    DamageSystem::setSeed(combatSeed);

    std::vector<Unit> teamA = instantiateTeam(*content_, player, TeamId::TeamA);

    std::vector<Unit> pve;
    pve.push_back(content_->createUnit(pickChampionByIndex(*content_, 0), teamBPos(0), TeamId::TeamB));
    pve.push_back(content_->createUnit(pickChampionByIndex(*content_, 1), teamBPos(1), TeamId::TeamB));

    std::vector<Unit> all;
    all.reserve(teamA.size() + pve.size());
    for (const Unit& u : teamA) all.push_back(u);
    for (const Unit& u : pve) all.push_back(u);

    GameState state(std::move(board), std::move(all), std::move(logger), *content_);
    Combat combat;
    combat.run(state);

    const int aliveA = countAlive(state.units(), TeamId::TeamA);
    const int aliveB = countAlive(state.units(), TeamId::TeamB);

    RoundResult result;
    result.survivingA = aliveA;
    result.survivingB = aliveB;
    result.playerAWon = aliveA > 0 && aliveB == 0;

    if (!result.playerAWon)
    {
        result.damageToA = computePlayerDamage(aliveB, roundIndex);
    }
    return result;
}

void RoundSystem::applyRoundEndEconomy(PlayerState& player, bool won)
{
    const std::int32_t baseGold = 5;
    const std::int32_t interest = player.interest();

    std::int32_t streakBonus = 0;
    if (player.winStreak() >= 5 || player.loseStreak() >= 5) streakBonus = 3;
    else if (player.winStreak() >= 3 || player.loseStreak() >= 3) streakBonus = 2;
    else if (player.winStreak() >= 2 || player.loseStreak() >= 2) streakBonus = 1;

    std::int32_t winBonus = won ? 1 : 0;

    player.addGold(baseGold + interest + streakBonus + winBonus);
}
