# Step 12F Trait Fidelity Mapping Plan

# Scope

Step 12F started as planning only. Step 12F-1 implemented safe stat-like trait variable mapping in importer normalization and content-fidelity reporting only. Step 12F-2 expanded the safe stat-like classifier and added top-unmapped-variable reporting. Runtime behavior, trait execution, combat behavior, and checked-in normalized live data were not changed.

Step 12F-3 added a cached trait-only import command and regenerated only `data/traits/*.json` from `data/_import_cache/cdragon_tft_en_us.json`. Champions, items, and abilities were not regenerated. `TraitEffectExecutor` and runtime effect execution were not changed.

Step 12F-4 fixed trait source identity collisions during trait import only. Same-display-name raw trait variants are now preserved as separate normalized trait files and loaded trait definitions when their `sourceId`/`apiName` differs. Combat runtime behavior and `TraitEffectExecutor` were not changed.

Goal: map imported TFT trait variables and preserved metadata into existing generic trait effects:

- `ApplyStatusToAllies`
- `ApplyStatusToEnemies`
- `Shield`
- `Heal`
- `DealDamage`
- `ApplyStatusToTraitUnits`
- `ApplyStatusToEnemyTeam`
- `StackStatusOnAttack`
- `ExecuteBelowHpPercent`
- `TempCritBonusVsLowHp`

Runtime must remain data-driven. Runtime code must not branch on champion, item, trait, or ability names.

# Files Reviewed

- `data/traits/*.json`
- `data/_import_cache/cdragon_tft_en_us.json`
- `simulator/include/content/Trait.hpp`
- `simulator/include/content/TraitData.hpp`
- `simulator/src/combat/TraitEffectExecutor.cpp`
- `simulator/src/content/ContentManager.cpp`
- `simulator/src/import/TFTDataImporter.cpp`
- `docs/STEP12_CONTENT_AUDIT.md`
- `docs/STEP12E_EFFECT_VOCAB_PLAN.md`

# Current Data State

Checked-in normalized trait JSON currently has 44 trait files after Step 12F-4.

Existing executable normalized traits:

- `Bastion`
- `Bulwark`
- `Challenger`
- `Commander`
- `Dark Lady`
- `Divine Duelist`
- `Fateweaver`
- `Gun Goddess`
- `Marauder`
- `Mecha`
- `Meeple`
- `N.O.V.A.`
- `Rogue`
- `Timebreaker`
- `Vanguard`

Existing normalized executable effects are still almost entirely `OnCombatStart` plus `ApplyStatusToTraitUnits` stat statuses. The checked-in trait JSON files are legacy-shaped and mostly do not contain the Step 12C metadata fields yet. The raw CommunityDragon cache still contains trait descriptions and variables and should be the planning source for importer mapping.

Raw cached trait observations:

- Source cache includes multiple `Stargazer` variants under different `apiName` values. Step 12F-4 preserves those variants as unique normalized files while keeping the original display name in metadata.
- Several source variables are clean stat variables and can map directly.
- Several source variables describe target roles, empowered hexes, delayed triggers, macro rewards, summons, or player-combat state and must not be forced into current effects.

Step 12F-2 validation output from the current raw cache:

```text
Trait stat mapping | current_executable_before_import=15 projected_executable_after_import=16 raw_trait_records=44 mapped_variables=69 unmapped_variables=583
Trait stat mapping top unmapped | Mountain_RoundsPerEmblem=15 Mountain_Health=14 Mountain_ADAP=13 Mountain_AS=13 Mountain_DR=13 Mountain_Resists=13 Mountain_StatIncrease=13 ShieldDuration=11 HealthThreshold=10 Wolf_Health_Teamwide=10
```

Previous Step 12F-1 projection:

```text
projected_executable_after_import=4 mapped_variables=21 unmapped_variables=631
```

This projection uses the current importer set-selection path and the Step 12F safe variable list. It reports future normalization coverage from raw cache data; it does not mean checked-in normalized trait JSON was regenerated.

Step 12F-3 regenerated trait output before the identity fix:

```text
Traits Imported: 44
JSON trait files loaded: 37
Traits | executable_effects=16 empty_or_no_executable_effects=21
Placeholder flags | traits=21
Trait stat mapping | current_executable_before_import=16 projected_executable_after_import=16 raw_trait_records=44 mapped_variables=69 unmapped_variables=583
```

Before Step 12F-3 regeneration:

```text
Traits | executable_effects=15 empty_or_no_executable_effects=22
Placeholder flags | traits=37
```

Step 12F-4 regenerated trait output after the identity fix:

```text
Traits Imported: 44
JSON trait files loaded: 44
Loaded Traits: 44
Traits | executable_effects=16 empty_or_no_executable_effects=28
Placeholder flags | traits=28
Trait stat mapping | current_executable_before_import=16 projected_executable_after_import=16 raw_trait_records=44 mapped_variables=69 unmapped_variables=583
```

