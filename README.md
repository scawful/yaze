# YAZE - Yet Another Zelda3 Editor

[![CI](https://github.com/scawful/yaze/workflows/CI%2FCD%20Pipeline/badge.svg)](https://github.com/scawful/yaze/actions)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

A ROM editor for The Legend of Zelda: A Link to the Past.

Built in C++23 with ImGui, includes a built-in SNES emulator, the Asar 65816 assembler, and a CLI tool (`z3ed`) for scripted ROM operations. Runs on Windows, macOS, and Linux. There's also an experimental WASM build for browsers.

## What It Does

- **Overworld Editor** - Edit 160 overworld maps, tiles, entrances, exits
- **Dungeon Editor** - Edit 296 dungeon rooms, objects, sprites, palettes
- **Graphics Editor** - Inspect 223 graphics sheets and preview guarded edits
- **Palette Editor** - Modify color palettes with live preview and explicit ROM-buffer commit
- **Message Editor** - Edit in-game text and dialogue
- **Sprite Editor** - View sprite graphics and animations
- **Music Editor** - (Experimental) View and edit SPC700 music data
- **Built-in Emulator** - Test changes without leaving the editor
- **CLI (z3ed)** - Script ROM operations, integrate with AI agents (Ollama or cloud APIs)

## Apps

- **Desktop app (yaze)** - Full GUI editor + emulator. See `docs/public/build/quick-reference.md`.
- **CLI (z3ed)** - Scriptable ROM editing and AI workflows. See `docs/public/usage/z3ed-cli.md`.
- **Web/WASM preview** - Browser-based editor with a subset of features. See `docs/public/usage/web-app.md`.

## What It Needs

- A legally obtained ALttP ROM (US or JP)
- CMake 3.25+, C++23 compiler (Clang 16+, GCC 13+, or MSVC 2022+)
- For AI features: Ollama running locally or a cloud API key (Gemini/OpenAI/Anthropic)

## Project Status

v0.8.0 is the current development line; v0.7.2 is the latest tagged release.
See [`CHANGELOG.md`](CHANGELOG.md) for details.

**v0.8.0 focus:** a bounded tester preview with honest editor save boundaries,
cross-platform release artifacts, Dungeon visual parity, and a calmer editor UI.

| Component | Tester status | Notes |
|-----------|---------------|-------|
| Dungeon Editor | Tester ready | Bounded room edits save through File > Save ROM; known visual exceptions remain. |
| Overworld Editor | Tester ready | Bounded map/entity edits save through File > Save ROM. |
| Message Editor | Tester ready | Valid text edits use the coordinated save path. |
| Palette Editor | Conditional | Use Palette **Save to ROM**, then File > Save ROM; JSON import/export is implemented when enabled. |
| Assembly Editor | Conditional | Source-file save and Asar ROM application are separate workflows. |
| Sprite Editor | Conditional | Custom `.zsm` editing only; use Dungeon for room sprite placement. |
| Settings | Conditional | Non-ROM settings persistence; verify changes after restart. |
| Graphics Editor | View only | Pending graphics edits deliberately block Save ROM until the serializer is safe. |
| Screen Editor | View only | Pending screen edits deliberately block coordinated Save ROM. |
| Music Editor | View only | Playback/inspection; coordinated ROM save and instrument/sample writers are incomplete. |
| Hex / Memory | View only | Advanced raw tooling without a complete dirty/undo/save contract. |
| Emulator | Experimental | Runtime testing; save-state UI remains incomplete. |

See [`docs/public/reference/feature-coverage-report.md`](docs/public/reference/feature-coverage-report.md)
for cross-app status, persistence notes, and test coverage.

## Quick Start

```bash
# Clone
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# Build (macOS, AI-enabled editor + CLI)
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze z3ed --parallel 4

# Run
./scripts/yaze zelda3.sfc
```

**Presets:** `mac-dbg`, `mac-rel`, `lin-dbg`, `lin-rel`, `win-dbg`, `win-rel`

For AI features, use `*-ai` presets (`mac-ai`, `win-ai`) which enable
Ollama/Gemini/OpenAI/Anthropic integration.

Repository-provided build/test presets and helpers cap their defaults at four
workers (some use fewer) to keep interactive agent sessions responsive. Pass
`--parallel <jobs>` explicitly to override that limit.

See [`docs/public/build/quick-reference.md`](docs/public/build/quick-reference.md) for full build instructions.

## Usage

```bash
# GUI editor
./scripts/yaze zelda3.sfc

# CLI - ROM info
./scripts/z3ed rom-info --rom=zelda3.sfc

# CLI - List dungeon sprites
./scripts/z3ed dungeon-list-sprites --room=1 --rom=zelda3.sfc

# CLI - Interactive TUI
./scripts/z3ed --tui

# Web version (experimental)
# https://scawful.github.io/yaze/
```

## Testing

```bash
# Fast local loop (quick labeled suites)
./scripts/test_fast.sh --quick

# Stable suites (label-filtered)
ctest --test-dir build_ai -C Debug -L unit
ctest --test-dir build_ai -C Debug -L integration

# Preset-based runs (see CMakePresets.json testPresets)
ctest --preset mac-ai-unit
ctest --preset mac-ai-quick-unit
ctest --preset mac-ai-quick-integration
```

Some tests require a ROM. Set `YAZE_TEST_ROM_VANILLA` or
`YAZE_TEST_ROM_VANILLA_PATH` (and optionally `YAZE_TEST_ROM_EXPANDED` or
`YAZE_TEST_ROM_EXPANDED_PATH`).

Web app smoke checks: load `src/web/tests/wasm_debug_api_tests.js` in the
browser console and run `await window.runWasmDebugApiTests()`.

## Documentation

- User docs: [`docs/public/`](docs/public/index.md)
- Developer docs: [`docs/internal/`](docs/internal/README.md)
- API reference: `doxygen Doxyfile` generates to `build/docs/`

## Contributing

See the [Git workflow guide](docs/public/developer/git-workflow.md). Discussion
on [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk).

## License

GPL v3. See [`LICENSE`](LICENSE).

## Screenshots

![Overworld Editor](https://github.com/scawful/yaze/assets/47263509/34b36666-cbea-420b-af90-626099470ae4)
![Dungeon Editor](https://github.com/scawful/yaze/assets/47263509/d8f0039d-d2e4-47d7-b420-554b20ac626f)
