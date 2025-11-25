# AI & gRPC Modularity Blueprint

*Date: November 16, 2025 – Author: GPT-5.1 Codex*

## 1. Scope & Goals

- Make AI/gRPC features optional without scattering `#ifdef` guards.
- Ensure Windows builds succeed regardless of whether AI tooling is enabled.
- Provide a migration path toward relocatable dependencies (`ext/`) and cleaner preset defaults for macOS + custom tiling window manager workflows (sketchybar/yabai/skhd, Emacs/Spacemacs).

## 2. Current Touchpoints

| Surface | Key Paths | Notes |
| --- | --- | --- |
| Editor UI | `src/app/editor/agent/**`, `app/gui/app/agent_chat_widget.cc`, `app/editor/agent/agent_chat_history_popup.cc` | Widgets always compile when `YAZE_ENABLE_GRPC=ON`, but they include protobuf types directly. |
| Core Services | `src/app/service/grpc_support.cmake`, `app/service/*.cc`, `app/test/test_recorder.cc` | `yaze_grpc_support` bundles servers, generated protos, and even CLI code (`cli/service/planning/tile16_proposal_generator.cc`). |
| CLI / z3ed | `src/cli/agent.cmake`, `src/cli/service/agent/*.cc`, `src/cli/service/ai/*.cc`, `src/cli/service/gui/*.cc` | gRPC, Gemeni/Ollama (JSON + httplib/OpenSSL) all live in one static lib. |
| Build Flags | `cmake/options.cmake`, scattered `#ifdef Z3ED_AI` and `#ifdef Z3ED_AI_AVAILABLE` | Flags do not describe GUI vs CLI vs runtime needs, so every translation unit drags in gRPC headers once `YAZE_ENABLE_GRPC=ON`. |
| Tests & Automation | `src/app/test/test_manager.cc`, `scripts/agent_test_suite.sh`, `.github/workflows/ci.yml` | Tests assume AI features exist; Windows agents hit linker issues when that assumption breaks. |

## 3. Coupling Pain Points

1. **Single Monolithic `yaze_agent`** – Links SDL, GUI, emulator, Abseil, yaml, nlohmann_json, httplib, OpenSSL, and gRPC simultaneously. No stubs exist when only CLI or GUI needs certain services (`src/cli/agent.cmake`).
2. **Editor Hard Links** – `yaze_editor` unconditionally links `yaze_agent` when `YAZE_MINIMAL_BUILD` is `OFF`, so even ROM-editing-only builds drag in AI dependencies (`src/app/editor/editor_library.cmake`).
3. **Shared Proto Targets** – `yaze_grpc_support` consumes CLI proto files, so editor-only builds still compile CLI automation code (`src/app/service/grpc_support.cmake`).
4. **Preprocessor Guards** – UI code mixes `Z3ED_AI` and `Z3ED_AI_AVAILABLE`; CLI code checks `Z3ED_AI` while build system only defines `Z3ED_AI` when `YAZE_ENABLE_AI=ON`. These mismatches cause dead code paths and missing symbols.

## 4. Windows Build Blockers

- **Runtime library mismatch** – yaml-cpp and other dependencies are built `/MT` while `yaze_emu` uses `/MD`, causing cascades of `LNK2038` and `_Lockit`/`libcpmt` conflicts (`logs/windows_ci_linker_error.log`).
- **OpenSSL duplication** – `yaze_agent` links cpp-httplib with OpenSSL while gRPC pulls BoringSSL, leading to duplicate symbol errors (`libssl.lib` vs `ssl.lib`) in the same log.
- **Missing native dialogs** – `FileDialogWrapper` symbols fail to link when macOS-specific implementations are not excluded on Windows (also visible in the same log).
- **Preset drift** – `win-ai` enables GRPC/AI without guaranteeing vcpkg/clang-cl or ROM assets; `win-dbg` disables gRPC entirely so editor agents fail to compile because of unconditional includes.

## 5. Proposed Modularization

