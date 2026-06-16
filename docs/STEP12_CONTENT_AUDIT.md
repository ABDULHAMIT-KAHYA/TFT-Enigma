# Executive Summary

Step 12A audited normalized content in `data/champions`, `data/abilities`, `data/traits`, and `data/items`, then reviewed the runtime/import paths in `TraitEffectExecutor`, `TraitSystem`, `ItemSystem`, `AbilitySystem`, and `TFTDataImporter`.

The simulator has a real deterministic effect runtime shell, but live content fidelity is currently low. Most live TFT content is reduced to generic stats or placeholder spells. No normalized item has triggered behavior, every normalized ability uses the same single-target magic damage shape, and every executable trait is a stat-only approximation.

Runtime behavior was not modified.

Step 12B added a validation-only content fidelity section to `.\engine.exe --validate`. It reports the same placeholder/executable content categories from normalized data and emits warnings only for the current placeholder-heavy state.

Step 12C added metadata preservation support for future normalized imports without changing combat behavior. `ContentManager` now loads optional metadata fields and runtime systems continue to ignore them. Existing checked-in normalized data remains placeholder-heavy; legacy files without explicit placeholder flags are classified by fallback inference during load/reporting.

Step 12D added item-category metadata and reporting without changing combat behavior. Future importer output writes `itemCategory`; current legacy normalized item files without category metadata are reported as `CombatItem` only when they already have runtime effects and otherwise as `Unknown`.

Step 12E-1 added the first two generic trait status fan-out effects without changing importer behavior or live normalized data. `ApplyStatusToAllies` and `ApplyStatusToEnemies` now parse and execute for synthetic trait definitions, while existing `ApplyStatusToEnemyTeam` remains backward compatible.

Step 12E-2 added generic trait `Shield` without changing importer behavior or live normalized data. The effect applies active shield statuses to alive allied units and legacy `ShieldOnCombatStart` remains compatible.

Step 12E-3 added generic trait `Heal` without changing importer behavior or live normalized data. The effect heals alive allied units through existing `Unit::heal` clamping and does not affect enemies or dead units.

Step 12E-4 added generic trait `DealDamage` without changing importer behavior or live normalized data. The effect damages all alive enemy units through existing `DamageSystem` calculation and `Unit::applyDamage`, so shield absorption works. Recursive damage hooks are intentionally not triggered yet.

Step 12F-1 added safe stat-like trait variable mapping in `TFTDataImporter` only. It maps a strict allowed set of raw CommunityDragon trait variables into existing generic trait stat effects for future imports, preserves unmapped variables as warnings, and does not change combat runtime behavior or checked-in normalized data.

Step 12F-2 expanded safe stat-like trait variable mapping in `TFTDataImporter` only. It adds armor/MR, teamwide stat, offensive-stat, health alias, damage amp, resistance, and resistance-debuff mappings, and reports the top unmapped trait variables by frequency.

Step 12F-3 regenerated only normalized trait JSON from the cached CommunityDragon payload using the Step 12F safe mappings. Runtime execution was unchanged, and champions, abilities, and items were not regenerated.

Step 12F-4 fixed trait source identity collisions during trait import only. Same-display-name trait variants now preserve unique `sourceId`/`apiName` identities as separate normalized files while keeping the original display name in `displayName`.

Step 12F-6 added metadata-only support for same-display-name trait variants. Variant metadata is imported, loaded, and reported by content fidelity validation, but trait activation remains unchanged and active variants remain zero.

# Content Statistics

| Metric | Count |
| --- | ---: |
| Total champions | 82 |
| Playable champions | 63 |
| Non-playable champions / objects | 19 |
| Total abilities | 78 |
| Total traits | 44 |
| Total items | 2962 |
| Traits with executable effects | 16 |
| Traits that are stat-only approximations | 16 |
| Traits with no executable effects | 28 |
| Trait variant groups | 1 |
| Trait variants | 7 |
| Active trait variants | 0 |
| Items with triggered effects | 0 |
| Items that are passive-stat-only | 817 |
| Items with no runtime effects | 2145 |
| Abilities using placeholder single-target magic damage logic | 78 |
| Abilities with non-placeholder behavior | 0 |
| Ability placeholder flags | 78 |
| Trait placeholder flags | 28 |
| Item placeholder flags | 2962 |
| Item category: CombatItem | 817 |
| Item category: Emblem | 0 |
| Item category: RadiantItem | 0 |
| Item category: Artifact | 0 |
| Item category: SupportItem | 0 |
| Item category: Consumable | 0 |
| Item category: Anvil | 0 |
| Item category: LootObject | 0 |
| Item category: Augment | 0 |
| Item category: Unknown | 2145 |

