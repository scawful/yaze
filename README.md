# YAZE - Yet Another Zelda3 Editor

[![CI](https://github.com/scawful/yaze/workflows/CI%2FCD%20Pipeline/badge.svg)](https://github.com/scawful/yaze/actions)
[![Code Quality](https://github.com/scawful/yaze/workflows/Code%20Quality/badge.svg)](https://github.com/scawful/yaze/actions)
[![Security](https://github.com/scawful/yaze/workflows/Security%20Scanning/badge.svg)](https://github.com/scawful/yaze/actions)
[![Release](https://github.com/scawful/yaze/workflows/Release/badge.svg)](https://github.com/scawful/yaze/actions)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

A cross-platform Zelda 3 ROM editor with a modern C++ GUI, Asar 65816 assembler integration, and an automation-friendly CLI (`z3ed`). YAZE bundles its toolchain, offers AI-assisted editing flows, and targets reproducible builds on Windows, macOS, and Linux.

## Highlights
- **All-in-one editing**: Overworld, dungeon, sprite, palette, and messaging tools with live previews.
- **Assembler-first workflow**: Built-in Asar integration, symbol extraction, and patch validation.
- **Automation & AI**: `z3ed` exposes CLI/TUI automation, proposal workflows, and optional AI agents.
- **Testing & CI hooks**: CMake presets, ROM-less test fixtures, and gRPC-based GUI automation support.
- **Cross-platform toolchains**: Single source tree targeting MSVC, Clang, and GCC with identical presets.

## Project Status
`0.3.x` builds are in active development. Release automation is being reworked, so packaged builds may lag behind main. Follow `develop` for the most accurate view of current functionality.

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
```bash
# macOS
cmake --preset mac-dbg
cmake --build --preset mac-dbg

# Linux
cmake --preset lin-dbg
cmake --build --preset lin-dbg

# Windows
cmake --preset win-dbg
cmake --build --preset win-dbg --target yaze

# Enable AI + gRPC tooling (any platform)
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze z3ed
```

## Applications & Workflows
- **`./build/bin/yaze`** – full GUI editor with multi-session dockspace, theming, and ROM patching.
- **`./build/bin/z3ed --tui`** – CLI/TUI companion for scripting, AI-assisted edits, and Asar workflows.
- **`./build_ai/bin/yaze_test --unit|--integration|--e2e`** – structured test runner for quick regression checks.

Typical commands:
```bash
# Launch GUI with a ROM
./build/bin/yaze zelda3.sfc

# Apply a patch via CLI
./build/bin/z3ed asar patch.asm --rom zelda3.sfc

# Run focused tests
cmake --build --preset mac-ai --target yaze_test
./build_ai/bin/yaze_test --unit
```

## Testing
- `./build_ai/bin/yaze_test --unit` for fast checks; add `--integration` or `--e2e --show-gui` for broader coverage.
- `ctest --preset dev` mirrors CI’s stable set; `ctest --preset all` runs the full matrix.
- Set `YAZE_TEST_ROM_PATH` or pass `--rom-path` when a test needs a real ROM image.

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
