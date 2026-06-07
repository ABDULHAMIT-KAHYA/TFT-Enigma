#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "core/Position.hpp"
struct ScenarioUnit
{
    std::string champion;
    int championIndex = -1;
    Position position{ 0, 0 };
    std::vector<std::string> items{};
};

struct CombatScenario
{
    std::uint32_t seed = 1u;
    std::int32_t dtMs = 100;
    std::vector<ScenarioUnit> teamA{};
    std::vector<ScenarioUnit> teamB{};
};

class ScenarioSystem
{
public:
    static CombatScenario loadFromFile(const std::string& filePath);
};
