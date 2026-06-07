#pragma once

struct AIConstants
{
    static constexpr float OneStarMultiplier = 1.0f;
    static constexpr float TwoStarMultiplier = 2.7f;
    static constexpr float ThreeStarMultiplier = 7.5f;

    static constexpr float UnitCostBaseWeight = 12.0f;
    static constexpr float UnitStarBonusWeight = 25.0f;

    static constexpr float CarryRangeBase = 0.7f;
    static constexpr float CarryRangeScale = 0.05f;
    static constexpr float CarryApWeight = 0.6f;
    static constexpr float CarryCritWeight = 30.0f;

    static constexpr float FrontlineDefenseScale = 0.005f;

    static constexpr float UnitCarryContributionWeight = 0.06f;
    static constexpr float UnitFrontlineContributionWeight = 0.008f;
    static constexpr float UnitItemsContributionWeight = 0.08f;

    static constexpr float TraitUnitMissingDefinitionBonus = 1.0f;
    static constexpr float TraitUnitNear1Bonus = 14.0f;
    static constexpr float TraitUnitNear2Bonus = 6.0f;
    static constexpr float TraitUnitFarBonus = 2.0f;
    static constexpr float TraitUnitNoNextBonus = 3.0f;

    static constexpr float HpPressureAllInThreshold = 12.0f;
    static constexpr float HpPressureStabilizeThreshold = 25.0f;
    static constexpr float HpPressureTempoThreshold = 40.0f;
    static constexpr float HpPressureAllInValue = 2.0f;
    static constexpr float HpPressureStabilizeValue = 1.1f;
    static constexpr float HpPressureTempoValue = 0.5f;
    static constexpr float EnemyScorePressureBaseline = 250.0f;
    static constexpr float EnemyScorePressureScale = 400.0f;
    static constexpr float EnemyScorePressureMaxAdd = 0.8f;

    static constexpr float DuplicateToTwoStarBonus = 140.0f;
    static constexpr float PairBonus = 40.0f;
    static constexpr float DuplicateToThreeStarBonus = 360.0f;
    static constexpr float TwoStarPairBonus = 110.0f;

    static constexpr float TraitAlignmentUnitBonus = 20.0f;

    static constexpr float CompPlannerShopUnitWeight = 0.25f;
    static constexpr float CompPlannerBenchUnitWeight = 0.55f;

    static constexpr float PercentScale = 100.0f;

    static constexpr float ItemStatWeightAttackDamage = 1.0f;
    static constexpr float ItemStatWeightAbilityPower = 0.8f;
    static constexpr float ItemStatWeightAttackSpeed = 35.0f;
    static constexpr float ItemStatWeightCritChance = 80.0f;
    static constexpr float ItemStatWeightCritDamage = 25.0f;
    static constexpr float ItemStatWeightArmor = 0.8f;
    static constexpr float ItemStatWeightMagicResist = 0.8f;
    static constexpr float ItemStatWeightMaxHp = 0.03f;
    static constexpr float ItemStatWeightOmnivamp = 120.0f;
    static constexpr float ItemStatWeightDamageAmp = 160.0f;
    static constexpr float ItemStatWeightManaGainOnAttack = 15.0f;

    static constexpr float ItemTriggeredEffectsBaseBonus = 12.0f;
    static constexpr float ItemTriggeredEffectPerEffectBonus = 3.0f;

    static constexpr float ScoutStar2Multiplier = 3.0f;
    static constexpr float ScoutStar3Multiplier = 9.0f;
    static constexpr float ScoutAoeThreatBonus = 25.0f;

    static constexpr float ScoutUnitBaseThreatScale = 10.0f;
    static constexpr float ScoutAdAsThreatScale = 2.2f;
    static constexpr float ScoutApThreatScale = 10.0f;
    static constexpr float ScoutCritThreatScale = 60.0f;
    static constexpr float ScoutItemOffenseThreatScale = 0.65f;
    static constexpr float ScoutItemCasterThreatScale = 0.55f;
    static constexpr float ScoutAoeCasterThreatScale = 0.75f;
    static constexpr float ScoutAoeApThreatScale = 2.0f;
    static constexpr float ScoutCarryItemCountScale = 10.0f;

