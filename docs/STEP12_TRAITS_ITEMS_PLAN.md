# Step 12 Traits and Items Plan

## 1. What Trait System Already Exists

The simulator already has a data-driven trait runtime shell:

- `simulator/include/content/Trait.hpp`
  - Defines `Trait`, `TraitHook`, and `TraitEffectType`.
- `simulator/include/content/TraitActivation.hpp`
  - Defines `ActiveTrait` with `traitName`, `activeCount`, and active `breakpoint`.
- `simulator/include/content/TraitData.hpp`
  - Defines `TraitDefinition`, `TraitTier`, and `TraitEffect`.
- `simulator/include/combat/TraitRuntime.hpp`
  - Stores per-team active traits and periodic/aura timing.
- `simulator/src/combat/TraitResolver.cpp`
  - Counts alive units by trait, resolves breakpoints, and returns sorted active traits.
- `simulator/src/combat/TraitSystem.cpp`
  - Wires combat hooks: combat start, periodic tick, aura update, attack, hit, cast, crit, kill, low health, and after damage.
- `simulator/src/combat/TraitEffectExecutor.cpp`
  - Executes a limited set of generic trait effects from `TraitDefinition` tiers.

Currently supported trait effect types:

- `ApplyStatusToTraitUnits`
- `ApplyStatusToEnemyTeam`
- `StackStatusOnAttack`
- `ShieldOnCombatStart`
- `ExecuteBelowHpPercent`
- `TempCritBonusVsLowHp`

Combat already invokes traits in:

- `simulator/src/combat/Combat.cpp`
- `simulator/src/combat/AbilitySystem.cpp`
- `simulator/src/combat/SpellResolver.cpp`

## 2. What Item System Already Exists

The simulator already has item data definitions and combat hooks:

- `simulator/include/content/Item.hpp`
  - Defines `Item` as `name`, `passiveStats`, and `triggeredEffects`.
- `simulator/include/combat/ItemSystem.hpp`
  - Defines item hooks for combat start, attack, hit, crit, cast, low health, and death.
- `simulator/src/combat/ItemSystem.cpp`
  - Executes item `triggeredEffects` using the same `AbilityEffect` shape used by abilities.
- `simulator/include/combat/ItemEffectSystem.hpp`
  - Defines passive stat application.
- `simulator/src/combat/ItemEffectSystem.cpp`
  - Applies item passive stats as long-duration `StatusEffect`s on combat start.
- `simulator/src/combat/StatSystem.cpp`
  - Reads stat-affecting `StatusEffect`s and applies flat/percent modifiers deterministically.
- `simulator/src/core/Unit.cpp`
  - Stores items, stores status effects, applies shields, DoT/HoT ticks, and removes expired effects.

Currently supported item behavior:

- Passive stats through `passiveStats`.
- Triggered damage/status/heal-from-damage effects through `triggeredEffects`.
- Hooks for `OnCombatStart`, `OnAttack`, `OnHit`, `OnCrit`, `OnCast`, `OnLowHealth`, and `OnDeath`.

## 3. What Is Fake, Stubbed, or Hardcoded

The core systems are real enough for deterministic validation, but Step 12 should treat several parts as approximations:

- `simulator/src/import/TFTDataImporter.cpp`
  - `writeTraitNormalized` writes empty `tiers` for the older normalization path.
  - `writeTraitNormalizedFromCdragon` only maps numeric trait variables that look like generic stats into `OnCombatStart` status effects.
  - `writeItemNormalizedFromCdragon` only maps numeric item variables into passive stats and always writes empty `triggeredEffects`.
  - `writeAbilityNormalized` always writes one `OnCast` magic `SingleTarget` damage effect with fixed `adRatio = 0.0f`, `apRatio = 0.8f`, no status logic, no targeting logic, and no special mechanics.
  - `pickDamageFromVariables` guesses damage from variable names containing `d`/`D`.
  - Champion `manaGainOnAttack` defaults to `10`.
  - Champion playability and tags are inferred from broad fields.
- `simulator/src/combat/TraitEffectExecutor.cpp`
  - Supports only a small generic effect vocabulary.
  - Trait-specific mechanics are not represented unless they fit those generic types.
- `simulator/src/combat/ItemSystem.cpp`
  - Uses `AbilityEffect` as a generic item effect model, which is convenient but not expressive enough for many TFT item mechanics.
- `simulator/src/combat/AbilitySystem.cpp`
  - Ability execution supports damage, healing, status application, area shapes, delays, crit options, and simple target types, but not rich live TFT spell scripts.
- `data/traits/*.json`
  - Many files are generated as generic stat tiers rather than faithful trait mechanics.
- `data/items/*.json`
  - Most live-imported items are passive-stat approximations, not full TFT item behavior.
- `data/abilities/*.json`
  - Imported abilities are simplified single-effect approximations.

## 4. What Live TFT Data Is Imported but Not Used

The importer downloads/caches the CommunityDragon TFT dataset:

- `data/_import_cache/cdragon_tft_en_us.json`
- Import code: `simulator/src/import/TFTDataImporter.cpp`

Imported or available fields that are only partially used:

