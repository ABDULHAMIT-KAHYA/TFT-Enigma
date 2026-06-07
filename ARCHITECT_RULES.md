# ARCHITECT_RULES.md

## TFT AI Simulator Architecture Rules

This document defines the permanent architecture rules for the TFT AI simulator.

The goal is to build a deterministic, data-driven TFT engine that can support simulation, Monte Carlo planning, self-play, and future learning AI without hardcoded set-specific logic.

---

## 1. Core Principle

The engine must separate:

```txt
Stable TFT rules
from
Set-specific game content
from
AI decision logic
```

Stable TFT rules may be hardcoded.

Examples:

```txt
shop size
bench size
max level
XP rules
interest rules
combat tick rate
board size
legal action types
```

Set-specific content must be imported from data.

Examples:

```txt
champion names
champion stats
trait names
trait breakpoints
trait effects
item names
ability names
ability values
```

AI logic must reason from state, not from champion names.

---

## 2. No Hardcoded Set Content

Never hardcode:

```txt
champion names
trait names
item names
ability names
comp names
set-specific keywords
```

Bad:

```cpp
if (champion.name == "Aatrox")
```

Good:

```cpp
if (champion.isPlayable)
```

Bad:

```cpp
if (trait.name == "Bastion")
```

Good:

```cpp
if (trait.role == TraitRole::Defensive)
```

If classification is needed, importer must write metadata into JSON.

---

## 3. Data-Driven Content

Imported data should drive:

```txt
champions
abilities
traits
items
playability
tags
costs
stats
breakpoints
effect variables
```

Runtime systems should read:

```txt
ChampionDefinition
AbilityDefinition
TraitDefinition
ItemDefinition
```

The simulator should work after importing a new TFT set without C++ logic changes.

---

## 4. Determinism Rule

All simulation must be deterministic.

Same seed + same input must produce same result.

Required:

```txt
seeded RNG only
no system-time randomness
no unordered random iteration dependency
no hidden global randomness
deterministic sorting for tie-breaks
```

Validation must include deterministic replay tests.

---

## 5. No Live Game Automation

This project is a simulator and research AI.

It must not:

```txt
read live game memory
control a live TFT client
automate real matches
use screen scraping for unfair advantage
bypass Riot systems
```

Allowed:

```txt
offline simulation
data import
self-play
Monte Carlo planning
research experiments
replay-style analysis
```

---

## 6. Module Boundaries

Project structure:

```txt
simulator/include/
  core/
  combat/
  content/
  macro/
  ai/
  validation/
  import/
  constants/

simulator/src/
  core/
  combat/
  content/
  macro/
  ai/
  validation/
  import/
```

### core

Owns basic engine types:

```txt
Unit
Board
Position
TeamId
Logger
Random
GameState
```

### combat

Owns combat simulation:

```txt
Combat
DamageSystem
ProjectileSystem
AbilitySystem
TargetSelector
SpellResolver
StatusEffect
TraitSystem
ItemSystem
StatSystem
MovementSystem
```

### content

Owns loaded data:

```txt
ContentManager
ChampionDefinition
AbilityDefinition
TraitDefinition
ItemDefinition
ChampionFilter
```

### macro

Owns TFT round/economy systems:

```txt
PlayerState
ShopSystem
SharedUnitPool
RoundSystem
RoundSchedule
EconomySystem
LegalActionGenerator
MacroExecutor
MacroSimulation
```

### ai

Owns decision making:

```txt
SimpleMacroAI
MacroActionScorer
BoardStrengthEvaluator
TraitSynergyEvaluator
PositioningOptimizer
ScoutSystem
RolloutPlanner
RolloutPolicy
FutureStateEvaluator
StateCloner
BranchPruner
```

### validation

Owns tests only.

Validation may use synthetic dummy data.

Runtime may not depend on validation.

---

## 7. Constants Rule

No unexplained magic numbers in runtime logic.

Use:

```txt
GameConstants.hpp
CombatConstants.hpp
MacroConstants.hpp
AIConstants.hpp
ValidationConstants.hpp
```

Allowed inline literals:

```txt
0
1
-1
true
false
nullptr
simple loop increments
```

Everything else should be named.

Bad:

```cpp
if (timeMs > 60000)
```

Good:

```cpp
if (timeMs > CombatConstants::MaxCombatDurationMs)
```

---

## 8. Validation Rule

Every major feature must include validation.

Required validation categories:

```txt
combat correctness
deterministic replay
macro economy
shop legality
shared pool behavior
trait activation
item behavior
AI action legality
rollout determinism
clone isolation
no hardcoded names
```

No step is complete until:

```powershell
g++ -std=c++17 -O2 @files -I../include -o engine.exe
.\engine.exe --validate
```

