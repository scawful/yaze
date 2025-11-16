# Dependency & Build Overview

_Last reviewed: November 2025. All information in this document is derived from the current
`src/CMakeLists.txt` tree and shipped presets._

This guide explains how the major YAZE libraries fit together, which build switches control
them, and when a code change actually forces a full rebuild. It is intentionally concise so you
can treat it as a quick reference while editing.

## Build Switches & Presets

| CMake option | Default | Effect |
| --- | --- | --- |
| `YAZE_BUILD_APP` | `ON` | Build the main GUI editor (`yaze`). Disable when you only need CLI/tests. |
| `YAZE_BUILD_Z3ED` | `ON` | Build the `z3ed` automation tool and supporting agent libraries. |
| `YAZE_BUILD_EMU` | `OFF` | Build the standalone emulator binary. Always enabled inside the GUI build. |
| `YAZE_BUILD_TESTS` | `ON` in `*-dbg` presets | Compiles test helpers plus `yaze_test`. Required for GUI test dashboard. |
| `YAZE_ENABLE_GRPC` | `OFF` | Pulls in gRPC/protobuf for automation and remote control features. |
| `YAZE_MINIMAL_BUILD` | `OFF` | Skips optional editors/assets. Useful for CI smoke builds. |
| `YAZE_BUILD_LIB` | `OFF` | Produces the `yaze_core` INTERFACE target used by external tooling. |
| `YAZE_BUILD_AGENT_UI` | `ON` when `YAZE_BUILD_GUI` is `ON` | Compiles ImGui chat widgets. Disable for lighter GUI builds. |
| `YAZE_ENABLE_REMOTE_AUTOMATION` | `OFF` in `win-*` core presets | Builds gRPC servers/clients plus proto generation. |
| `YAZE_ENABLE_AI_RUNTIME` | `OFF` in `win-*` core presets | Enables Gemini/Ollama transports, proposal planning, and advanced routing code. |
| `YAZE_ENABLE_AGENT_CLI` | `ON` when `YAZE_BUILD_CLI` is `ON` | Compiles the conversational agent stack used by `z3ed`. |

Use the canned presets from `CMakePresets.json` so these options stay consistent across
platforms: `mac-dbg`, `mac-ai`, `lin-dbg`, `win-dbg`, etc. The `*-ai` presets enable both
`YAZE_BUILD_Z3ED` and `YAZE_ENABLE_GRPC` so the CLI and agent features match what ships.

## Library Layers

### 1. Foundation (`src/util`, `incl/`)
- **`yaze_common`**: cross-platform macros, generated headers, and lightweight helpers shared by
  every other target.
- **`yaze_util`**: logging, file I/O, BPS patch helpers, and the legacy flag system. Only depends
  on `yaze_common` plus optional gRPC/Abseil symbols.
- **Third-party**: SDL2, ImGui, Abseil, yaml-cpp, FTXUI, Asar. They are configured under
  `cmake/dependencies/*.cmake` and linked where needed.

Touching headers in this layer effectively invalidates most of the build. Keep common utilities
stable and prefer editor-specific helpers instead of bloating `yaze_util`.

### 2. Graphics & UI (`src/app/gfx`, `src/app/gui`)
- **`yaze_gfx`**: bitmap containers, palette math, deferred texture arena, canvas abstractions.
  Depends on SDL2 + `yaze_util`.
- **`yaze_gui`**: shared ImGui widgets, docking layout utilities, and theme plumbing. Depends on
  ImGui + `yaze_gfx`.

Changes here rebuild all editors but do not touch the lower-level Zelda 3 logic. Use the graphics
layer for rendering and asset streaming primitives; keep domain logic in the Zelda 3 library.

### 3. Game Domain (`src/zelda3`, `src/app/editor`)
- **`yaze_zelda3`**: map/room/sprite models, parsers, and ROM serialization. It links
  `yaze_gfx`, `yaze_util`, and Abseil.
