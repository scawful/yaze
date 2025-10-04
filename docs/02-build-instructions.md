# Build Instructions

YAZE uses CMake 3.16+ with modern target-based configuration. The project includes comprehensive Windows support with Visual Studio integration, vcpkg package management, and automated setup scripts.

## ⚡ Build Environment Verification

**Before building for the first time, run the verification script to ensure your environment is properly configured:**

### Windows (PowerShell)
```powershell
.\scripts\verify-build-environment.ps1

# With automatic fixes
.\scripts\verify-build-environment.ps1 -FixIssues

# Clean CMake cache
.\scripts\verify-build-environment.ps1 -CleanCache

# Verbose output
.\scripts\verify-build-environment.ps1 -Verbose
```

### macOS/Linux
```bash
./scripts/verify-build-environment.sh

# With automatic fixes
./scripts/verify-build-environment.sh --fix

# Clean CMake cache
./scripts/verify-build-environment.sh --clean

# Verbose output
./scripts/verify-build-environment.sh --verbose
```

**The verification script checks:**
- ✓ CMake 3.16+ installation
- ✓ Git and submodule synchronization
- ✓ C++ compiler (GCC 13+, Clang 16+, MSVC 2019+)
- ✓ Platform-specific dependencies (GTK+3 on Linux, Xcode on macOS)
- ✓ CMake cache freshness
- ✓ Dependency compatibility (gRPC, httplib, nlohmann/json)
- ✓ Visual Studio 2022 detection (Windows only)

## Quick Start

### macOS (Apple Silicon)
```bash
# Verify environment first
./scripts/verify-build-environment.sh

# Build
cmake --preset debug
cmake --build build
```

### Linux
```bash
# Verify environment first
./scripts/verify-build-environment.sh

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Windows (Visual Studio CMake Workflow)
```powershell
# Verify environment first
.\scripts\verify-build-environment.ps1

# Recommended: Use Visual Studio's built-in CMake support
# 1. Open Visual Studio 2022
# 2. Select "Open a local folder" 
# 3. Navigate to the yaze directory
# 4. Visual Studio will detect CMakeLists.txt automatically
# 5. Select configuration (Debug/Release) from the dropdown
# 6. Press F5 to build and run

# Alternative: Command line build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Minimal Build (CI/Fast)
```bash
cmake -B build -DYAZE_MINIMAL_BUILD=ON
cmake --build build
```

## Dependencies

### Required
- CMake 3.16+
- C++23 compiler (GCC 13+, Clang 16+, MSVC 2019+)
- Git with submodule support

### Bundled Libraries (Header-Only & Source)
**Core Libraries:**
- **SDL2** - Graphics and input handling
- **ImGui** - Immediate mode GUI framework
- **Abseil** - Google's C++ standard library extensions
- **Asar** - 65816 assembler for SNES
- **GoogleTest** - C++ testing framework

**Third-Party Header-Only Libraries:**
- **nlohmann/json** (third_party/json) - Modern C++ JSON library
- **cpp-httplib** (third_party/httplib) - HTTP client/server library

**Optional Libraries:**
- **Native File Dialog Extended (NFD)** - Native file dialogs (excluded in minimal builds)
- **gRPC** - Remote procedure call framework (optional, disabled by default)

### Dependency Isolation

YAZE uses careful dependency isolation to prevent conflicts:

**gRPC Configuration (when enabled with -DYAZE_WITH_GRPC=ON):**
- Uses FetchContent to build from source
- Isolated from system protobuf/abseil installations
- Automatically disables system package detection
- Compatible with Clang 18 and C++23

**Header-Only Libraries:**
- nlohmann/json and cpp-httplib are header-only
- No linking required, zero binary conflicts
- Included via git submodules in third_party/

**Submodule Management:**
All dependencies are included as git submodules for:
- ✅ Version consistency across all platforms
- ✅ No external package manager required
- ✅ Reproducible builds
- ✅ Offline development capability

