# YAZE Build Environment Verification Scripts

This directory contains build environment verification and setup scripts for YAZE development.

## Quick Start

### Verify Build Environment

**Windows (PowerShell):**
```powershell
.\scripts\verify-build-environment.ps1
```

**macOS/Linux:**
```bash
./scripts/verify-build-environment.sh
```

## Scripts Overview

### `verify-build-environment.ps1` / `.sh`

Comprehensive build environment verification script that checks:

- ✅ **CMake Installation** - Version 3.16+ required
- ✅ **Git Installation** - With submodule support
- ✅ **C++ Compiler** - GCC 13+, Clang 16+, or MSVC 2019+
- ✅ **Platform Tools** - Xcode (macOS), Visual Studio (Windows), build-essential (Linux)
- ✅ **Git Submodules** - All dependencies synchronized (auto-fixes if missing/empty)
- ✅ **CMake Cache** - Freshness check (warns if >7 days old)
- ✅ **Dependency Compatibility** - gRPC isolation, httplib, nlohmann/json
- ✅ **CMake Configuration** - Test configuration (verbose mode only)

**Automatic Fixes:**
The script now automatically fixes common issues without requiring `-FixIssues`:
- 🔧 **Missing/Empty Submodules** - Automatically runs `git submodule update --init --recursive`
- 🔧 **Old CMake Cache** - Prompts for confirmation when using `-FixIssues` (auto-skips otherwise)

#### Usage

**Windows:**
```powershell
# Basic verification (auto-fixes submodules)
.\scripts\verify-build-environment.ps1

# With interactive fixes (prompts for cache cleaning)
.\scripts\verify-build-environment.ps1 -FixIssues

# Force clean old CMake cache (no prompts)
.\scripts\verify-build-environment.ps1 -CleanCache

# Verbose output (includes CMake configuration test)
.\scripts\verify-build-environment.ps1 -Verbose

# Combined options
.\scripts\verify-build-environment.ps1 -FixIssues -Verbose
```

**macOS/Linux:**
```bash
# Basic verification (auto-fixes submodules)
./scripts/verify-build-environment.sh

# With interactive fixes (prompts for cache cleaning)
./scripts/verify-build-environment.sh --fix

# Force clean old CMake cache (no prompts)
./scripts/verify-build-environment.sh --clean

# Verbose output
./scripts/verify-build-environment.sh --verbose

# Combined options
./scripts/verify-build-environment.sh --fix --verbose
```

#### Exit Codes

- `0` - Success, environment ready for development
- `1` - Issues found, manual intervention required

## Common Workflows

### First-Time Setup

```bash
# 1. Clone repository with submodules
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# 2. Verify environment
./scripts/verify-build-environment.sh --verbose

# 3. If issues found, fix automatically
./scripts/verify-build-environment.sh --fix

# 4. Build
cmake --preset debug  # macOS
# OR
cmake -B build -DCMAKE_BUILD_TYPE=Debug  # All platforms
cmake --build build
```

### After Pulling Changes

```bash
# 1. Update submodules
git submodule update --init --recursive

# 2. Verify environment (check cache age)
./scripts/verify-build-environment.sh

# 3. If cache is old, clean and rebuild
./scripts/verify-build-environment.sh --clean
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Troubleshooting Build Issues

```bash
# 1. Clean everything and verify
./scripts/verify-build-environment.sh --clean --fix --verbose

# 2. This will:
#    - Sync all git submodules
#    - Remove old CMake cache
#    - Test CMake configuration
#    - Report any issues

# 3. Follow recommended actions in output
```

### Before Opening Pull Request

```bash
# Verify clean build environment
./scripts/verify-build-environment.sh --verbose

# Should report: "Build Environment Ready for Development!"
```

## Automatic Fixes

The script automatically fixes common issues when detected:

### Always Auto-Fixed (No Confirmation Required)

1. **Missing/Empty Git Submodules**
   ```bash
   git submodule sync --recursive
   git submodule update --init --recursive
   ```
   - Runs automatically when submodules are missing or empty
   - No user confirmation required
   - Re-verifies after sync to ensure success

### Fixed with `-FixIssues` / `--fix` Flag

2. **Clean Old CMake Cache** (with confirmation prompt)
   - Prompts user before removing build directories
   - Only when cache is older than 7 days
   - User can choose to skip

### Fixed with `-CleanCache` / `--clean` Flag

3. **Force Clean CMake Cache** (no confirmation)
   - Removes `build/`, `build_test/`, `build-grpc-test/`
   - Removes Visual Studio cache (`out/`)
   - No prompts, immediate cleanup

### Optional Verbose Tests

When run with `--verbose` or `-Verbose`:

4. **Test CMake Configuration**
   - Creates temporary build directory
   - Tests minimal configuration
   - Reports success/failure
   - Cleans up test directory

## Integration with Visual Studio

The verification script integrates with Visual Studio CMake workflow:

1. **Pre-Build Check**: Run verification before opening VS
2. **Submodule Sync**: Ensures all dependencies are present
3. **Cache Management**: Prevents stale CMake cache issues

**Visual Studio Workflow:**
```powershell
# 1. Verify environment
.\scripts\verify-build-environment.ps1 -Verbose

