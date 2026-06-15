# TFT Engine Rules

You are helping develop a high-performance C++ TFT simulator.

## Core Rules
- Use C++20.
- Keep simulation deterministic.
- Do not remove existing working systems.
- Do not rewrite unrelated files.
- Prefer small, testable changes.
- Add or update unit tests for every feature.
- Run build after every meaningful change.
- Fix compiler errors before stopping.
- Explain changed files briefly.
- Never hardcode champion names.
- Keep game data data-driven from imported JSON.
- Preserve existing CLI commands.
- Prefer clean architecture over quick hacks.

## Workflow
1. Inspect project structure.
2. Find current build/test command.
3. Propose a short plan.
4. Implement one small feature.
5. Build.
6. Run tests.
7. Fix errors.
8. Repeat.