    static constexpr float ContestPenaltyPerUnit = 22.0f;
    static constexpr float PoolEmptyPenalty = 120.0f;
    static constexpr float PoolLowPenalty = 35.0f;

    static constexpr float BuyUnitUnitValueWeight = 0.12f;
    static constexpr float BuyUnitDeltaBoardWeight = 0.7f;
    static constexpr float SellUnitDeltaBoardWeight = 0.7f;
    static constexpr float SellUnitUnitValuePenaltyWeight = 0.04f;
    static constexpr float SellUnitGoldBackWeight = 8.0f;
    static constexpr float SellUnitFreeBenchBonus = 60.0f;

    static constexpr float RerollBasePressureWeight = 35.0f;
    static constexpr float RerollUpgradePotentialWeight = 0.08f;
    static constexpr float RerollPenaltyLowEcon = 16.0f;
    static constexpr float RerollPenaltyHighEcon = 9.0f;
    static constexpr float RerollPressurePenaltyScale = 0.5f;
    static constexpr float RerollStabilizeTagThreshold = 0.7f;
    static constexpr float RerollUpgradeHuntTagThreshold = 80.0f;

    static constexpr float BuyXpBoardFullBase = 95.0f;
    static constexpr float BuyXpBoardNotFullBase = 25.0f;
    static constexpr float BuyXpDeltaBoardWeight = 0.7f;
    static constexpr float BuyXpPressureWeight = 45.0f;
    static constexpr float BuyXpGoldPenaltyLowEcon = 14.0f;
    static constexpr float BuyXpGoldPenaltyHighEcon = 8.0f;
    static constexpr float BuyXpBreakInterestPenaltyPerGold = 3.2f;
    static constexpr float BuyXpMaxLevelPenalty = 500.0f;
    static constexpr float BuyXpLevelsNowBonus = 220.0f;
    static constexpr float BuyXpLevelsSoonBonus = 95.0f;
    static constexpr float BuyXpDeployImmediateBonus = 80.0f;
    static constexpr float BuyXpDeploySoonBonus = 35.0f;
    static constexpr float BuyXpTooFarBasePenalty = 85.0f;
    static constexpr float BuyXpTooFarRemainingScale = 40.0f;
    static constexpr float BuyXpTooFarRemainingMax = 4.0f;
    static constexpr float BuyXpTooFarRemainingPenaltyPer = 28.0f;
    static constexpr float BuyXpNotCappedPenalty = 45.0f;
    static constexpr float BuyXpNoDeployPenalty = 70.0f;
    static constexpr float BuyXpBenchDeployableDelta = 35.0f;
    static constexpr float BuyXpUpgradeOverXpThreshold = 140.0f;
    static constexpr float BuyXpUpgradeOverXpPenalty = 55.0f;
    static constexpr float BuyXpUpgradeOverXpPressureMax = 0.6f;
    static constexpr float BuyXpTempoPressureThreshold = 0.8f;
    static constexpr float BuyXpTempoBonus = 55.0f;
    static constexpr int BuyXpLevelsSoonBuys = 2;
    static constexpr int BuyXpMinGoldAfterTempo = 10;

    static constexpr float MoveBenchToBoardUnitValueWeight = 0.08f;
    static constexpr float MoveBenchToBoardTraitWeight = 0.02f;
    static constexpr float MoveBenchToBoardDeltaBoardWeight = 0.9f;
    static constexpr float MoveBoardToBenchUnitValuePenaltyWeight = 0.04f;
    static constexpr float MoveBoardToBenchDeltaBoardWeight = 0.9f;
    static constexpr float MoveBoardToBenchIfSpaceBonus = 10.0f;

    static constexpr float EquipItemItemValueWeight = 0.22f;
    static constexpr float EquipItemUnitValueWeight = 0.01f;
    static constexpr float EquipItemDeltaBoardWeight = 0.7f;

    static constexpr float RepositionBaseNoEnemyCarry = 1.0f;
    static constexpr float RepositionBaseWithEnemyCarry = 4.0f;
    static constexpr float RepositionDeltaWeight = 1.0f;

    static constexpr float EndTurnGreedBonus = 8.0f;
    static constexpr float EndTurnLowPressureThreshold = 0.2f;

    static constexpr float ScoreBoardScale = 0.002f;
    static constexpr float CompConfidenceScale = 25.0f;

