# Step 12E Effect Vocabulary Plan

# Scope

Step 12E started as planning only. Step 12E-1 implemented the first two safe generic trait status fan-out effects. Step 12E-2 implemented generic trait `Shield`. Step 12E-3 implemented generic trait `Heal`. Step 12E-4 implemented generic trait `DealDamage`. Importer behavior and live content mappings remain unchanged.

Goal for the next implementation step: add the smallest safe generic effect vocabulary expansion that can represent simple trait/item/ability mechanics without hardcoded champion, item, trait, or ability names.

Target effect vocabulary:

- `ApplyStatusToAllies`
- `ApplyStatusToEnemies`
- `Shield`
- `Heal`
- `DealDamage`

Step 12E-1 implemented:

- `ApplyStatusToAllies`
- `ApplyStatusToEnemies`
- Backward-compatible `ApplyStatusToEnemyTeam`

Step 12E-1 validation added:

- `TraitEffect: ApplyStatusToAllies applies to alive allies`
- `TraitEffect: ApplyStatusToEnemies applies to alive enemies`
- `TraitEffect: status fanout skips dead units`
- `TraitEffect: ApplyStatusToEnemyTeam remains compatible`

Step 12E-2 implemented:

- `Shield`
- Backward-compatible active shielding for `ShieldOnCombatStart`

Step 12E-2 validation added:

- `TraitEffect: Shield applies to alive allies`
- `TraitEffect: Shield skips dead units`
- `TraitEffect: Shield absorbs incoming damage`
- `TraitEffect: ShieldOnCombatStart remains compatible`

Step 12E-3 implemented:

- `Heal`

Step 12E-3 validation added:

- `TraitEffect: Heal increases damaged allied HP`
- `TraitEffect: Heal does not exceed max HP`
- `TraitEffect: Heal skips dead units`
- `TraitEffect: Heal does not affect enemies`

Step 12E-4 implemented:

- `DealDamage`

Step 12E-4 validation added:

- `TraitEffect: DealDamage reduces enemy HP`
- `TraitEffect: DealDamage does not affect allies`
- `TraitEffect: DealDamage skips dead enemies`
- `TraitEffect: DealDamage is absorbed by shields`
- `TraitEffect: DealDamage supports physical magic true formulas`

# Files Reviewed

- `simulator/src/combat/TraitEffectExecutor.cpp`
- `simulator/include/combat/TraitEffectExecutor.hpp`
- `simulator/src/combat/ItemSystem.cpp`
- `simulator/include/combat/ItemSystem.hpp`
- `simulator/src/combat/AbilitySystem.cpp`
- `simulator/include/combat/AbilitySystem.hpp`
- `simulator/src/combat/SpellResolver.cpp`
- `simulator/include/content/Ability.hpp`
- `simulator/include/content/Trait.hpp`
- `simulator/include/content/TraitData.hpp`
- `simulator/include/combat/StatusEffect.hpp`
- `simulator/include/combat/StatusEffectType.hpp`
- `simulator/src/content/ContentManager.cpp`
- `simulator/src/validation/CombatValidation.cpp`

# 1. Existing Partial Support

## ApplyStatusToAllies

Implemented for trait effects in Step 12E-1.

- Ability/item target resolution can apply status to friendly units when `AbilityEffect.appliesStatusEffect` is true and the status type is classified as friendly.
- Trait system has `ApplyStatusToTraitUnits`, but that only applies to allied units that have the active trait.
- `ApplyStatusToAllies` now applies `TraitEffect.statusEffect` to all alive allied units in deterministic `state.units()` order.

## ApplyStatusToEnemies

Implemented for trait effects in Step 12E-1.

- `TraitEffectType::ApplyStatusToEnemyTeam` already applies `TraitEffect.statusEffect` to all alive enemy units.
- Ability/item effects can apply status to target/area units through `AbilityEffect.appliesStatusEffect`.
- `ApplyStatusToEnemies` now applies `TraitEffect.statusEffect` to all alive enemy units in deterministic `state.units()` order.
- Existing `ApplyStatusToEnemyTeam` remains backward compatible and routes through the same executor behavior.

## Shield

Implemented for trait effects in Step 12E-2.

