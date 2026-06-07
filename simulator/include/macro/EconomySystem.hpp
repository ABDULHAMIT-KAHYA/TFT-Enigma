#pragma once

#include <cstdint>
#include "macro/PlayerState.hpp"
struct EconomyResult
{
    std::int32_t baseGold = 0;
    std::int32_t interest = 0;
    std::int32_t winBonus = 0;
    std::int32_t streakBonus = 0;
    std::int32_t total = 0;
};

class EconomySystem
{
public:
    static EconomyResult applyRoundEnd(PlayerState& player, bool won);
};