- **`yaze_editor`**: ImGui editors (overworld, dungeon, palette, etc.). Depends on
  `yaze_gui`, `yaze_zelda3`, `yaze_gfx`, and optional agent/test hooks.
- **`yaze_emulator`**: CPU, PPU, and APU subsystems plus the debugger UIs (`src/app/emu`). The GUI
  app links this to surface emulator panels.

Touching Zelda 3 headers triggers rebuilds of the editor and CLI but leaves renderer-only changes
alone. Touching editor UI code does **not** require rebuilding `yaze_emulator`.

### 4. Tooling & Export Targets
- **`yaze_agent`** (`src/cli/agent`): shared logic behind the CLI and AI workflows. Built whenever
  `YAZE_ENABLE_AGENT_CLI` is enabled (automatically true when `YAZE_BUILD_Z3ED=ON`). When both the CLI and the agent UI are disabled, CMake now emits a lightweight stub target so GUI-only builds don't drag in unnecessary dependencies.
- **`z3ed` binary** (`src/cli/z3ed.cmake`): links `yaze_agent`, `yaze_zelda3`, `yaze_gfx`, and
  Abseil/FTXUI.
- **`yaze_core_lib`** (`src/core`): static library that exposes project management helpers and the
  Asar integration. When `YAZE_BUILD_LIB=ON` it can be consumed by external tools.
- **`yaze_test_support`** (`src/app/test`): harness for the in-editor dashboard and `yaze_test`.
- **`yaze_grpc_support`**: server-only aggregation of gRPC/protobuf code, gated by `YAZE_ENABLE_REMOTE_AUTOMATION`. CLI clients (`cli/service/gui/**`, `cli/service/planning/**`) now live solely in `yaze_agent` so GUI builds can opt out entirely.

### 5. Final Binaries
- **`yaze`**: GUI editor. Links every layer plus `yaze_test_support` when tests are enabled.
- **`yaze_test`**: GoogleTest runner (unit, integration, e2e). Built from `test/CMakeLists.txt`.
- **`z3ed`**: CLI + TUI automation tool. Built when `YAZE_BUILD_Z3ED=ON`.
- **`yaze_emu`**: optional standalone emulator for fast boot regression tests.

## Rebuild Cheatsheet

| Change | Targets Affected |
| --- | --- |
| `src/util/*.h` or `incl/yaze/*.h` | Everything (foundation dependency) |
| `src/app/gfx/**` | `yaze_gfx`, `yaze_gui`, editors, CLI. Emulator core unaffected. |
| `src/zelda3/**` | All editors, CLI, tests. Rebuild does **not** touch renderer-only changes. |
| `src/app/editor/**` | GUI editor + CLI (shared panels). Emulator/test support untouched. |
| `src/app/emu/**` | Emulator panels + GUI app. CLI and Zelda 3 libraries unaffected. |
| `src/cli/**` | `yaze_agent`, `z3ed`. No impact on GUI/editor builds. |
| `src/app/test/**` | `yaze_test_support`, `yaze_test`, GUI app (only when tests enabled). |

Use this table when deciding whether to invalidate remote build caches or to schedule longer CI
runs. Whenever possible, localize changes to the upper layers to avoid rebuilding the entire
stack.

## Tips for Faster Iteration

1. **Leverage presets** – `cmake --build --preset mac-ai --target yaze` automatically enables
   precompiled headers and shared dependency trees.
2. **Split work by layer** – renderer bugs usually live in `yaze_gfx`; leave Zelda 3 logic alone
   unless you need ROM serialization tweaks.
3. **Turn off unused targets** – set `YAZE_BUILD_Z3ED=OFF` when working purely on GUI features to
   shave a few hundred object files.
4. **Test without ROMs** – `docs/public/developer/testing-without-roms.md` documents the mock ROM
   harness so you do not need to rebuild assets between iterations.
5. **See also** – for deep dives into refactors or planned changes, read the internal blueprints in
   `docs/internal/blueprints/` instead of bloating the public docs.
