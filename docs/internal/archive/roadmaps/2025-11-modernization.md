# Modernization Plan – November 2025

Status: **Draft**  
Owner: Core tooling team  
Scope: `core/asar_wrapper`, CLI/GUI flag system, project persistence, docs

## Context
- The Asar integration is stubbed out (`src/core/asar_wrapper.cc`), yet the GUI, CLI, and docs still advertise a working assembler workflow.
- The GUI binary (`yaze`) still relies on the legacy `util::Flag` parser while the rest of the tooling has moved to Abseil flags, leading to inconsistent UX and duplicated parsing logic.
- Project metadata initialization uses `std::localtime` (`src/core/project.cc`), which is not thread-safe and can race when the agent/automation stack spawns concurrent project creation tasks.
- Public docs promise Dungeon Editor rendering details and “Examples & Recipes,” but those sections are either marked TODO or empty.

## Goals
1. Restore a fully functioning Asar toolchain across GUI/CLI and make sure automated tests cover it.
2. Unify flag parsing by migrating the GUI binary (and remaining utilities) to Abseil flags, then retire `util::flag`.
3. Harden project/workspace persistence by replacing unsafe time handling and improving error propagation during project bootstrap.
4. Close the documentation gaps so the Dungeon Editor guide reflects current rendering, and the `docs/public/examples/` tree provides actual recipes.

## Work Breakdown

### 1. Asar Restoration
- Fix the Asar CMake integration under `ext/asar` and link it back into `yaze_core_lib`.
- Re-implement `AsarWrapper` methods (patch, symbol extraction, validation) and add regression tests in `test/integration/asar_*`.
- Update `z3ed`/GUI code paths to surface actionable errors when the assembler fails.
- Once complete, scrub docs/README claims to ensure they match the restored behavior.

### 2. Flag Standardization
- Replace `DEFINE_FLAG` usage in `src/app/main.cc` with `ABSL_FLAG` + `absl::ParseCommandLine`.
- Delete `util::flag.*` and migrate any lingering consumers (e.g., dev tools) to Abseil.
- Document the shared flag set in a single reference (README + `docs/public/developer/debug-flags.md`).

### 3. Project Persistence Hardening
- Swap `std::localtime` for `absl::Time` or platform-safe helpers and handle failures explicitly.
- Ensure directory creation and file writes bubble errors back to the UI/CLI instead of silently failing.
- Add regression tests that spawn concurrent project creations (possibly via the CLI) to confirm deterministic metadata.

### 4. Documentation Updates
- Finish the Dungeon Editor rendering pipeline description (remove the TODO block) so it reflects the current draw path.
- Populate `docs/public/examples/` with at least a handful of ROM-editing recipes (overworld tile swap, dungeon entrance move, palette tweak, CLI plan/accept flow).
- Add a short “automation journey” that links `README` → gRPC harness (`src/app/service/imgui_test_harness_service.cc`) → `z3ed` agent commands.

## Exit Criteria
- `AsarWrapper` integration tests green on macOS/Linux/Windows runners.
- No binaries depend on `util::flag`; `absl::flags` is the single source of truth.
- Project creation succeeds under parallel stress and metadata timestamps remain valid.
- Public docs no longer contain TODO placeholders or empty directories for the sections listed above.
