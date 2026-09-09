#include "macro/LobbySimulation.hpp"
#include "ai/ScoutSystem.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "content/ChampionFilter.hpp"
#include "content/ContentManager.hpp"
#include "core/Random.hpp"
#include "macro/EconomySystem.hpp"
#include "macro/MacroSimulation.hpp"
#include "macro/PlayerState.hpp"
#include "macro/RoundSchedule.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/ShopSystem.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <sstream>
#include <streambuf>

namespace
{
constexpr int LobbyPlayerCount = 8;
constexpr int LobbyOpeningGold = 12;
constexpr int LobbyMaxOpeningBuys = 3;

std::uint32_t mixLobbySeed(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t x = a ^ (b + GameConstants::SeedMixConstant + (a << GameConstants::SeedMixShiftA) + (a >> GameConstants::SeedMixShiftB));
    x ^= (x << GameConstants::SeedScrambleShift1);
    x ^= (x >> GameConstants::SeedScrambleShift2);
    x ^= (x << GameConstants::SeedScrambleShift3);
    return x;
}

struct NullBuffer final : std::streambuf
{
    int overflow(int c) override { return c; }
};

bool alive(const PlayerState& p)
{
    return p.health() > 0;
}

void buyOpeningBoard(PlayerState& player, ShopSystem& shop, Random& rng)
{
    shop.reroll(player, rng, false);
    for (int bought = 0; bought < LobbyMaxOpeningBuys; ++bought)
    {
        bool didBuy = false;
        for (std::size_t i = 0; i < player.shop().size(); ++i)
        {
            const ShopOffer& offer = player.shop()[i];
            if (!offer.championName.empty() && player.canAfford(offer.cost) && player.bench().size() < player.benchLimit())
            {
                didBuy = shop.buy(player, i);
                if (didBuy)
                {
                    break;
                }
            }
        }
        if (!didBuy)
        {
            break;
        }
    }
    while (static_cast<int>(player.board().size()) < player.unitCap() && !player.bench().empty())
    {
        player.moveBenchToBoard(0);
    }
}

void takeAliveTurn(PlayerState& player,
                   SimpleMacroAI& ai,
                   ShopSystem& shop,
                   Random& rng,
                   const ContentManager& content,
                   const EnemySnapshot* enemy,
                   const SharedUnitPool* pool,
                   int stage,
                   int roundIndex,
                   std::ostream& out)
{
    shop.reroll(player, rng, false);
    MacroTurnStats stats{};
    MacroSimulation::takeTurnForValidationAt(player, ai, shop, rng, content, enemy, pool, stage, roundIndex, out, stats);
}

std::vector<int> activeIndices(const std::vector<PlayerState>& players)
{
    std::vector<int> indices;
    indices.reserve(players.size());
    for (std::size_t i = 0; i < players.size(); ++i)
    {
        if (alive(players[i]))
        {
            indices.push_back(static_cast<int>(i));
        }
    }
    return indices;
}

std::vector<std::pair<int, int>> deterministicPairs(std::vector<int> active, int roundIndex, int& bye)
{
    bye = -1;
    if (active.empty())
    {
        return {};
    }
    const int rotateBy = roundIndex % static_cast<int>(active.size());
    std::rotate(active.begin(), active.begin() + rotateBy, active.end());

    if ((active.size() % 2u) != 0u)
    {
        bye = active.back();
        active.pop_back();
    }

    std::vector<std::pair<int, int>> pairs;
    for (std::size_t i = 0; i + 1 < active.size(); i += 2)
    {
        pairs.push_back({ active[i], active[i + 1] });
    }
    return pairs;
}

bool sharedPoolCountsValid(const ContentManager& content, const SharedUnitPool& pool)
{
    for (const auto& [name, champ] : content.champions())
    {
        if (!isPlayableChampion(champ))
        {
            continue;
        }
        if (pool.availableCount(name) < 0)
        {
            return false;
        }
    }
    return true;
}

std::string summarize(const std::vector<LobbyPlayerResult>& players,
                      int roundsPlayed,
                      int actionsAfterElimination,
                      bool sharedPoolValid,
                      bool completed)
{
    std::vector<LobbyPlayerResult> sorted = players;
    std::sort(sorted.begin(), sorted.end(), [](const LobbyPlayerResult& a, const LobbyPlayerResult& b)
    {
        if (a.placement != b.placement) return a.placement < b.placement;
        return a.name < b.name;
    });

    std::ostringstream ss;
    ss << "rounds=" << roundsPlayed
       << "|afterElim=" << actionsAfterElimination
       << "|pool=" << (sharedPoolValid ? 1 : 0)
       << "|done=" << (completed ? 1 : 0);
    for (const LobbyPlayerResult& p : sorted)
    {
        ss << "|" << p.placement << ":" << p.name << ":" << p.health << ":" << p.gold << ":" << p.level << ":" << (p.eliminated ? 1 : 0);
    }
    return ss.str();
}
}

