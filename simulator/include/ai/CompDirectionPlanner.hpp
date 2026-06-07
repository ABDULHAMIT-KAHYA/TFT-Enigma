#pragma once

#include <string>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
struct CompDirection
{
    std::vector<std::string> coreTraits;
    std::vector<std::string> coreUnits;
    float confidence = 0.0f;
    std::string debug{};
};

class CompDirectionPlanner
{
public:
    static CompDirection infer(const PlayerState& player, const ContentManager& content);
};

