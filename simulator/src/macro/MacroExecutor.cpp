#include "macro/MacroExecutor.hpp"
#include "constants/GameConstants.hpp"
#include <iostream>

static void autoLevelFromXp(PlayerState& player)
{
    while (player.level() < GameConstants::MaxLevel)
    {
        const std::int32_t need = ShopSystem::xpToNextLevel(player.level());
        if (need <= 0)
        {
            break;
        }
        if (player.xp() < need)
        {
            break;
        }
        player.addXp(-need);
        player.setLevel(player.level() + 1);
    }
}

bool MacroExecutor::apply(const MacroAction& action,
                          PlayerState& player,
                          ShopSystem& shop,
                          Random& rng,
                          std::ostream& out)
{
    switch (action.type)
    {
        case MacroActionType::BuyUnit:
        {
            if (action.shopIndex < 0)
            {
                return false;
            }
            const std::size_t idx = static_cast<std::size_t>(action.shopIndex);
            const bool ok = shop.buy(player, idx);
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::SellUnit:
        {
            bool ok = false;
            if (action.benchIndex >= 0)
            {
                ok = shop.sellBench(player, static_cast<std::size_t>(action.benchIndex));
            }
            else if (action.boardIndex >= 0)
            {
                ok = shop.sellBoard(player, static_cast<std::size_t>(action.boardIndex));
            }
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::RerollShop:
        {
            if (!player.canAfford(ShopSystem::rerollCost()))
            {
                out << "Action: " << action.debugName << " FAIL\n";
                return false;
            }
            shop.reroll(player, rng, true);
            out << "Action: " << action.debugName << " OK\n";
            return true;
        }
        case MacroActionType::BuyXp:
        {
            if (!player.spendGold(ShopSystem::xpForBuy()))
            {
                out << "Action: " << action.debugName << " FAIL\n";
                return false;
            }
            player.addXp(ShopSystem::xpForBuy());
            autoLevelFromXp(player);
            out << "Action: " << action.debugName << " OK\n";
            return true;
        }
        case MacroActionType::MoveBenchToBoard:
        {
            if (action.benchIndex < 0)
            {
                return false;
            }
            const bool ok = player.moveBenchToBoard(static_cast<std::size_t>(action.benchIndex));
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::MoveBoardToBench:
        {
            if (action.boardIndex < 0)
            {
                return false;
            }
            const bool ok = player.moveBoardToBench(static_cast<std::size_t>(action.boardIndex));
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::RepositionUnit:
        {
            if (action.boardIndex < 0)
            {
                return false;
            }
            const bool ok =
                player.repositionBoardUnit(static_cast<std::size_t>(action.boardIndex), action.targetPosition);
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::EquipItem:
        {
            if (action.boardIndex < 0 || action.itemIndex < 0)
            {
                return false;
            }
            const bool ok =
                player.equipItem(static_cast<std::size_t>(action.boardIndex), static_cast<std::size_t>(action.itemIndex));
            out << "Action: " << action.debugName << (ok ? " OK" : " FAIL") << "\n";
            return ok;
        }
        case MacroActionType::EndTurn:
        {
            out << "Action: " << action.debugName << "\n";
            return true;
        }
    }
    return false;
}