    static constexpr float UpgradeBiasDivisor = 300.0f;
    static constexpr float UpgradeBiasBonus = 40.0f;

    static constexpr float BuyUnitEconPenaltyLowEcon = 22.0f;
    static constexpr float BuyUnitEconPenaltyHighEcon = 11.0f;
    static constexpr float BuyUnitPressureEconRelief = 0.35f;

    static constexpr float BoardStrengthEconomyGoldWeight = 0.35f;
    static constexpr float BoardStrengthEconomyHpWeight = 0.15f;
    static constexpr float BoardStrengthEconomyLevelWeight = 4.0f;
    static constexpr float BoardStrengthEconomyStreakWeight = 2.5f;

    static constexpr float BoardStrengthFrontlineHpMin = 1.0f;
    static constexpr float BoardStrengthFrontlineDefenseScale = 0.005f;
    static constexpr float BoardStrengthFrontlineItemDefenseWeight = 0.2f;
    static constexpr int BoardStrengthFrontlineTopUnits = 2;
    static constexpr float BoardStrengthFrontlineTotalScale = 0.02f;

    static constexpr float BoardStrengthCarryItemOffenseWeight = 0.3f;
    static constexpr float BoardStrengthCarryItemCasterWeight = 0.2f;
    static constexpr float BoardStrengthCarryTotalScale = 0.03f;

    static constexpr float BoardStrengthBenchItemDiscount = 0.5f;
    static constexpr float BoardStrengthUnitPowerScale = 0.06f;
    static constexpr float BoardStrengthItemPowerScale = 0.03f;
    static constexpr float BoardStrengthTraitPowerScale = 0.08f;
    static constexpr float BoardStrengthUpgradePotentialScale = 0.05f;

    static constexpr float TraitSynergyBoardUnitValueWeight = 0.9f;
    static constexpr float TraitSynergyBoardCostWeight = 6.0f;
    static constexpr float TraitSynergyBoardStarWeight = 22.0f;
    static constexpr float TraitSynergyBenchUnitValueWeight = 0.45f;
    static constexpr float TraitSynergyBenchCostWeight = 3.0f;
    static constexpr float TraitSynergyBenchStarWeight = 12.0f;
    static constexpr float TraitSynergyShopCostWeight = 1.5f;

    static constexpr float TraitSynergyProfileBoostDivisor = 800.0f;
    static constexpr float TraitSynergyProfileBoostMaxAdd = 0.45f;
    static constexpr float TraitSynergyActiveTierBase = 32.0f;
    static constexpr float TraitSynergyActiveTierPerBreakpoint = 9.0f;
    static constexpr float TraitSynergyOverflowBoostPerUnit = 0.28f;
    static constexpr float TraitSynergyQualityBoostDivisor = 900.0f;
    static constexpr float TraitSynergyQualityBoostMaxAdd = 0.55f;

    static constexpr float TraitSynergyNearNowBase = 26.0f;
    static constexpr float TraitSynergyNearNowPerBreakpoint = 7.0f;
    static constexpr float TraitSynergyNearNowBenchMultiplier = 1.18f;
    static constexpr float TraitSynergyNearNowShopMultiplier = 1.08f;
    static constexpr float TraitSynergyNearSoonBase = 10.0f;
    static constexpr float TraitSynergyNearSoonPerBreakpoint = 3.0f;
    static constexpr float TraitSynergyNearSoonBenchMultiplier = 1.10f;
    static constexpr float TraitSynergyNearSoonShopMultiplier = 1.05f;
    static constexpr float TraitSynergyNearFallbackBonus = 1.5f;
    static constexpr float TraitSynergyTotalNearWeight = 0.75f;

    static constexpr float UpgradeAvailabilityMin = 0.45f;
    static constexpr float UpgradeAvailabilityScale = 0.55f;

    static constexpr float UpgradeNearTwoStarMod2Weight = 25.0f;
    static constexpr float UpgradeNearThreeStarMod2Weight = 70.0f;
    static constexpr float UpgradeNearTwoStarMod1Weight = 9.0f;
    static constexpr float UpgradeNearThreeStarMod1Weight = 22.0f;

