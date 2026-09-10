#include "ai/AgentEvaluator.hpp"

#include "ai/LearnedPolicyModel.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "macro/LobbySimulation.hpp"

#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
constexpr int LobbySeats = 8;

struct SideMetrics
{
    int seats = 0;
    int wins = 0;
    int top2 = 0;
    int top4 = 0;
    int eighth = 0;
    int finalHp = 0;
    int finalGold = 0;
    int finalLevel = 0;
    int placementSum = 0;

    void add(const LobbyPlayerResult& player)
    {
        seats += 1;
        placementSum += player.placement;
        if (player.placement == 1) wins += 1;
        if (player.placement <= 2) top2 += 1;
        if (player.placement <= 4) top4 += 1;
        if (player.placement == 8) eighth += 1;
        finalHp += player.health;
        finalGold += player.gold;
        finalLevel += player.level;
    }

    double avgPlacement() const { return seats > 0 ? static_cast<double>(placementSum) / seats : 0.0; }
    double rate(int count) const { return seats > 0 ? static_cast<double>(count) / seats : 0.0; }
    double avg(int total) const { return seats > 0 ? static_cast<double>(total) / seats : 0.0; }
};

bool candidateSeat(int gameIndex, int seat)
{
    return ((seat + gameIndex) % 2) == 0;
}
}

int AgentEvaluator::run(const ContentManager& content,
                        int games,
                        std::uint32_t seed,
                        const std::string& policyModelPath,
                        std::ostream& out)
{
    if (games <= 0)
    {
        games = 1;
    }

    LearnedPolicyModel policy;
    std::string error;
    if (!policy.load(policyModelPath, error))
    {
        out << "ERROR: failed to load candidate policy model: " << error << "\n";
        return 1;
    }

    SideMetrics candidate{};
    SideMetrics baseline{};
    int invalidGames = 0;
    int timeoutCount = 0;

    for (int game = 0; game < games; ++game)
    {
        std::vector<SimpleMacroAIConfig> configs;
        configs.resize(LobbySeats);
        for (int seat = 0; seat < LobbySeats; ++seat)
        {
            if (candidateSeat(game, seat))
            {
                configs[static_cast<std::size_t>(seat)].learnedPolicy = &policy;
            }
        }

        const LobbySimulationResult result = LobbySimulation::simulate(content,
                                                                       seed + static_cast<std::uint32_t>(game),
                                                                       false,
                                                                       nullptr,
                                                                       &configs);
        if (!result.completed || !result.sharedPoolValid || !result.economyAccountingBalanced || result.players.size() != LobbySeats)
        {
            invalidGames += 1;
        }
        timeoutCount += result.combatTimeoutCount;

        for (int seat = 0; seat < static_cast<int>(result.players.size()); ++seat)
        {
            if (candidateSeat(game, seat))
            {
                candidate.add(result.players[static_cast<std::size_t>(seat)]);
            }
            else
            {
                baseline.add(result.players[static_cast<std::size_t>(seat)]);
            }
        }
    }

    const double placementAdvantage = baseline.avgPlacement() - candidate.avgPlacement();
    out << "Agent evaluation complete | games=" << games
        << " | seed=" << seed
        << " | invalidGames=" << invalidGames
        << " | timeoutCount=" << timeoutCount
        << " | policy=" << policyModelPath << "\n";
    out << std::fixed << std::setprecision(4);
    out << "Candidate learned | avgPlacement=" << candidate.avgPlacement()
        << " | winRate=" << candidate.rate(candidate.wins)
        << " | top4Rate=" << candidate.rate(candidate.top4)
        << " | top2Rate=" << candidate.rate(candidate.top2)
        << " | eighthRate=" << candidate.rate(candidate.eighth)
        << " | avgHp=" << candidate.avg(candidate.finalHp)
        << " | avgGold=" << candidate.avg(candidate.finalGold)
        << " | avgLevel=" << candidate.avg(candidate.finalLevel)
        << "\n";
    out << "Baseline heuristic | avgPlacement=" << baseline.avgPlacement()
        << " | winRate=" << baseline.rate(baseline.wins)
        << " | top4Rate=" << baseline.rate(baseline.top4)
        << " | top2Rate=" << baseline.rate(baseline.top2)
        << " | eighthRate=" << baseline.rate(baseline.eighth)
        << " | avgHp=" << baseline.avg(baseline.finalHp)
        << " | avgGold=" << baseline.avg(baseline.finalGold)
        << " | avgLevel=" << baseline.avg(baseline.finalLevel)
        << "\n";
    out << "Head-to-head placement advantage=" << placementAdvantage << "\n";
    out << "Promotion decision: " << (placementAdvantage >= 0.10 && invalidGames == 0 && timeoutCount == 0 ? "PROMOTED" : "CANDIDATE REJECTED") << "\n";

    return invalidGames == 0 ? 0 : 1;
}
