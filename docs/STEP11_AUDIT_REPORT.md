# Root Cause

The remaining Step 11 validation failure was `StrategicAI FAIL: high gold preserves economy`.

The failing validation is in `simulator/src/validation/CombatValidation.cpp` and creates a calm 50-gold state with only `RerollShop` and `EndTurn` legal actions. It expects `EndTurn` to score at least as high as `RerollShop`.

`MacroActionScorer` penalized `EndTurn` for being under-leveled, but `RerollShop` did not receive a comparable low-pressure economy penalty. In a no-pressure max-interest state, the AI could prefer spending gold on rerolling over preserving economy.

# Files Changed

- `simulator/src/ai/MacroActionScorer.cpp`
  - Added a max-interest preservation bonus to `EndTurn` when pressure is low.
- `simulator/include/constants/AIConstants.hpp`
  - Added `EndTurnMaxInterestPreservationBonus`.

# Build Command and Result

Command run from `D:\TFT-Galaxy-engine\tft-ai\simulator\src`:

```powershell
$files = Get-ChildItem -Path . -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }; g++ -std=c++20 -O2 @files -I../include -o engine.exe
```

Result: PASS.

# Validation Command and Result

Command run from `D:\TFT-Galaxy-engine\tft-ai\simulator\src`:

```powershell
.\engine.exe --validate
```

Result: PASS.

The previously failing check now passes:

```text
PASS: StrategicAI PASS: high gold preserves economy
```

# Remaining Failures

None in `.\engine.exe --validate`.

# Is Step 11 Complete?

Yes for the current validation gate. The simulator builds with the C++20 fallback command and `.\engine.exe --validate` passes, including deterministic replay, cloned state isolation, rollout determinism, branch pruning determinism, debug output determinism, macro validation, strategic AI validation, and hardcoded-name validation.

# Next 5 Engineering Tasks

1. Replace the blocked PowerShell script workflow with a reliable checked-in build command or generated CMake target.
2. Add a standalone unit-test harness so validation checks can be run and reported independently.
3. Add deterministic replay fixtures that persist expected seeds, actions, and outcomes outside the monolithic validation runner.
4. Run and verify live TFT data import end to end with `--import-live-tft`.
5. Expand macro AI scenario coverage for roll/level/economy choices across stage, HP, board-cap, and upgrade-potential states.
