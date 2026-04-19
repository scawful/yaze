# YAZE Preset Build Root Handoff

**Date**: 2026-04-06
**Status**: Centralized preset build root landed; old build folder cleanup still pending
**Priority**: High

## What Changed

- `CMakePresets.json` now routes configure presets into `build/presets/${presetName}`.
- `CMakePresets-simple.json` follows the same centralized layout.
- `CMakeUserPresets.json.example` now uses `$env{YAZE_BUILD_ROOT}/${presetName}`.
- `.clangd` now points at `build/presets/mac-ai` for the compile database.
- Editor/shell helpers were updated outside this repo to use presets instead of hardcoded `build_ai` and `build_test` paths.

## Verified So Far

- `cmake --list-presets=configure` succeeds.
- `cmake --preset mac-ai` started successfully and created `build/presets/mac-ai`.
- The configure run timed out while fetching/configuring dependencies, but the new preset root is definitely active.

## Important Context

- There are many unrelated in-progress code changes in the worktree. Do not reset or clean the repo blindly.
- Old build directories still exist and were intentionally left in place:
  - `build`
  - `build_ai`
  - `build_codex`
  - `build-ios`
  - `build-ios-sim`
  - `build-wasm`
  - any other ad hoc build folders under the repo root
- A fresh centralized preset directory now exists at `build/presets/mac-ai`.

## Recommended Next Steps

1. Finish configuring the main dev preset:

```bash
cmake --preset mac-ai
```

2. Configure the fast test preset so both primary editor flows exist under the new root:

```bash
cmake --preset mac-test
```

3. Inspect the new centralized tree and confirm expected compile databases/executables:

```bash
ls -la build/presets
ls -la build/presets/mac-ai
ls -la build/presets/mac-test
```

4. Build and smoke-test the main preset:

```bash
cmake --build --preset mac-ai --target yaze -j"$(sysctl -n hw.ncpu)"
cmake --build --preset mac-test --target yaze_test -j"$(sysctl -n hw.ncpu)"
ctest --preset fast --output-on-failure
```

5. Search for remaining hardcoded old build paths in repo scripts/docs and migrate them to preset-aware commands or `build/presets/<preset>`.

6. After confirming the new layout is fully working, archive or delete obsolete build folders carefully instead of using a blanket cleanup.

## Suggested Cleanup Strategy

- Keep `build/presets/` as the canonical multi-preset root.
- Preserve non-preset folders only if they are still required by tooling that cannot use presets.
- Prefer migrating helper scripts to:
  - `cmake --preset <preset>`
  - `cmake --build --preset <preset>`
  - `ctest --preset <preset>`
- If legacy folders are removed, do it one at a time and only after confirming no remaining scripts/docs still depend on them.

## Reference Files

- `CMakePresets.json`
- `CMakePresets-simple.json`
- `CMakeUserPresets.json.example`
- `.clangd`
- `docs/internal/hand-off/HANDOFF_PRESET_BUILD_ROOT_2026-04-06.md`