## Platform Setup

### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Optional: Install Homebrew dependencies (auto-detected)
brew install cmake pkg-config
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libgtk-3-dev libdbus-1-dev
```

### Windows

YAZE provides comprehensive Windows support with Visual Studio integration, automated environment verification, and detailed troubleshooting guides.

#### Quick Start

```cmd
# 1. Verify environment
.\scripts\verify-build-environment.ps1

# 2. Build (choose one)
# Basic editor:
cmake --preset windows-debug
cmake --build build --config Debug

# With AI features:
cmake --preset windows-ai-debug
cmake --build build --config Debug

# 3. Run
build\bin\Debug\yaze.exe
```

#### Requirements

**Essential:**
- **Windows 10/11** (64-bit)
- **Visual Studio 2022** or **Visual Studio 2019** with:
  - Desktop development with C++
  - C++ CMake tools for Windows
  - Windows 10/11 SDK
- **Git** for cloning and submodules

**Optional:**
- **vcpkg** for easier dependency management
- **Node.js** for collaboration server features

#### Build Options

YAZE offers multiple build configurations for different use cases:

**Option A: Basic Editor (Recommended for first build)**
```cmd
cmake --preset windows-debug
cmake --build build --config Debug
```
- Full ROM editor with all core features
- Fastest build time (~5 minutes)
- No external dependencies beyond Visual Studio

**Option B: Editor with AI Features**
```cmd
cmake --preset windows-ai-debug
cmake --build build --config Debug
```
- ROM editor + AI chat assistant + code generation
- Requires bundled JSON library (included)
- Build time: ~7 minutes

**Option C: Full Build (AI + Collaboration)**
```cmd
cmake --preset windows-collab-debug
cmake --build build --config Debug
```
- Everything + network collaboration + automated testing
- Compiles gRPC from source
- First build: 15-20 minutes (subsequent builds much faster)

#### Available CMake Presets

| Preset | JSON | gRPC | Use Case | Build Time |
|--------|------|------|----------|------------|
| `windows-debug` | ❌ | ❌ | Basic ROM editing | ~5 min |
| `windows-release` | ❌ | ❌ | Production build | ~5 min |
| `windows-dev` | ❌ | ❌ | Core development | ~5 min |
| `windows-ai-debug` | ✅ | ❌ | AI features | ~7 min |
| `windows-ai-release` | ✅ | ❌ | AI production | ~7 min |
| `windows-collab-debug` | ✅ | ✅ | Full features + collaboration | ~20 min* |
| `windows-arm64-debug` | ❌ | ❌ | ARM64 Windows | ~5 min |

*First build only; subsequent builds: ~1 min

#### vcpkg Integration (Optional)

**Setup vcpkg for SDL2:**
```powershell
# PowerShell
.\scripts\setup-vcpkg-windows.ps1

# Command Prompt
.\scripts\setup-vcpkg-windows.bat