Collision before/after:

```text
Before Step 12F-4: raw_trait_records=44 normalized_trait_files=37 loaded_trait_definitions=37
After Step 12F-4:  raw_trait_records=44 normalized_trait_files=44 loaded_trait_definitions=44
```

The only raw display-name collision found was `Stargazer`. The preserved variants are:

- `Stargazer` / `TFT17_Stargazer` -> `data/traits/stargazer.json`
- `Stargazer` / `TFT17_Stargazer_Wolf` -> `data/traits/tft17_stargazer_wolf.json`
- `Stargazer` / `TFT17_Stargazer_Medallion` -> `data/traits/tft17_stargazer_medallion.json`
- `Stargazer` / `TFT17_Stargazer_Huntress` -> `data/traits/tft17_stargazer_huntress.json`
- `Stargazer` / `TFT17_Stargazer_Serpent` -> `data/traits/tft17_stargazer_serpent.json`
- `Stargazer` / `TFT17_Stargazer_Shield` -> `data/traits/tft17_stargazer_shield.json`
- `Stargazer` / `TFT17_Stargazer_Fountain` -> `data/traits/tft17_stargazer_fountain.json`
- `Stargazer` / `TFT17_Stargazer_Mountain` -> `data/traits/tft17_stargazer_mountain.json`

# Safe Mappings

Safe now means the mapping can be generated from generic source variable names and metadata patterns, using existing effect types, with deterministic execution and no runtime name checks.

## Stat Buffs To Trait Units

Use `ApplyStatusToTraitUnits` when the source text says the trait units gain the stat, or when variables are trait-unit-specific.

Safe variable patterns:

- `AttackSpeedPercent`
- `AD`
- `AP`
- `BonusAD`
- `BonusHealth`
- `HealthBonus`
- `Omnivamp`
- `CritChance`
- `CritDamage`
- `Durability`
- `DamageReductionPct`
- `EnhancedDurability`
- `PercentDamageIncrease`

Step 12F-1 implemented exactly these allowed stat-like variables. Broader stat variables such as armor, MR, mana, shields, executes, teamwide-only variables, role-targeted variables, and conditional variables remain unmapped unless covered by this exact list.

Step 12F-2 added these additional safe stat-like variables:

- `BonusArmor`
- `Armor`
- `BonusMR`
- `MR`
- `MagicResist`
- `TeamwideBonus`
- `TeamwideAS`
- `TeamwideResists`
- `BonusOffensiveStat`
- `BonusOffensiveStats`
- `AttackSpeed`
- `PctResists`
- `MaxHealth`
- `MaxHp`
- `HP`
- `Health`
- `DamageAmp`
- `DamageIncrease`
- `BonusDamage`
- `DamageReduction`
- `Resistances`

Variables that imply multiple stats generate multiple existing status effects where obvious, such as `TeamwideResists`/`Resistances` to Armor plus MagicResist and `BonusOffensiveStat(s)` to AttackDamage plus AbilityPower.

Candidate traits:

- `Challenger`: `AttackSpeedPercent`
- `Marauder`: `Omnivamp`, `AD`
- `Rogue`: `AP`, `AD`
- `Fateweaver`: `CritChance`, `CritDamage`
- `Mecha`: `AP`, `AD`
- `Meeple`: `BonusHealth`
- `Sniper`: `PercentDamageIncrease`
- `Vanguard`: `DamageReductionPct`, `EnhancedDurability` only as an approximation until shield-conditional durability exists

## Teamwide Ally Buffs

Use `ApplyStatusToAllies` when the source variable explicitly uses `Teamwide`, or source text says "your team" or "allies gain" and no positional/role condition is attached.

Safe variable patterns:

- `TeamwideBonus`
- `TeamwideAS`
- `TeamwideResists`
- `TeamManaRegen`
- `AttackSpeed`
- `BonusDefensiveStat*`
- `BonusOffensiveStat*`
- `Durability`

Step 12F-1 only uses `ApplyStatusToAllies` when a mapped allowed variable is accompanied by generic team/allies wording and the description does not contain blocked positional/conditional wording such as empowered hexes, adjacent targeting, tanks, strongest targets, chosen modes, marked targets, or event triggers.

Candidate traits:

- `Brawler`: `TeamwideBonus` to all allies as max HP percent; `HealthBonus` to trait units.
- `Bastion`: `TeamwideResists` to all allies as armor and MR; `BonusArmor` and `BonusMR` to trait units.
- `Challenger`: `TeamwideAS` to all allies; `AttackSpeedPercent` to trait units.
- `Marauder`: `TeamwideBonus` to all allies as omnivamp; `Omnivamp` and `AD` to trait units.
- `Timebreaker`: `AttackSpeed` to all allies; `TimebreakerAdditionalAS` to trait units.
- `Dark Lady`: `Durability` to all allies.
- `Redeemer`: `BonusOffensiveStat*` and `BonusDefensiveStat*` are conceptually teamwide, but the live trait scales by active non-unique trait count and therefore needs `ConditionalEffect` or a computed-scaling effect before it is exact.

