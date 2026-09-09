#include "validation/ReplaySystem.hpp"
#include "combat/Combat.hpp"
#include "combat/DamageSystem.hpp"
#include "content/ContentManager.hpp"
#include "core/GameState.hpp"
#include "core/Json.hpp"
#include "core/RandomManager.hpp"
#include "validation/ScenarioSystem.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr int ReplayVersion = 1;

    std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (!f) throw std::runtime_error("Failed to open replay: " + path.string());
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::string escapeJson(const std::string& s)
    {
        std::ostringstream out;
        for (char ch : s)
        {
            switch (ch)
            {
                case '\\': out << "\\\\"; break;
                case '"': out << "\\\""; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default: out << ch; break;
            }
        }
        return out.str();
    }

    std::string jsonString(const JsonValue& obj, std::string_view key)
    {
        if (!obj.isObject() || !obj.hasKey(key) || !obj.at(key).isString()) return "";
        return obj.at(key).asString();
    }

    std::uint64_t jsonU64(const JsonValue& obj, std::string_view key)
    {
        if (!obj.isObject() || !obj.hasKey(key) || !obj.at(key).isNumber()) return 0;
        return static_cast<std::uint64_t>(obj.at(key).asNumber());
    }

    int jsonInt(const JsonValue& obj, std::string_view key)
    {
        if (!obj.isObject() || !obj.hasKey(key) || !obj.at(key).isNumber()) return 0;
        return static_cast<int>(obj.at(key).asNumber());
    }

    GameState buildScenarioState(const ContentManager& content, const CombatScenario& scenario, std::ostream& logOut)
    {
        Logger logger(logOut);
        logger.setMode(LogMode::Silent);
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
                if (const Item* item = content.getItem(itemName)) unit.addItem(*item);
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
                if (const Item* item = content.getItem(itemName)) unit.addItem(*item);
            }
            all.push_back(std::move(unit));
        }

        GameState state(std::move(board), std::move(all), std::move(logger), content);
        state.setDtMs(scenario.dtMs);
        return state;
    }

    ReplayRunResult runScenario(const ContentManager& content,
                                const std::string& scenarioPath,
                                bool captureFrames,
                                std::ostream& out)
    {
        const CombatScenario scenario = ScenarioSystem::loadFromFile(scenarioPath);
        RandomManager::global().setSeed(scenario.seed);
        DamageSystem::setSeed(scenario.seed);

        std::ostringstream combatLog;
        GameState state = buildScenarioState(content, scenario, combatLog);
        state.setSnapshotRecording(captureFrames);
        state.captureSnapshot("initial");

        Combat combat;
        combat.run(state);

        ReplayRunResult result{};
        result.ok = true;
        result.winner = state.hasAlive(TeamId::TeamA) ? "TeamA" : state.hasAlive(TeamId::TeamB) ? "TeamB" : "Draw";
        result.finalHash = state.deterministicHash();
        result.frameCount = state.snapshots().size();
        if (captureFrames)
        {
            out << "  \"frames\": [\n";
            for (std::size_t i = 0; i < state.snapshots().size(); ++i)
            {
                out << state.snapshots()[i] << (i + 1 == state.snapshots().size() ? "" : ",") << "\n";
            }
            out << "  ]\n";
        }
        return result;
    }
}

namespace ReplaySystem
{
    ReplayRunResult recordScenario(const ContentManager& content,
                                   const std::string& scenarioPath,
                                   const std::string& outputPath,
                                   std::ostream& out)
    {
        std::ostringstream frames;
        ReplayRunResult result = runScenario(content, scenarioPath, true, frames);
        if (!result.ok) return result;

        const CombatScenario scenario = ScenarioSystem::loadFromFile(scenarioPath);
        std::ofstream f(outputPath, std::ios::out | std::ios::binary);
        if (!f)
        {
            out << "ERROR: failed to write replay: " << outputPath << "\n";
            result.ok = false;
            return result;
        }

        f << "{\n";
        f << "  \"version\": " << ReplayVersion << ",\n";
        f << "  \"engine\": \"TFT-AI\",\n";
        f << "  \"scenarioPath\": \"" << escapeJson(scenarioPath) << "\",\n";
        f << "  \"seed\": " << scenario.seed << ",\n";
        f << "  \"dtMs\": " << scenario.dtMs << ",\n";
        f << "  \"winner\": \"" << result.winner << "\",\n";
        f << "  \"finalHash\": " << result.finalHash << ",\n";
        f << frames.str();
        f << "}\n";

        out << "Replay recorded: " << outputPath << "\n";
        out << "Winner: " << result.winner << "\n";
        out << "FinalHash: " << result.finalHash << "\n";
        out << "Frames: " << result.frameCount << "\n";
        return result;
    }

    ReplayRunResult playReplay(const ContentManager& content,
                               const std::string& replayPath,
                               std::ostream& out,
                               bool verify)
    {
        ReplayRunResult result{};
        try
        {
            const JsonValue root = parseJson(readFile(replayPath));
            const int version = jsonInt(root, "version");
            if (version != ReplayVersion)
            {
                out << "ERROR: unsupported replay version " << version << "\n";
                return result;
            }
            const std::string scenarioPath = jsonString(root, "scenarioPath");
            if (scenarioPath.empty())
            {
                out << "ERROR: replay missing scenarioPath\n";
                return result;
            }
            if (!std::filesystem::exists(scenarioPath))
            {
                out << "ERROR: replay scenario missing: " << scenarioPath << "\n";
                return result;
            }

            const std::string recordedWinner = jsonString(root, "winner");
            const std::uint64_t recordedHash = jsonU64(root, "finalHash");
            std::ostringstream ignoredFrames;
            result = runScenario(content, scenarioPath, false, ignoredFrames);
            if (!result.ok) return result;

            out << "Replay: " << replayPath << "\n";
            out << "RecordedWinner: " << recordedWinner << " CurrentWinner: " << result.winner << "\n";
            out << "RecordedHash: " << recordedHash << " CurrentHash: " << result.finalHash << "\n";

            if (verify)
            {
                if (recordedWinner != result.winner)
                {
                    out << "ERROR: replay winner mismatch\n";
                    result.ok = false;
                    return result;
                }
                if (recordedHash != result.finalHash)
                {
                    out << "ERROR: replay hash mismatch\n";
                    result.ok = false;
                    return result;
                }
                out << "Replay verification: PASS\n";
            }
            result.ok = true;
            return result;
        }
        catch (const std::exception& e)
        {
            out << "ERROR: malformed replay: " << e.what() << "\n";
            result.ok = false;
            return result;
        }
    }
}