- `StatusEffectType::Shield` exists.
- `TraitEffectType::ShieldOnCombatStart` exists and remains trait-unit-only.
- `TraitEffectType::Shield` now applies a shield to all alive allied units in deterministic `state.units()` order.
- Trait shields now set active `remainingMs`, allowing `Unit::applyDamage` to absorb incoming damage through existing shield status logic.
- `AbilityEffect` has `shieldAmount`, but `AbilitySystem::executeEffectNow` currently does not apply it.
- Item effects can apply a shield if encoded as `appliesStatusEffect` with a `StatusEffectType::Shield`.

## Heal

Implemented for trait effects in Step 12E-3.

- `AbilityEffect.healAmount` is parsed and applied by `AbilitySystem`.
- `AbilityEffect.healPercentOfDamage` exists and is used by `ItemSystem` on `OnHit`.
- `StatusEffectType::HealOverTime` exists.
- `TraitEffectType::Heal` now heals all alive allied units in deterministic `state.units()` order.
- Healing uses `Unit::heal`, so HP is clamped to effective max HP.

## DealDamage

Implemented for trait effects in Step 12E-4.

- `AbilityEffect.damageFormula` supports `baseDamage`, `adRatio`, `apRatio`, and `damageType`.
- `AbilitySystem` applies ability damage through `DamageSystem::calculateDamageDebug`, triggers damage-taken mana, omnivamp, on-hit, on-damage-taken, after-damage, kill, death, and low-health hooks.
- `ItemSystem` also applies triggered item damage through `AbilityEffect.damageFormula`.
- `TraitEffectType::DealDamage` now uses `TraitEffect.damageFormula` and `DamageSystem::calculateDamageDebug`.
- Trait `DealDamage` damages all alive enemy units in deterministic `state.units()` order.
- Trait `DealDamage` applies final damage through `Unit::applyDamage`, so existing shield absorption works.
- Trait `DealDamage` intentionally does not trigger recursive on-hit, after-damage, death, low-health, or damage-taken mana hooks yet.

# 2. Enum And Model Changes Needed

## TraitEffectType

Step 12E-1 and Step 12E-2 added these values to `simulator/include/content/Trait.hpp`:

- `ApplyStatusToAllies`
- `ApplyStatusToEnemies`
- `Shield`
- `Heal`
- `DealDamage`

Keep existing values for backward compatibility:

- `ApplyStatusToTraitUnits`
- `ApplyStatusToEnemyTeam`
- `StackStatusOnAttack`
- `ShieldOnCombatStart`
- `ExecuteBelowHpPercent`
- `TempCritBonusVsLowHp`

Recommended compatibility behavior:

- Treat `ApplyStatusToEnemies` and existing `ApplyStatusToEnemyTeam` as equivalent executor paths.
- Do not rename `ApplyStatusToEnemyTeam` yet, because existing normalized JSON may already use it.

## TraitEffect Model

Extend `TraitEffect` in `simulator/include/content/TraitData.hpp` with fields already proven in `AbilityEffect`:

- `DamageFormula damageFormula{}`
- `std::int32_t healAmount = 0`
- Optional `TargetType targetType = TargetType::CurrentEnemy` only if single-target trait damage/heal is needed in this step.
- Optional `AreaShape areaShape = AreaShape::SingleTarget` and `std::int32_t radius = 0` only if Step 12E wants area damage/status immediately.

Smallest safe version:

- Add `damageFormula` and `healAmount`.
- Reuse existing `shieldAmount`.
- Reuse existing `statusEffect`.
- Avoid adding area targeting until a follow-up step.

## AbilityEffect Model

No mandatory model change is needed for Step 12E.

Existing fields already cover:

- `damageFormula` for `DealDamage`
- `healAmount` for `Heal`
- `shieldAmount` for future direct shield support
- `appliesStatusEffect` + `appliedStatusEffect` for status effects
- target and area fields

Implementation should consider making `shieldAmount` executable in `AbilitySystem`, but no data/import mapping should use it until synthetic validations pass.

## StatusEffect Model

No mandatory model change is needed.

Existing `StatusEffect` already covers:

- stat buffs/debuffs
- shield status
- heal-over-time
- damage-over-time
- crowd control metadata

