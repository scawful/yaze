# YAZE Scripts Index

Status: ACTIVE
Last Reviewed: 2026-09-16

Every script in `scripts/` (top level) is listed below exactly once with a
classification. This file is an index, not a manual: run `<script> --help` for
options, and follow the linked doc when a script has one.

## Classification

| Class | Meaning |
|-------|---------|
| **canonical** | The supported entry point for its job. Keep it working; update it when behaviour changes. |
| **compatibility** | Still functional and still referenced, but a canonical script covers the same ground. Prefer the canonical one for new work. |
| **deprecated** | Unreferenced, superseded, or targeting paths/tools that no longer exist. Do not build on these; they are removal candidates. |

Before deleting anything in the deprecated column, re-check references
(`rg -n "scripts/<name>" .`) and history (`git log -- scripts/<name>`) — several
entries are only reachable from archived docs.

## Binary wrappers

Each wrapper picks the newest matching build (`build_ai` first). Override with
the listed environment variable.

| Script | Class | Purpose | Override |
|--------|-------|---------|----------|
| `yaze` | canonical | Launch the yaze GUI; supports `--which` and `--doctor` | `YAZE_BIN` |
| `z3ed` | canonical | Launch the z3ed CLI; supports `--which` and `--doctor` | `Z3ED_BIN` |
| `z3disasm` | canonical | Launch z3disasm from a z3dk checkout; supports `--which` and `--doctor` | `Z3DISASM_BIN`, `Z3DK_ROOT` |
| `yaze_test` | compatibility | Launch a test binary. Only works with the override set: the build produces per-suite binaries (`yaze_test_unit`, `yaze_test_integration`, `yaze_test_gui`, ...), not a single `yaze_test` | `YAZE_TEST_BIN` |

## Build and environment

| Script | Class | Purpose |
|--------|-------|---------|
| `dev-setup.sh` | canonical | One-command setup for a new developer machine |
| `verify-build-environment.sh` / `.ps1` | canonical | Per-machine toolchain diagnostics; `--fix` / `-FixIssues` repairs common problems |
| `setup-vcpkg-windows.ps1` | canonical | Bootstrap and populate vcpkg on Windows |
| `agent_build.sh` | canonical | Build AI-enabled targets for agent workflows (`YAZE_BUILD_DIR` overrides the build dir) |
| `build_cleaner.py` | canonical | Maintain CMake source lists and self-header includes; also a `build_cleaner` CMake target |
| `audit_test_registration.py` | canonical | Require every `*_test.cc` source to be registered or explicitly excluded with a reason |
| `fetch_usdasm.sh` | canonical | Fetch the usdasm disassembly on demand (`USDASM_DIR`, `USDASM_REPO_URL`) |
| `requirements.txt` | canonical | Python dependencies for the scripts in this directory |
| `gemini_build.sh` | compatibility | Thin wrapper that forwards to `agent_build.sh`; kept for older docs |
| `dev_start_yaze.sh` | compatibility | Build with gRPC and launch yaze; `agent_build.sh` plus `scripts/yaze` does the same |
| `test-linux-build.sh` | deprecated | Local Docker rehearsal of the Linux CI build. Unreferenced; Linux coverage now comes from CI and `scripts/cloud/bootstrap.sh` |

## Apple platforms

| Script | Class | Purpose |
|--------|-------|---------|
| `build-ios.sh` | canonical | Build iOS static libs via CMake and generate the Xcode project with XcodeGen |
| `xcodebuild-ios.sh` | canonical | Build / archive / export / deploy the iOS app (`build`, `ipa`, `deploy`) |
| `signing.env.example` | canonical | Template for `scripts/signing.env` (gitignored), auto-sourced by `xcodebuild-ios.sh` |
| `create-macos-bundle.sh` | deprecated | Standalone `.app` bundler. Unreferenced; macOS packaging lives in `scripts/release/` and the release workflow |

See [docs/public/build/quick-reference.md](../docs/public/build/quick-reference.md)
for signing and device-deploy details, and `scripts/dev/ios-ipad-workflow.sh` for
multi-iPad deployment.

## Web and WASM

| Script | Class | Purpose |
|--------|-------|---------|
| `build-wasm.sh` | canonical | Build the WASM app (`debug`/`release`/`ai`/`smoke`); used by CI |
| `package-wasm-assets.sh` | canonical | Stage assets into the WASM dist directory; called by `build-wasm.sh` and CI |
| `serve-wasm.sh` | canonical | Local dev server with the COOP/COEP headers the build needs |
| `build_z3ed_wasm.sh` | compatibility | CLI-only z3ed WASM build; `build-wasm.sh` is the maintained path |

