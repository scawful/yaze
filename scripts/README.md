# YAZE Build Scripts

This directory contains build automation and maintenance scripts for the YAZE project.

## build_cleaner.py

Automates CMake source list maintenance and header include management with IWYU-style analysis.

### Features

1. **CMake Source List Maintenance**: Automatically updates source file lists in CMake files
2. **Self-Header Includes**: Ensures source files include their corresponding headers
3. **IWYU-Style Analysis**: Suggests missing headers based on symbol usage
4. **.gitignore Support**: Respects .gitignore patterns when scanning files
5. **Auto-Discovery**: Can discover CMake libraries that opt-in to auto-maintenance

### Usage

```bash
# Dry-run to see what would change (recommended first step)
python3 scripts/build_cleaner.py --dry-run

# Update CMake source lists and header includes
python3 scripts/build_cleaner.py

# Run IWYU-style header analysis
python3 scripts/build_cleaner.py --iwyu

# Auto-discover CMake libraries marked for auto-maintenance
python3 scripts/build_cleaner.py --auto-discover

# Update only CMake source lists
python3 scripts/build_cleaner.py --cmake-only

# Update only header includes
python3 scripts/build_cleaner.py --includes-only
```

### Opting-In to Auto-Maintenance

By default, the script only auto-maintains source lists that are explicitly marked. To mark a CMake variable for auto-maintenance, add a comment above the `set()` statement:

```cmake
# This list is auto-maintained by scripts/build_cleaner.py
set(
  YAZE_APP_EMU_SRC
  app/emu/audio/apu.cc
  app/emu/cpu/cpu.cc
  # ... more files
)
```

The script looks for comments containing "auto-maintain" (case-insensitive) within 3 lines above the `set()` statement.

### Excluding Files from Processing

To exclude a specific file from all processing (CMake lists, header includes, IWYU), add this token near the top of the file:

```cpp
// build_cleaner:ignore
```

### .gitignore Support

The script automatically respects `.gitignore` patterns. To enable this feature, install the `pathspec` dependency:

```bash
pip3 install -r scripts/requirements.txt
# or
pip3 install pathspec
```

### IWYU Configuration

The script includes basic IWYU-style analysis that suggests headers based on symbol prefixes. To customize which headers are suggested, edit the `COMMON_HEADERS` dictionary in the script:

```python
COMMON_HEADERS = {
    'std::': ['<memory>', '<string>', '<vector>', ...],
    'absl::': ['<absl/status/status.h>', ...],
    'ImGui::': ['<imgui.h>'],
    'SDL_': ['<SDL.h>'],
}
```

**Note**: The IWYU analysis is conservative and may suggest headers that are already transitively included. Use with care and review suggestions before applying.

### Integration with CMake

The script is integrated into the CMake build system:

```bash
# Run as a CMake target
cmake --build build --target build_cleaner
```

### Dependencies

- Python 3.7+
- `pathspec` (optional, for .gitignore support): `pip3 install pathspec`

### How It Works

1. **CMake Maintenance**: Scans directories specified in the configuration and updates `set(VAR_NAME ...)` blocks with the current list of source files
2. **Self-Headers**: For each `.cc`/`.cpp` file, ensures it includes its corresponding `.h` file
3. **IWYU Analysis**: Scans source files for symbols and suggests appropriate headers based on prefix matching

### Current Auto-Maintained Variables

**All 20 library source lists are now auto-maintained by default:**

- Core: `YAZE_APP_EMU_SRC`, `YAZE_APP_CORE_SRC`, `YAZE_APP_EDITOR_SRC`, `YAZE_APP_ZELDA3_SRC`, `YAZE_NET_SRC`, `YAZE_UTIL_SRC`
- GFX: `GFX_TYPES_SRC`, `GFX_BACKEND_SRC`, `GFX_RESOURCE_SRC`, `GFX_CORE_SRC`, `GFX_UTIL_SRC`, `GFX_RENDER_SRC`, `GFX_DEBUG_SRC`
- GUI: `GUI_CORE_SRC`, `CANVAS_SRC`, `GUI_WIDGETS_SRC`, `GUI_AUTOMATION_SRC`, `GUI_APP_SRC`
- Other: `YAZE_AGENT_SOURCES`, `YAZE_TEST_SOURCES`

The script intelligently preserves conditional blocks (if/endif) and excludes conditional files from the main source list.

## verify-build-environment.\*

`verify-build-environment.ps1` (Windows) and `verify-build-environment.sh` (macOS/Linux) are the primary diagnostics for contributors. They now:

- Check for `clang-cl`, Ninja, NASM, Visual Studio workloads, and VS Code (optional).
- Validate vcpkg bootstrap status plus `vcpkg/installed` cache contents.
- Warn about missing ROM assets (`zelda3.sfc`, `assets/zelda3.sfc`, etc.).
- Offer `-FixIssues` and `-CleanCache` switches to repair Git config, resync submodules, and wipe stale build directories.

Run the script once per machine (and rerun after major toolchain updates) to ensure presets such as `win-dbg`, `win-ai`, `mac-ai`, and `ci-windows-ai` have everything they need.

## setup-vcpkg-windows.ps1

Automates the vcpkg bootstrap flow on Windows:

1. Clones and bootstraps vcpkg (if not already present).
2. Verifies that `git`, `clang-cl`, and Ninja are available, printing friendly instructions when they are missing.
3. Installs the default triplet (`x64-windows` or `arm64-windows` when detected) and confirms that `vcpkg/installed/<triplet>` is populated.
4. Reminds you to rerun `.\scripts\verify-build-environment.ps1 -FixIssues` to double-check the environment.

Use it immediately after cloning the repository or whenever you need to refresh your local dependency cache before running `win-ai` or `ci-windows-ai` presets.