    static constexpr float UpgradeShopPairTwoStarMod2Weight = 18.0f;
    static constexpr float UpgradeShopPairThreeStarMod2Weight = 40.0f;
    static constexpr float UpgradeShopPairTwoStarMod1Weight = 6.0f;
    static constexpr float UpgradeShopPairThreeStarMod1Weight = 12.0f;

    static constexpr float UpgradeTotalNearTwoStarWeight = 0.55f;
    static constexpr float UpgradeTotalShopPairsWeight = 0.6f;
    static constexpr float UpgradeTotalGoldBase = 0.7f;
    static constexpr float UpgradeTotalGoldScale = 0.6f;

    static constexpr float SimpleMacroAIStabilizeHealthThreshold = 30.0f;
    static constexpr float SimpleMacroAIEnemyStrengthGapThreshold = 80.0f;
    static constexpr int SimpleMacroAITempoLoseStreakThreshold = 2;

    static constexpr float SimpleMacroAIGreedRerollPenalty = 75.0f;
    static constexpr float SimpleMacroAIGreedBuyXpNotNowPenalty = 55.0f;
    static constexpr float SimpleMacroAIGreedEndTurnBonus = 15.0f;
    static constexpr float SimpleMacroAIStagePressureGoldExcessPenaltyPer = 1.6f;
    static constexpr float SimpleMacroAIStagePressureHighGoldLowLevelEndTurnPenalty = 180.0f;

    static constexpr float FutureEvalUrgencyHpThreshold = 45.0f;
    static constexpr float FutureEvalUrgencyHpFloor = 10.0f;

    static constexpr float FutureEvalMinBoardNormBase = 0.18f;
    static constexpr float FutureEvalMinBoardNormPerStage = 0.08f;
    static constexpr float FutureEvalMinBoardNormPerLevel = 0.02f;
    static constexpr float FutureEvalBoardDeficitPenaltyWeight = 520.0f;
    static constexpr float FutureEvalBoardDeficitPenaltyPow = 1.6f;

    static constexpr float FutureEvalGoldValueBasePerGold = 0.55f;
    static constexpr float FutureEvalGoldValueStageScale = 0.35f;
    static constexpr float FutureEvalGoldValueUrgencyScale = 0.55f;

    static constexpr float FutureEvalGoldHoardPenaltyPerGold = 4.0f;
    static constexpr float FutureEvalHoardCapStageBase = 55.0f;
    static constexpr float FutureEvalHoardCapStagePer = 10.0f;
    static constexpr float FutureEvalHoardCapMin = 10.0f;
    static constexpr float FutureEvalHoardCapUrgencyCut = 25.0f;
    static constexpr float FutureEvalHighGoldLowLevelThreshold = 80.0f;
    static constexpr float FutureEvalHighGoldLowLevelPenalty = 280.0f;

    static constexpr float FutureEvalPlacementFromTop4Base = 7.5f;
    static constexpr float FutureEvalPlacementFromTop4Scale = 4.5f;

    static constexpr float FutureEvalHpWeight = 6.5f;
    static constexpr float FutureEvalLevelWeight = 14.0f;
    static constexpr float FutureEvalUpgradePotentialWeight = 0.7f;
    static constexpr float FutureEvalTop4Weight = 260.0f;

    static constexpr float FutureEvalLevelDeficitPenalty = 220.0f;
    static constexpr float FutureEvalHighGoldLowLevelPenaltyScale = 1.35f;
    static constexpr float FutureEvalCapBlockedBenchPenaltyWeight = 0.45f;
    static constexpr float FutureEvalRelativeBoardDiffWeight = 0.55f;
    static constexpr float FutureEvalRelativeHpDiffWeight = 1.15f;
    static constexpr float FutureEvalWinProbWeight = 220.0f;

    static constexpr float SimpleMacroAITempoBuyXpWhenBoardFullBonus = 35.0f;
    static constexpr float SimpleMacroAITempoRerollPenalty = 15.0f;

    static constexpr float SimpleMacroAIStabilizeRerollBonus = 45.0f;
    static constexpr float SimpleMacroAIStabilizeEndTurnPenalty = 25.0f;

    static constexpr float SimpleMacroAIAllInRerollBonus = 80.0f;
    static constexpr float SimpleMacroAIAllInBuyXpLevelsNowBonus = 55.0f;
    static constexpr float SimpleMacroAIAllInEndTurnPenalty = 80.0f;