Do not add formula fields to `StatusEffect`; keep direct damage/heal/shield as effect-level behavior, not status behavior.

## ContentManager Parser

Step 12E-1 through Step 12E-4 updated `simulator/src/content/ContentManager.cpp`:

- Parse new `TraitEffectType` string values.
- Continue parsing existing `shieldAmount`, `statusEffect`, and existing fields.
- Parse `healAmount` into `TraitEffect.healAmount`.
- Parse `damageFormula` into `TraitEffect.damageFormula`.

# 3. Executor Changes Needed

## Shared Helpers

`TraitEffectExecutor.cpp`, `AbilitySystem.cpp`, and `ItemSystem.cpp` currently duplicate pieces of status/damage/heal logic. For Step 12E, avoid broad refactors. Add small local helpers inside `TraitEffectExecutor.cpp` first.

Recommended local helpers:

- `selectAllies(GameState&, TeamId)` returns alive allied units in deterministic `state.units()` order.
- `selectEnemies(GameState&, TeamId)` returns alive enemy units in deterministic `state.units()` order.
- `findDefaultTraitSource(...)` reuses the current first alive allied trait-unit source logic.
- `applyTraitDamage(...)` uses the same `DamageSystem::calculateDamageDebug` path as abilities/items.
- `applyTraitHeal(...)` calls `Unit::heal`.
- `applyTraitShield(...)` creates a `StatusEffectType::Shield` status and applies it.

## ApplyStatusToAllies

Executor behavior:

- On matching hook, choose a deterministic applier:
  - `source` if alive and on the active team.
  - Otherwise first alive allied unit with the active trait.
  - Otherwise first alive allied unit.
- Apply `effect.statusEffect` to every alive allied unit.
- Do not require target units to have the trait.

## ApplyStatusToEnemies

Executor behavior:

- Same as existing `ApplyStatusToEnemyTeam`.
- Apply `effect.statusEffect` to every alive enemy unit.
- Keep `ApplyStatusToEnemyTeam` as a compatibility path.

## Shield

Implemented executor behavior:

- Use `effect.shieldAmount`.
- Apply shield to allies by default for trait effects.
- Shield all alive allied units, because the effect name is generic and deterministic.
- Use explicit `durationMs` from `effect.statusEffect.durationMs` if a `statusEffect` is supplied; otherwise use existing `CombatConstants::TraitShieldOnCombatStartDurationMs`.
- Do not consume or alter `StatusEffectType::Shield` semantics elsewhere.

## Heal

Implemented executor behavior:

- Use `effect.healAmount`.
- Heal all alive allied units by default.
- Clamp no lower than zero by doing nothing when `healAmount <= 0`.
- Rely on `Unit::heal` for max HP clamping.

## DealDamage

Implemented executor behavior:

- Use `effect.damageFormula`.
- Deal damage to all alive enemy units for team-wide trait effects.
- Use `DamageSystem::calculateDamageDebug`.
- Apply final damage through `Unit::applyDamage`.
- Do not trigger damage-taken mana, item on-hit, ability on-hit, trait after-damage, kill, death, or low-health callbacks yet. This avoids recursive hook chains until the architecture explicitly supports sourced trait damage events.
- Validate direct HP change and shield absorption only.

# 4. Synthetic Validations Needed

Add validations in `simulator/src/validation/CombatValidation.cpp` before using live import mappings.

Recommended synthetic validation group:

- `TraitEffect: ApplyStatusToAllies`
  - Build a small synthetic `ContentManager`/trait definition or direct `GameState` fixture with two allies and one enemy.
  - Activate trait.
  - Assert both allies receive the status and enemy does not.

- `TraitEffect: ApplyStatusToEnemies`
  - Assert all enemies receive the status and allies do not.
  - Include compatibility check for existing `ApplyStatusToEnemyTeam` if parser/executor paths share behavior.

- `TraitEffect: Shield` - implemented in Step 12E-2.
  - Apply shield to allies.
  - Assert shield status exists with expected value and deterministic duration.
  - Assert dead units do not receive shields.
  - Assert shield absorbs incoming damage through `Unit::applyDamage`.
  - Assert legacy `ShieldOnCombatStart` remains compatible.

