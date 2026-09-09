#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

class ContentManager;

struct LobbyPlayerResult
{
    std::string name{};
    std::int32_t health = 0;
    std::int32_t gold = 0;
    int level = 1;
    int placement = 0;
    bool eliminated = false;
};

struct LobbySimulationResult
{
    std::uint32_t seed = 0;
    int initialPlayers = 0;
    int roundsPlayed = 0;
    int actionsAfterElimination = 0;
    bool sharedPoolValid = true;
    bool completed = false;
    std::string winner{};
    std::string summary{};
    std::vector<LobbyPlayerResult> players{};
};

class LobbySimulation
{
public:
    static LobbySimulationResult simulate(const ContentManager& content,
                                          std::uint32_t seed,
                                          bool verbose,
                                          std::ostream* out);

    static int run(const ContentManager& content, std::uint32_t seed, std::ostream& out);
};
