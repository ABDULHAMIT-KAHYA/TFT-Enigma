#include "content/ContentManager.hpp"
#include "ai/AIPlayer.hpp"
#include "validation/CombatValidation.hpp"
#include "validation/ScenarioSystem.hpp"
#include "ai/SelfPlay.hpp"
#include "import/TFTDataImporter.hpp"
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
    bool useMonteCarlo = false;
    bool mcDebug = false;
    int selfplay = 0;
    std::string scenarioPath;
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
        else if (arg == "--selfplay" && i + 1 < argc)
        {
            selfplay = std::max(1, std::stoi(argv[i + 1]));
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
        else if (arg == "--scenario" && i + 1 < argc)
        {
            scenarioPath = argv[i + 1];
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

    if (!scenarioPath.empty())
    {
        std::cout << "Scenario run\n";
    }
    else if (selfplay > 0)
    {
        std::cout << "Selfplay\n";
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
        return SelfPlay::run(content, baseSeed, selfplay, std::cout);
    }

    if (validate)
    {
        ValidationReport report = CombatValidation::runAll(content, std::cout);
        report.print(std::cout);
        return report.hasFail() ? 1 : 0;
    }

    return MacroSimulation::run(content, baseSeed, useMonteCarlo, mcDebug, std::cout);
}
