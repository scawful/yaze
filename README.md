# YAZE - Yet Another Zelda3 Editor

[![CI](https://github.com/scawful/yaze/workflows/CI%2FCD%20Pipeline/badge.svg)](https://github.com/scawful/yaze/actions)
[![Code Quality](https://github.com/scawful/yaze/workflows/Code%20Quality/badge.svg)](https://github.com/scawful/yaze/actions)
[![Security](https://github.com/scawful/yaze/workflows/Security%20Scanning/badge.svg)](https://github.com/scawful/yaze/actions)
[![Release](https://github.com/scawful/yaze/workflows/Release/badge.svg)](https://github.com/scawful/yaze/actions)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

A cross-platform Zelda 3 ROM editor with a modern C++ GUI, Asar 65816 assembler integration, and an automation-friendly CLI (`z3ed`). YAZE bundles its toolchain, offers AI-assisted editing flows, and targets reproducible builds on Windows, macOS, and Linux. A preview web version is also available for browser-based editing.

## Highlights
- **All-in-one editing**: Overworld, dungeon, sprite, palette, and messaging tools with live previews.
- **Assembler-first workflow**: Built-in Asar integration, symbol extraction, and patch validation.
- **Automation & AI**: `z3ed` exposes CLI/TUI automation, proposal workflows, and optional AI agents.
- **Web preview**: Experimental browser-based editor (WASM) - see [Web App Guide](docs/public/usage/web-app.md).
- **Testing & CI hooks**: CMake presets, ROM-less test fixtures, and gRPC-based GUI automation support.
- **Cross-platform toolchains**: Single source tree targeting MSVC, Clang, and GCC with identical presets.
- **Modular AI stack**: Toggle agent UI (`YAZE_BUILD_AGENT_UI`), remote automation/gRPC (`YAZE_ENABLE_REMOTE_AUTOMATION`), and AI runtimes (`YAZE_ENABLE_AI_RUNTIME`) per preset.

## Project Status
`0.5.0-alpha` builds are in active development, focusing on the new Music Editor, Web Assembly port, and SDL3 migration. See [`docs/public/release-notes.md`](docs/public/release-notes.md) for details.

## Quick Start

### Clone & Bootstrap
```bash
git clone --recursive https://github.com/scawful/yaze.git
cd yaze
```

Run the environment verifier once per machine:
```bash
# macOS / Linux
./scripts/verify-build-environment.sh --fix

# Windows (PowerShell)
.\scripts\verify-build-environment.ps1 -FixIssues
```

### Configure & Build
- Use the CMake preset that matches your platform (`mac-dbg`, `lin-dbg`, `win-dbg`, etc.).
- Build with `cmake --build --preset <name> [--target …]`.
- See [`docs/public/build/quick-reference.md`](docs/public/build/quick-reference.md) for the canonical
  list of presets, AI build policy, and testing commands.

### Agent Feature Flags

| Option | Default | Effect |
| --- | --- | --- |
| `YAZE_BUILD_AGENT_UI` | `ON` when GUI builds are enabled | Compiles the chat/dialog widgets so the editor can host agent sessions. Turn this `OFF` when you want a lean GUI-only build. |
| `YAZE_ENABLE_REMOTE_AUTOMATION` | `ON` for `*-ai` presets | Builds the gRPC servers/clients and protobufs that power GUI automation. |
| `YAZE_ENABLE_AI_RUNTIME` | `ON` for `*-ai` presets | Enables Gemini/Ollama transports, proposal planning, and advanced routing logic. |
| `YAZE_ENABLE_AGENT_CLI` | `ON` when CLI builds are enabled | Compiles the conversational agent stack consumed by `z3ed`. Disable to skip the CLI entirely. |

Windows `win-*` presets keep every switch `OFF` by default (`win-dbg`, `win-rel`, `ci-windows`) so MSVC builds stay fast. Use `win-ai`, `win-vs-ai`, or the new `ci-windows-ai` preset whenever you need remote automation or AI runtime features.

All bundled third-party code (SDL, ImGui, ImGui Test Engine, Asar, nlohmann/json, cpp-httplib, nativefiledialog-extended) now lives under `ext/` for easier vendoring and cleaner include paths.

## Applications & Workflows
- **`./build/bin/yaze`** – full GUI editor with multi-session dockspace, theming, and ROM patching.
- **Web App (Preview)** – browser-based editor at your deployed instance; see [`docs/public/usage/web-app.md`](docs/public/usage/web-app.md) for details and limitations.
- **`./build/bin/z3ed --tui`** – CLI/TUI companion for scripting, AI-assisted edits, and Asar workflows.
- **`ctest --test-dir build -L unit|integration|e2e`** – structured test runner for quick regression checks.
- **`z3ed` + macOS automation** – pair the CLI with sketchybar/yabai/skhd or Emacs/Spacemacs to drive ROM workflows without opening the GUI.

Typical commands:
```bash
# Launch GUI with a ROM
./build/bin/yaze roms/alttp_vanilla.sfc

# Apply a patch via CLI
./build/bin/z3ed asar patch.asm --rom roms/alttp_vanilla.sfc

# Run focused tests
cmake --build --preset mac-ai --target yaze_test
ctest --test-dir build -L unit
```

## Testing
- `ctest --test-dir build -L unit` for fast checks; add `-L integration` or `-L e2e --output-on-failure` for broader coverage.
- `ctest --preset dev` mirrors CI’s stable set; `ctest --preset all` runs the full matrix.
- Set `YAZE_TEST_ROM_VANILLA` or pass `--rom-vanilla` when a test needs a real ROM image (legacy `--rom-path` still works).

## Documentation
- Human-readable docs live under `docs/public/` with an entry point at [`docs/public/index.md`](docs/public/index.md).
- Run `doxygen Doxyfile` to generate API + guide pages (`build/docs/html` and `build/docs/latex`).
- Agent playbooks, architecture notes, and testing recipes now live in [`docs/internal/`](docs/internal/README.md).

## Contributing & Community
- Review [`CONTRIBUTING.md`](CONTRIBUTING.md) and the build/test guides in `docs/public/`.
- Conventional commit messages (`feat:`, `fix:`, etc.) keep history clean; use topic branches for larger work.
- Chat with the team on [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk).

## License
YAZE is licensed under the GNU GPL v3. See [`LICENSE`](LICENSE) for details and third-party notices.

## Screenshots
![YAZE GUI Editor](https://github.com/scawful/yaze/assets/47263509/8b62b142-1de4-4ca4-8c49-d50c08ba4c8e)
![Dungeon Editor](https://github.com/scawful/yaze/assets/47263509/d8f0039d-d2e4-47d7-b420-554b20ac626f)
![Overworld Editor](https://github.com/scawful/yaze/assets/47263509/34b36666-cbea-420b-af90-626099470ae4)
