# TFT Engine AI Guide

## Project Goal

Build a deterministic, data-driven Teamfight Tactics simulator capable of:

* Combat simulation
* Macro decision making
* Monte Carlo rollouts
* Self-play training
* Future reinforcement learning

## Architecture Rules

* Use C++20.
* Keep simulation deterministic.
* Never hardcode champion names.
* Never hardcode traits.
* Never hardcode items.
* Prefer data-driven systems.
* Preserve existing CLI commands.
* Prefer composition over inheritance.
* Do not rewrite unrelated systems.

## Development Workflow

For every task:

1. Find relevant files first.
2. Read only related files.
3. Create a short plan.
4. Implement one small change.
5. Build.
6. Run tests.
7. Fix compile errors.
8. Explain modified files.

## Build

Current build command:

```powershell
<PUT YOUR BUILD COMMAND HERE>
```

## Tests

Current test command:

```powershell
<PUT YOUR TEST COMMAND HERE>
```

## Important Systems

### Core

* GameState
* Unit
* Board
* Position

### Combat

* CombatSystem
* DamageSystem
* ProjectileSystem
* StatusEffectSystem

### Abilities

* AbilitySystem
* AbilityTriggers
* AbilityExecution

### Macro

* ShopSystem
* EconomySystem
* SharedUnitPool
* AIPlayer
* MonteCarloRolloutPlanner
* PositioningOptimizer

## Current Development Stage

Step 11:

* GameState cloning
* Deterministic replay
* State hashing
* Verification framework
* Unit test coverage

Do not start trait engine or item engine work until Step 11 is complete.

## Change Scope

Limit work to:

* One feature
* One bug fix
* One architecture improvement

per iteration.

Never perform large refactors without approval.
## How To Work In This Codebase

For every feature or bug fix, follow this process.

### 1. Search first

Before editing, search for related symbols.

Use these commands:

```powershell
rg "ClassName" simulator
rg "functionName" simulator
rg "enum class" simulator/include
rg "TODO|FIXME|HACK" simulator

For example:

rg "DamageSystem" simulator
rg "GameState" simulator
rg "takeTurn" simulator
rg "reroll" simulator
2. Find all related files

Do not edit only the first file found.

For every symbol you change, search:

rg "SymbolName" simulator

Check:

header .hpp
source .cpp
tests
CLI usage
importer usage
AI usage
3. Read before editing

Before changing code, read:

the .hpp
the matching .cpp
any tests
any caller files

Never guess the API.

4. Make a small plan

Before editing, write:

Files to edit:
- ...
Reason:
- ...
Expected behavior:
- ...
Tests to run:
- ...
5. Edit all relevant places

When changing a feature, update all needed places:

declaration in .hpp
implementation in .cpp
call sites
tests
CLI/debug output if needed
docs if behavior changed
6. Build

After every code change, run:

<PUT_BUILD_COMMAND_HERE>
7. Run tests

After build succeeds, run:

<PUT_TEST_COMMAND_HERE>
8. Fix errors

If build or tests fail:

Read the exact error.
Search the failing symbol.
Fix the smallest related issue.
Build again.
Run tests again.
9. Final report

At the end, report:

Changed files:
- ...

What changed:
- ...

Build result:
- ...

Test result:
- ...

Risks:
- ...

Then tell Continue:

```text
Before editing, always follow AGENT_GUIDE.md section "How To Work In This Codebase".

For any code change:
1. Search related symbols.
2. Read header/source/tests.
3. List files to edit.
4. Edit all relevant files.
5. Build.
6. Run tests.
7. Fix errors.