## Install and release

| Script | Class | Purpose |
|--------|-------|---------|
| `install-nightly.sh` | canonical | Install a nightly build from the remote repository |
| `install-nightly-local.sh` | canonical | Install a nightly built from the local checkout (`YAZE_NIGHTLY_*` overrides) |
| `extract_changelog.py` | deprecated | Reads `docs/H1-changelog.md`, which no longer exists. Use `scripts/release/extract-release-notes.sh` |
| `merge_feature.sh` | deprecated | Opinionated feature → develop → master merge helper. Unreferenced; see [git-workflow.md](../docs/public/developer/git-workflow.md) |

## Lint, hooks, and quality gates

| Script | Class | Purpose |
|--------|-------|---------|
| `install-git-hooks.sh` | canonical | Install/uninstall the pre-commit and pre-push hooks |
| `pre-commit.sh` | canonical | Pre-commit validation (installed by `install-git-hooks.sh`) |
| `pre-push.sh` | canonical | Pre-push validation, fast by default with change-aware UI regression coverage |
| `lint.sh` | canonical | clang-format and clang-tidy with the project configuration |
| `quality_check.sh` | compatibility | Wraps `lint.sh` with extra reporting; referenced from architecture docs |
| `pre-push-test.sh` / `.ps1` | compatibility | Broader pre-push sweep including symbol checks; see [pre-push-checklist.md](../docs/internal/testing/pre-push-checklist.md) |
| `find-unsafe-array-access.sh` | deprecated | One-off static scan from the 2025 WASM bounds-checking audit, which is closed |

Formatting entry points share `.clang-format-version`, discover tools through
`scripts/lib/clang_tools.sh`, and pass `--style=file`. Use
`scripts/quality_check.sh` for advisory whole-tree reporting or
`scripts/quality_check.sh --gate` for a failing quality gate. Refresh the
compile database with `scripts/dev/update_compile_commands.sh <preset>`.

## Symbol conflict detection

Full documentation: [symbol-conflict-detection.md](../docs/internal/testing/symbol-conflict-detection.md).
`extract-symbols.sh` and `check-duplicate-symbols.sh` are invoked by
`.githooks/pre-commit`.

| Script | Class | Purpose |
|--------|-------|---------|
| `extract-symbols.sh` | canonical | Build a JSON symbol database from compiled object files |
| `check-duplicate-symbols.sh` | canonical | Report ODR conflicts from that database (exit 1 on conflict) |
| `verify-symbols.sh` | canonical | Library-level ODR scan used by pre-push flows (`BUILD_DIR`, `VERBOSE`, `SHOW_ALL`) |
| `test-symbol-detection.sh` | canonical | Integration test for the symbol detection scripts |

## CMake and configuration validation

| Script | Class | Purpose |
|--------|-------|---------|
| `validate-cmake-config.cmake` | canonical | Validate an already-configured build dir (`cmake -P scripts/validate-cmake-config.cmake build`) |
| `validate-cmake-config.sh` | canonical | Validate a *set of feature flags* for internal consistency before configuring |
| `check-include-paths.sh` | canonical | Check `compile_commands.json` for missing include paths (`jq` recommended) |
| `test-cmake-presets.sh` | canonical | Confirm every preset for a platform configures successfully |
| `test-config-matrix.sh` | canonical | Exercise feature-flag combinations |
| `visualize-deps.py` | canonical | Emit GraphViz/Mermaid/text dependency graphs and detect cycles |

## Test execution

Test strategy and labels live in [test/README.md](../test/README.md) and
[docs/internal/testing/](../docs/internal/testing/README.md).