    static constexpr float SimpleMacroAIMoveMinScore = 15.0f;
    static constexpr float SimpleMacroAISellMinScore = 25.0f;
    static constexpr float SimpleMacroAINonDurableLowPositiveThreshold = 18.0f;
    static constexpr float SimpleMacroAIInitialBestScore = -1e30f;

    static constexpr float StageLevelDeficitEndTurnPenalty = 85.0f;
    static constexpr float StageLevelDeficitHighGoldPenalty = 2.6f;
    static constexpr float StageLevelDeficitBuyXpBonus = 65.0f;
    static constexpr float StageLevelDeficitBuyXpHighGoldBonus = 35.0f;
    static constexpr float StageLevelDeficitBuyXpBenchCapBonus = 45.0f;
    static constexpr float StageLevelDeficitCapPressurePenalty = 0.35f;
    static constexpr float StageLevelDeficitBuyXpFarBonusPerBuy = 32.0f;

    static constexpr float PositioningMinAttackSpeed = 0.1f;
    static constexpr float PositioningRangeBonus = 12.0f;
    static constexpr float PositioningAdAsWeight = 2.0f;
    static constexpr float PositioningApWeight = 10.0f;
    static constexpr float PositioningCritWeight = 60.0f;

    static constexpr float PositioningTankDefenseScale = 0.006f;
    static constexpr float PositioningCarryItemOffenseWeight = 0.6f;
    static constexpr float PositioningCarryItemCasterWeight = 0.55f;
    static constexpr float PositioningTankItemDefenseWeight = 0.9f;
    static constexpr float PositioningOverallCarryBonusWeight = 0.08f;
    static constexpr float PositioningOverallTankBonusWeight = 0.02f;
    static constexpr float PositioningOverallTotalWeight = 0.01f;

    static constexpr float PositioningCarrySelectionOffenseWeight = 0.2f;
    static constexpr float PositioningCarrySelectionCasterWeight = 0.2f;

    static constexpr int PositioningEnemyMirrorYBase = 5;
    static constexpr float PositioningJumpThreatMultiplier = 1.25f;
    static constexpr float PositioningAoeThreatDivisor = 500.0f;
    static constexpr float PositioningSpreadPenaltyBase = 10.0f;
    static constexpr float PositioningSpreadPenaltyScale = 18.0f;
    static constexpr float PositioningSpreadPenaltyFrontRowMultiplier = 0.3f;

    static constexpr float PositioningCarryBackRowBonus = 160.0f;
    static constexpr float PositioningCarryMidRowBonus = 40.0f;
    static constexpr float PositioningCarryFrontRowPenalty = -200.0f;
    static constexpr float PositioningCarryHeatPenaltyWeight = 0.28f;
    static constexpr float PositioningCarryEdgeBonusScale = 2.0f;
    static constexpr float PositioningCarryAwayFromEnemyBase = 20.0f;
    static constexpr float PositioningCarryAwayFromEnemyDistancePenalty = 5.0f;

    static constexpr float PositioningTankFrontRowBonus = 120.0f;
    static constexpr float PositioningTankMidRowBonus = 20.0f;
    static constexpr float PositioningTankBackRowPenalty = -80.0f;
    static constexpr float PositioningTankHeatPenaltyWeight = 0.08f;
    static constexpr float PositioningTankVsEnemyCarryBase = 12.0f;
    static constexpr float PositioningTankVsEnemyCarryDistancePenalty = 2.0f;

    static constexpr float PositioningRangedBackRowBonus = 70.0f;
    static constexpr float PositioningRangedMidRowBonus = 25.0f;
    static constexpr float PositioningRangedFrontRowPenalty = -90.0f;
    static constexpr float PositioningRangedHeatPenaltyWeight = 0.14f;

    static constexpr float PositioningMeleeFrontRowBonus = 45.0f;
    static constexpr float PositioningMeleeMidRowBonus = 20.0f;
    static constexpr float PositioningMeleeBackRowPenalty = -50.0f;
    static constexpr float PositioningMeleeHeatPenaltyWeight = 0.10f;

    static constexpr float PositioningCarryThreatenedHeatThreshold = 160.0f;
    static constexpr float PositioningCarryThreatenedAoeFactorThreshold = 0.5f;
    static constexpr float PositioningCarryThreatenedHeatThresholdLow = 110.0f;