# 2. Open in Visual Studio
# File → Open → Folder → Select yaze directory

# 3. Visual Studio detects CMakeLists.txt automatically
# 4. Select Debug/Release from toolbar
# 5. Press F5 to build and run
```

## What Gets Checked

### CMake (Required)
- Minimum version 3.16
- Command available in PATH
- Compatible with project CMake files

### Git (Required)
- Git command available
- Submodule support
- All submodules present and synchronized:
  - `src/lib/SDL`
  - `src/lib/abseil-cpp`
  - `src/lib/asar`
  - `src/lib/imgui`
  - `third_party/json`
  - `third_party/httplib`

### Compilers (Required)
- **Windows**: Visual Studio 2019+ with C++ workload
- **macOS**: Xcode Command Line Tools
- **Linux**: GCC 13+ or Clang 16+, build-essential package

### Platform Dependencies

**Linux Specific:**
- GTK+3 development libraries (`libgtk-3-dev`)
- DBus development libraries (`libdbus-1-dev`)
- pkg-config tool

**macOS Specific:**
- Xcode Command Line Tools
- Cocoa framework (automatic)

**Windows Specific:**
- Visual Studio 2022 recommended
- Windows SDK 10.0.19041.0 or later

### CMake Cache

Checks for build directories:
- `build/` - Main build directory
- `build_test/` - Test build directory
- `build-grpc-test/` - gRPC test builds
- `out/` - Visual Studio CMake output

Warns if cache files are older than 7 days.

### Dependencies

**gRPC Isolation (when enabled):**
- Verifies `CMAKE_DISABLE_FIND_PACKAGE_Protobuf=TRUE`
- Verifies `CMAKE_DISABLE_FIND_PACKAGE_absl=TRUE`
- Prevents system package conflicts

**Header-Only Libraries:**
- `third_party/httplib` - cpp-httplib HTTP library
- `third_party/json` - nlohmann/json library

## Automatic Fixes

When run with `--fix` or `-FixIssues`:

1. **Sync Git Submodules**
   ```bash
   git submodule sync --recursive
   git submodule update --init --recursive
   ```

2. **Clean CMake Cache** (when combined with `--clean`)
   - Removes `build/`, `build_test/`, `build-grpc-test/`
   - Removes Visual Studio cache (`out/`)

3. **Test CMake Configuration**
   - Creates temporary build directory
   - Tests minimal configuration
   - Reports success/failure
   - Cleans up test directory

## CI/CD Integration

The verification script can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions step
- name: Verify Build Environment
  run: |
    ./scripts/verify-build-environment.sh --verbose
  shell: bash
```

## Troubleshooting

### Script Reports "CMake Not Found"

**Windows:**
```powershell
# Check if CMake is installed
cmake --version

# If not found, add to PATH or install
choco install cmake

# Restart PowerShell
```

**macOS/Linux:**
```bash
# Check if CMake is installed
cmake --version

# Install if missing
brew install cmake  # macOS
sudo apt install cmake  # Ubuntu/Debian
```

### "Git Submodules Missing"

```bash
# Manually sync and update
git submodule sync --recursive
git submodule update --init --recursive

# Or use fix option
./scripts/verify-build-environment.sh --fix
```

### "CMake Cache Too Old"

```bash
# Clean automatically
./scripts/verify-build-environment.sh --clean

# Or manually
rm -rf build build_test build-grpc-test
```

### "Visual Studio Not Found" (Windows)

```powershell
# Install Visual Studio 2022 with C++ workload
# Download from: https://visualstudio.microsoft.com/

# Required workload:
# "Desktop development with C++"
```

### Script Fails on Network Issues (gRPC)

The script verifies configuration but doesn't download gRPC unless building with `-DYAZE_WITH_GRPC=ON`.

If you encounter network issues:
```bash
# Use minimal build (no gRPC)
cmake -B build -DYAZE_MINIMAL_BUILD=ON
```

## See Also

- [Build Instructions](../docs/02-build-instructions.md) - Complete build guide
- [Getting Started](../docs/01-getting-started.md) - First-time setup
- [Platform Compatibility](../docs/B2-platform-compatibility.md) - Platform-specific notes
- [Contributing](../docs/B1-contributing.md) - Development guidelines