| Script | Class | Purpose |
|--------|-------|---------|
| `test_fast.sh` | canonical | High-signal subset of stable unit + integration tests for quick iteration |
| `test_runner.py` | canonical | Sharded parallel test execution; driven by `cmake/TestInfrastructure.cmake` |
| `aggregate_test_results.py` | canonical | Merge parallel test results into one report; used by CI |
| `oracle_smoke.sh` | canonical | Oracle-of-Secrets regression smoke checks, JSON summary on stdout |
| `rom_safety_preflight.sh` | canonical | ROM write-safety preflight; see [rom-safety-guardrails.md](../docs/internal/agents/rom-safety-guardrails.md) |
| `validate-yaze.sh` | canonical | Compare a yaze-written ROM against a golden ROM with ZScreamCLI |
| `agent_test_suite.sh` | compatibility | Ollama/Gemini provider and tool-calling smoke tests |
| `run_overworld_tests.sh` | compatibility | Overworld-focused test runner; `ctest` labels cover the same suites |
| `test_dungeon_loading.sh` | deprecated | Ad-hoc dungeon room load check, unreferenced |
| `test_gui_tools.sh` | deprecated | Ad-hoc GUI automation check, unreferenced |
| `test_ai_features.sh` / `.ps1` | deprecated | AI feature smoke, unreferenced and overlapping `agent_test_suite.sh` |
| `demo_agent_gui.sh` | deprecated | Demo recording helper, unreferenced |
| `start_collab_server.sh` | deprecated | Launcher for the external `yaze-server` repository, unreferenced |

## ROM data tooling (Python)

These shell out to `scripts/z3ed`. All paths come from flags or environment
variables; there are no machine-specific defaults.

| Script | Class | Purpose |
|--------|-------|---------|
| `analyze_room.py` | canonical | Parse dungeon room objects and layer assignment; see [README_analyze_room.md](README_analyze_room.md) |
| `dump_object_handlers.py` | canonical | Dump the dungeon object handler tables; feeds [alttp-object-handlers.md](../docs/internal/zelda3/alttp-object-handlers.md) |
| `location_mapper.py` | canonical | Generate docs for dungeons/shrines/caves/houses/shops across ROM profiles (`ALTTP_ROM`, `Z3ED_BIN`) |
| `discover_rooms.py` | canonical | Auto-discover room layouts from an entrance ID and update profile configs |

`dungeon_overview.py` was removed on 2026-09-16: it was an Oracle-only,
single-dungeon predecessor of `location_mapper.py` with hardcoded home-directory
paths, and its Goron Mines data already lives in
`scripts/profiles/oracle_of_secrets.py`.

## Subdirectories

| Directory | Contents |
|-----------|----------|
| `agents/` | Agent coordination (`coord`), protocol audits, CI helpers, reference checkers, and their unit tests — see [agents/README.md](agents/README.md) |
| `ai/` | Model evaluation suite (`run-model-eval.sh`, `eval-runner.py`, `compare-models.py`, `eval-tasks.yaml`) plus agent research tools (navigator, map compiler, profiler, asm tuner) |
| `cloud/` | Cloud agent bootstrap and its tests |
| `dev/` | Local developer workflows: `local-workflow.sh` (canonical build + deploy), iPad deployment, guardrail and smoke loops |
| `i18n/` | Translation catalog extraction and validation |
| `package/` | Packaging entry point (`release.sh`) |
| `profiles/` | ROM profile definitions consumed by `location_mapper.py` and `discover_rooms.py` |
| `release/` | Release validation: DMG/archive/package smoke checks, release-note extraction |

## Conventions

- **`build_cleaner.py` markers.** A `set()` block is auto-maintained when a
  comment within three lines above it contains `auto-maintain` (for example
  `# This list is auto-maintained by scripts/build_cleaner.py`). A source file is
  skipped entirely when it contains the token `build_cleaner:ignore` near the top.
  `.gitignore` support needs `pathspec` (`pip3 install -r scripts/requirements.txt`).
- **No host-specific paths.** Scripts must take paths from a CLI flag or an
  environment variable with a repository-relative default. This is enforced by
  `scripts/agents/tests/test_tool_path_defaults.py`
  (`python3 -m unittest discover -s scripts/agents/tests`).

## Known gaps

Each of these needs its own change; none is fixed by this index.

- `cmake/TestInfrastructure.cmake` references `scripts/smart_test_selector.py`,
  which does not exist. The target that uses it fails if invoked.
- Several public docs still describe a single `yaze_test` binary with
  `--unit` / `--integration` / `--e2e` flags (`docs/public/developer/testing-guide.md`,
  `docs/public/examples/README.md`, `test/README.md`, `test/e2e/README.md`).
  The build produces per-suite binaries run through `ctest` labels.
- Homebrew formulae under `homebrew/Formula/` are out of date; tracked separately
  in [homebrew/README.md](../homebrew/README.md).