## Enemy Debuffs

Use `ApplyStatusToEnemies` or existing `ApplyStatusToEnemyTeam` for all-alive-enemy debuffs.

Safe variable patterns:

- `PctResists`
- `ShredAndSunder`

Candidate traits:

- `Eradicator`: `PctResists` can become enemy armor/MR reduction if the current status system supports negative armor/MR modifiers cleanly.
- `N.O.V.A.`: `ShredAndSunder` is blocked by delayed/champion-mode selection, but the debuff shape itself can use `ApplyStatusToEnemies` later.

## Shields

Use `Shield` or existing `ShieldOnCombatStart` for flat shield amounts when target set is all allies or trait units.

Safe variable patterns:

- `ShieldHP`
- `ShieldValue`

Candidate traits:

- `Voyager`: `ShieldHP` is a clean flat shield value, but source targets Tanks while non-Tanks receive damage amp. Current effect vocabulary cannot target roles, so exact mapping is blocked. A temporary all-ally shield would not be correct.
- `N.O.V.A.`: `ShieldValue` is blocked by delayed champion-mode selection and "strongest Tank" targeting.

## Healing

Use `Heal` for fixed flat heals to all alive allies.

Safe variable patterns:

- None of the current raw trait variables clearly provide flat all-ally healing.

Blocked healing patterns:

- `Heal` and `PercentHealthHeal` are percent max HP and/or delayed/conditional.
- `Huntress_Heal` is on marked enemy death and empowered-hex gated.
- `Fountain_HealthRegen*` is periodic percent max HP and empowered-hex gated.

## Damage

Use `DealDamage` for fixed direct damage to all alive enemies.

Safe variable patterns:

- None of the current raw trait variables are a clean unconditional all-enemy fixed damage amount.

Blocked damage patterns:

- `FlatDamage` on `Gun Goddess` is tied to a unique mode/attack cadence.
- `BonusTrueDamage` on `N.O.V.A.` is stacking bonus true damage after a delayed surge.
- `Serpent_Poison` is repeat-damage-over-time based on damage dealt.
- `ExecuteHPPercent` should use `ExecuteBelowHpPercent`, not generic damage.

# Blocked Mappings

## Needs ConditionalEffect

Traits with effects gated by HP thresholds, current shield state, star level, active trait count, chosen mode, unit role, strongest unit, target distance, or win/loss state:

- `Vanguard`: durability while shielded, shield again at health threshold.
- `Rogue`: retarget/untargetable behavior below health threshold.
- `Party Animal`: once per combat low-health untargetable repair.
- `Redeemer`: scales by number of active non-unique traits.
- `Sniper`: damage amp increases per hex distance.
- `Galaxy Hunter`: bonus AD while a clone is alive.
- `N.O.V.A.`: delayed surge depends on selected/member-specific mode.
- `Arbiter`: chosen cause/effect law system.
- `Fateweaver`: Lucky behavior for chance effects cannot be represented by stat-only mapping.

## Needs Aura Or Position Targeting

Traits involving adjacent allies, empowered hexes, or role/position-targeted units:

- `Bulwark`: relic grants adjacent allies shield and attack speed.
- `Stargazer` variants: empowered hexes, constellation variants, marked enemies, hex-specific bonuses.
- `Voyager`: Tanks receive shields, other allies receive damage amp, Voyagers receive double.
- `N.O.V.A.`: strongest Tank shield and all-ally delayed effects.

## Needs Summon Or Advanced Mechanics

Traits involving spawned units, clones, transformations, generated slots, projectiles, or pets:

- `Shepherd`: summons Bia/Bayin.
- `Primordian`: spawns Swarmlings from damage.
- `Mecha`: transformation, upgraded ability, double team-slot counting.
- `Meeple`: meeps, cloning slot, high-tier summons.
- `Dark Star`: black holes and supermassive state.
- `Space Groove`: enters Groove state with stacking timed bonuses.

## Macro-Only Or Economy Systems

Traits that affect rewards, player combat state, shop/economy, or post-combat choices:

- `Anima`: Tech research and loot.
- `Factory New`: armory upgrades.
- `Oracle`: periodic reward.
- `Divine Duelist`: tactician heal from player damage/wins.
- `Timebreaker`: free rerolls on loss and stored XP on win.
- `Stargazer` variants: gold, emblems, player-level hex reveal.
- `Choose Trait`, `Gun Goddess`: choice/mode systems.

## Cannot Map From Current Data

Traits whose normalized JSON currently has no metadata and whose raw cache variables are empty or too opaque for safe generic mapping:

