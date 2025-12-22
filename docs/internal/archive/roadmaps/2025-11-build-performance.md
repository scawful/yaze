# Build Performance & Agent-Friendly Tooling (November 2025)

Status: **Draft**  
Owner: CODEX (open to CLAUDE/GEMINI participation)

## Goals
- Reduce incremental build times on all platforms by tightening target boundaries, isolating optional
  components, and providing cache-friendly presets.
- Allow long-running or optional tasks (e.g., asset generation, documentation, verification scripts)
  to run asynchronously or on-demand so agents don’t block on them.
- Provide monitoring/metrics hooks so agents and humans can see where build time is spent.
- Organize helper scripts (build, verification, CI triggers) so agents can call them predictably.

## Plan Overview

### 1. Library Scoping & Optional Targets
1. Audit `src/CMakeLists.txt` and per-module cmake files for broad `add_subdirectory` usage.
   - Identify libraries that can be marked `EXCLUDE_FROM_ALL` and only built when needed (e.g.,
     optional tools, emulator targets).
   - Leverage `YAZE_MINIMAL_BUILD`, `YAZE_BUILD_Z3ED`, etc., but ensure presets reflect the smallest
     viable dependency tree.
2. Split heavy modules (e.g., `app/editor`, `app/emu`) into more granular targets if they are
   frequently touched independently.
3. Add caching hints (ccache, sccache) in the build scripts/presets for all platforms.

### 2. Background / Async Tasks
1. Move long-running scripts (asset bundling, doc generation, lints) into optional targets invoked by
   a convenience meta-target (e.g., `yaze_extras`) so normal builds stay lean.
2. Provide `scripts/run-background-tasks.sh` that uses `nohup`/`start` to launch doc builds, GH
   workflow dispatch, or other heavy processes asynchronously; log their status for monitoring.
3. Ensure CI workflows skip optional tasks unless explicitly requested (e.g., via workflow inputs).

### 3. Monitoring & Metrics
1. Add a lightweight timing report to `scripts/verify-build-environment.*` or a new
   `scripts/measure-build.sh` that runs `cmake --build` with `--trace-expand`/`ninja -d stats` and
   reports hotspots.
2. Integrate a summary step in CI (maybe a bash step) that records build duration per preset and
   uploads as an artifact or comment.
3. Document how agents should capture metrics when running builds (e.g., use `time` wrappers, log
   output to `logs/build_<preset>.log`).

### 4. Agent-Friendly Script Organization
1. Gather recurring helper commands into `scripts/agents/`:
   - `run-gh-workflow.sh` (wrapper around `gh workflow run`)
   - `smoke-build.sh <preset>` (configures & builds a preset in a dedicated directory, records time)
   - `run-tests.sh <preset> <labels>` (standardizes test selections)
2. Provide short README in `scripts/agents/` explaining parameters, sample usage, and expected output
   files for logging back to the coordination board.
3. Update `AGENTS.md` to reference these scripts so every persona knows the canonical tooling.

### 5. Deliverables / Tracking
- Update CMake targets/presets to reflect modular build improvements.
- New scripts under `scripts/agents/` + documentation.
- Monitoring notes in CI (maybe via job summary) and local scripts.
- Coordination board entries per major milestone (library scoping, background tooling, metrics,
  script rollout).

## Dependencies / Risks
- Coordinate with CLAUDE_AIINF when touching presets or build scripts—they may modify the same files
  for AI workflow fixes.
- When changing CMake targets, ensure existing presets still configure successfully (run verification
  scripts + smoke builds on mac/linux/win).
- Adding background tasks/scripts should not introduce new global dependencies; use POSIX Bash and
  PowerShell equivalents where required.
## Windows Stability Focus (New)
- **Tooling verification**: expand `scripts/verify-build-environment.ps1` to check for Visual Studio workload, Ninja, and vcpkg caches so Windows builds fail fast when the environment is incomplete.
- **CMake structure**: ensure optional components (HTTP API, emulator, CLI helpers) are behind explicit options and do not affect default Windows presets; verify each target links the right runtime/library deps even when `YAZE_ENABLE_*` flags change.
- **Preset validation**: add Windows smoke builds (Ninja + VS) to the helper scripts/CI so we can trigger focused runs when changes land.