These metrics are now printed by the validation runner under:

```text
[VALIDATION] START: Content fidelity
Content Fidelity Summary
Data root: D:\TFT-Galaxy-engine\tft-ai\data
JSON files | champions=82 abilities=78 traits=44 items=2962
Loaded content | champions=82 abilities=78 traits=44 items=2962
Abilities | placeholder_single_target_magic=78 non_placeholder=0
Traits | executable_effects=16 empty_or_no_executable_effects=28
Trait variants | groups=1 variants=7 active_variants=0
Trait variant groups | Stargazer=7
Trait stat mapping | current_executable_before_import=16 projected_executable_after_import=16 raw_trait_records=44 mapped_variables=69 unmapped_variables=583
Trait stat mapping top unmapped | Mountain_RoundsPerEmblem=15 Mountain_Health=14 Mountain_ADAP=13 Mountain_AS=13 Mountain_DR=13 Mountain_Resists=13 Mountain_StatIncrease=13 ShieldDuration=11 HealthThreshold=10 Wolf_Health_Teamwide=10
Items | passive_stats_only=817 triggered_effects=0 no_runtime_effects=2145
Placeholder flags | abilities=78 traits=28 items=2962
Item categories | CombatItem=817 Emblem=0 RadiantItem=0 Artifact=0 SupportItem=0 Consumable=0 Anvil=0 LootObject=0 Augment=0 Unknown=2145
```

Current validation warning categories:

- Placeholder single-target magic abilities present.
- Traits with empty/no executable effects present.
- Passive-stat-only items present.
- Items with no runtime effects present.

These are warnings, not failures.

Step 12F trait stat mapping metrics are projection metrics from `data/_import_cache/cdragon_tft_en_us.json`. They show future importer coverage for the current raw cache and allowed variable list; they do not mean normalized trait JSON was regenerated in this step.

After Step 12F-3, normalized traits were regenerated. The source cache had 44 raw trait records, but the previous filename policy emitted 37 normalized trait files because same-name variants collapsed to the same filename.

After Step 12F-4, trait source identity is preserved:

```text
Before Step 12F-4: raw_trait_records=44 normalized_trait_files=37 loaded_trait_definitions=37
After Step 12F-4:  raw_trait_records=44 normalized_trait_files=44 loaded_trait_definitions=44
```

The only collision group was the `Stargazer` display name, represented by eight different source ids: `TFT17_Stargazer`, `TFT17_Stargazer_Wolf`, `TFT17_Stargazer_Medallion`, `TFT17_Stargazer_Huntress`, `TFT17_Stargazer_Serpent`, `TFT17_Stargazer_Shield`, `TFT17_Stargazer_Fountain`, and `TFT17_Stargazer_Mountain`.

After Step 12F-6, the `Stargazer` collision group has explicit inert variant metadata:

```text
Trait variants | groups=1 variants=7 active_variants=0
Trait variant groups | Stargazer=7
```

`TFT17_Stargazer` remains the base parent and the seven `TFT17_Stargazer_*` records are children with selection keys `Wolf`, `Medallion`, `Huntress`, `Serpent`, `Shield`, `Fountain`, and `Mountain`.

Criteria used:

- Trait with executable effects: at least one tier has at least one effect.
- Stat-only trait approximation: all effects are `OnCombatStart` + `ApplyStatusToTraitUnits` + stat-affecting `StatusEffect`.
- Passive-stat-only item: has `passiveStats` and no `triggeredEffects`.
- Placeholder ability: exactly one `OnCast` effect, `Magic` damage, `adRatio = 0`, `apRatio = 0.8`, `SingleTarget`, `radius = 0`, `delayMs = 0`, `canCrit = false`, no status, no heal, no shield.

Cached live-data snapshot observations from `data/_import_cache/cdragon_tft_en_us.json`:

