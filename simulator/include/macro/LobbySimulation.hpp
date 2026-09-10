#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "macro/TrainingData.hpp"

class ContentManager;
struct SimpleMacroAIConfig;

struct LobbyPlayerResult
{
    std::string name{};
    std::int32_t health = 0;
    std::int32_t gold = 0;
    int level = 1;
    int placement = 0;
    bool eliminated = false;
    std::string compStyle{};
};

struct LobbyDecisionRecord
{
    std::uint32_t gameSeed = 0;
    int round = 0;
    int playerId = 0;
    std::int32_t hp = 0;
    std::int32_t gold = 0;
    int level = 1;
    std::string boardSummary{};
    std::string benchSummary{};
    std::string traitSummary{};
    std::string itemSummary{};
    std::string shopSummary{};
    std::vector<std::string> legalActionIds{};
    std::string chosenAction{};
    StateFeatures stateFeatures{};
    std::vector<ActionEncoding> legalActionEncodings{};
    ActionEncoding chosenActionEncoding{};
    int eventualPlacement = 0;
    float terminalReward = 0.0f;
};
struct LobbySimulationResult
{
    std::uint32_t seed = 0;
    int initialPlayers = 0;
    int roundsPlayed = 0;
    int actionsAfterElimination = 0;
    int maxFinalGold = 0;
    int maxObservedGold = 0;
    int maxEconomyEventsPerPlayerRound = 0;
    int totalEconomyIncome = 0;
    int totalTurnGoldDelta = 0;
    int totalItemsGranted = 0;
    int totalItemsEquipped = 0;
    int totalItemsOnBench = 0;
    int maxEquippedItemsOnUnit = 0;
    bool economyAccountingBalanced = true;
    bool sharedPoolValid = true;
    bool completed = false;
    std::string winner{};
    int combatTimeoutCount = 0;
    std::string summary{};
    std::vector<LobbyPlayerResult> players{};
    std::vector<LobbyDecisionRecord> decisionRecords{};
};

class LobbySimulation
{
public:
    static LobbySimulationResult simulate(const ContentManager& content,
                                          std::uint32_t seed,
                                          bool verbose,
                                          std::ostream* out,
                                          const std::vector<SimpleMacroAIConfig>* aiConfigs = nullptr);

    static int run(const ContentManager& content, std::uint32_t seed, std::ostream& out);
};



