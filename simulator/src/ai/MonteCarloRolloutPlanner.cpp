#include "ai/MonteCarloRolloutPlanner.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/MacroActionScorer.hpp"
#include "ai/SimpleMacroAI.hpp"
#include "macro/MacroSimulation.hpp"
#include "macro/ShopSystem.hpp"
#include "core/Random.hpp"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <streambuf>
namespace
{
    struct NullBuffer final : std::streambuf
    {
        int overflow(int c) override { return c; }
    };

    static std::uint32_t mixSeed(std::uint32_t a, std::uint32_t b)
    {
        std::uint32_t x = a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
        x ^= (x >> 16);
        x *= 0x7feb352du;
        x ^= (x >> 15);
        x *= 0x846ca68bu;
        x ^= (x >> 16);
        return x;
    }
}

MonteCarloRolloutPlanner::MonteCarloRolloutPlanner(std::uint32_t seed) : seed_(seed) {}

MacroAction MonteCarloRolloutPlanner::chooseAction(const PlayerState& player,
                                                   const ContentManager& content,
                                                   const std::vector<MacroAction>& legalActions,
                                                   const EnemySnapshot* enemy,
                                                   const SharedUnitPool* pool,
                                                   int candidates,
                                                   int rolloutsPerCandidate,
                                                   int maxActions) const
{
    if (legalActions.empty())
    {
        return MacroAction{ MacroActionType::EndTurn };
    }

    candidates = std::clamp(candidates, 1, static_cast<int>(legalActions.size()));
    rolloutsPerCandidate = std::clamp(rolloutsPerCandidate, 1, 512);
    maxActions = std::clamp(maxActions, 1, 60);

    std::vector<ActionScore> scored = MacroActionScorer::scoreActions(player, content, legalActions, enemy, pool, 1, 0);
    std::stable_sort(scored.begin(), scored.end(), [](const ActionScore& a, const ActionScore& b)
    {
        return a.score > b.score;
    });

    if (scored.empty())
    {
        return MacroAction{ MacroActionType::EndTurn };
    }

    const float baseline = BoardStrengthEvaluator::evaluate(player, content).total;

    const int candidateCount = std::min<int>(candidates, static_cast<int>(scored.size()));
    float bestValue = -1e30f;
    float bestHeuristic = -1e30f;
    MacroAction best = scored[0].action;

    NullBuffer nullBuf;
    std::ostream nullOut(&nullBuf);

    SimpleMacroAIConfig rolloutCfg{};
    SimpleMacroAI rolloutAi(mixSeed(seed_, 0xA11A11A1u), rolloutCfg);

    for (int ci = 0; ci < candidateCount; ++ci)
    {
        const MacroAction& first = scored[ci].action;
        const float heuristic = scored[ci].score;

        float sumDelta = 0.0f;
        for (int r = 0; r < rolloutsPerCandidate; ++r)
        {
            PlayerState sim = player;

            SharedUnitPool poolCopy = pool ? *pool : SharedUnitPool(content);
            ShopSystem shop(content, poolCopy);

            Random rng(mixSeed(seed_, mixSeed(static_cast<std::uint32_t>(ci + 1), static_cast<std::uint32_t>(r + 1))));

            MacroSimulation::takeTurnWithForcedFirstAction(sim,
                                                          rolloutAi,
                                                          shop,
                                                          rng,
                                                          content,
                                                          enemy,
                                                          &poolCopy,
                                                          first,
                                                          1,
                                                          0,
                                                          maxActions,
                                                          nullOut);

            const float after = BoardStrengthEvaluator::evaluate(sim, content).total;
            sumDelta += (after - baseline);
        }

        const float avgDelta = sumDelta / static_cast<float>(rolloutsPerCandidate);

        if (avgDelta > bestValue || (avgDelta == bestValue && heuristic > bestHeuristic))
        {
            bestValue = avgDelta;
            bestHeuristic = heuristic;
            best = first;
        }
    }

    return best.type == MacroActionType::EndTurn && best.debugName.empty()
        ? MacroAction{ MacroActionType::EndTurn }
        : best;
}