| Raw cached-data metric | Count |
| --- | ---: |
| Latest set champions | 83 |
| Latest set traits | 44 |
| Latest set champions with spell data | 83 |
| Latest set spells with variables | 74 |
| Latest set spells with descriptions/tooltips | 83 |
| Latest set traits with variables | 43 |
| Latest set traits with descriptions | 44 |
| Root item records | 3599 |
| Root item records with effects object | 3599 |
| Root item records with description | 3422 |

# Trait Fidelity

Runtime files reviewed:

- `simulator/src/combat/TraitEffectExecutor.cpp`
- `simulator/src/combat/TraitSystem.cpp`
- `simulator/src/combat/TraitResolver.cpp`
- `simulator/include/content/Trait.hpp`
- `simulator/include/content/TraitData.hpp`
- `simulator/include/content/TraitActivation.hpp`
- `simulator/include/combat/TraitRuntime.hpp`

What is preserved:

- Trait names.
- Unit trait membership through champion JSON.
- Breakpoints.
- Active trait counting by alive units.
- Deterministic sorted active-trait order.
- Generic hook routing for combat start, periodic, aura update, attack, hit, cast, crit, kill, low health, and after damage.
- Generic stat/status effects when normalized into `TraitDefinition.tiers`.

What is low fidelity:

- 16 of 44 traits currently have executable tier effects.
- All 16 executable traits are stat-only approximations.
- Current normalized trait effect type usage is only `ApplyStatusToTraitUnits`.
- No normalized trait currently uses the other runtime-supported types: `ApplyStatusToEnemyTeam`, `StackStatusOnAttack`, `ShieldOnCombatStart`, `ExecuteBelowHpPercent`, or `TempCritBonusVsLowHp`.
- Trait descriptions, icons, conditional mechanics, counters, summons, conversions, per-event scaling, generated units, team-wide non-trait-unit effects, and round/macro effects are discarded or not represented.

# Item Fidelity

Runtime files reviewed:

- `simulator/src/combat/ItemSystem.cpp`
- `simulator/src/combat/ItemEffectSystem.cpp`
- `simulator/include/content/Item.hpp`
- `simulator/include/combat/ItemSystem.hpp`
- `simulator/include/combat/ItemEffectSystem.hpp`

What is preserved:

- Item names.
- Some numeric item effect variables when they can be mapped to generic stats.
- Passive stat application through long-duration `StatusEffect`s.
- Runtime hooks exist for combat start, attack, hit, crit, cast, low health, and death.

What is low fidelity:

- 0 of 2962 normalized items have `triggeredEffects`.
- 817 items are passive-stat-only.
- 2145 items have no runtime effects at all.
- Root cached live data has 3599 item records with `effects`, but importer normalization drops triggered mechanics and writes `triggeredEffects: []`.
- Item metadata such as composition, source components, uniqueness, tags, associated traits, incompatible traits, descriptions, and icons is not used by combat.
- Effects such as proc damage, shields, cooldowns, stacking, item-specific target selection, aura behavior, artifact mechanics, radiant variants, consumables, anvils, emblems, or loot objects are not faithfully represented.

# Ability Fidelity

Runtime files reviewed:

- `simulator/src/combat/AbilitySystem.cpp`
- `simulator/src/combat/SpellResolver.cpp`
- `simulator/include/content/Ability.hpp`
- `simulator/include/combat/AbilitySystem.hpp`
- `simulator/include/combat/SpellResolver.hpp`

What is preserved:

- Ability ids/names.
- Mana cost/start mana enough to trigger casts.
- A generic `OnCast` effect path.
- Runtime support for basic target types, simple area shapes, damage formulas, healing, status application, delays, and physical crit options.

What is low fidelity:

- 78 of 78 normalized abilities match the placeholder single-target magic damage pattern.
- 0 abilities have non-placeholder behavior.
- Normalized ability area shapes are only `SingleTarget`.
- Normalized ability triggers are only `OnCast`.
- Normalized ability damage types are only `Magic`.
- Live spell descriptions/tooltips and most spell variables are not represented.
- Targeting, multiple effects, projectile behavior, cast-specific delays, multi-hit patterns, shields, healing, summons, dashes, CC, execute logic, empowered attacks, transformations, cloning, and spell-specific scaling are discarded unless manually encoded in normalized JSON.

# Importer Loss Analysis

Importer file reviewed:

