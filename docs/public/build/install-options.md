# YAZE Installation Options (Distribution Guide)

Status: Draft  
Audience: Users/distributors who want alternatives to direct GitHub release binaries.

## Overview
YAZE is distributed primarily via GitHub release binaries. This guide summarizes current install paths and outlines packaging-friendly options per platform. Use the table to pick what is available today vs. what would require packaging work.

## Platform Matrix
| Platform | Status | Recommended Path | Notes |
|----------|--------|------------------|-------|
| macOS (Intel/Apple) | Available | GitHub release tarball; custom Homebrew tap (see below) | Prefer Apple silicon builds; Intel works under Rosetta. |
| Windows (x64) | Available | GitHub release zip; vcpkg-from-source (community) | No official winget/choco package yet. |
| Linux (x86_64) | Available | GitHub release AppImage (if provided) or build from source | Test on Ubuntu/Debian/Fedora; Wayland users may need XWayland. |
| Web (WASM) | Preview | Hosted demo or local `npm http-server` of `build-wasm` artifact | Requires modern browser; no install. |

## macOS
### 1) Release binary (recommended)
1. Download the macOS tarball from GitHub releases.
2. `tar -xf yaze-<version>-macos.tar.gz && cd yaze-<version>-macos`
3. Run `./yaze.app/Contents/MacOS/yaze` (GUI) or `./bin/z3ed` (CLI).

### 2) Homebrew (custom tap)
- If you publish a tap: `brew tap <your/tap>` then `brew install yaze`.
- Sample formula inputs:
  - URL: GitHub release tarball.
  - Dependencies: `cmake`, `ninja`, `pkg-config`, `sdl2`, `glew`, `glm`, `ftxui`, `abseil`, `protobuf`, `gtest`.
- For development builds: `cmake --preset mac-dbg` then `cmake --build --preset mac-dbg`.

## Windows
### 1) Release zip (recommended)
1. Download the Windows zip from GitHub releases.
2. Extract to a writable directory.
3. Run `yaze.exe` (GUI) or `z3ed.exe` (CLI) from the `bin` folder.

### 2) vcpkg-from-source (DIY)
If you prefer source builds with vcpkg dependencies:
1. Install vcpkg and integrate: `vcpkg integrate install`.
2. Configure: `cmake --preset win-dbg -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake`.
3. Build: `cmake --build --preset win-dbg`.
4. Run tests (optional): `ctest --test-dir build`.

## Linux
### 1) Release AppImage (if available)
- `chmod +x yaze-<version>-linux.AppImage && ./yaze-<version>-linux.AppImage`
- If graphics fail under Wayland, try `XWAYLAND_FORCE=1 ./yaze-<version>-linux.AppImage`.

### 2) Build from source
Prereqs: `cmake`, `ninja-build`, `pkg-config`, `libsdl2-dev`, `libglew-dev`, `libglm-dev`, `protobuf-compiler`, `libprotobuf-dev`, `libabsl-dev`, `libftxui-dev` (or build from source), `zlib1g-dev`.
```
cmake --preset lin-dbg
cmake --build --preset lin-dbg
ctest --test-dir build -L stable   # optional
```

## Web (WASM Preview)
- Use the published web build (if provided) or self-host the `build-wasm` output:
```
cd build-wasm && npx http-server .
```
- Open the local URL in a modern browser; no installation required.

## Packaging Notes
- Prefer static/runtime-complete bundles for end users (AppImage on Linux, app bundle on macOS, zip on Windows).
- When creating packages (Homebrew/Chocolatey/winget), pin the release URL and checksum and align dependencies to the CMake presets (`mac-*/lin-*/win-*`).
- Keep CLI and GUI in the same archive to avoid mismatched versions; CLI entry is `z3ed`, GUI entry is `yaze`.

## Local Nightly (Self-Build)
For an isolated nightly build separate from your dev tree, use:
```bash
scripts/install-nightly.sh
```
This installs into `~/.local/yaze/nightly/current` and exposes wrapper commands
(`yaze-nightly`, `z3ed-nightly`, `yaze-mcp-nightly`).
Re-run the script to update to the latest commit.
By default, the clone lives under `~/.yaze/nightly/repo` (override with
`YAZE_NIGHTLY_REPO`). On macOS, a stable app link is created at
`~/Applications/Yaze Nightly.app` (override with `YAZE_NIGHTLY_APP_DIR`).

## Quick Links
- Build quick reference: `docs/public/build/quick-reference.md`
- CMake presets: `CMakePresets.json`
- Tests (optional after build): `ctest --test-dir build -L stable`
