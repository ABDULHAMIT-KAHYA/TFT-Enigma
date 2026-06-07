#include "macro/LegalActionGenerator.hpp"
#include "macro/ShopSystem.hpp"
#include "constants/GameConstants.hpp"
#include <algorithm>
#include <sstream>

static std::string idxLabel(const char* prefix, int idx)
{
    std::ostringstream ss;
    ss << prefix << idx;
    return ss.str();
}

std::vector<MacroAction> LegalActionGenerator::generate(const PlayerState& player)
{
    std::vector<MacroAction> actions;

    const bool benchHasSpace = player.bench().size() < player.benchLimit();
    const bool boardHasSpace = static_cast<int>(player.board().size()) < player.unitCap();

    for (std::size_t i = 0; i < player.shop().size(); ++i)
    {
        const ShopOffer& o = player.shop()[i];
        if (o.championName.empty())
        {
            continue;
        }
        if (!benchHasSpace)
        {
            continue;
        }
        if (!player.canAfford(o.cost))
        {
            continue;
        }
        MacroAction a;
        a.type = MacroActionType::BuyUnit;
        a.shopIndex = static_cast<int>(i);
        a.goldCost = o.cost;
        a.debugName = "BuyUnit " + idxLabel("#", static_cast<int>(i));
        actions.push_back(std::move(a));
    }

    for (std::size_t i = 0; i < player.bench().size(); ++i)
    {
        MacroAction a;
        a.type = MacroActionType::SellUnit;
        a.benchIndex = static_cast<int>(i);
        a.goldCost = 0;
        a.debugName = "SellBench " + idxLabel("#", static_cast<int>(i));
        actions.push_back(std::move(a));
    }
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        MacroAction a;
        a.type = MacroActionType::SellUnit;
        a.boardIndex = static_cast<int>(i);
        a.goldCost = 0;
        a.debugName = "SellBoard " + idxLabel("#", static_cast<int>(i));
        actions.push_back(std::move(a));
    }

    if (player.canAfford(ShopSystem::rerollCost()))
    {
        MacroAction a;
        a.type = MacroActionType::RerollShop;
        a.goldCost = ShopSystem::rerollCost();
        a.debugName = "RerollShop";
        actions.push_back(std::move(a));
    }

    if (player.level() < GameConstants::MaxLevel && player.canAfford(ShopSystem::xpForBuy()))
    {
        MacroAction a;
        a.type = MacroActionType::BuyXp;
        a.goldCost = ShopSystem::xpForBuy();
        a.debugName = "BuyXp";
        actions.push_back(std::move(a));
    }

    if (boardHasSpace)
    {
        for (std::size_t i = 0; i < player.bench().size(); ++i)
        {
            MacroAction a;
            a.type = MacroActionType::MoveBenchToBoard;
            a.benchIndex = static_cast<int>(i);
            a.goldCost = 0;
            a.debugName = "MoveBenchToBoard " + idxLabel("#", static_cast<int>(i));
            actions.push_back(std::move(a));
        }
    }

    if (benchHasSpace)
    {
        for (std::size_t i = 0; i < player.board().size(); ++i)
        {
            MacroAction a;
            a.type = MacroActionType::MoveBoardToBench;
            a.boardIndex = static_cast<int>(i);
            a.goldCost = 0;
            a.debugName = "MoveBoardToBench " + idxLabel("#", static_cast<int>(i));
            actions.push_back(std::move(a));
        }
    }

    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        const OwnedUnit& u = player.board()[i];
        bool occupied[GameConstants::FormationWidth * GameConstants::FormationHeight] = {};
        for (std::size_t j = 0; j < player.board().size(); ++j)
        {
            if (j == i)
            {
                continue;
            }
            const OwnedUnit& o = player.board()[j];
            if (!o.hasFormation)
            {
                continue;
            }
            const int ox = std::clamp(o.formation.x, 0, GameConstants::FormationWidth - 1);
            const int oy = std::clamp(o.formation.y, 0, GameConstants::FormationHeight - 1);
            occupied[oy * GameConstants::FormationWidth + ox] = true;
        }
        for (int fx = 0; fx < static_cast<int>(GameConstants::FormationWidth); ++fx)
        {
            for (int fy = 0; fy < static_cast<int>(GameConstants::FormationHeight); ++fy)
            {
                if (u.hasFormation && u.formation.x == fx && u.formation.y == fy)
                {
                    continue;
                }
                if (occupied[fy * GameConstants::FormationWidth + fx])
                {
                    continue;
                }
                MacroAction a;
                a.type = MacroActionType::RepositionUnit;
                a.boardIndex = static_cast<int>(i);
                a.targetPosition = Position{ fx, fy };
                a.goldCost = 0;
                a.debugName = "Reposition " + idxLabel("#", static_cast<int>(i));
                actions.push_back(std::move(a));
            }
        }
    }

    for (std::size_t itemIdx = 0; itemIdx < player.itemBench().size(); ++itemIdx)
    {
        if (player.itemBench()[itemIdx].empty())
        {
            continue;
        }
        for (std::size_t unitIdx = 0; unitIdx < player.board().size(); ++unitIdx)
        {
            if (player.board()[unitIdx].items.size() >= GameConstants::MaxItemsPerUnit)
            {
                continue;
            }
            MacroAction a;
            a.type = MacroActionType::EquipItem;
            a.boardIndex = static_cast<int>(unitIdx);
            a.itemIndex = static_cast<int>(itemIdx);
            a.goldCost = 0;
            a.debugName = "EquipItem unit#" + std::to_string(unitIdx) + " item#" + std::to_string(itemIdx);
            actions.push_back(std::move(a));
        }
    }

    MacroAction end;
    end.type = MacroActionType::EndTurn;
    end.goldCost = 0;
    end.debugName = "EndTurn";
    actions.push_back(std::move(end));

    return actions;
}