- `Choose Trait`
- `God-Blessed`
- `Shepherd` combat details beyond summon labels
- Several hash-only `Stargazer` variables without stable semantic names

# Required New Effect Types

Needed before blocked mappings can become faithful:

- `ConditionalEffect`
- `CooldownGate`
- `OncePerCombatGate`
- `ApplyStatusInRadius`
- `ApplyStatusToRole`
- `ApplyStatusToStrongestAlly`
- `ApplyStatusToMarkedUnits`
- `DealPercentHealthDamage`
- `DealDamageOverTime`
- `HealPercentMaxHp`
- `PeriodicHeal`
- `ModifyMana`
- `Aura`
- `SummonUnit`
- `TransformUnit`
- `RetargetOrUntargetable`
- `GrantTrait`
- `MacroReward`
- `PostCombatCounter`
- `ChoiceMode`

# First Implementation Batch

The first batch should extend importer normalization only, not runtime execution. It should generate existing generic trait effects from generic variable names and source metadata patterns. No runtime trait-name logic should be added.

## 1. Brawler

Source variables:

- `TeamwideBonus`
- `HealthBonus`

Planned generated effects:

- `TeamwideBonus`: `OnCombatStart` + `ApplyStatusToAllies` + `StatusEffect(MaxHp, Percent, TeamwideBonus)`.
- `HealthBonus`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(MaxHp, Percent, HealthBonus)`.

Expected breakpoint scaling:

- Breakpoints `2,4,6`.
- `TeamwideBonus` constant `0.05`.
- `HealthBonus` scales approximately `0.25, 0.45, 0.65`.

Validation needed:

- Synthetic trait fixture proves all alive allies receive the teamwide max HP bonus.
- Synthetic trait fixture proves only trait units receive the larger trait-unit max HP bonus.
- Dead units are skipped.
- Highest active breakpoint is selected deterministically.

## 2. Challenger

Source variables:

- `TeamwideAS`
- `AttackSpeedPercent`

Planned generated effects:

- `TeamwideAS`: `OnCombatStart` + `ApplyStatusToAllies` + `StatusEffect(AttackSpeed, Percent, TeamwideAS)`.
- `AttackSpeedPercent`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(AttackSpeed, Percent, AttackSpeedPercent)`.

Expected breakpoint scaling:

- Breakpoints `2,3,4,5`.
- `TeamwideAS` constant `0.10`.
- `AttackSpeedPercent` scales approximately `0.15, 0.28, 0.42, 0.55`.

Validation needed:

- Allies without the trait receive only the teamwide attack-speed status.
- Trait units receive both statuses.
- Burst-on-kill variables remain unmapped with import warning.

## 3. Marauder

Source variables:

- `TeamwideBonus`
- `Omnivamp`
- `AD`

Planned generated effects:

- `TeamwideBonus`: `OnCombatStart` + `ApplyStatusToAllies` + `StatusEffect(Omnivamp, Flat, TeamwideBonus)`.
- `Omnivamp`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(Omnivamp, Flat, Omnivamp)`.
- `AD`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(AttackDamage, Percent, AD)`.

Expected breakpoint scaling:

- Breakpoints `2,4,6`.
- `TeamwideBonus` constant `0.05`.
- `Omnivamp` scales approximately `0.05, 0.07, 0.10`.
- `AD` scales approximately `0.18, 0.35, 0.55`.

Validation needed:

- Teamwide omnivamp applies to all allies.
- Trait-unit omnivamp and AD apply only to trait units.
- Overheal-to-shield variables remain unmapped with import warning.

## 4. Bastion

Source variables:

- `TeamwideResists`
- `BonusArmor`
- `BonusMR`
- `EnhancedTeamwideArmor`

Planned generated effects:

- `TeamwideResists`: `OnCombatStart` + `ApplyStatusToAllies` armor and MR flat statuses.
- `BonusArmor`: `OnCombatStart` + `ApplyStatusToTraitUnits` armor flat status.
- `BonusMR`: `OnCombatStart` + `ApplyStatusToTraitUnits` MR flat status.
- `EnhancedTeamwideArmor`: only map if source breakpoint indicates the top tier grants extra non-Bastion/teamwide resists and if overapplication to trait units can be avoided. Current vocabulary cannot exclude trait units, so keep this warning-only for the first batch.

Expected breakpoint scaling:

- Breakpoints `2,4,6`.
- `TeamwideResists` constant `15`.
- `BonusArmor`/`BonusMR` scale approximately `16, 40, 60`.

Validation needed:

- All allies receive teamwide armor and MR.
- Trait units receive trait-specific armor and MR.
- First-10-seconds doubling remains unmapped with import warning.

## 5. Rogue

Source variables:

- `AP`
- `AD`

Planned generated effects:

- `AP`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(AbilityPower, Flat, AP)`.
- `AD`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(AttackDamage, Percent, AD)`.

Expected breakpoint scaling:

