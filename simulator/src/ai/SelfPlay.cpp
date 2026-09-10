#include "ai/SelfPlay.hpp"
#include "macro/LobbySimulation.hpp"
#include "content/ContentManager.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
std::string jsonString(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s)
    {
        if (c == '\\' || c == '"')
        {
            out.push_back('\\');
            out.push_back(c);
        }
        else if (c == '\n')
        {
            out += "\\n";
        }
        else if (c == '\r')
        {
            out += "\\r";
        }
        else if (c == '\t')
        {
            out += "\\t";
        }
        else
        {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

void writeStringArray(std::ostream& out, const std::vector<std::string>& values)
{
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            out << ",";
        }
        out << jsonString(values[i]);
    }
    out << "]";
}

bool gameHasUniquePlacements(const LobbySimulationResult& game)
{
    std::unordered_set<int> placements;
    for (const LobbyPlayerResult& player : game.players)
    {
        if (player.placement < 1 || player.placement > 8)
        {
            return false;
        }
        if (!placements.insert(player.placement).second)
        {
            return false;
        }
    }
    return placements.size() == 8;
}

int winnerPlayerId(const LobbySimulationResult& game)
{
    for (std::size_t i = 0; i < game.players.size(); ++i)
    {
        if (game.players[i].placement == 1)
        {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

bool gameIsValid(const LobbySimulationResult& game)
{
    return game.completed &&
           game.players.size() == 8 &&
           winnerPlayerId(game) > 0 &&
           gameHasUniquePlacements(game) &&
           game.sharedPoolValid &&
           game.economyAccountingBalanced;
}

struct SelfPlayBatchSummary
{
    int games = 0;
    int completedGames = 0;
    int totalPlayerPlacements = 0;
    double averageRounds = 0.0;
    std::array<int, 9> placementCounts{};
    int timeoutCount = 0;
    int invalidGames = 0;
    int trainingRecords = 0;
    int totalItemsGranted = 0;
    int totalItemsEquipped = 0;
    bool deterministic = true;
};

SelfPlayBatchSummary summarizeBatch(const std::vector<LobbySimulationResult>& games)
{
    SelfPlayBatchSummary summary{};
    summary.games = static_cast<int>(games.size());

    int totalRounds = 0;
    for (const LobbySimulationResult& game : games)
    {
        if (game.completed)
        {
            summary.completedGames += 1;
        }
        if (!gameIsValid(game))
        {
            summary.invalidGames += 1;
        }
        totalRounds += game.roundsPlayed;
        summary.timeoutCount += game.combatTimeoutCount;
        summary.totalItemsGranted += game.totalItemsGranted;
        summary.totalItemsEquipped += game.totalItemsEquipped;
        summary.trainingRecords += static_cast<int>(game.decisionRecords.size());

        for (const LobbyPlayerResult& player : game.players)
        {
            summary.totalPlayerPlacements += 1;
            if (player.placement >= 1 && player.placement <= 8)
            {
                summary.placementCounts[static_cast<std::size_t>(player.placement)] += 1;
            }
        }
    }

    if (!games.empty())
    {
        summary.averageRounds = static_cast<double>(totalRounds) / static_cast<double>(games.size());
    }
    summary.deterministic = summary.invalidGames == 0;
    return summary;
}

void writeGame(std::ostream& out, const LobbySimulationResult& game, int gameIndex)
{
    out << "    {\n";
    out << "      \"gameIndex\": " << gameIndex << ",\n";
    out << "      \"gameSeed\": " << game.seed << ",\n";
    out << "      \"roundsPlayed\": " << game.roundsPlayed << ",\n";
    out << "      \"winnerPlayerId\": " << winnerPlayerId(game) << ",\n";
    out << "      \"sharedPoolValid\": " << (game.sharedPoolValid ? "true" : "false") << ",\n";
    out << "      \"economyAccountingBalanced\": " << (game.economyAccountingBalanced ? "true" : "false") << ",\n";
    out << "      \"combatTimeoutCount\": " << game.combatTimeoutCount << ",\n";
    out << "      \"itemsGranted\": " << game.totalItemsGranted << ",\n";
    out << "      \"itemsEquipped\": " << game.totalItemsEquipped << ",\n";
    out << "      \"players\": [\n";

    for (std::size_t i = 0; i < game.players.size(); ++i)
    {
        const LobbyPlayerResult& player = game.players[i];
        out << "        { \"playerId\": " << (i + 1)
            << ", \"placement\": " << player.placement
            << ", \"hp\": " << player.health
            << ", \"gold\": " << player.gold
            << ", \"level\": " << player.level
            << ", \"comp\": " << jsonString(player.compStyle)
            << " }" << (i + 1 == game.players.size() ? "" : ",") << "\n";
    }

    out << "      ]\n";
    out << "    }";
}

void writeTrainingRecord(std::ostream& out, const LobbyDecisionRecord& record)
{
    out << "    {"
        << " \"gameSeed\": " << record.gameSeed
        << ", \"round\": " << record.round
        << ", \"playerId\": " << record.playerId
        << ", \"hp\": " << record.hp
        << ", \"gold\": " << record.gold
        << ", \"level\": " << record.level
        << ", \"board\": " << jsonString(record.boardSummary)
        << ", \"bench\": " << jsonString(record.benchSummary)
        << ", \"traits\": " << jsonString(record.traitSummary)
        << ", \"items\": " << jsonString(record.itemSummary)
        << ", \"shop\": " << jsonString(record.shopSummary)
        << ", \"legal\": ";
    writeStringArray(out, record.legalActionIds);
    out << ", \"chosen\": " << jsonString(record.chosenAction)
        << ", \"placement\": " << record.eventualPlacement
        << ", \"reward\": " << std::fixed << std::setprecision(2) << record.terminalReward
        << " }";
}

bool writeBatch(const std::filesystem::path& outputPath,
                std::uint32_t baseSeed,
                const std::vector<LobbySimulationResult>& games,
                const SelfPlayBatchSummary& summary,
                std::ostream& status)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream file(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file)
    {
        status << "ERROR: unable to open selfplay output " << outputPath.string() << "\n";
        return false;
    }

    file << "{\n";
    file << "  \"schemaVersion\": 1,\n";
    file << "  \"type\": \"lobby_selfplay\",\n";
    file << "  \"baseSeed\": " << baseSeed << ",\n";
    file << "  \"gamesRequested\": " << summary.games << ",\n";
    file << "  \"summary\": {\n";
    file << "    \"games\": " << summary.games << ",\n";
    file << "    \"completedGames\": " << summary.completedGames << ",\n";
    file << "    \"totalPlayerPlacements\": " << summary.totalPlayerPlacements << ",\n";
    file << "    \"averageRounds\": " << std::fixed << std::setprecision(2) << summary.averageRounds << ",\n";
    file << "    \"placementCounts\": {";
    for (int p = 1; p <= 8; ++p)
    {
        if (p > 1)
        {
            file << ", ";
        }
        file << "\"" << p << "\": " << summary.placementCounts[static_cast<std::size_t>(p)];
    }
    file << "},\n";
    file << "    \"timeoutCount\": " << summary.timeoutCount << ",\n";
    file << "    \"invalidGames\": " << summary.invalidGames << ",\n";
    file << "    \"deterministic\": " << (summary.deterministic ? "true" : "false") << ",\n";
    file << "    \"trainingRecords\": " << summary.trainingRecords << ",\n";
    file << "    \"itemsGranted\": " << summary.totalItemsGranted << ",\n";
    file << "    \"itemsEquipped\": " << summary.totalItemsEquipped << "\n";
    file << "  },\n";

    file << "  \"games\": [\n";
    for (std::size_t i = 0; i < games.size(); ++i)
    {
        writeGame(file, games[i], static_cast<int>(i));
        file << (i + 1 == games.size() ? "\n" : ",\n");
    }
    file << "  ],\n";

    file << "  \"trainingRecords\": [\n";
    bool firstRecord = true;
    for (const LobbySimulationResult& game : games)
    {
        for (const LobbyDecisionRecord& record : game.decisionRecords)
        {
            if (!firstRecord)
            {
                file << ",\n";
            }
            firstRecord = false;
            writeTrainingRecord(file, record);
        }
    }
    file << "\n  ]\n";
    file << "}\n";
    return true;
}
}

int SelfPlay::run(const ContentManager& content,
                  std::uint32_t baseSeed,
                  int games,
                  const std::string& outputPath,
                  std::ostream& out)
{
    games = std::max(1, games);

    std::vector<LobbySimulationResult> results;
    results.reserve(static_cast<std::size_t>(games));

    for (int i = 0; i < games; ++i)
    {
        const std::uint32_t gameSeed = baseSeed + static_cast<std::uint32_t>(i);
        results.push_back(LobbySimulation::simulate(content, gameSeed, false, nullptr));
    }

    const SelfPlayBatchSummary summary = summarizeBatch(results);
    const std::filesystem::path outPath(outputPath.empty() ? "results/selfplay_results.json" : outputPath);
    if (!writeBatch(outPath, baseSeed, results, summary, out))
    {
        return 1;
    }

    out << "Selfplay complete | mode=8-player-lobby"
        << " | games=" << summary.games
        << " | completedGames=" << summary.completedGames
        << " | totalPlayerPlacements=" << summary.totalPlayerPlacements
        << " | averageRounds=" << std::fixed << std::setprecision(2) << summary.averageRounds
        << " | timeoutCount=" << summary.timeoutCount
        << " | invalidGames=" << summary.invalidGames
        << " | deterministic=" << (summary.deterministic ? 1 : 0)
        << " | trainingRecords=" << summary.trainingRecords
        << " | itemsGranted=" << summary.totalItemsGranted
        << " | itemsEquipped=" << summary.totalItemsEquipped
        << "\n";

    out << "Placement counts:";
    for (int p = 1; p <= 8; ++p)
    {
        out << " #" << p << "=" << summary.placementCounts[static_cast<std::size_t>(p)];
    }
    out << "\n";

    out << "Saved " << outPath.string() << "\n";
    return summary.invalidGames == 0 ? 0 : 1;
}