- Champion icons and splash paths are written by `writeChampionNormalized`, but `ContentManager::parseChampion` only stores `spritePath`; `iconPath` and `splashPath` are not used by runtime combat.
- Trait variable data is reduced to generic stat statuses. Non-stat variables, descriptions, conditional effects, summon mechanics, counters, transformations, scaling rules, and trait-specific logic are dropped.
- Item variable data is reduced to passive stats. Triggered item mechanics are not imported into `triggeredEffects`.
- Spell data is reduced to one guessed damage value and a generic magic single-target cast. Targeting, status effects, shields, summons, multi-hit behavior, scaling arrays, cast timings, projectile details, and special rules are not preserved.
- Raw item/champion/trait text descriptions from CommunityDragon are not retained in normalized runtime data.
- Set metadata is used only for set selection/reporting.
- Many imported items appear to include augments, loot, or non-combat objects; runtime loads them as items, but combat only uses items explicitly attached to scenario units.

## 5. Exact Files Involved

Trait runtime:

- `simulator/include/content/Trait.hpp`
- `simulator/include/content/TraitActivation.hpp`
- `simulator/include/content/TraitData.hpp`
- `simulator/include/combat/TraitRuntime.hpp`
- `simulator/include/combat/TraitResolver.hpp`
- `simulator/src/combat/TraitResolver.cpp`
- `simulator/include/combat/TraitSystem.hpp`
- `simulator/src/combat/TraitSystem.cpp`
- `simulator/include/combat/TraitEffectExecutor.hpp`
- `simulator/src/combat/TraitEffectExecutor.cpp`

Item runtime:

- `simulator/include/content/Item.hpp`
- `simulator/include/combat/ItemSystem.hpp`
- `simulator/src/combat/ItemSystem.cpp`
- `simulator/include/combat/ItemEffectSystem.hpp`
- `simulator/src/combat/ItemEffectSystem.cpp`

Ability runtime:

- `simulator/include/content/Ability.hpp`
- `simulator/include/combat/AbilitySystem.hpp`
- `simulator/src/combat/AbilitySystem.cpp`
- `simulator/include/combat/SpellResolver.hpp`
- `simulator/src/combat/SpellResolver.cpp`

Shared combat/stat state:

- `simulator/include/core/Unit.hpp`
- `simulator/src/core/Unit.cpp`
- `simulator/include/combat/StatusEffect.hpp`
- `simulator/include/combat/StatSystem.hpp`
- `simulator/src/combat/StatSystem.cpp`
- `simulator/src/combat/Combat.cpp`

Content loading and import:

- `simulator/include/content/ContentManager.hpp`
- `simulator/src/content/ContentManager.cpp`
- `simulator/include/content/UnitFactory.hpp`
- `simulator/src/content/UnitFactory.cpp`
- `simulator/include/import/TFTDataImporter.hpp`
- `simulator/src/import/TFTDataImporter.cpp`

Current data:

- `data/champions/*.json`
- `data/abilities/*.json`
- `data/traits/*.json`
- `data/items/*.json`
- `data/_import_cache/cdragon_tft_en_us.json`
- `simulator/data/*`

Validation:

- `simulator/include/validation/CombatValidation.hpp`
- `simulator/src/validation/CombatValidation.cpp`
- `data/scenarios/five_v_five.json`

## 6. Best Step 12 Implementation Order

1. Add audit validation for imported content shape.
   - Count how many traits have tiers/effects.
   - Count how many items have passive stats versus triggered effects.
   - Count how many abilities are generic single-target placeholder casts.
   - This should be validation-only and should not change combat behavior.

2. Strengthen normalized data metadata without executing new mechanics.
   - Preserve source ids, descriptions, raw variable names, effect names, and import warnings in normalized JSON.
   - Keep runtime deterministic by treating new metadata as inert until validated.

3. Formalize the generic effect vocabulary.
   - Decide which trait/item/ability effects are allowed in Step 12.
   - Prefer generic data-driven effects over champion, trait, or item name checks.
   - Add parser validation for unknown effect types instead of silent fallback.

4. Implement trait effect coverage first.
   - Traits are team-wide and breakpoint-based, so they are the clearest next layer after Step 11.
   - Add validation scenarios for one effect type at a time.
   - Keep all trait execution routed through `TraitSystem` and `TraitEffectExecutor`.

5. Implement item triggered effects second.
   - Reuse `ItemSystem` hooks.
   - Add item-specific validation scenarios using synthetic items first.
   - Import live items into generic triggered effects only after synthetic validation passes.

6. Improve ability import third.
   - Preserve spell targeting, area, delays, damage type, ratios, status effects, shields, and healing from live data where mappings are reliable.
   - Avoid trait/item-specific hardcoding.

7. Re-run deterministic validation after every new effect category.
   - Required gate: build plus `.\engine.exe --validate`.
   - Add replay cases before enabling any effect that uses RNG, scheduled events, stacking, or target selection.

## 7. Risks That Could Break Combat Determinism

- Iterating `std::unordered_map` data while applying effects can create nondeterministic ordering unless results are sorted before execution.
- Scheduled events that capture raw `Unit*` can become fragile if future systems reorder or resize `state.units()`.
- New triggered effects can recursively call hooks and create order-dependent chains.
- Adding RNG to traits/items/abilities must use seeded `RandomManager` or explicit seeded `Random`; no system-time randomness.
- Status stacking by name can become nondeterministic if multiple effects use identical generated names or are applied in different orders.
- Periodic and aura effects must use fixed tick times and deterministic tie-breaks.
- Floating-point thresholds can diverge if comparisons are not clamped/rounded consistently.
- Effects that summon, clone, remove, revive, or reorder units can invalidate target indices, event captures, board occupancy, and replay logs.
- Import fallbacks can silently change behavior when CommunityDragon schema changes; Step 12 should validate import warnings and normalized output before runtime use.
- New live-data mechanics must not introduce champion, trait, or item name scripts in runtime systems, per `ARCHITECT_RULES.md`.
