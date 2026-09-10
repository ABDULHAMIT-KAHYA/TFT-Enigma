#include "content/ContentManager.hpp"
#include "ai/AIPlayer.hpp"
#include "validation/CombatValidation.hpp"
#include "validation/ScenarioSystem.hpp"
#include "ai/SelfPlay.hpp"
#include "ai/AgentEvaluator.hpp"
#include "import/TFTDataImporter.hpp"
#include "validation/ReplaySystem.hpp"
#include "macro/LobbySimulation.hpp"
#include "macro/MacroSimulation.hpp"
#include "core/RandomManager.hpp"
#include "macro/PlayerState.hpp"
#include "core/Random.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/ShopSystem.hpp"
#include "combat/DamageSystem.hpp"
#include "core/Logger.hpp"
#include "combat/Combat.hpp"
#include "core/GameState.hpp"
#include <vector>
#include <iostream>
#include <exception>
#include <string>
#include <filesystem>
#include <algorithm>

int main(int argc, char** argv)
{
    const std::uint32_t baseSeed = 1u;
    bool validate = false;
    bool importLive = false;
    bool importCachedTraits = false;
    bool importCachedItems = false;
    bool useMonteCarlo = false;
    bool mcDebug = false;
    bool lobby = false;
    std::uint32_t lobbySeed = baseSeed;
    std::uint32_t selfplaySeed = baseSeed;
    int selfplay = 0;
    int evaluateAgents = 0;
    std::uint32_t evalSeed = 500000u;
    std::string policyModelPath{};
    std::string selfplayOutput = "results/selfplay_results.json";
    std::string scenarioPath;
    std::string recordReplayScenario;
    std::string recordReplayOutput;
    std::string playReplayPath;
    std::string verifyReplayPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "--validate")
        {
            validate = true;
        }
        else if (arg == "--import-live-tft")
        {
            importLive = true;
        }
        else if (arg == "--import-cached-traits")
        {
            importCachedTraits = true;
        }
        else if (arg == "--import-cached-items")
        {
            importCachedItems = true;
        }
        else if (arg == "--selfplay" && i + 1 < argc)
        {
            selfplay = std::max(1, std::stoi(argv[i + 1]));
            i += 1;
        }
        else if (arg == "--selfplay-seed" && i + 1 < argc)
        {
            selfplaySeed = static_cast<std::uint32_t>(std::stoul(argv[i + 1]));
            i += 1;
        }
        else if (arg == "--selfplay-output" && i + 1 < argc)
        {
            selfplayOutput = argv[i + 1];
            i += 1;
        }
        else if (arg == "--evaluate-agents" && i + 1 < argc)
        {
            evaluateAgents = std::max(1, std::stoi(argv[i + 1]));
            i += 1;
        }
        else if (arg == "--eval-seed" && i + 1 < argc)
        {
            evalSeed = static_cast<std::uint32_t>(std::stoul(argv[i + 1]));
            i += 1;
        }
        else if (arg == "--policy-model" && i + 1 < argc)
        {
            policyModelPath = argv[i + 1];
            i += 1;
        }
        else if (arg == "--mc")
        {
            useMonteCarlo = true;
        }
        else if (arg == "--mc-debug")
        {
            useMonteCarlo = true;
            mcDebug = true;
        }
        else if (arg == "--lobby")
        {
            lobby = true;
        }
        else if (arg == "--lobby-seed" && i + 1 < argc)
        {
            lobby = true;
            lobbySeed = static_cast<std::uint32_t>(std::stoul(argv[i + 1]));
            i += 1;
        }
        else if (arg == "--scenario" && i + 1 < argc)
        {
            scenarioPath = argv[i + 1];
            i += 1;
        }
        else if (arg == "--record-replay" && i + 2 < argc)
        {
            recordReplayScenario = argv[i + 1];
            recordReplayOutput = argv[i + 2];
            i += 2;
        }
        else if (arg == "--play-replay" && i + 1 < argc)
        {
            playReplayPath = argv[i + 1];
            i += 1;
        }
        else if (arg == "--verify-replay" && i + 1 < argc)
        {
            verifyReplayPath = argv[i + 1];
            i += 1;
        }
    }

    const std::filesystem::path dataRoot = std::filesystem::absolute(std::filesystem::path("../../data"));

    if (importLive)
    {
        std::cout << "TFT live import\n";
        std::cout << "Data root: " << dataRoot.string() << "\n";
        TFTDataImporter importer;
        importer.importLiveTft(dataRoot.string(), std::cout);
        return 0;
    }

    if (importCachedTraits)
    {
        std::cout << "TFT cached trait import\n";
        std::cout << "Data root: " << dataRoot.string() << "\n";
        TFTDataImporter importer;
        importer.importTraitsFromCachedTft(dataRoot.string(), std::cout);
        return 0;
    }

    if (importCachedItems)
    {
        std::cout << "TFT cached item import\n";
        std::cout << "Data root: " << dataRoot.string() << "\n";
        TFTDataImporter importer;
        importer.importItemsFromCachedTft(dataRoot.string(), std::cout);
        return 0;
    }

    if (!scenarioPath.empty())
    {
        std::cout << "Scenario run\n";
    }
    else if (lobby)
    {
        std::cout << "Lobby simulation\n";
    }
    else if (selfplay > 0)
    {
        std::cout << "Selfplay\n";
    }
    else if (evaluateAgents > 0)
    {
        std::cout << "Agent evaluation\n";
    }
    else
    {
        std::cout << (validate ? "Combat validation\n" : "Macro layer simulation (AI)\n");
    }
    std::cout << std::flush;

    std::cout << "=== CONTENT LOADING ===\n";
    std::cout << "Data root: " << dataRoot.string() << "\n";

    std::int32_t championFiles = 0;
    const std::filesystem::path championsDir = dataRoot / "champions";
    if (std::filesystem::exists(championsDir))
    {
        for (const auto& e : std::filesystem::directory_iterator(championsDir))
        {
            if (!e.is_regular_file())
            {
                continue;
            }
            const std::string ext = e.path().extension().string();
            if (ext == ".json")
            {
                championFiles += 1;
            }
        }
    }
    std::cout << "Champion files: " << championFiles << "\n";

    ContentManager content;
    try
    {
        content.loadAll(dataRoot.string());
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << "\n";
        return 1;
    }

    if (content.championCount() == 0)
    {
        std::cout << "ERROR: No champions loaded from data root\n";
        return 1;
    }

    std::cout << "Loaded Champions: " << content.championCount() << "\n";
    std::cout << "Loaded Abilities: " << content.abilityCount() << "\n";
    std::cout << "Loaded Traits: " << content.traitCount() << "\n";
    std::cout << "Loaded Items: " << content.itemCount() << "\n";
    std::cout << "Validation: PASS\n\n";

    std::vector<std::string> firstNames;
    firstNames.reserve(content.champions().size());
    for (const auto& [name, _] : content.champions())
    {
        firstNames.push_back(name);
    }
    std::sort(firstNames.begin(), firstNames.end());
    std::cout << "First champions: ";
    for (std::size_t i = 0; i < std::min<std::size_t>(5, firstNames.size()); ++i)
    {
        if (i) std::cout << ", ";
        std::cout << firstNames[i];
    }
    std::cout << "\n\n";
    std::cout << std::flush;

    if (!recordReplayScenario.empty())
    {
        const ReplayRunResult r = ReplaySystem::recordScenario(content, recordReplayScenario, recordReplayOutput, std::cout);
        return r.ok ? 0 : 1;
    }

    if (!playReplayPath.empty())
    {
        const ReplayRunResult r = ReplaySystem::playReplay(content, playReplayPath, std::cout, false);
        return r.ok ? 0 : 1;
    }

    if (!verifyReplayPath.empty())
    {
        const ReplayRunResult r = ReplaySystem::playReplay(content, verifyReplayPath, std::cout, true);
        return r.ok ? 0 : 1;
    }

    if (!scenarioPath.empty())
    {
        const CombatScenario scenario = ScenarioSystem::loadFromFile(scenarioPath);

        RandomManager::global().setSeed(scenario.seed);
        DamageSystem::setSeed(scenario.seed);

        Logger logger(std::cout);
        logger.setMode(LogMode::Verbose);
        Board board(10, 10);

        std::vector<Unit> all;
        for (const ScenarioUnit& u : scenario.teamA)
        {
            const std::string champ = !u.champion.empty()
                ? u.champion
                : pickChampionByIndex(content, static_cast<std::size_t>(u.championIndex));
            Unit unit = content.createUnit(champ, u.position, TeamId::TeamA);
            for (const std::string& itemName : u.items)
            {
                if (const Item* item = content.getItem(itemName))
                {
                    unit.addItem(*item);
                }
            }
            all.push_back(std::move(unit));
        }
        for (const ScenarioUnit& u : scenario.teamB)
        {
            const std::string champ = !u.champion.empty()
                ? u.champion
                : pickChampionByIndex(content, static_cast<std::size_t>(u.championIndex));
            Unit unit = content.createUnit(champ, u.position, TeamId::TeamB);
            for (const std::string& itemName : u.items)
            {
                if (const Item* item = content.getItem(itemName))
                {
                    unit.addItem(*item);
                }
            }
            all.push_back(std::move(unit));
        }

        GameState state(std::move(board), std::move(all), std::move(logger), content);
        state.setDtMs(scenario.dtMs);

        CombatValidation::setEnabled(true);
        CombatValidation::setDetailedLogs(true);

        Combat combat;
        combat.run(state);
        return 0;
    }

    if (selfplay > 0)
    {
        return SelfPlay::run(content, selfplaySeed, selfplay, selfplayOutput, std::cout);
    }

    if (evaluateAgents > 0)
    {
        if (policyModelPath.empty())
        {
            std::cerr << "ERROR: --evaluate-agents requires --policy-model <path>\n";
            return 1;
        }
        return AgentEvaluator::run(content, evaluateAgents, evalSeed, policyModelPath, std::cout);
    }

    if (lobby)
    {
        return LobbySimulation::run(content, lobbySeed, std::cout);
    }

    if (validate)
    {
        ValidationReport report = CombatValidation::runAll(content, std::cout);
        report.print(std::cout);
        return report.hasFail() ? 1 : 0;
    }

    return MacroSimulation::run(content, baseSeed, useMonteCarlo, mcDebug, std::cout);
}





