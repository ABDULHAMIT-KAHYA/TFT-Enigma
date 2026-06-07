#include "macro/EconomySystem.hpp"
#include "constants/MacroConstants.hpp"
#include <algorithm>

EconomyResult EconomySystem::applyRoundEnd(PlayerState& player, bool won)
{
    if (won)
    {
        player.recordWin();
    }
    else
    {
        player.recordLoss();
    }

    EconomyResult r{};
    r.baseGold = MacroConstants::BaseRoundGold;
    r.interest = player.interest();
    r.winBonus = won ? MacroConstants::WinBonusGold : 0;

    const std::int32_t streak = won ? player.winStreak() : player.loseStreak();
    if (streak >= MacroConstants::StreakTier3) r.streakBonus = MacroConstants::StreakBonus3;
    else if (streak >= MacroConstants::StreakTier2) r.streakBonus = MacroConstants::StreakBonus2;
    else if (streak >= MacroConstants::StreakTier1) r.streakBonus = MacroConstants::StreakBonus1;

    r.total = r.baseGold + r.interest + r.winBonus + r.streakBonus;
    player.addGold(r.total);
    return r;
}