| Proposed CMake Option | Purpose | Default | Notes |
| --- | --- | --- | --- |
| `YAZE_BUILD_AGENT_UI` | Compile ImGui agent widgets (editor). | `ON` for GUI presets, `OFF` elsewhere. | Controls `app/editor/agent/**` sources. |
| `YAZE_ENABLE_REMOTE_AUTOMATION` | Build/ship gRPC servers & automation bridges. | `ON` in `*-ai` presets. | Owns `yaze_grpc_support` + proto generation. |
| `YAZE_ENABLE_AI_RUNTIME` | Include AI runtime (Gemini/Ollama, CLI planners). | `ON` in CLI/AI presets. | Governs `cli/service/ai/**`. |
| `YAZE_ENABLE_AGENT_CLI` | Build `z3ed` with full agent features. | `ON` when CLI requested. | Allows `z3ed` to be disabled independently. |

Implementation guidelines:

1. **Split Targets**
   - `yaze_agent_core`: command routing, ROM helpers, no AI.
   - `yaze_agent_ai`: depends on JSON + OpenSSL + remote automation.
   - `yaze_agent_ui_bridge`: tiny facade that editor links only when `YAZE_BUILD_AGENT_UI=ON`.
2. **Proto Ownership**
   - Keep proto generation under `yaze_grpc_support`, but do not add CLI sources to that target. Instead, expose headers/libs and let CLI link them conditionally.
3. **Stub Providers**
   - Provide header-compatible no-op classes (e.g., `AgentChatWidgetBridge::Create()` returning `nullptr`) when UI is disabled, removing the need for `#ifdef` in ImGui panels.
4. **Dependency Injection**
   - Replace `#ifdef Z3ED_AI_AVAILABLE` in `agent_chat_widget.cc` with an interface returned from `AgentFeatures::MaybeCreateChatPanel()`.

## 6. Preset & Feature Matrix

| Preset | GUI | CLI | GRPC | AI Runtime | Agent UI |
| --- | --- | --- | --- | --- | --- |
| `mac-dbg` | ✅ | ✅ | ⚪ | ⚪ | ✅ |
| `mac-ai` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `lin-dbg` | ✅ | ✅ | ⚪ | ⚪ | ✅ |
| `ci-windows` | ✅ | ✅ | ⚪ | ⚪ | ⚪ (core only) |
| `ci-windows-ai` (new nightly) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `win-dbg` | ✅ | ✅ | ⚪ | ⚪ | ✅ |
| `win-ai` | ✅ | ✅ | ✅ | ✅ | ✅ |

Legend: ✅ enabled, ⚪ disabled.

## 7. Migration Steps

1. **Define Options** in `cmake/options.cmake` and propagate via presets.
2. **Restructure Libraries**:
   - Move CLI AI/runtime code into `yaze_agent_ai`.
   - Add `yaze_agent_stub` for builds without AI.
   - Make `yaze_editor` link against stub/real target via generator expressions.
3. **CMake Cleanup**:
   - Limit `yaze_grpc_support` to gRPC-only code.
   - Guard JSON/OpenSSL includes behind `YAZE_ENABLE_AI_RUNTIME`.
4. **Windows Hardening**:
   - Force `/MD` everywhere and ensure yaml-cpp inherits `CMAKE_MSVC_RUNTIME_LIBRARY`.
   - Allow only one SSL provider based on feature set.
   - Add preset validation in `scripts/verify-build-environment.ps1`.
5. **CI/CD Split**:
   - Current `.github/workflows/ci.yml` runs GRPC on all platforms; adjust to run minimal Windows build plus nightly AI build to save time and reduce flakiness.
6. **Docs + Scripts**:
   - Update build guides to describe new options.
   - Document how macOS users can integrate headless builds with sketchybar/yabai/skhd (focus on CLI usage + automation).
7. **External Dependencies**:
   - Relocate submodules to `ext/` and update scripts so the new layout is enforced before toggling feature flags.

## 8. Deliverables

- This blueprint (`docs/internal/agents/ai-modularity.md`).
- Updated CMake options, presets, and stubs.
- Hardened Windows build scripts/logging.
- CI/CD workflow split + release automation updates.
- Documentation refresh & dependency relocation.