LobbySimulationResult LobbySimulation::simulate(const ContentManager& content,
                                                std::uint32_t seed,
                                                bool verbose,
                                                std::ostream* out)
{
    NullBuffer nb;
    std::ostream nullOut(&nb);
    std::ostream& log = (verbose && out) ? *out : nullOut;

    SharedUnitPool pool(content);
    ShopSystem shop(content, pool);
    RoundSystem rounds(content, pool);

    std::vector<PlayerState> players;
    players.reserve(LobbyPlayerCount);
    std::array<Random, LobbyPlayerCount> rngs = {
        Random(seed ^ 0x1001u), Random(seed ^ 0x1002u), Random(seed ^ 0x1003u), Random(seed ^ 0x1004u),
        Random(seed ^ 0x1005u), Random(seed ^ 0x1006u), Random(seed ^ 0x1007u), Random(seed ^ 0x1008u)
    };
    std::array<SimpleMacroAI, LobbyPlayerCount> ais = {
        SimpleMacroAI(seed ^ 0xA001u), SimpleMacroAI(seed ^ 0xA002u), SimpleMacroAI(seed ^ 0xA003u), SimpleMacroAI(seed ^ 0xA004u),
        SimpleMacroAI(seed ^ 0xA005u), SimpleMacroAI(seed ^ 0xA006u), SimpleMacroAI(seed ^ 0xA007u), SimpleMacroAI(seed ^ 0xA008u)
    };

    for (int i = 0; i < LobbyPlayerCount; ++i)
    {
        std::ostringstream name;
        name << "Lobby Player " << (i + 1);
        players.emplace_back(name.str());
        players.back().addGold(MacroConstants::StartingGold + LobbyOpeningGold);
        if (i >= 2)
        {
            players.back().setLevel(2);
        }
        if (i >= 5)
        {
            players.back().setLevel(3);
        }
        buyOpeningBoard(players.back(), shop, rngs[static_cast<std::size_t>(i)]);
    }

    std::array<int, LobbyPlayerCount> placements{};
    int nextPlacement = LobbyPlayerCount;
    int roundsPlayed = 0;
    int actionsAfterElimination = 0;

    for (int roundIndex = 0; roundIndex < MacroConstants::MaxRounds; ++roundIndex)
    {
        const std::vector<int> activeBefore = activeIndices(players);
        if (activeBefore.size() <= 1)
        {
            break;
        }

        const RoundInfo info = RoundSchedule::get(roundIndex);
        log << "\n=== LOBBY ROUND " << info.label << (info.isPve ? " PvE" : " PvP") << " ===\n";

        for (std::size_t i = 0; i < players.size(); ++i)
        {
            PlayerState& player = players[i];
            if (!alive(player))
            {
                if (!player.shop().empty())
                {
                    actionsAfterElimination += 1;
                }
                continue;
            }

            EnemySnapshot enemy{};
            const std::vector<int> currentActive = activeIndices(players);
            for (int idx : currentActive)
            {
                if (idx != static_cast<int>(i))
                {
                    enemy = ScoutSystem::snapshot(players[static_cast<std::size_t>(idx)], content);
                    break;
                }
            }
            takeAliveTurn(player,
                          ais[i],
                          shop,
                          rngs[i],
                          content,
                          &enemy,
                          &pool,
                          info.stage,
                          roundIndex,
                          log);
        }

        std::vector<int> eliminatedThisRound;
        if (info.isPve)
        {
            for (int idx : activeBefore)
            {
                PlayerState& player = players[static_cast<std::size_t>(idx)];
                const std::uint32_t fightSeed = mixLobbySeed(seed, static_cast<std::uint32_t>(roundIndex * LobbyPlayerCount + idx));
                const RoundResult r = rounds.runPvE(player, roundIndex, fightSeed);
                player.takeDamage(r.damageToA);
                (void)EconomySystem::applyRoundEnd(player, r.playerAWon);
                if (!alive(player))
                {
                    eliminatedThisRound.push_back(idx);
                }
            }
        }
        else
        {
            int bye = -1;
            const std::vector<std::pair<int, int>> pairs = deterministicPairs(activeBefore, roundIndex, bye);
            for (std::size_t pairIndex = 0; pairIndex < pairs.size(); ++pairIndex)
            {
                const int ai = pairs[pairIndex].first;
                const int bi = pairs[pairIndex].second;
                PlayerState& a = players[static_cast<std::size_t>(ai)];
                PlayerState& b = players[static_cast<std::size_t>(bi)];
                const std::uint32_t fightSeed = mixLobbySeed(seed, static_cast<std::uint32_t>(roundIndex * LobbyPlayerCount + static_cast<int>(pairIndex)));
                const RoundResult r = rounds.runPvP(a, b, roundIndex, fightSeed);
                a.takeDamage(r.damageToA);
                b.takeDamage(r.damageToB);
                (void)EconomySystem::applyRoundEnd(a, r.playerAWon);
                (void)EconomySystem::applyRoundEnd(b, r.playerBWon);
                if (!alive(a)) eliminatedThisRound.push_back(ai);
                if (!alive(b)) eliminatedThisRound.push_back(bi);
            }
            if (bye >= 0)
            {
                players[static_cast<std::size_t>(bye)].recordWin();
                players[static_cast<std::size_t>(bye)].addGold(MacroConstants::BaseRoundGold + players[static_cast<std::size_t>(bye)].interest() + MacroConstants::WinBonusGold);
            }
        }

        std::sort(eliminatedThisRound.begin(), eliminatedThisRound.end(), [&](int a, int b)
        {
            const PlayerState& pa = players[static_cast<std::size_t>(a)];
            const PlayerState& pb = players[static_cast<std::size_t>(b)];
            if (pa.health() != pb.health()) return pa.health() < pb.health();
            return a < b;
        });
        eliminatedThisRound.erase(std::unique(eliminatedThisRound.begin(), eliminatedThisRound.end()), eliminatedThisRound.end());
        for (int idx : eliminatedThisRound)
        {
            if (placements[static_cast<std::size_t>(idx)] == 0)
            {
                placements[static_cast<std::size_t>(idx)] = nextPlacement--;
                players[static_cast<std::size_t>(idx)].shopMutable().clear();
            }
        }

        roundsPlayed = roundIndex + 1;
    }

    std::vector<int> survivors = activeIndices(players);
    std::sort(survivors.begin(), survivors.end(), [&](int a, int b)
    {
        const PlayerState& pa = players[static_cast<std::size_t>(a)];
        const PlayerState& pb = players[static_cast<std::size_t>(b)];
        if (pa.health() != pb.health()) return pa.health() > pb.health();
        if (pa.level() != pb.level()) return pa.level() > pb.level();
        if (pa.gold() != pb.gold()) return pa.gold() > pb.gold();
        return a < b;
    });

    for (int idx : survivors)
    {
        if (placements[static_cast<std::size_t>(idx)] == 0)
        {
            placements[static_cast<std::size_t>(idx)] = nextPlacement--;
        }
    }

    LobbySimulationResult result{};
    result.seed = seed;
    result.initialPlayers = LobbyPlayerCount;
    result.roundsPlayed = roundsPlayed;
    result.actionsAfterElimination = actionsAfterElimination;
    result.sharedPoolValid = sharedPoolCountsValid(content, pool);
    result.completed = !survivors.empty();
    result.players.reserve(players.size());

    for (std::size_t i = 0; i < players.size(); ++i)
    {
        LobbyPlayerResult p{};
        p.name = players[i].name();
        p.health = players[i].health();
        p.gold = players[i].gold();
        p.level = players[i].level();
        p.placement = placements[i];
        p.eliminated = p.health <= 0;
        if (p.placement == 1)
        {
            result.winner = p.name;
        }
        result.players.push_back(std::move(p));
    }

    result.summary = summarize(result.players, result.roundsPlayed, result.actionsAfterElimination, result.sharedPoolValid, result.completed);
    return result;
}

int LobbySimulation::run(const ContentManager& content, std::uint32_t seed, std::ostream& out)
{
    const LobbySimulationResult result = simulate(content, seed, true, &out);
    out << "\n=== LOBBY RESULT ===\n";
    out << "Seed: " << result.seed << "\n";
    out << "Rounds played: " << result.roundsPlayed << "\n";
    out << "Winner: " << result.winner << "\n";
    std::vector<LobbyPlayerResult> players = result.players;
    std::sort(players.begin(), players.end(), [](const LobbyPlayerResult& a, const LobbyPlayerResult& b)
    {
        return a.placement < b.placement;
    });
    for (const LobbyPlayerResult& p : players)
    {
        out << "#" << p.placement << " " << p.name
            << " HP=" << p.health
            << " Gold=" << p.gold
            << " Level=" << p.level
            << (p.eliminated ? " eliminated" : " alive")
            << "\n";
    }
    out << "Shared pool valid: " << (result.sharedPoolValid ? "yes" : "no") << "\n";
    return result.completed && result.sharedPoolValid ? 0 : 1;
}