# Install dependencies
vcpkg install sdl2:x64-windows yaml-cpp:x64-windows
```

**Note**: vcpkg is optional. YAZE bundles most dependencies and can build without it.

#### Platform-Specific Considerations

**Windows Compatibility Features:**
- ✅ Native path separator handling via `std::filesystem`
- ✅ Platform-specific process management (`tasklist` vs `pgrep`)
- ✅ Correct home directory resolution (`%USERPROFILE%` vs `$HOME`)
- ✅ MSVC-compatible warning directives
- ✅ Cross-platform thread management with `std::thread`
- ✅ Unicode support for file paths and names

**Known Platform Differences:**
- **File Locking**: Windows has stricter file locking than Unix
- **Process Spawning**: Uses `cmd.exe` instead of `bash`
- **Background Processes**: Server runs in foreground (no `&` operator)
- **Path Case Sensitivity**: NTFS is case-preserving but case-insensitive

**Supported Architectures:**
- **x64 (64-bit)**: Primary target for modern systems
- **ARM64**: For ARM-based Windows devices (Surface Pro X, etc.)
- **x86 (32-bit)**: Not supported

## Build Targets

### Applications
- **yaze**: Main GUI editor application
- **z3ed**: Command-line interface tool

### Libraries  
- **yaze_c**: C API library for extensions
- **asar-static**: 65816 assembler library

### Development (Debug Builds Only)
- **yaze_emu**: Standalone SNES emulator
- **yaze_test**: Comprehensive test suite

## Build Configurations

### Debug (Full Features)
```bash
cmake --preset debug  # macOS
# OR
cmake -B build -DCMAKE_BUILD_TYPE=Debug  # All platforms
```
**Includes**: NFD, ImGuiTestEngine, PNG support, emulator, all tools

### Minimal (CI/Fast Builds)
```bash
cmake -B build -DYAZE_MINIMAL_BUILD=ON
```
**Excludes**: Emulator, CLI tools, UI tests, optional dependencies

### Release
```bash
cmake --preset release  # macOS
# OR  
cmake -B build -DCMAKE_BUILD_TYPE=Release  # All platforms
```

## Testing System

YAZE includes a comprehensive testing system with different test categories designed for various use cases:

### Test Categories

#### Unit Tests
- **Core functionality**: AsarWrapper, ROM operations, SNES tiles, palettes
- **Graphics**: Compression, tile unpacking, color conversion
- **Zelda3 components**: Message system, overworld, object parsing
- **Location**: `test/unit/`

#### Integration Tests
- **ASAR integration**: Assembly compilation and ROM patching
- **Editor integration**: Tile16 editor, dungeon editor
- **Location**: `test/integration/`

#### End-to-End (E2E) Tests
- **ROM-dependent tests**: Load, edit, save, reload, verify integrity
- **ZSCustomOverworld upgrade tests**: Vanilla → v2 → v3 upgrade paths
- **Location**: `test/e2e/`

### Running Tests

#### Local Development
```bash
# Run all tests
./build/test/yaze_test

# Run specific test categories
./build/test/yaze_test --unit
./build/test/yaze_test --integration
./build/test/yaze_test --e2e --rom-path zelda3.sfc

# Run with verbose output
./build/test/yaze_test --verbose

# Get help
./build/test/yaze_test --help
```

#### CI/CD Environment
The CI builds use a simplified test executable (`yaze_test_ci.cc`) that:
- Excludes ROM-dependent tests (no ROM files available)
- Excludes E2E tests (require actual ROM files)
- Focuses on unit tests and core functionality
- Uses minimal configuration for reliability

#### ROM-Dependent Tests
These tests require actual ROM files and are only available in local development:

```bash
# E2E ROM tests (requires zelda3.sfc)
./build/test/yaze_test --e2e --rom-path zelda3.sfc

# ZSCustomOverworld upgrade tests
./build/test/yaze_test --zscustomoverworld --rom-path zelda3.sfc
```

**Note**: ROM-dependent tests are automatically skipped in CI builds and minimal builds.

### Test Organization

```
test/
├── unit/                    # Unit tests (CI-safe)
│   ├── core/               # Core functionality tests
│   ├── gfx/                # Graphics tests
│   └── zelda3/             # Zelda3-specific tests
├── integration/            # Integration tests (CI-safe)
├── e2e/                    # End-to-end tests (ROM-dependent)
│   ├── rom_dependent/      # ROM load/save/edit tests
│   └── zscustomoverworld/  # Upgrade path tests
├── mocks/                  # Mock objects for testing
├── assets/                 # Test assets and ROMs
└── deprecated/             # Outdated tests (moved from emu/)
```

### Test Executables

#### Development Build (`yaze_test.cc`)
- Full argument parsing for AI agents
- SDL initialization for graphics tests
- Support for all test categories
- ROM path configuration
- Verbose output options

#### CI Build (`yaze_test_ci.cc`)
- Simplified main function
- No SDL initialization
- Automatic exclusion of ROM-dependent tests
- Minimal configuration for reliability
- Used when `YAZE_MINIMAL_BUILD=ON`

### Test Configuration

#### CMake Options
```bash
# Enable/disable test categories
-DYAZE_ENABLE_ROM_TESTS=ON          # Enable ROM-dependent tests
-DYAZE_ENABLE_UI_TESTS=ON           # Enable ImGui Test Engine
-DYAZE_ENABLE_EXPERIMENTAL_TESTS=ON # Enable experimental tests
-DYAZE_MINIMAL_BUILD=ON             # Use CI test executable
```

#### Test Filters
The test system supports Google Test filters for selective execution:

```bash
# Run only core tests
./build/test/yaze_test --gtest_filter="*Core*"

