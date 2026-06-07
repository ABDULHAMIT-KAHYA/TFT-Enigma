#include "validation/ScenarioSystem.hpp"
#include "core/Json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

static std::string readFileToString(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f)
    {
        throw std::runtime_error("Failed to open scenario file: " + path.string());
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool hasKeyObj(const JsonValue& v, std::string_view key)
{
    return v.isObject() && v.hasKey(key);
}

static std::int32_t getInt(const JsonValue& obj, std::string_view key, std::int32_t fallback)
{
    if (!hasKeyObj(obj, key)) return fallback;
    const JsonValue& v = obj.at(key);
    if (!v.isNumber()) return fallback;
    return static_cast<std::int32_t>(std::lround(v.asNumber()));
}

static std::string getString(const JsonValue& obj, std::string_view key, std::string fallback)
{
    if (!hasKeyObj(obj, key)) return fallback;
    const JsonValue& v = obj.at(key);
    if (!v.isString()) return fallback;
    return v.asString();
}

static Position getPos(const JsonValue& obj)
{
    Position p{ 0, 0 };
    if (!obj.isObject())
    {
        return p;
    }
    p.x = getInt(obj, "x", 0);
    p.y = getInt(obj, "y", 0);
    return p;
}

static ScenarioUnit parseScenarioUnit(const JsonValue& v)
{
    if (!v.isObject())
    {
        throw std::runtime_error("Scenario unit must be object");
    }
    ScenarioUnit u{};
    u.champion = getString(v, "champion", "");
    u.championIndex = getInt(v, "championIndex", -1);
    if (u.champion.empty() && u.championIndex < 0)
    {
        throw std::runtime_error("Scenario unit missing champion");
    }
    if (hasKeyObj(v, "pos"))
    {
        u.position = getPos(v.at("pos"));
    }
    else if (hasKeyObj(v, "position"))
    {
        u.position = getPos(v.at("position"));
    }
    if (hasKeyObj(v, "items") && v.at("items").isArray())
    {
        for (const JsonValue& it : v.at("items").asArray())
        {
            if (it.isString())
            {
                u.items.push_back(it.asString());
            }
        }
    }
    return u;
}

CombatScenario ScenarioSystem::loadFromFile(const std::string& filePath)
{
    const JsonValue root = parseJson(readFileToString(filePath));
    if (!root.isObject())
    {
        throw std::runtime_error("Scenario root must be object");
    }

    CombatScenario s{};
    s.seed = static_cast<std::uint32_t>(getInt(root, "seed", 1));
    s.dtMs = getInt(root, "dtMs", 100);

    if (hasKeyObj(root, "teamA") && root.at("teamA").isArray())
    {
        for (const JsonValue& u : root.at("teamA").asArray())
        {
            s.teamA.push_back(parseScenarioUnit(u));
        }
    }
    if (hasKeyObj(root, "teamB") && root.at("teamB").isArray())
    {
        for (const JsonValue& u : root.at("teamB").asArray())
        {
            s.teamB.push_back(parseScenarioUnit(u));
        }
    }
    return s;
}
