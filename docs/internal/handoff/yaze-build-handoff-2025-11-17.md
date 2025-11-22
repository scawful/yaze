# YAZE Build & AI Modularity – Handoff (2025‑11‑17)

## Snapshot
- **Scope:** Ongoing work to modularize AI features (gRPC + Protobuf), migrate third‑party code into `ext/`, and stabilize CI across macOS, Linux, and Windows.
- **Progress:** macOS `ci-macos` now builds all primary targets (`yaze`, `yaze_emu`, `z3ed`, `yaze_test_*`) with AI gating and lightweight Ollama model tests. Documentation and scripts reflect the new `ext/` layout and AI presets. Flag parsing was rewritten to avoid exceptions for MSVC/`clang-cl`.
- **Blockers:** Windows and Linux CI jobs are still failing due to missing Abseil headers in `yaze_util` and (likely) the same include propagation issue affecting other util sources. Duplicate library warnings remain but are non‑blocking.

## Key Changes Since Last Handoff
1. **AI Feature Gating**
   - New CMake options (`YAZE_ENABLE_AI_RUNTIME`, `YAZE_ENABLE_REMOTE_AUTOMATION`, `YAZE_BUILD_AGENT_UI`, `YAZE_ENABLE_AGENT_CLI`, `YAZE_BUILD_Z3ED`) control exactly which AI components build on each platform.
   - `gemini`/`ollama` services now compile conditionally with stub fallbacks when AI runtime is disabled.
   - `test/CMakeLists.txt` only includes `integration/ai/*` suites when `YAZE_ENABLE_AI_RUNTIME` is ON to keep non‑AI builds green.

2. **External Dependencies**
   - SDL, ImGui, ImGui Test Engine, nlohmann/json, httplib, nativefiledialog, etc. now live under `ext/` with updated CMake includes.
   - `scripts/agent_test_suite.sh` and CI workflows pass `OLLAMA_MODEL=qwen2.5-coder:0.5b` and bootstrap Ollama/Ninja/NASM on Windows.

3. **Automated Testing**
   - GitHub Actions `ci.yml` now contains `ci-windows-ai` and `z3ed-agent-test` (macOS) jobs that exercise gRPC + AI paths.
   - `yaze_test` suites run via `gtest_discover_tests`; GUI/experimental suites are tagged `gui;experimental` to allow selective execution.

## Outstanding Issues & Next Steps

### 1. Windows CI (Blocking)
- **Symptom:** `clang-cl` fails compiling `src/util/{hex,log,platform_paths}.cc` with `absl/...` headers not found.
- **Current mitigation attempts:**
  - `yaze_util` now links against `absl::strings`, `absl::str_format`, `absl::status`, `absl::statusor`, etc.
  - Added a hard‑coded include path (`${CMAKE_BINARY_DIR}/_deps/grpc-src/third_party/abseil-cpp`) when `YAZE_ENABLE_GRPC` is ON.
- **Suspect:** On Windows (with multi-config Ninja + ExternalProject), Abseil headers may live under `_deps/grpc-src/src` or another staging folder; relying on a literal path is brittle.
- **Action Items:**
  1. Inspect `cmake --build --preset ci-windows --target yaze_util -v` to see actual include search paths and confirm where `str_cat.h` resides on the runner.
  2. Replace the manual include path with `target_link_libraries(yaze_util PRIVATE absl::strings absl::status ...)` plus `target_sources` using `$<TARGET_PROPERTY:absl::strings,INTERFACE_INCLUDE_DIRECTORIES>` via `target_include_directories(yaze_util PRIVATE "$<TARGET_PROPERTY:absl::strings,INTERFACE_INCLUDE_DIRECTORIES>")`. This ensures we mirror whatever layout gRPC provides.
  3. Re-run the Windows job (locally or in CI) to confirm the header issue is resolved.

### 2. Linux CI (Needs Verification)
- **Status:** Not re-run since the AI gating changes. Need to confirm `ci-linux` still builds `yaze`, `z3ed`, and all `yaze_test_*` targets with `YAZE_ENABLE_AI_RUNTIME=OFF` by default.
- **Action Items:**
  1. Execute `cmake --preset ci-linux && cmake --build --preset ci-linux --target yaze yaze_test_stable`.
  2. Check for missing Abseil include issues similar to Windows; apply the same include propagation fix if necessary.

### 3. Duplicate Library Warnings
- **Context:** Link lines on macOS/Windows include both `-force_load yaze_test_support` and a regular `libyaze_test_support.a`, causing duplicate warnings.
- **Priority:** Low (does not break builds), but consider swapping `-force_load` for generator expressions that only apply on targets needing whole-archive semantics.

## Platform Status Matrix

| Platform / Preset | Status | Notes |
| --- | --- | --- |
| **macOS – `ci-macos`** | ✅ Passing | Builds `yaze`, `yaze_emu`, `z3ed`, and all `yaze_test_*`; runs Ollama smoke tests with `qwen2.5-coder:0.5b`. |
| **Linux – `ci-linux`** | ⚠️ Not re-run post-gating | Needs a fresh run to ensure new CMake options didn’t regress core builds/tests. |
| **Windows – `ci-windows` / `ci-windows-ai`** | ❌ Failing | Abseil headers missing in `yaze_util` (see Section 1). |
| **macOS – `z3ed-agent-test`** | ✅ Passing | Brew installs `ollama`/`ninja`, executes `scripts/agent_test_suite.sh` in mock ROM mode. |
| **GUI / Experimental suites** | ✅ (macOS), ⚠️ (Linux/Win) | Compiled only when `YAZE_ENABLE_AI_RUNTIME=ON`; Linux/Win not verified since gating change. |

## Recommended Next Steps
1. **Fix Abseil include propagation on Windows (highest priority)**
   - Replace the hard-coded include path with generator expressions referencing `absl::*` targets, or detect the actual header root under `_deps/grpc-src` on Windows.
   - Run `cmake --build --preset ci-windows --target yaze_util -v` to inspect the include search paths and confirm the correct directory is being passed.
   - Re-run `ci-windows` / `ci-windows-ai` after adjusting the include setup.
2. **Re-run Linux + Windows CI end-to-end once the include issue is resolved** to ensure `yaze`, `yaze_emu`, `z3ed`, and all `yaze_test_*` targets still pass with the current gating rules.
3. **Optional cleanup:** investigate the repeated `-force_load libyaze_test_support.a` warnings on macOS/Windows once the builds are green.

## Additional Context
- macOS’s agent workflow provisions Ollama and runs `scripts/agent_test_suite.sh` with `OLLAMA_MODEL=qwen2.5-coder:0.5b`. Set `USE_MOCK_ROM=false` to validate real ROM flows.
- `yaze_test_gui` and `yaze_test_experimental` are only added when `YAZE_ENABLE_AI_RUNTIME` is enabled. This keeps minimal builds green but reduces coverage on Linux/Windows until their AI builds are healthy.
- `src/util/flag.*` no longer throws exceptions to satisfy `clang-cl /EHs-c-`. Use `detail::FlagParseFatal` for future error reporting.

## Open Questions
1. Should we manage Abseil as an explicit CMake package (e.g., `cmake/dependencies/absl.c`), rather than relying on gRPC’s vendored tree?
2. Once Windows is stable, do we want to add a PowerShell-based Ollama smoke test similar to the macOS workflow?
3. After cleaning up warnings, can we enable `/WX` (Windows) or `-Werror` (Linux/macOS) on critical targets to keep the tree tidy?

Please keep this document updated as you make progress so the next engineer has immediate context.

