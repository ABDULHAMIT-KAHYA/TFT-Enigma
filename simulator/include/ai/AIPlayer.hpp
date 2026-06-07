#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>
#include "content/ContentManager.hpp"
#include "macro/PlayerState.hpp"
#include "core/Random.hpp"
#include "macro/RoundSystem.hpp"
#include "macro/ShopSystem.hpp"
struct BoardScore
{
    float frontline = 0.0f;
    float dps = 0.0f;
    float survivability = 0.0f;
    float traitSynergy = 0.0f;
    float carryPotential = 0.0f;

    float total() const;
};

struct CombatPrediction
{
    float winChance = 0.0f;
    float avgSurvivorDelta = 0.0f;
};

class AIPlayer
{
public:
    AIPlayer(std::string name, std::uint32_t seed);

    void takeTurn(PlayerState& self,
                  const std::vector<PlayerState>& enemies,
                  ShopSystem& shop,
                  RoundSystem& rounds,
                  const ContentManager& content,
                  std::int32_t roundIndex,
                  Random& rng,
                  std::ostream& out);

private:
    std::uint32_t mixSeed(std::uint32_t a, std::uint32_t b) const;

    std::unordered_map<std::string, int> traitCounts(const PlayerState& player,
                                                     const ContentManager& content) const;

    BoardScore evaluateBoard(const PlayerState& player,
                             const ContentManager& content) const;

    CombatPrediction predictCombat(const PlayerState& self,
                                   const PlayerState& enemy,
                                   ShopSystem& shop,
                                   RoundSystem& rounds,
                                   const ContentManager& content,
                                   std::int32_t roundIndex,
                                   int simulations,
                                   std::uint32_t seedBase) const;

    std::string choosePrimaryTrait(const PlayerState& player,
                                  const ContentManager& content) const;

    int chooseCarryIndex(const PlayerState& player,
                         const ContentManager& content) const;

    int chooseFrontlineIndex(const PlayerState& player,
                             const ContentManager& content,
                             int skipIndex) const;

    float unitDpsScore(const ChampionDefinition& def) const;
    float unitTankScore(const ChampionDefinition& def) const;
    float unitCarryScore(const ChampionDefinition& def) const;
    float itemCarryWeight(const Item& item) const;
    float itemTankWeight(const Item& item) const;

    void applyPositioning(PlayerState& self,
                          const ContentManager& content,
                          const std::vector<PlayerState>& enemies,
                          std::ostream& out) const;

    void applyItemPlacement(PlayerState& self,
                            const ContentManager& content,
                            std::ostream& out) const;

    void shopBuyPhase(PlayerState& self,
                      const ContentManager& content,
                      ShopSystem& shop,
                      Random& rng,
                      std::ostream& out) const;

    bool shouldReroll(const PlayerState& self,
                      const PlayerState* enemy,
                      const ContentManager& content) const;

    bool shouldLevel(const PlayerState& self,
                     const PlayerState* enemy,
                     const ContentManager& content,
                     std::int32_t roundIndex) const;

    std::string name_;
    std::uint32_t seed_;
    std::string targetTrait_;
};

