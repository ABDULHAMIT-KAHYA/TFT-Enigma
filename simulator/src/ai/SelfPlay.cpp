#include "ai/SelfPlay.hpp"
#include <iostream>
#include "combat/Combat.hpp"
#include "content/ContentManager.hpp"
#include "core/GameState.hpp"
#include "core/Logger.hpp"
#include "core/RandomManager.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static std::string jsonString(std::string_view s)
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

static std::vector<std::string> listChampionNames(const ContentManager& content)
{
    std::vector<std::string> names;
    names.reserve(content.champions().size());
    for (const auto& [name, _] : content.champions())
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

static Position teamAPos(int index)
{
    return Position{ 3 + (index % 5), 7 + (index / 5) };
}

static Position teamBPos(int index)
{
    return Position{ 3 + (index % 5), 2 - (index / 5) };
}

static int runOneCombat(const ContentManager& content,
                        std::uint32_t seed,
                        const std::vector<std::string>& names,
                        int teamSize,
                        std::map<std::string, int>& picks,
                        std::map<std::string, int>& wins)
{
    RandomManager::global().setSeed(seed);

    std::vector<Unit> teamA;
    std::vector<Unit> teamB;
    teamA.reserve(teamSize);
    teamB.reserve(teamSize);

    for (int i = 0; i < teamSize; ++i)
    {
        const int idxA = RandomManager::global().randomInt(static_cast<int>(names.size()));
        const int idxB = RandomManager::global().randomInt(static_cast<int>(names.size()));
        const std::string& aName = names[static_cast<std::size_t>(idxA)];
        const std::string& bName = names[static_cast<std::size_t>(idxB)];

        teamA.push_back(content.createUnit(aName, teamAPos(i), TeamId::TeamA));
        teamB.push_back(content.createUnit(bName, teamBPos(i), TeamId::TeamB));

        picks[aName] += 1;
        picks[bName] += 1;
    }

    Logger logger(std::cout);
    logger.setMode(LogMode::Silent);
    Board board(10, 10);

    std::vector<Unit> all;
    all.reserve(teamA.size() + teamB.size());
    for (const Unit& u : teamA) all.push_back(u);
    for (const Unit& u : teamB) all.push_back(u);

    GameState state(std::move(board), std::move(all), std::move(logger), content);
    state.setDtMs(100);

    Combat combat;
    combat.run(state);

    const bool aAlive = state.hasAlive(TeamId::TeamA);
    const bool bAlive = state.hasAlive(TeamId::TeamB);
    if (aAlive && !bAlive)
    {
        for (const Unit& u : teamA) wins[u.getName()] += 1;
        return 1;
    }
    if (bAlive && !aAlive)
    {
        for (const Unit& u : teamB) wins[u.getName()] += 1;
        return 2;
    }
    return 0;
}

static void writeResults(const std::filesystem::path& outPath,
                         int games,
                         const std::map<std::string, int>& picks,
                         const std::map<std::string, int>& wins)
{
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream f(outPath, std::ios::out | std::ios::binary | std::ios::trunc);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"games\": " << games << ",\n";
    ss << "  \"champions\": [\n";

    bool first = true;
    for (const auto& [name, p] : picks)
    {
        const int w = wins.count(name) ? wins.at(name) : 0;
        if (!first) ss << ",\n";
        first = false;
        ss << "    { \"name\": " << jsonString(name)
           << ", \"picks\": " << p
           << ", \"wins\": " << w
           << " }";
    }
    ss << "\n  ]\n";
    ss << "}\n";

    const std::string out = ss.str();
    f.write(out.data(), static_cast<std::streamsize>(out.size()));
}

int SelfPlay::run(const ContentManager& content, std::uint32_t baseSeed, int games, std::ostream& out)
{
    games = std::max(1, games);

    const std::vector<std::string> names = listChampionNames(content);
    if (names.size() < 2)
    {
        out << "ERROR: need at least 2 champions loaded for selfplay\n";
        return 1;
    }

    std::map<std::string, int> picks;
    std::map<std::string, int> wins;

    int aWins = 0;
    int bWins = 0;
    int draws = 0;

    for (int i = 0; i < games; ++i)
    {
        const std::uint32_t seed = baseSeed ^ (static_cast<std::uint32_t>(i) * 2654435761u);
        const int winner = runOneCombat(content, seed, names, 4, picks, wins);
        if (winner == 1) aWins += 1;
        else if (winner == 2) bWins += 1;
        else draws += 1;
    }

    out << "Selfplay complete | games=" << games
        << " | A wins=" << aWins
        << " | B wins=" << bWins
        << " | draws=" << draws
        << "\n";

    writeResults(std::filesystem::path("results") / "selfplay_results.json", games, picks, wins);
    out << "Saved results/selfplay_results.json\n";

    return 0;
}

