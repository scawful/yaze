# YAZE Test Binary UX Proposals (yaze_test / ctest Integration)

Status: IN_PROGRESS  
Owner: ai-infra-architect  
Created: 2025-12-01  
Last Reviewed: 2025-12-01  
Next Review: 2025-12-08  
Board: docs/internal/agents/coordination-board.md (2025-12-01 ai-infra-architect – z3ed CLI UX/TUI Improvement Proposals)

## Summary
- Make the test binaries and ctest entry points friendlier for humans and agents: clearer filters, artifacts, and machine-readable outputs.
- Provide a first-class manifest of suites/labels/requirements and a consistent way to supply ROM/AI/headless settings.
- Reduce friction when iterating locally (fast presets, targeted subsets) and when collecting results for automation/CI.

## Observations
- The unified `yaze_test` binary emits gtest text only; ctest wraps it but lacks machine-readable summaries unless parsed. Agents currently scrape stdout.
- Suite/label requirements (ROM path, AI runtime, headless display) are implicit; misconfiguration silently skips or hard-fails without actionable guidance.
- Filter UX is split: gtest filters vs ctest `-L/-R`; no single recommended entry that also records artifacts/logs for failures.
- Artifacts (logs, screenshots, recordings) from GUI/agent tests are not consistently emitted or linked from results.
- Preset coupling is manual; users must remember which CMake preset enables ROM/AI/headless options. No quick “fast subset” toggle inside the binary.

## Improvement Proposals
- **Manifest & discovery**: Generate a JSON manifest (per build) listing tests, labels, requirements (ROM path, AI runtime), and expected artifacts. Expose via `yaze_test --export-manifest <path>` and `ctest -T mem`-style option. Agents can load it instead of scraping.
- **Structured output**: Add `--output-format {text,json,junit}` to `yaze_test` to emit summaries (pass/fail, duration, seed, artifacts) in one file; default to text for humans, JSON for automation. Wire ctest to collect the JSON and place it in a predictable directory.
- **Requirements gating**: On startup, detect missing ROM/AI/headless support and fail fast with actionable messages and suggested CMake flags/env (e.g., `YAZE_ENABLE_ROM_TESTS`, `YAZE_TEST_ROM_VANILLA`). Offer a `--mock-rom-ok` mode to downgrade ROM tests when a mock is acceptable.
- **Filters & subsets**: Provide a unified front-end flag set (`--label stable|gui|rom_dependent|experimental`, `--gtest_filter`, `--list`) that internally routes to gtest/labels so humans/agents don’t guess. Add `--shard <N/M>` for parallel runs.
- **Artifacts & logs**: Standardize artifact output location (`build/artifacts/tests/<run-id>/`) and name failing-test logs/screenshots accordingly. Emit paths in JSON output. Ensure GUI/agent recordings are captured when labels include `gui` or `experimental`.
- **Preset hints**: Print which CMake preset was used to build the binary and whether ROM/AI options are compiled in. Add `--recommend-preset` helper to suggest `mac-test/mac-ai/mac-dev` based on requested labels.
- **Headless helpers**: Add `--headless-check` to validate SDL/display availability and exit gracefully with instructions to use headless mode; integrate into ctest label defaults.
- **Exit codes**: Ensure non-zero exit codes for any test failure and distinct codes for configuration failures vs runtime failures to simplify automation.

## Exit Criteria (for this scope)
- `yaze_test` (and/or a small wrapper) can emit JSON/JUnit summaries and a manifest without stdout scraping.
- Clear, actionable errors when requirements (ROM/AI/display) are missing, with suggested flags/presets.
- Artifacts for failing GUI/agent tests are written to predictable paths and referenced in structured output.
- Unified filter/label interface documented and consistent with ctest usage.