    static constexpr float PositioningBodyguardAdjacencyTankBonusD1 = 55.0f;
    static constexpr float PositioningBodyguardAdjacencyCarryBonusD1 = 25.0f;
    static constexpr float PositioningBodyguardAdjacencyTankBonusD2 = 18.0f;
    static constexpr float PositioningBodyguardAdjacencyCarryBonusD2 = 6.0f;

    static constexpr float PositioningVeryNegativeScore = -1e9f;
    static constexpr float PositioningVeryNegativeScoreEarlyExit = -1e8f;

    static constexpr float AIPlayerBoardScoreFrontlineWeight = 0.9f;
    static constexpr float AIPlayerBoardScoreDpsWeight = 1.1f;
    static constexpr float AIPlayerBoardScoreSurvivabilityWeight = 1.0f;
    static constexpr float AIPlayerBoardScoreTraitSynergyWeight = 0.8f;
    static constexpr float AIPlayerBoardScoreCarryPotentialWeight = 0.7f;

    static constexpr float AIPlayerSentinelScore = -1.0f;
    static constexpr float AIPlayerHalf = 0.5f;
    static constexpr float AIPlayerDurabilityMinDenominator = 0.05f;

    static constexpr int AIPlayerRangeClampMax = 5;
    static constexpr float AIPlayerRangeBonusPerTile = 15.0f;
    static constexpr float AIPlayerCarryDpsWeight = 1.3f;
    static constexpr float AIPlayerCarryApWeight = 10.0f;

    static constexpr float AIPlayerItemCarryWeightAttackDamage = 25.0f;
    static constexpr float AIPlayerItemCarryWeightAttackSpeed = 20.0f;
    static constexpr float AIPlayerItemCarryWeightCritChance = 30.0f;
    static constexpr float AIPlayerItemCarryWeightCritDamage = 15.0f;
    static constexpr float AIPlayerItemCarryWeightAbilityPower = 20.0f;

    static constexpr float AIPlayerItemTankWeightArmor = 18.0f;
    static constexpr float AIPlayerItemTankWeightMagicResist = 18.0f;
    static constexpr float AIPlayerItemTankWeightAttackSpeed = 2.0f;
    static constexpr float AIPlayerItemTankWeightMaxHp = 0.2f;
    static constexpr float AIPlayerItemTankWeightDamageReduction = 120.0f;

    static constexpr float AIPlayerFrontlineFromTankWeight = 0.55f;

    static constexpr float AIPlayerTraitProgressWeight = 40.0f;
    static constexpr float AIPlayerTraitReachedBonus = 15.0f;

    static constexpr float AIPlayerShopCostWeight = 20.0f;
    static constexpr float AIPlayerShopCarryWeight = 0.1f;
    static constexpr float AIPlayerShopTankWeight = 0.02f;
    static constexpr float AIPlayerShopTraitBonusBase = 10.0f;
    static constexpr float AIPlayerShopTraitBonusPerCount = 5.0f;
    static constexpr float AIPlayerShopPrimaryTraitBonus = 25.0f;

    static constexpr int AIPlayerRerollHealthThreshold = 35;
    static constexpr float AIPlayerRerollBehindScoreGap = 30.0f;
    static constexpr int AIPlayerRerollMinGoldWhenBehind = 10;

    static constexpr float AIPlayerLevelUnderPressureScoreGap = 10.0f;
    static constexpr int AIPlayerLevelMinExtraGoldUnderPressure = 4;
    static constexpr int AIPlayerLevelRoundThreshold = 3;
    static constexpr int AIPlayerLevelMinExtraGoldAfterThreshold = 8;

    static constexpr int AIPlayerPredictCombatMinSimulations = 1;
    static constexpr int AIPlayerPredictCombatMaxSimulations = 25;
    static constexpr int AIPlayerPredictCombatDefaultSimulations = 7;

    static constexpr float AIPlayerTempoWinChanceThreshold = 0.45f;
    static constexpr float AIPlayerRollDownWinChanceThreshold = 0.35f;

    static constexpr int AIPlayerTempoLevelMinGold = 8;
    static constexpr int AIPlayerRollDownMaxRerolls = 4;
    static constexpr int AIPlayerStabilizeMaxRerolls = 3;
};