# Exclude ROM tests
./build/test/yaze_test --gtest_filter="-*RomTest*"

# Run specific test suite
./build/test/yaze_test --gtest_filter="AsarWrapperTest.*"
```

## IDE Integration

### Visual Studio (Windows)
**CMake Workflow (Recommended):**
1. **File → Open → Folder**
2. Navigate to yaze directory
3. Visual Studio detects `CMakeLists.txt` automatically
4. Select configuration from toolbar (Debug/Release)
5. Press F5 to build and run

**Why CMake Mode?**
- ✅ **No project generation** - CMake files are the source of truth
- ✅ **Always in sync** - Changes to CMakeLists.txt reflect immediately
- ✅ **Better IntelliSense** - Direct CMake target understanding
- ✅ **Native CMake support** - Modern Visual Studio feature
- ✅ **Cross-platform consistency** - Same workflow on all platforms

**Features:**
- Full IntelliSense support
- Integrated debugging with breakpoints
- Automatic vcpkg dependency management (if configured)
- Multi-architecture support (x64, x86, ARM64)
- CMakeSettings.json for custom configurations

### VS Code
1. Install CMake Tools extension
2. Open project, select "Debug" preset
3. Language server uses `compile_commands.json` automatically

### CLion
- Opens CMake projects directly
- Select Debug configuration

### Xcode (macOS)
```bash
cmake --preset debug -G Xcode
open build/yaze.xcodeproj
```

## Windows Development Scripts

### vcpkg Setup (Optional)
- **`setup-vcpkg-windows.ps1`**: Setup vcpkg for SDL2 dependency
- **`setup-vcpkg-windows.bat`**: Batch version of vcpkg setup

**Usage:**
```powershell
# Setup vcpkg (optional - for SDL2 via vcpkg instead of bundled)
.\scripts\setup-vcpkg-windows.ps1

# Or using batch
.\scripts\setup-vcpkg-windows.bat
```

## Features by Build Type

| Feature | Debug | Release | Minimal (CI) |
|---------|-------|---------|--------------|
| GUI Editor | ✅ | ✅ | ✅ |
| Native File Dialogs | ✅ | ✅ | ❌ |
| PNG Support | ✅ | ✅ | ❌ |
| Emulator | ✅ | ✅ | ❌ |
| CLI Tools | ✅ | ✅ | ❌ |
| Test Suite | ✅ | ❌ | ✅ (limited) |
| UI Testing | ✅ | ❌ | ❌ |
| ROM Tests | ✅ | ❌ | ❌ |

## CMake Compatibility

### Submodule Compatibility Issues
YAZE includes several submodules (abseil-cpp, SDL) that may have CMake compatibility issues. The project automatically handles these with:

**Automatic Policy Management:**
- `CMAKE_POLICY_VERSION_MINIMUM=3.5` (handles SDL requirements)
- `CMAKE_POLICY_VERSION_MAXIMUM=3.28` (prevents future issues)
- `CMAKE_WARN_DEPRECATED=OFF` (suppresses submodule warnings)
- `ABSL_PROPAGATE_CXX_STD=ON` (handles Abseil C++ standard propagation)
- `THREADS_PREFER_PTHREAD_FLAG=OFF` (fixes Windows pthread issues)

**Manual Configuration (if needed):**
```bash
cmake -B build \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_POLICY_VERSION_MAXIMUM=3.28 \
  -DCMAKE_WARN_DEPRECATED=OFF \
  -DABSL_PROPAGATE_CXX_STD=ON \
  -DTHREADS_PREFER_PTHREAD_FLAG=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