- `simulator/src/import/TFTDataImporter.cpp`

Current preserved import fields:

- Champions: name, cost, hp, attack damage, armor, magic resist, range, attack speed, ability power, crit chance, crit damage, durability, traits, playability tags, and ability name/id.
- Traits: name, breakpoints, and a subset of numeric variables that map to stat-like effects.
- Items: name and a subset of numeric variables that map to stat-like passive effects.
- Abilities: id, name, mana cost, one guessed base-damage value, fixed `adRatio = 0`, fixed `apRatio = 0.8`, fixed magic damage, fixed single-target shape.

Step 12F-1 trait stat variable mapping:

- Allowed variables: `AttackSpeedPercent`, `AD`, `BonusAD`, `AP`, `BonusHealth`, `HealthBonus`, `Omnivamp`, `CritChance`, `CritDamage`, `Durability`, `DamageReductionPct`, `EnhancedDurability`, and `PercentDamageIncrease`.
- Default target: `ApplyStatusToTraitUnits`.
- Teamwide target: `ApplyStatusToAllies` only when generic metadata clearly says team/allies and does not include blocked positional, role-targeted, chosen, marked, delayed, conditional, or event-triggered language.
- Unsupported variables remain in `rawVariables` and receive import warnings.

Step 12F-2 added safe trait stat variable mappings:

- Defensive stats: `BonusArmor`, `Armor`, `BonusMR`, `MR`, `MagicResist`, `TeamwideResists`, `Resistances`.
- Team/offensive stats: `TeamwideBonus`, `TeamwideAS`, `BonusOffensiveStat`, `BonusOffensiveStats`, `AttackSpeed`.
- Health aliases: `MaxHealth`, `MaxHp`, `HP`, `Health`.
- Damage stats: `DamageAmp`, `DamageIncrease`, `BonusDamage`, `DamageReduction`.
- Debuff stats: `PctResists`.

Current projected mapping improvement:

- Step 12F-1: `mapped_variables=21`, `projected_executable_after_import=4`.
- Step 12F-2: `mapped_variables=69`, `projected_executable_after_import=16`.

Current discarded or degraded data:

- Champion role, icon, square icon, tile icon, and richer spell metadata.
- Trait descriptions, icons, non-stat variables, conditional logic, counters, event logic, and special mechanics.
- Item descriptions, component/from data, tags, uniqueness, associated/incompatible traits, and all triggered effects.
- Spell descriptions/tooltips, variable arrays beyond guessed damage, damage type, targeting, area, delay, status, healing, shield, summon, movement, and multi-effect behavior.
- Import warnings are counted but not attached to normalized records.

Step 12C metadata now preserved when present/generated:

- Shared content metadata: `sourceId`, `displayName`, `description`, `tooltip`, `iconPath`, `squareIconPath`, `tileIconPath`, `splashPath`, `rawVariables`, `importWarnings`, and `isPlaceholder`.
- Trait variant metadata: `isVariant`, `variantGroup`, `variantParentSourceId`, and `variantSelectionKey`.
- Ability spell metadata: `targetingMetadata`, `areaMetadata`, `damageMetadata`, and `effectMetadata`.
- Trait metadata: raw source variables from each imported effect/breakpoint and warnings for unmapped, non-numeric, zero, or fully unmapped variables.
- Item metadata: raw source effect variables and warnings for unmapped, non-numeric, zero, or missing effect objects.
- Item category metadata: `itemCategory` values include `CombatItem`, `Emblem`, `RadiantItem`, `Artifact`, `SupportItem`, `Consumable`, `Anvil`, `LootObject`, `Augment`, and `Unknown`.

This metadata is loaded into `ContentMetadata` and intentionally ignored by combat execution for now.

Step 12D importer classification rules:

- `Augment`: generic source text or icon path contains augment/hexcore markers.
- `Anvil`: generic source text contains anvil markers.
- `RadiantItem`: generic source text contains radiant markers.
- `Artifact`: generic source text or icon path contains artifact markers.
- `SupportItem`: generic source text contains support markers.
- `Emblem`: generic source text contains emblem markers.
- `Consumable`: generic source text contains consumable, gold, reroll, XP, or shop markers.
- `LootObject`: generic source text contains loot, orb, or chest markers.
- `CombatItem`: no earlier category matched, but source `from`, `composition`, or effect variables indicate an item-like runtime record.
- `Unknown`: no category matched.