- `TraitEffect: Heal` - implemented in Step 12E-3.
  - Damage allied units first, apply heal, assert HP increases but does not exceed max HP.
  - Assert dead units are not healed.
  - Assert enemies are not healed.

- `TraitEffect: DealDamage`
  - Implemented in Step 12E-4.
  - Applies fixed damage to alive enemies.
  - Asserts allies are unchanged.
  - Asserts dead enemies are skipped.
  - Asserts shield absorption works through the existing `Unit::applyDamage` path.
  - Covers physical, magic, and true damage formulas with deterministic zero-resistance fixtures.

- `AbilityEffect: Shield`
  - If Step 12E implements executable `AbilityEffect.shieldAmount`, add a synthetic ability cast validation.
  - If not implemented in 12E, explicitly leave this as a follow-up.

- Deterministic replay validation
  - Run the same synthetic scenario twice with the same seed and assert identical HP/status/log-relevant outcomes.

Validation should use synthetic data only. Do not depend on live champion, item, trait, or ability names.

# 5. Determinism Risks

- Team-wide effects must iterate `state.units()` in stable order; do not use unordered containers for target iteration.
- Damage effects can trigger recursive hooks if they call item/trait/ability callbacks. Keep Step 12E direct and narrow.
- Scheduled effects with equal timestamps must preserve existing event queue ordering.
- Applying duplicate status names can make stack counts order-sensitive; validations should cover max-stack behavior only in a later step.
- Physical crit damage uses RNG through `DamageSystem::rollChance`; Step 12E synthetic `DealDamage` should use true or non-crit magic damage first.
- Floating-point damage formulas should round through the same helper style as `AbilitySystem`/`ItemSystem`.
- Shield as a status must not accidentally stack forever through long durations without clear expiration.
- Source selection fallback must be deterministic when `source == nullptr`.
- Killing units during a loop over `state.units()` must not invalidate references; collect `Unit*` targets first if executor logic becomes more complex.

# 6. Recommended Implementation Order

1. Add enum/parser support only:
   - Extend `TraitEffectType`.
   - Update `parseTraitEffectType`.
   - Parse `TraitEffect.damageFormula` and `healAmount`.
   - Build.

2. Add synthetic validation scaffolding:
   - Create data-independent trait fixtures for each new effect.
   - Add tests as expected failures only while implementing locally, but do not commit failing validation.

3. Implement `ApplyStatusToAllies`: complete in Step 12E-1.

   - Added parser support.
   - Added deterministic executor fan-out to alive allied units.
   - Added synthetic validation.

4. Implement `ApplyStatusToEnemies` compatibility: complete in Step 12E-1.

   - Added parser support.
   - Routed new enum to deterministic alive enemy fan-out.
   - Kept `ApplyStatusToEnemyTeam` working.
   - Added synthetic validation.

5. Implement `DealDamage`: complete in Step 12E-4.

   - Added parser support for `damageFormula`.
   - Used `DamageSystem::calculateDamageDebug` and `Unit::applyDamage`.
   - Added physical, magic, and true formula coverage.
   - Did not wire live importer mappings.

Original planned detail for the first two completed entries:

- `ApplyStatusToAllies`
   - Lowest risk; reuses existing status application.
   - Validate ally/enemy targeting and deterministic replay.

- `ApplyStatusToEnemies`
   - Route new enum to the existing enemy-team behavior.
   - Keep `ApplyStatusToEnemyTeam` working.

8. Rebuild and run:

```powershell
cd D:\TFT-Galaxy-engine\tft-ai\simulator\src
$files = Get-ChildItem -Path . -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
g++ -std=c++20 -O2 @files -I../include -o engine.exe
.\engine.exe --validate
```

# Step 12E Completion Criteria

Step 12E implementation should be considered complete only when:

- All five vocabulary entries parse from normalized JSON.
- Synthetic validations prove each executor behavior.
- Existing validation remains PASS.
- No live-data import starts using the new effects yet.
- No champion, item, trait, or ability names are hardcoded.
- Combat determinism validation remains PASS.

Current status: complete for the five scoped generic trait effects. Importer mappings, item/ability executor expansion, sourced damage-event hook fanout, and complex live mechanics remain follow-up work.
