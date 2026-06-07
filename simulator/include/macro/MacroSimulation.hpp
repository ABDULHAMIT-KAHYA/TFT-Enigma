#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

class ContentManager;
class PlayerState;
class Random;
class ShopSystem;
class SimpleMacroAI;
class SharedUnitPool;
struct MacroAction;
struct EnemySnapshot;

struct MacroTurnStats
{
    int repositionActionsExecuted = 0;
    std::vector<std::string> executedActionKeys{};
};

class MacroSimulation
{
public:
    static int run(const ContentManager& content, std::uint32_t seed, bool useMonteCarlo, bool mcDebug, std::ostream& out);

    static void takeTurnForValidation(PlayerState& player,
                                      SimpleMacroAI& ai,
                                      ShopSystem& shop,
                                      Random& rng,
                                      const ContentManager& content,
                                      const EnemySnapshot* enemy,
                                      const SharedUnitPool* pool,
                                      std::ostream& out,
                                      MacroTurnStats& stats);

    static void takeTurnForValidationAt(PlayerState& player,
                                        SimpleMacroAI& ai,
                                        ShopSystem& shop,
                                        Random& rng,
                                        const ContentManager& content,
                                        const EnemySnapshot* enemy,
                                        const SharedUnitPool* pool,
                                        int stage,
                                        int roundIndex,
                                        std::ostream& out,
                                        MacroTurnStats& stats);

    static void takeTurnWithForcedFirstAction(PlayerState& player,
                                              SimpleMacroAI& ai,
                                              ShopSystem& shop,
                                              Random& rng,
                                              const ContentManager& content,
                                              const EnemySnapshot* enemy,
                                              const SharedUnitPool* pool,
                                              const MacroAction& forcedFirstAction,
                                              int stage,
                                              int roundIndex,
                                              int maxActions,
                                              std::ostream& out);
};