passes.

---

## 9. Combat Rules

Combat must be:

```txt
deterministic
tick-based
bounded by timeout
event-scheduled
fully validated
```

Combat must support:

```txt
attack windup
backswing
projectile delay
cast windup
cast recovery
mana lock
cast interruption
target retention
retarget delay
CC gating
status effects
traits
items
```

Combat must never silently hang.

If combat exceeds max duration:

```txt
force draw
log timeout
validation catches it
```

---

## 10. Macro Rules

Macro gameplay must be legal and sane.

Systems must enforce:

```txt
shop odds
shared unit pool
buy/sell costs
reroll cost
XP buying
level cap
bench cap
board cap
upgrade rules
interest
streaks
player damage
round schedule
```

AI must not create action churn.

Blocked patterns:

```txt
buy -> sell same unit same turn
deploy -> sell same unit
move board -> bench -> board loop
repeated reposition spam
selling board with major strength loss
endless reroll loops
```

Use action budgets:

```txt
MaxActionsPerTurn
MaxTransactionsPerTurn
MaxSellsPerTurn
MaxBoardSellsPerTurn
MaxRerollsPerTurn
MaxXpBuysPerTurn
MaxRepositionActionsPerTurn
```

---

## 11. AI Decision Rule

AI should not rely on scripted TFT guides.

Bad:

```txt
always roll at 3-2
always level at 4-1
always play specific comp
```

Good:

```txt
generate legal actions
score current state
simulate future states
compare expected value
choose best action
```

The AI may use generic pressure curves:

```txt
stage pressure
HP urgency
expected level by stage
board strength deficit
economy threshold
upgrade potential
contest pressure
```

But it must not use set-specific champion or comp scripts.

---

## 12. Monte Carlo Rule

Monte Carlo must choose actions by simulation.

It should compare:

```txt
roll now
save gold
buy XP
level now
buy unit
sell unit
reposition
pivot
```

Using:

```txt
expected future value
HP preservation
board strength
economy
upgrade probability
trait direction
opponent strength
death risk
top4 probability
placement EV
```

Monte Carlo must:

```txt
clone state
simulate future rounds
not mutate live state
use seeded RNG
cache duplicate states
prune weak branches
terminate safely
produce debug output
```

---

## 13. Opponent-Aware Rollout Rule

Rollouts should not evaluate only the player.

Future rollout EV should include:

```txt
my future board
enemy future board
relative strength
expected combat result
HP delta
survival chance
top4 chance
```

Enemy snapshots may be approximated, but must be deterministic.

---

## 14. Self-Play Rule

Self-play starts only after:

```txt
validation passes
normal games finish
macro turns are sane
Monte Carlo is deterministic
state cloning is reliable
opponent-aware rollouts work
100+ games can run without crash
```

Self-play should store:

```txt
state features
legal actions
chosen action
rollout EV
combat result
round result
final placement
seed
```

Learning AI should train from:

```txt
state -> action -> outcome
```

Not from hardcoded strategy.

---

## 15. File Safety Rule

Do not place generated data inside `simulator/src`.

Data root must be:

```txt
tft-ai/data/
```

Not:

```txt
tft-ai/simulator/src/data/
```

Importer must print data root before writing.

---

## 16. Build Rule

Preferred PowerShell build:

```powershell
cd D:\TFT-Galaxy-engine\tft-ai\simulator\src

$files = Get-ChildItem -Path . -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }

g++ -std=c++17 -O2 @files -I../include -o engine.exe
```

Then:

```powershell
.\engine.exe --validate
.\engine.exe
```

---

## 17. Debug Rule

Important systems must have debug output.

Useful debug categories:

```txt
BoardScore
TraitScore
CompDirection
DecisionMode
ActionBudget
Rollout EV
Top4 probability
Death risk
Combat timeline
Validation progress
```

Debug output must help identify why the AI made a decision.

---

## 18. Refactor Rule

Refactors must not change behavior unless explicitly stated.

After refactor:

```txt
build must pass
validation must pass
normal run must complete
deterministic replay must stay stable
```

---

## 19. Performance Rule

Avoid exponential explosion.

Use:

```txt
top-K actions
branch pruning
state hashing
rollout cache
depth limit
early termination
action budgets
```

Never allow Monte Carlo to simulate unlimited action trees.

---

## 20. Final Rule

If a behavior can be learned by simulation, do not hardcode it.

Hardcode only:

```txt
stable rules
safety limits
generic constants
validation-only dummy data
```

Learn or infer:

```txt
when to roll
when to level
when to greed
when to stabilize
when to pivot
which board is stronger
which future line has better EV
```

Goal:

```txt
Build a deterministic TFT AI engine that can eventually discover strong strategies through simulation and self-play.
```
