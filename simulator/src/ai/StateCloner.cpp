#include "ai/StateCloner.hpp"
#include "content/ChampionFilter.hpp"
#include "constants/GameConstants.hpp"
#include <algorithm>

ClonedMacroState::ClonedMacroState(const ContentManager& content,
                                   const PlayerState& p,
                                   const SharedUnitPool& sharedPool,
                                   const Random& r)
    : player(p),
      pool(sharedPool),
      rng(r),
      shop(content, pool),
      rounds(content, pool)
{
}

ClonedMacroState StateCloner::clone(const ContentManager& content,
                                   const PlayerState& player,
                                   const SharedUnitPool& pool,
                                   const Random& rng)
{
    return ClonedMacroState(content, player, pool, rng);
}

PlayerState StateCloner::enemyFromSnapshot(const EnemySnapshot& snap)
{
    PlayerState e("Enemy");
    e.setLevel(std::max(1, snap.level));
    e.addGold(std::max<std::int32_t>(0, snap.gold));
    e.addXp(std::max<std::int32_t>(0, snap.xp));
    const std::int32_t missing = std::max<std::int32_t>(0, 100 - snap.hp);
    if (missing > 0)
    {
        e.takeDamage(missing);
    }

    for (const EnemySnapshot::UnitInfo& u : snap.units)
    {
        OwnedUnit ou{};
        ou.championName = u.championName;
        ou.starLevel = std::clamp(u.starLevel, 1, 3);
        ou.cost = std::max(1, u.cost);
        ou.hasFormation = true;
        ou.formation = Position{
            std::clamp(u.position.x, 0, GameConstants::FormationWidth - 1),
            std::clamp(u.position.y, 0, GameConstants::FormationHeight - 1)
        };
        e.boardMutable().push_back(std::move(ou));
    }

    return e;
}