## CI/CD and Release Builds

### GitHub Actions Workflows

The project includes comprehensive CI/CD workflows:

- **`ci.yml`**: Multi-platform CI with test execution
- **`release.yml`**: Automated release builds with packaging

### Test Execution in CI

**CI Test Strategy:**
- **Core Tests**: Always run (AsarWrapper, SnesTile, Compression, SnesPalette, Hex, Message)
- **Unit Tests**: Run with `continue-on-error=true` for information
- **ROM Tests**: Automatically excluded (no ROM files available)
- **E2E Tests**: Automatically excluded (require ROM files)

**Test Filters in CI:**
```bash
# Core tests (must pass)
ctest -R "AsarWrapperTest|SnesTileTest|CompressionTest|SnesPaletteTest|HexTest|MessageTest"

# Additional unit tests (informational)
ctest -R ".*Test" -E ".*RomTest.*|.*E2E.*|.*ZSCustomOverworld.*|.*IntegrationTest.*"
```

### vcpkg Fallback Mechanisms

All Windows CI/CD builds include automatic fallback mechanisms:

**When vcpkg succeeds:**
- Full build with all dependencies (SDL2)
- Complete feature set available

**When vcpkg fails (network issues):**
- Automatic fallback to minimal build configuration
- Uses source-built dependencies (Abseil, etc.)
- Still produces functional executables

### Supported Architectures

**Windows:**
- x64 (64-bit) - Primary target for modern systems
- ARM64 - For ARM-based Windows devices (Surface Pro X, etc.)

**macOS:**
- Universal binary (Apple Silicon + Intel)

**Linux:**
- x64 (64-bit)

## Windows Troubleshooting

This section provides detailed solutions for common Windows build issues.

### Quick Diagnostics

Always start with the verification script to identify issues:

```cmd
.\scripts\verify-build-environment.ps1

# With verbose output
.\scripts\verify-build-environment.ps1 -Verbose

# With automatic fixes
.\scripts\verify-build-environment.ps1 -FixIssues
```

### Common Errors and Solutions

#### Error 1: "nlohmann/json.hpp: No such file or directory"

**Full Error:**
```
fatal error C1083: Cannot open include file: 'nlohmann/json.hpp': No such file or directory
```

**Cause**: Building code that uses JSON without enabling JSON support.

**Solutions:**

1. **Use AI preset** (enables JSON automatically):
   ```cmd
   cmake --preset windows-ai-debug
   cmake --build build --config Debug
   ```

2. **Enable JSON manually**:
   ```cmd
   cmake --preset windows-debug -DYAZE_WITH_JSON=ON
   cmake --build build --config Debug
   ```

3. **Verify JSON library exists**:
   ```cmd
   dir third_party\json\include\nlohmann\json.hpp
   ```
   
4. **Initialize submodules** (if file is missing):
   ```cmd
   git submodule update --init --recursive
   ```

