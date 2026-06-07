#include "ai/FutureStateEvaluator.hpp"
#include "ai/BoardStrengthEvaluator.hpp"
#include "ai/UpgradePotentialEvaluator.hpp"
#include "ai/UnitValueEvaluator.hpp"
#include "constants/AIConstants.hpp"
#include "constants/MacroConstants.hpp"
#include "macro/RoundSchedule.hpp"
#include <algorithm>
#include <cmath>

static float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

static int expectedLevelForStage(int stage, int roundIndex)
{
    if (stage <= 1) return 3;
    if (stage == 2) return 4;
    if (stage == 3) return 6;
    if (stage == 4) return 7;
    return 8;
}

FutureEval FutureStateEvaluator::evaluate(const PlayerState& player,
                                          const ContentManager& content,
                                          const EnemySnapshot* enemy,
                                          const SharedUnitPool* pool,
                                          int stage,
                                          int roundIndex,
                                          const PlayerState* opponent)
{
    const BoardScore bs = BoardStrengthEvaluator::evaluate(player, content);

    const float hp = static_cast<float>(std::max<std::int32_t>(0, player.health()));
    const float gold = static_cast<float>(std::max<std::int32_t>(0, player.gold()));
    const float lvl = static_cast<float>(std::max(1, player.level()));

    const float boardOnly =
        bs.unitPower +
        bs.traitPower +
        bs.itemPower +
        bs.frontlinePower +
        bs.carryPower;

    const UpgradePotentialScore upg = UpgradePotentialEvaluator::evaluate(player, content, pool);

    const int s = std::clamp(stage, 1, 9);
    const float stageNorm = clamp01(static_cast<float>(s - 1) / 6.0f);
    const float hpNorm = clamp01(hp / AIConstants::PercentScale);

    const float urgencyDenom =
        std::max(1.0f, AIConstants::FutureEvalUrgencyHpThreshold - AIConstants::FutureEvalUrgencyHpFloor);
    const float urgency =
        clamp01((AIConstants::FutureEvalUrgencyHpThreshold - hp) / urgencyDenom);

    const float requiredBoardNorm =
        clamp01(AIConstants::FutureEvalMinBoardNormBase +
                AIConstants::FutureEvalMinBoardNormPerStage * static_cast<float>(s) +
                AIConstants::FutureEvalMinBoardNormPerLevel * (lvl - 4.0f));
    const float requiredBoard = requiredBoardNorm * AIConstants::EnemyScorePressureScale;

    const float deficit = std::max(0.0f, requiredBoard - boardOnly);
    const float deficitNorm = deficit / std::max(1.0f, AIConstants::EnemyScorePressureScale);
    const float deficitPenalty =
        std::pow(deficitNorm, AIConstants::FutureEvalBoardDeficitPenaltyPow) *
        AIConstants::FutureEvalBoardDeficitPenaltyWeight *
        (1.0f + stageNorm + urgency * 1.6f);

    float goldValuePer = AIConstants::FutureEvalGoldValueBasePerGold *
                         (1.0f - stageNorm * AIConstants::FutureEvalGoldValueStageScale -
                          urgency * AIConstants::FutureEvalGoldValueUrgencyScale);
    goldValuePer = std::max(0.08f, goldValuePer);

    float hoardCap =
        AIConstants::FutureEvalHoardCapStageBase -
        static_cast<float>(std::max(0, s - 2)) * AIConstants::FutureEvalHoardCapStagePer -
        urgency * AIConstants::FutureEvalHoardCapUrgencyCut;
    hoardCap = std::clamp(hoardCap, AIConstants::FutureEvalHoardCapMin, AIConstants::FutureEvalHoardCapStageBase);

    const float excessGold = std::max(0.0f, gold - hoardCap);
    const float hoardSeverity = 0.15f + 0.85f * clamp01(deficitNorm * 2.0f) + urgency;
    const float hoardPenalty = excessGold * AIConstants::FutureEvalGoldHoardPenaltyPerGold * hoardSeverity;

    const float highGoldLowLevelPenalty =
        (s >= 3 && lvl <= 4.0f && gold >= AIConstants::FutureEvalHighGoldLowLevelThreshold)
            ? AIConstants::FutureEvalHighGoldLowLevelPenalty * (1.0f + urgency) * AIConstants::FutureEvalHighGoldLowLevelPenaltyScale
            : 0.0f;

    const int expLvl = expectedLevelForStage(s, roundIndex);
    const float lvlDef = std::max(0.0f, static_cast<float>(expLvl) - lvl);
    const float levelDeficitPenalty =
        lvlDef * AIConstants::FutureEvalLevelDeficitPenalty * (1.0f + stageNorm + urgency * 1.5f);

    float capBlockedPenalty = 0.0f;
    if (static_cast<int>(player.board().size()) >= player.unitCap() && !player.bench().empty() && lvlDef > 0.0f)
    {
        float bestBench = 0.0f;
        float worstBoard = 1e9f;
        for (const OwnedUnit& u : player.bench())
        {
            bestBench = std::max(bestBench, UnitValueEvaluator::evaluate(u, content, &player).total);
        }
        for (const OwnedUnit& u : player.board())
        {
            worstBoard = std::min(worstBoard, UnitValueEvaluator::evaluate(u, content, &player).total);
        }
        if (worstBoard > 1e8f) worstBoard = 0.0f;
        const float blocked = std::max(0.0f, bestBench - worstBoard);
        capBlockedPenalty = blocked * AIConstants::FutureEvalCapBlockedBenchPenaltyWeight * lvlDef * (1.0f + urgency);
    }

    const float tempo = clamp01(boardOnly / std::max(1.0f, requiredBoard));
    const float econNorm = clamp01(gold / static_cast<float>(MacroConstants::MaxInterestGold));
    const float upgradeNorm = clamp01(upg.total / AIConstants::EnemyScorePressureScale);

    const float top4 =
        clamp01(0.15f +
                0.55f * hpNorm +
                0.55f * tempo +
                0.12f * econNorm +
                0.10f * upgradeNorm +
                0.05f * clamp01((lvl - 4.0f) / 5.0f) -
                0.08f * stageNorm -
                0.12f * urgency * stageNorm);

    const float hpPenalty = (hp <= 0.0f) ? 1e6f : 0.0f;

    float enemyBoard = 0.0f;
    float enemyHp = 100.0f;
    if (opponent)
    {
        const BoardScore ebs = BoardStrengthEvaluator::evaluate(*opponent, content);
        enemyBoard =
            ebs.unitPower +
            ebs.traitPower +
            ebs.itemPower +
            ebs.frontlinePower +
            ebs.carryPower;
        enemyHp = static_cast<float>(std::max<std::int32_t>(0, opponent->health()));
    }
    else if (enemy)
    {
        enemyBoard = enemy->boardStrength;
        enemyHp = static_cast<float>(std::max<std::int32_t>(0, enemy->hp));
    }

    const float boardDiff = boardOnly - enemyBoard;
    const float hpDiff = hp - enemyHp;
    const float winSignal = (boardDiff / 210.0f) + (hpDiff / 110.0f);
    const float winProb = clamp01(0.5f + 0.5f * std::tanh(winSignal));

    FutureEval out{};
    out.board = boardOnly;
    out.enemyBoard = enemyBoard;
    out.top4Prob = top4;
    out.winProb = winProb;
    out.placementEV = std::clamp(AIConstants::FutureEvalPlacementFromTop4Base -
                                     top4 * AIConstants::FutureEvalPlacementFromTop4Scale,
                                 1.0f,
                                 8.0f);
    out.ev =
        boardOnly +
        hp * AIConstants::FutureEvalHpWeight +
        gold * goldValuePer +
        lvl * AIConstants::FutureEvalLevelWeight +
        upg.total * AIConstants::FutureEvalUpgradePotentialWeight +
        top4 * AIConstants::FutureEvalTop4Weight -
        std::max(0.0f, -boardDiff) * AIConstants::FutureEvalRelativeBoardDiffWeight -
        std::max(0.0f, -hpDiff) * AIConstants::FutureEvalRelativeHpDiffWeight +
        winProb * AIConstants::FutureEvalWinProbWeight -
        deficitPenalty -
        hoardPenalty -
        highGoldLowLevelPenalty -
        levelDeficitPenalty -
        capBlockedPenalty -
        hpPenalty;
    return out;
}