The classifier uses source fields such as `apiName`, `icon`, `desc`, `description`, `tags`, trait association arrays, `from`, `composition`, and `effects`. It does not use hand-picked individual item exceptions.

Important importer approximations:

- `writeTraitNormalizedFromCdragon` converts only stat-looking trait variables into `OnCombatStart` trait-unit statuses.
- `writeItemNormalizedFromCdragon` converts only stat-looking item variables into passive stats and always emits no triggered effects.
- `writeAbilityNormalized` emits one generic magic single-target spell for every ability.
- `pickDamageFromVariables` guesses damage from variable names containing `d` or `D`.
- Champion mana gain defaults to `10`.

# Missing Effect Types

Missing or insufficient runtime vocabulary for live TFT fidelity:

- Conditional stat scaling by current HP, missing HP, enemy count, ally count, distance, stage, star level, item count, or trait count.
- Team-wide effects that apply to all allies, not only trait units.
- Enemy debuffs with duration, refresh, stacking, or resistance shred.
- Auras with source/radius positioning and deterministic refresh.
- Cooldowns and once-per-combat triggers.
- On-shield-break, on-heal, on-damage-taken threshold, on-ally-death, on-enemy-death, on-round-start, and after-cast hooks.
- Summons, spawned units, clones, pets, turrets, and temporary units.
- Movement, dashes, pulls, knockups, displacement, and retarget forcing.
- Projectile count, bounces, chains, split targeting, and multi-hit sequencing.
- Damage-over-time and heal-over-time sourced attribution for traits/items.
- Executes with payout/side effects.
- Damage conversion, damage redirection, immunity, untargetability, revive, stasis, and cleanse.
- Item cooldowns, stacking items, aura items, emblem trait grants, consumables, anvils, loot, and macro-only items.
- Variable formula arrays by star level, breakpoint, or item tier.

# Recommended Generic Effect Vocabulary

Recommended order for extending generic data-driven effects:

1. `ApplyStatusToAllies` - implemented for traits in Step 12E-1.
2. `ApplyStatusToEnemies` - implemented for traits in Step 12E-1.
3. `ApplyStatusInRadius`
4. `DealDamage` - implemented for traits in Step 12E-4.
5. `DealPercentHealthDamage`
6. `Heal` - implemented for traits in Step 12E-3.
7. `Shield` - implemented for traits in Step 12E-2.
8. `StackStatus`
9. `ConditionalEffect`
10. `CooldownGate`
11. `OncePerCombatGate`
12. `ModifyMana`
13. `GrantTrait`
14. `SummonUnit`
15. `SpawnProjectile`
16. `MultiHitSequence`
17. `ChainOrBounce`
18. `MoveOrDash`
19. `DisplaceTarget`
20. `RetargetOrTaunt`
21. `CleanseOrImmunity`
22. `ReviveOrStasis`
23. `TransformUnit`
24. `Execute`
25. `RecordCounter`

Each effect should be deterministic, validated in isolation, serializable from normalized JSON, and independent of hardcoded champion/trait/item names.

Step 12E-1 validation coverage:

- `TraitEffect: ApplyStatusToAllies applies to alive allies`
- `TraitEffect: ApplyStatusToEnemies applies to alive enemies`
- `TraitEffect: status fanout skips dead units`
- `TraitEffect: ApplyStatusToEnemyTeam remains compatible`

Step 12E-2 validation coverage:

- `TraitEffect: Shield applies to alive allies`
- `TraitEffect: Shield skips dead units`
- `TraitEffect: Shield absorbs incoming damage`
- `TraitEffect: ShieldOnCombatStart remains compatible`

Step 12E-3 validation coverage:

- `TraitEffect: Heal increases damaged allied HP`
- `TraitEffect: Heal does not exceed max HP`
- `TraitEffect: Heal skips dead units`
- `TraitEffect: Heal does not affect enemies`

Step 12E-4 validation coverage:

- `TraitEffect: DealDamage reduces enemy HP`
- `TraitEffect: DealDamage does not affect allies`
- `TraitEffect: DealDamage skips dead enemies`
- `TraitEffect: DealDamage is absorbed by shields`
- `TraitEffect: DealDamage supports physical magic true formulas`