- Breakpoints `2,3,4,5`.
- `AP` scales approximately `12, 25, 40, 55`.
- `AD` scales approximately `0.12, 0.25, 0.40, 0.55`.

Validation needed:

- Trait units receive AP and AD scaling.
- Allies without the trait do not receive Rogue stats.
- Low-health retarget behavior remains unmapped with import warning.

## 6. Fateweaver

Source variables:

- `CritChance`
- `CritDamage`

Planned generated effects:

- `CritChance`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(CritChance, Flat, CritChance)`.
- `CritDamage`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(CritDamage, Flat, CritDamage / 100 if source value is percent-points)`.

Expected breakpoint scaling:

- Breakpoints `2,4`.
- Only breakpoint `4` has direct stat values in current source.
- `CritChance` approximately `0.20`.
- `CritDamage` approximately `0.20` after percent-point conversion.

Validation needed:

- Breakpoint 2 does not generate empty executable effects unless a real source variable exists.
- Breakpoint 4 applies crit chance and crit damage.
- Lucky/precision mechanics remain unmapped with import warning.

## 7. Timebreaker

Source variables:

- `AttackSpeed`
- `TimebreakerAdditionalAS`

Planned generated effects:

- `AttackSpeed`: `OnCombatStart` + `ApplyStatusToAllies` + `StatusEffect(AttackSpeed, Percent, AttackSpeed)`.
- `TimebreakerAdditionalAS`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(AttackSpeed, Percent, TimebreakerAdditionalAS)`.

Expected breakpoint scaling:

- Breakpoints `2,3,4`.
- `AttackSpeed` approximately `0.15`.
- `TimebreakerAdditionalAS` applies at breakpoint `4`, approximately `0.50`.

Validation needed:

- Allies receive teamwide attack speed at active breakpoints.
- Trait units receive additional attack speed only at the breakpoint where source value exists.
- Reroll/XP macro effects remain unmapped with import warning.

## 8. Eradicator

Source variables:

- `PctResists`

Planned generated effects:

- `PctResists`: `OnCombatStart` + `ApplyStatusToEnemies` + armor and MR debuff statuses.

Expected breakpoint scaling:

- Breakpoint `1`.
- Enemy armor and MR reduction approximately `0.10` as percent or equivalent flat/percent modifier, depending on current stat modifier semantics.

Validation needed:

- All alive enemies receive armor and MR debuffs.
- Allies are unaffected.
- Dead enemies are skipped.
- Confirm negative percent/flat modifier semantics before enabling in imported data.

## 9. Dark Lady

Source variables:

- `Durability`

Planned generated effects:

- `Durability`: `OnCombatStart` + `ApplyStatusToAllies` + `StatusEffect(DamageReduction, Flat, Durability)`.

Expected breakpoint scaling:

- Breakpoint `1`.
- `Durability` approximately `0.04`.

Validation needed:

- All alive allies receive damage reduction.
- Transformed bonus remains unmapped with import warning because current runtime cannot detect the alternate form.

## 10. Sniper

Source variables:

- `PercentDamageIncrease`

Planned generated effects:

- `PercentDamageIncrease`: `OnCombatStart` + `ApplyStatusToTraitUnits` + `StatusEffect(DamageAmplification, Flat or Percent, normalized value)`.

Expected breakpoint scaling:

- Breakpoints `2,3,4`.
- Base damage amp approximately `18,25,35` percent-points, normalized to `0.18,0.25,0.35` if runtime expects fractional flat value.

Validation needed:

- Trait units receive baseline damage amp.
- Per-hex scaling remains unmapped with import warning.

# Validation Plan

Step 12F-1 added content-fidelity projection metrics before enabling broad importer mapping:

- Current normalized traits with executable effects.
- Projected raw-cache trait records with safe stat mappings after future import.
- Mapped safe stat variable count.
- Unmapped variable count.
- Top unmapped raw trait variable names by frequency.

Future Step 12F validations before enabling broader importer mapping:

1. Variable classifier tests:
   - `Teamwide*` variables produce `ApplyStatusToAllies`.
   - Trait-unit variables produce `ApplyStatusToTraitUnits`.
   - Enemy debuff variables produce `ApplyStatusToEnemies`.
   - Unknown variables are preserved and warned, not dropped.

2. Generated JSON load tests:
   - Generated trait definitions parse through `ContentManager`.
   - Every generated effect has a valid hook, type, status/effect payload, and duration.

3. Synthetic trait execution tests:
   - Brawler teamwide plus trait-unit max HP.
   - Challenger teamwide plus trait-unit attack speed.
   - Marauder teamwide omnivamp plus trait-unit AD.
   - Eradicator enemy armor/MR debuff.
   - A mixed breakpoint test proves highest active tier selection.

4. Regression tests:
   - Existing Step 12E synthetic effect tests still pass.
   - Deterministic replay remains identical across same-seed runs.
   - Content fidelity validation reports higher executable trait coverage without changing pass/fail criteria yet.

5. Import warning tests:
   - Burst, per-hex, shield-conditional, summon, macro, and choice variables remain in `rawVariables`.
   - These unsupported variables emit clear `importWarnings`.

# Determinism Risks

- Mapping variables in source-object iteration order can make generated effect order unstable. Sort generated effects by breakpoint, hook, effect type, target scope, stat, and source variable name.
- Multiple effects on the same stat can stack differently if order-dependent status replacement is introduced later. Current add-only status behavior is stable but validations should check final stat, not vector order alone.
- Percent-point normalization must be deterministic and explicit. Values like `18` damage amp should not sometimes mean `18.0` and sometimes `0.18`.
- Enemy debuffs using negative modifiers must be validated against `StatSystem` before imported traits depend on them.
- Teamwide plus trait-unit buffs can double-apply to trait units by design. The mapping rules must only do this where source text clearly says teamwide plus trait-unit bonus.
- Source identity collisions are fixed for same-display-name variants by preserving `displayName` separately from runtime `name`/`sourceId`. Future work still needs explicit activation semantics for variant traits whose champion data references only the display trait name.
- Delayed, periodic, low-health, and kill-triggered mappings should remain blocked until event ordering is validated.
- Avoid interpreting hash-only variables as mechanics unless paired with stable metadata text and validation.

# Recommended Step 12F Implementation Order

1. Add importer-side variable classification helpers for trait mappings only. Completed in Step 12F-1 and expanded in Step 12F-2.
2. Add deterministic generated-effect ordering. Completed in Step 12F-1 for generated trait stat effects and preserved in Step 12F-2.
3. Generate stat/status mappings for Brawler, Challenger, Marauder, Bastion, Rogue, Fateweaver, Timebreaker, Dark Lady, Sniper, and optionally Eradicator if negative debuffs validate cleanly. Partially enabled by classifier; current projection reports 16 raw trait records with safe stat mappings.
4. Preserve all unsupported variables with specific warnings.
5. Add synthetic generated-trait validations.
6. Run build and `.\engine.exe --validate`.
7. Update content fidelity metrics and this plan with actual represented-trait counts.

# Step 12F-1 Implementation Notes

- Changed only `TFTDataImporter` trait normalization and content-fidelity validation/reporting.
- Added a trait-only classifier separate from the existing broad item stat mapper.
- Generated `OnCombatStart` trait effects using `ApplyStatusToTraitUnits` by default.
- Generated `ApplyStatusToAllies` only for allowed variables when generic metadata clearly says team/allies and does not include blocked targeting or event language.
- Preserved unmapped, non-numeric, zero, and unsupported variables in `rawVariables` plus `importWarnings`.
- Did not change `TraitEffectExecutor`.
- Did not change live normalized data.

# Step 12F-2 Implementation Notes

- Expanded only importer-side safe stat variable classification.
- Added compact lowercase variable matching so raw names like `HealthBonus`, `TeamwideAS`, and `BonusMR` are not missed because of camel-case style.
- Added mappings for armor, MR, resist pairs, teamwide attack speed, inferred `TeamwideBonus`, offensive stat pairs, health aliases, damage amplification aliases, and resistance debuffs.
- Added content-fidelity reporting for top unmapped raw trait variables by frequency.
- Kept blocked variables such as `HealthThreshold`, `ShieldDuration`, `Mountain_*`, `Wolf_*`, and other conditional/positional/variant variables unmapped.
- Did not change `TraitEffectExecutor`.
- Did not change live normalized data.

# Step 12F-3 Implementation Notes

- Added `--import-cached-traits`.
- The command reads `data/_import_cache/cdragon_tft_en_us.json`.
- The command deletes and rewrites only `data/traits/*.json`.
- The command does not download live data.
- The command does not write champions, items, abilities, or scenarios.
- Mapped traits now write `isPlaceholder: false`; unmapped traits remain `isPlaceholder: true`.
- Regenerated traits preserve `description`, `tooltip`, `sourceId`, `rawVariables`, and `importWarnings`.
- Validation remains PASS.

# Step 12F-4 Implementation Notes

- Fixed the trait import filename policy so display-name collisions use unique source identities.
- Preserved the original CommunityDragon display name in `displayName`.
- Preserved `sourceId` as the raw `apiName`.
- Kept the base non-variant trait identity as the display name when the source id is the base trait, so existing champion trait references still resolve.
- Updated `ContentManager` metadata loading for `displayName`.
- Regenerated only `data/traits/*.json` with `.\engine.exe --import-cached-traits`.
- Did not change `TraitEffectExecutor`, combat runtime behavior, importer mappings, or live champion/item/ability data.
- Validation remains PASS.

# Step 12F-5 Stargazer Variant Activation Plan

## Champion Reference Findings

Normalized champion JSON stores trait references as display names only. No normalized champion trait reference currently uses a `TFT17_*` source id.

Current normalized Stargazer champions:

- `Jax` -> `Stargazer`
- `Lulu` -> `Stargazer`
- `Nunu & Willump` -> `Stargazer`
- `Talon` -> `Stargazer`
- `Twisted Fate` -> `Stargazer`
- `Xayah` -> `Stargazer`

The raw CommunityDragon cache for Set 17 also stores champion trait references as display names only. The same six champions reference `Stargazer`; none reference `TFT17_Stargazer_Wolf`, `TFT17_Stargazer_Medallion`, `TFT17_Stargazer_Huntress`, `TFT17_Stargazer_Serpent`, `TFT17_Stargazer_Shield`, `TFT17_Stargazer_Fountain`, or `TFT17_Stargazer_Mountain`.

Implication: source-id-specific trait definitions cannot become active by champion membership alone without an additional variant-selection layer. Existing champion trait matching must keep resolving the display trait `Stargazer`.

## Raw Data Findings

Raw CommunityDragon contains one base display trait plus seven source-id-specific variant definitions, all with display name `Stargazer`.

- `TFT17_Stargazer`: base trait, breakpoints `3,5,7,8,9,10`; description says Stargazers chart a different constellation every game and gain various bonuses in empowered hexes.
- `TFT17_Stargazer_Wolf`: variant "The Boar", breakpoints `3,4,5,6`; win-combat gold plus empowered-hex health, AD, and AP.
- `TFT17_Stargazer_Medallion`: variant "The Medallion", breakpoint `3`; empowered-hex damage amp scaling with each 3-star ally.
- `TFT17_Stargazer_Huntress`: variant "The Huntress", breakpoints `3,5,7`; marks highest-health enemies, empowered-hex attack speed, Stargazer healing on marked enemy death.
- `TFT17_Stargazer_Serpent`: variant "The Serpent", breakpoints `3,5,7`; empowered-hex durability plus Stargazer poison damage-over-time.
- `TFT17_Stargazer_Shield`: variant "The Altar", breakpoint `3`; sacrifice counter on any champion death, empowered-hex health and attack speed, cashout after sacrifice threshold.
- `TFT17_Stargazer_Fountain`: variant "The Fountain", breakpoints `3,5`; periodic empowered-hex healing and stacking AD/AP.
- `TFT17_Stargazer_Mountain`: variant "The Mountain", breakpoints `3,4,5,6,7,8,9,10,11`; periodic Stargazer Emblem macro reward plus empowered-hex stat layers.

Selection signal in raw data is descriptive, not champion-membership based: the base trait says "different constellation every game" and each variant says "This game: ...". No current normalized field records the selected constellation, encounter seed, set mechanic state, or game rule that chooses one variant.

Most variant mechanics depend on systems Step 12F intentionally does not implement yet:

- empowered hex selection and position targeting
- per-game variant selection
- player-combat win rewards
- counters across rounds or deaths
- marked enemy state
- periodic heal/stacking
- damage-over-time
- macro emblem rewards

## Variant Activation Options

### Option A: Display Trait With Variant Children

Model `Stargazer` as the activating parent trait keyed by champion display references. Preserve variant definitions as children selected by a parent metadata field, for example:

- parent `name`: `Stargazer`
- parent `sourceId`: `TFT17_Stargazer`
- child variants: source ids `TFT17_Stargazer_*`
- active child selected by match metadata, scenario rules, or deterministic default when explicitly configured

Pros:

- Keeps existing champion trait references intact.
- Preserves source identity and variant metadata.
- Makes per-game constellation selection explicit and data-driven.
- Avoids runtime branching on trait names if the parent-child relationship is loaded from metadata.

Cons:

- Requires a trait-variant activation model.
- Requires validation for fallback/default selection.
- Requires later rules for empowered hexes before most variants can execute faithfully.

### Option B: SourceId-Specific Champion Trait Activation

Rewrite or augment champion trait references to include active source ids such as `TFT17_Stargazer_Wolf`.

Pros:

- Existing active-trait counting could activate a source-specific definition with minimal resolver changes.
- Simple for synthetic fixtures.

Cons:

- Raw and normalized champion data do not provide source-id-specific memberships.
- Would require injecting a selected variant into every Stargazer champion at import or scenario-load time.
- Risks breaking existing `Stargazer` trait references and composition logic.
- Makes variant choice look like unit identity data, when raw data describes it as a per-game constellation rule.

### Option C: Active Variant Selected By Metadata, Encounter, Or Rules

Keep champion references as `Stargazer`; add a combat/scenario/import metadata field that selects an active variant source id for the match, for example:

- `activeTraitVariants: { "Stargazer": "TFT17_Stargazer_Wolf" }`
- deterministic scenario default selected by fixture
- live-game import later fills this from game/encounter metadata if available

Pros:

- Data-driven and deterministic.
- Does not alter champion trait references.
- Allows synthetic validation per variant.
- Can keep variants inert unless explicitly selected.

Cons:

- Needs new scenario/game-state metadata.
- Needs resolver changes to compose parent active count with selected child effects.
- Current raw cache does not expose the live selected constellation field.

### Option D: Keep Variants Inert Until Advanced Trait System

Load all variant definitions for visibility, but only the base display trait participates in active trait resolution. Variant definitions stay placeholder/inert until the runtime has variant selection, empowered hexes, and advanced effect support.

Pros:

- Safest for Step 12F.
- Preserves validation PASS and current champion trait behavior.
- Avoids incorrect combat behavior from partially activating the wrong constellation.
- Keeps all raw metadata visible for later mapping.

Cons:

- Stargazer remains low-fidelity for combat.
- Executable trait count does not improve from these variants yet.
- Future implementation still needs a parent-child/selection model.

## Recommended Approach

For Step 12F, use Option D now and design toward Option A plus Option C.

Recommended policy:

1. Keep the base display trait `Stargazer` as the only definition activated by current champion trait references.
2. Keep source-id-specific `TFT17_Stargazer_*` variants loaded but inert by default.
3. Add no generated executable effects for Stargazer variants in Step 12F unless a later step adds explicit variant selection and empowered-hex targeting.
4. Preserve variant source ids, `displayName`, descriptions, tooltips, breakpoints, raw variables, and import warnings.
5. In the next implementation step, add metadata-only parent/variant relationship fields during trait import, with runtime still ignoring them.
6. Later, add deterministic scenario/game-state selection such as `activeTraitVariants["Stargazer"] = "TFT17_Stargazer_Wolf"` before any variant effects execute.

This keeps current champion references working and avoids silently applying every Stargazer constellation at once, which would be much worse than keeping them inert.

## Risks

- Activating all same-display-name variants together would overcount and stack mutually exclusive constellations.
- Replacing champion `Stargazer` references with source ids would break existing trait counting, composition planning, and data compatibility.
- Choosing a default variant without explicit game metadata could make deterministic but wrong combat.
- Variant effects require empowered-hex position targeting; mapping them as all-allies or all-Stargazers would overapply stats.
- Several variants require macro or advanced systems: gold rewards, emblem rewards, death counters, marked enemies, periodic healing, stacking buffs, and poison damage-over-time.
- Parent/child relationships must be data-driven by source metadata, not runtime trait-name branches.

## Next Implementation Step

Step 12F-6 should be metadata-only:

- Add optional normalized trait metadata for `displayName`, `sourceId`, `variantGroup`, `variantParentSourceId`, `variantSelectionKey`, and `isVariant`.
- Generate those fields generically when multiple raw trait records share the same display name and have distinct `apiName` values.
- Keep variants inert for runtime activation.
- Add validation/reporting that counts trait variant groups and confirms `Stargazer` has one activating display parent plus seven inert source-id variants.
- Do not map `Mountain_*`, `Wolf_*`, or other Stargazer variables yet.

# Step 12F-6 Implementation Notes

- Added optional trait/content metadata fields:
  - `isVariant`
  - `variantGroup`
  - `variantParentSourceId`
  - `variantSelectionKey`
- Updated trait import normalization to emit those fields for duplicate display-name groups.
- The base `Stargazer` record remains the activating display trait:
  - `name`: `Stargazer`
  - `sourceId`: `TFT17_Stargazer`
  - `isVariant`: `false`
  - `variantGroup`: `Stargazer`
  - `variantParentSourceId`: `TFT17_Stargazer`
- The seven source-id-specific Stargazer records are inert children:
  - `TFT17_Stargazer_Wolf` -> `variantSelectionKey`: `Wolf`
  - `TFT17_Stargazer_Medallion` -> `variantSelectionKey`: `Medallion`
  - `TFT17_Stargazer_Huntress` -> `variantSelectionKey`: `Huntress`
  - `TFT17_Stargazer_Serpent` -> `variantSelectionKey`: `Serpent`
  - `TFT17_Stargazer_Shield` -> `variantSelectionKey`: `Shield`
  - `TFT17_Stargazer_Fountain` -> `variantSelectionKey`: `Fountain`
  - `TFT17_Stargazer_Mountain` -> `variantSelectionKey`: `Mountain`
- `ContentManager` loads the new metadata fields.
- Content fidelity validation reports variant groups and confirms no variants are active:

```text
Trait variants | groups=1 variants=7 active_variants=0
Trait variant groups | Stargazer=7
```

- Regenerated only `data/traits/*.json` with `.\engine.exe --import-cached-traits`.
- Did not change `TraitResolver`, `TraitEffectExecutor`, trait activation, combat runtime behavior, or Stargazer mechanics.
- Validation remains PASS.