5. **Build without AI** (if you don't need it):
   ```cmd
   cmake --preset windows-debug
   cmake --build build --config Debug
   ```

#### Error 2: "Cannot find nlohmann_json::nlohmann_json target"

**Full Error:**
```
CMake Error: The following imported targets are referenced, but are missing:
nlohmann_json::nlohmann_json
```

**Cause**: CMake isn't creating the JSON library target.

**Solutions:**

1. **Clean and rebuild**:
   ```cmd
   rmdir /s /q build
   cmake --preset windows-ai-debug
   cmake --build build --config Debug
   ```

2. **Check CMake output**:
   ```cmd
   cmake --preset windows-ai-debug 2>&1 | findstr "json"
   ```
   Should show: `✓ JSON support enabled (nlohmann/json)`

3. **Verify submodule**:
   ```cmd
   git submodule status third_party/json
   ```
   Should show a commit hash (no `-` prefix)

#### Error 3: "vcpkg toolchain file not found"

**Full Error:**
```
CMake Warning: CMAKE_TOOLCHAIN_FILE is set but file does not exist
```

**Cause**: Preset expects vcpkg but it's not installed.

**Solution 1: Use bundled dependencies** (recommended):
```cmd
cmake --preset windows-debug -DCMAKE_TOOLCHAIN_FILE=""
cmake --build build --config Debug
```

**Solution 2: Install vcpkg**:
```cmd
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"

# Restart shell, then:
vcpkg install sdl2:x64-windows yaml-cpp:x64-windows
cmake --preset windows-debug
```

**Solution 3: Point to your vcpkg installation**:
```cmd
cmake --preset windows-debug ^
  -DCMAKE_TOOLCHAIN_FILE="D:\your\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

#### Error 4: "SDL2 not found"

**Full Error:**
```
CMake Error: Could not find a package configuration file provided by "SDL2"
```

**Solution 1: With vcpkg**:
```cmd
vcpkg install sdl2:x64-windows
cmake --preset windows-debug
```

**Solution 2: Manual installation**:
1. Download SDL2 from https://libsdl.org/download-2.0.php
2. Choose "SDL2-devel-2.x.x-VC.zip"
3. Extract to `C:\SDL2`
4. Set environment variable:
   ```cmd
   setx SDL2_DIR "C:\SDL2\cmake"
   ```
5. Restart shell and rebuild

#### Error 5: Build is Very Slow (15-20 minutes)

**Cause**: Building with gRPC enabled compiles gRPC + Protobuf from source.

**Solutions:**

1. **Be patient**: First build is slow, subsequent builds are ~1 minute
2. **Use precompiled gRPC via vcpkg**:
   ```cmd
   vcpkg install grpc:x64-windows
   cmake --preset windows-collab-debug
   ```
   (Note: vcpkg gRPC install also takes 15-20 mins, but only once)

3. **Build without gRPC** (if you don't need collaboration):
   ```cmd
   cmake --preset windows-ai-debug
   ```

4. **Use parallel builds**:
   ```cmd
   cmake --build build --config Debug -j 12
   ```

#### Error 6: "Cannot open file 'yaze.exe': Permission denied"

**Full Error:**
```
LINK : fatal error LNK1168: cannot open yaze.exe for writing
```

**Cause**: Previous instance of `yaze.exe` is still running.

**Solutions:**

1. **Kill all instances**:
   ```cmd
   taskkill /F /IM yaze.exe
   cmake --build build --config Debug
   ```

2. **Use Task Manager**:
   - Open Task Manager (Ctrl+Shift+Esc)
   - Find `yaze.exe` processes
   - End tasks
   - Rebuild

#### Error 7: "C++ standard 'cxx_std_23' not supported"

**Full Error:**
```
CMake Error: The C++ compiler does not support C++23
```

**Cause**: Visual Studio version is too old.

**Solutions:**

1. **Update Visual Studio**:
   - Open Visual Studio Installer
   - Update to latest version (VS2022 17.4+ recommended)
   - Ensure "C++ CMake tools" workload is installed

2. **Temporary workaround** (use C++20):
   Edit `CMakeLists.txt` line 106:
   ```cmake
   set(CMAKE_CXX_STANDARD 20)  # Changed from 23
   ```
   (May cause compilation issues with newer features)

#### Error 8: Visual Studio Can't Find CMakePresets.json

**Observation**: VS doesn't show any build configurations.

**Solutions:**

1. **Reload project**:
   - File → Close Folder
   - File → Open → Folder
   - Select yaze directory
   - Wait for CMake to configure

2. **Use CMake GUI** (alternative):
   - Open CMake GUI
   - Set source: `<path to yaze>`
   - Set build: `<path to yaze>/build`
   - Click Configure
   - Choose "Visual Studio 17 2022"
   - Set options (YAZE_WITH_JSON, etc.)
   - Click Generate
   - Open `build/yaze.sln` in VS

3. **Check CMakePresets.json exists**:
   ```cmd
   dir CMakePresets.json
   ```

#### Error 9: "error MSB8066: Custom build exited with code 1"

**Full Error:**
```
error MSB8066: Custom build for 'CMakeLists.txt' exited with code 1
```

**Cause**: CMake configuration failed, but VS doesn't show the actual error.

**Solutions:**

1. **Run CMake from command line**:
   ```cmd
   cmake --preset windows-debug 2>&1 | more
   ```
   This shows the actual CMake error.

2. **Check CMake output in VS**:
   - View → Output
   - Show output from: CMake
   - Scroll up to find actual error message

3. **Clean CMake cache**:
   ```cmd
   rmdir /s /q build
   cmake --preset windows-debug
   ```

### Debugging CMake Configuration

**Enable verbose CMake output:**
```cmd
cmake --preset windows-debug --trace-expand > cmake_trace.txt
```
Look through `cmake_trace.txt` for errors.

**Check what's being built:**
```cmd
cmake --preset windows-debug
cmake --build build --config Debug --verbose
```

**Verify preset settings:**
```cmd
cmake --preset windows-debug --trace-expand | findstr "YAZE_WITH_JSON"
cmake --preset windows-debug --trace-expand | findstr "Z3ED_AI"
```

### Tips for Successful Builds

1. **Start simple**:
   ```cmd
   cmake --preset windows-debug
   cmake --build build --config Debug
   ```

2. **Add features incrementally**:
   ```cmd
   # Once basic build works, add JSON:
   cmake --preset windows-ai-debug
   cmake --build build --config Debug
   
   # Then add gRPC if needed:
   cmake --preset windows-collab-debug
   cmake --build build --config Debug
   ```

3. **Clean between major changes**:
   ```cmd
   rmdir /s /q build
   cmake --preset <new-preset>
   cmake --build build --config Debug
   ```

4. **Use parallel builds**:
   ```cmd
   cmake --build build --config Debug -j 12
   ```

5. **Watch for warnings**:
   ```cmd
   cmake --build build --config Debug 2>&1 | findstr "warning"
   ```

### Still Having Issues?

**Collect diagnostic information:**

1. Run verification script:
   ```cmd
   .\scripts\verify-build-environment.ps1 -Verbose > diagnostic.txt
   ```

2. Get CMake configuration:
   ```cmd
   cmake --preset windows-debug 2>&1 >> diagnostic.txt
   ```

3. Get build output:
   ```cmd
   cmake --build build --config Debug 2>&1 >> diagnostic.txt
   ```

**Create a GitHub Issue** with:
- Windows version (run `winver`)
- Visual Studio version
- Contents of `diagnostic.txt`
- Which preset you're using
- Full error message

## Contributing

### Code Style

- **C++23**: Use modern language features
- **Google C++ Style**: Follow Google C++ style guide
- **Naming**: Use descriptive names, avoid abbreviations

### Error Handling

- **Use absl::Status** for error handling
- **Use absl::StatusOr** for operations that return values

### Pull Request Process

1. **Run tests**: Ensure all stable tests pass (`ctest --preset stable`)
2. **Check formatting**: Use `clang-format`
3. **Update documentation**: If your changes affect behavior or APIs

### Commit Messages

```
type(scope): brief description

Detailed explanation of changes.

Fixes #issue_number
```