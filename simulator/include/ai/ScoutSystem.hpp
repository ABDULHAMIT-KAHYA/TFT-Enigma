#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "core/Position.hpp"
struct EnemySnapshot
{
    struct UnitInfo
    {
        std::string championName{};
        int cost = 1;
        int starLevel = 1;
        int range = 1;
        float threat = 0.0f;
        float aoeThreat = 0.0f;
        float itemOffense = 0.0f;
        float itemDefense = 0.0f;
        float itemCaster = 0.0f;
        bool hasJumpThreat = false;
        Position position{ 3, 1 };
    };

    float boardStrength = 0.0f;
    std::vector<std::string> activeTraits{};
    int carryUnit = -1;
    std::vector<int> frontlineUnits{};
    float itemStrength = 0.0f;
    std::vector<Position> positioning{};
    std::vector<UnitInfo> units{};
    int level = 1;
    std::int32_t gold = 0;
    std::int32_t xp = 0;
    std::int32_t winStreak = 0;
    std::int32_t loseStreak = 0;
    std::int32_t hp = 100;
};

class ScoutSystem
{
public:
    static EnemySnapshot snapshot(const PlayerState& enemy, const ContentManager& content);
};