Live importer mappings still do not emit these new effect types.

# Top 20 Highest-Impact Improvements

1. Add a content fidelity validation that reports placeholder counts every `--validate` run.
2. Preserve raw import metadata in normalized JSON: api name, description, tooltip, icon, source path, and import warnings.
3. Add explicit placeholder flags to imported abilities/items/traits.
4. Expand trait normalization to keep unmapped variables instead of dropping them.
5. Add `ApplyStatusToAllies` and `ApplyStatusToEnemies` effect types. Completed for trait runtime and synthetic validation in Step 12E-1; live importer mapping remains pending.
6. Add item triggered-effect import for simple `OnHit`, `OnAttack`, `OnCast`, and `OnCombatStart` effects.
7. Add cooldown/once-per-combat gates before importing proc items.
8. Add generic damage/heal/shield effect types for traits and items. Trait `Shield` completed in Step 12E-2, trait `Heal` completed in Step 12E-3, and trait `DealDamage` completed in Step 12E-4; item mappings and live importer mappings remain pending.
9. Add conditional effect predicates for HP percent, source trait, target trait, and combat time.
10. Add deterministic aura radius support.
11. Add deterministic multi-target selection policies with stable tie-breaks.
12. Preserve spell target type and area shape from live data when available.
13. Preserve spell damage type from live data when available.
14. Import spell status effects when they map cleanly to existing `StatusEffect`.
15. Import spell shields and heals as first-class effects.
16. Add synthetic validation scenarios for each new generic effect type before live-data import uses it.
17. Add data-quality reports for items with no runtime effects.
18. Separate combat items, augments, loot objects, emblems, and macro-only items in normalized data.
19. Add deterministic replay fixtures for trait/item/ability scenarios.
20. Add a no-name-hardcoding scan for all new trait/item/ability runtime logic.

Step 12F-1 progress:

- Added strict importer-side trait stat classifier.
- Added deterministic generated-effect ordering for mapped trait stat effects.
- Added content-fidelity projection output for mapped/unmapped raw trait variables.
- Runtime execution remains unchanged.

Step 12F-2 progress:

- Expanded importer-only trait stat mappings.
- Added top-unmapped trait variable reporting.
- Preserved runtime execution unchanged.

Step 12F-3 progress:

- Added `--import-cached-traits`.
- Regenerated only `data/traits/*.json`.
- Improved runtime-loaded trait coverage from 15 to 16 traits with executable effects.
- Before the identity fix, reduced traits with no executable effects from 22 to 21.
- Before the identity fix, reduced explicit trait placeholder flags from 37 to 21.
- Build and validation remain PASS.

Step 12F-4 progress:

- Fixed same-display-name trait file collisions.
- Regenerated only `data/traits/*.json`.
- Preserved 44 raw cached trait records as 44 normalized trait files and 44 loaded trait definitions.
- Preserved original display names separately from unique source identities.
- Build and validation remain PASS.

Step 12F-6 progress:

- Added metadata-only trait variant fields.
- Regenerated only `data/traits/*.json`.
- Reported one trait variant group: `Stargazer=7`.
- Confirmed `active_variants=0`.
- Preserved trait activation, `TraitResolver`, `TraitEffectExecutor`, and combat runtime behavior unchanged.
- Build and validation remain PASS.

# Risks To Determinism

- `std::unordered_map` iteration can produce unstable effect ordering unless sorted before execution.
- Live-data import schema changes can silently change normalized output.
- Scheduled lambdas currently capture unit pointers in some paths; future unit insertion/removal could invalidate them.
- Summons, clones, revives, and transformations can reorder units and invalidate target/event references.
- Recursive hooks can create order-dependent chains.
- Trait `DealDamage` currently avoids recursive damage hooks; enabling those hooks later needs explicit event ordering and replay validation.
- Proc effects that use RNG must use seeded simulator RNG only.
- Periodic/aura effects need fixed tick scheduling and stable tie-breaks.
- Floating-point formula parsing can diverge unless rounded/clamped consistently.
- Multiple effects with identical status names can make stack counting order-sensitive.
- Targeting policies for equal-distance/equal-HP targets must sort deterministically.
- Imported item/trait effects that alter movement or targeting can affect board occupancy order.
- Hidden fallback defaults can change replay results when live data fields are absent or renamed.
