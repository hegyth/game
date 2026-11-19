# VELRATH Agent Guidelines

VELRATH is a competitive third-person Action RPG for Windows PC / Steam, built with Unreal Engine 5.7.4 and C++. It uses Gameplay Ability System, Enhanced Input, and authoritative multiplayer.

## Working rules

- Before changing anything, inspect only files relevant to the task. Do not repeat a full project audit for every task.
- Do not inspect `Binaries/`, `Intermediate/`, `Saved/`, or `DerivedDataCache/` without a specific reason.
- Keep changes within the requested scope. Do not alter unrelated gameplay mechanics or create premature architectural abstractions.
- Prefer established Unreal Engine 5.7.4 APIs and GAS patterns.
- All consequential combat decisions must be server-authoritative. Do not claim multiplayer behavior was verified unless multiple game instances were actually run.
- Do not modify binary Unreal assets directly.
- Do not delete user files.
- Do not run Git commit, push, reset, or rebase without explicit permission.
- Do not update Unreal Engine or the toolchain independently.
- Dedicated Server builds are unavailable with the current Launcher engine. Use listen server or PIE for local multiplayer testing when relevant.

## Validation

- Use `Scripts/Build.ps1` and `Scripts/Test.ps1` for validation.
- After C++ changes, run the Editor build and relevant tests.
- If a required test cannot be run, state that explicitly and explain why.

## Task report

After each task, briefly report:

- what changed;
- files affected;
- what was verified;
- what was not verified;
- remaining issues.
