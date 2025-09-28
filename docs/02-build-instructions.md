# Build Instructions

YAZE uses CMake 3.16+ with modern target-based configuration. The project includes comprehensive Windows support with Visual Studio integration, vcpkg package management, and automated setup scripts.

## Quick Start

### macOS (Apple Silicon)
```bash
cmake --preset debug
cmake --build build
```

### Linux
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Windows (Recommended)
```powershell
# Automated setup (first time only)
.\scripts\setup-windows-dev.ps1

# Generate Visual Studio projects (with proper vcpkg integration)
python scripts/generate-vs-projects.py

# Build with Clang (recommended for better Abseil compatibility)
.\scripts\build-windows.ps1 -Compiler clang

# Or use CMake directly with Clang
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build
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

### Bundled Libraries
- SDL2, ImGui, Abseil, Asar, GoogleTest
- Native File Dialog Extended (NFD)
- All dependencies included in repository

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

#### Automated Setup (Recommended)
The project includes comprehensive setup scripts for Windows development:

```powershell
# Complete development environment setup
.\scripts\setup-windows-dev.ps1

# Generate Visual Studio project files (with proper vcpkg integration)
python scripts/generate-vs-projects.py

# Test CMake configuration
.\scripts\test-cmake-config.ps1
```

**What the setup script installs:**
- Chocolatey package manager
- CMake 3.16+
- Git, Ninja, Python 3
- Visual Studio 2022 detection and verification

#### Manual Setup Options

**Option 1 - Minimal (CI/Fast Builds):**
- Visual Studio 2019+ with C++ CMake tools
- No additional dependencies needed (all bundled)

**Option 2 - Full Development with vcpkg:**
- Visual Studio 2019+ with C++ CMake tools
- vcpkg package manager for dependency management

#### vcpkg Integration

**Automatic Setup:**
```powershell
# PowerShell
.\scripts\setup-windows-dev.ps1

# Command Prompt
scripts\setup-windows-dev.bat
```

**Manual vcpkg Setup:**
```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
set VCPKG_ROOT=%CD%
```

**Dependencies (vcpkg.json):**
- sdl2 (graphics/input with Vulkan support)

**Note**: Abseil and gtest are built from source via CMake rather than through vcpkg to avoid compatibility issues.

#### Windows Build Commands

**Using CMake Presets:**
```cmd
# Debug build (minimal, no tests)
cmake --preset windows-debug
cmake --build build --preset windows-debug

# Development build (includes Google Test)
cmake --preset windows-dev
cmake --build build --preset windows-dev

# Release build (optimized, no tests)
cmake --preset windows-release
cmake --build build --preset windows-release
```

**Using Visual Studio Projects:**
```powershell
# Generate project files (with proper vcpkg integration)
python scripts/generate-vs-projects.py

# Open YAZE.sln in Visual Studio
# Select configuration (Debug/Release) and platform (x64/x86/ARM64)
# Press F5 to build and run
```

**Build Types:**
- **windows-debug**: Minimal debug build, no Google Test
- **windows-dev**: Development build with Google Test and ROM testing
- **windows-release**: Optimized release build, no Google Test

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
**Recommended approach:**
```powershell
# Setup development environment
.\scripts\setup-windows-dev.ps1

# Generate Visual Studio project files (with proper vcpkg integration)
python scripts/generate-vs-projects.py

# Open YAZE.sln in Visual Studio 2022
# Select configuration (Debug/Release) and platform (x64/x86/ARM64)
# Press F5 to build and run
```

**Features:**
- Full IntelliSense support
- Integrated debugging
- Automatic vcpkg dependency management (SDL2)
- Multi-platform support (x64, ARM64)
- Automatic asset copying
- Generated project files stay in sync with CMake configuration

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

The project includes several PowerShell and Batch scripts to streamline Windows development:

### Setup Scripts
- **`setup-windows-dev.ps1`**: Complete development environment setup
- **`setup-windows-dev.bat`**: Batch version of setup script

**What they install:**
- Chocolatey package manager
- CMake 3.16+
- Git, Ninja, Python 3
- Visual Studio 2022 detection

### Project Generation Scripts
- **`generate-vs-projects.py`**: Generate Visual Studio project files with proper vcpkg integration
- **`generate-vs-projects.bat`**: Batch version of project generation

**Features:**
- Automatic CMake detection and installation
- Visual Studio 2022 detection
- Multi-architecture support (x64, ARM64)
- vcpkg integration
- CMake compatibility fixes

### Testing Scripts
- **`test-cmake-config.ps1`**: Test CMake configuration without full build

**Usage:**
```powershell
# Test configuration
.\scripts\test-cmake-config.ps1

# Test with specific architecture
.\scripts\test-cmake-config.ps1 -Architecture x86

# Clean test build
.\scripts\test-cmake-config.ps1 -Clean
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

## Troubleshooting

### Windows CMake Issues

**CMake Not Found:**
```powershell
# Run the setup script
.\scripts\setup-windows-dev.ps1

# Or install manually via Chocolatey
choco install cmake
```

**Submodule Compatibility Errors:**
```powershell
# Test CMake configuration
.\scripts\test-cmake-config.ps1

# Clean build with compatibility flags
Remove-Item -Recurse -Force build
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_WARN_DEPRECATED=OFF
```

**Visual Studio Project Issues:**
```powershell
# Regenerate project files
.\scripts\generate-vs-projects.ps1

# Clean and rebuild
Remove-Item -Recurse -Force build
cmake --preset windows-debug
```

### vcpkg Issues

**Dependencies Not Installing:**
```cmd
# Check vcpkg installation
vcpkg version

# Reinstall dependencies
vcpkg install --triplet x64-windows sdl2

# Check installed packages
vcpkg list
```

**Network/Download Failures:**
- The CI/CD workflows automatically fall back to minimal builds
- For local development, ensure stable internet connection
- If vcpkg consistently fails, use minimal build mode:
  ```bash
  cmake -B build -DYAZE_MINIMAL_BUILD=ON
  ```

**Visual Studio Integration:**
```cmd
# Re-integrate vcpkg
cd C:\vcpkg
.\vcpkg.exe integrate install
```

**ZLIB or Other Dependencies Not Found:**
```bash
# Regenerate project files with proper vcpkg integration
python scripts/generate-vs-projects.py

# Ensure vcpkg is properly set up
.\scripts\setup-windows-dev.ps1
```

### Test Issues

**Test Discovery Failures:**
```bash
# Use CI test executable for minimal builds
cmake -B build -DYAZE_MINIMAL_BUILD=ON
cmake --build build

# Check test executable
./build/test/yaze_test --help
```

**ROM Test Failures:**
```bash
# Ensure ROM file exists
ls -la zelda3.sfc

# Run with explicit ROM path
./build/test/yaze_test --e2e --rom-path zelda3.sfc
```

**SDL Initialization Errors:**
- These are expected in CI builds
- Use minimal build configuration for CI compatibility
- For local development, ensure SDL2 is properly installed

### Architecture Errors (macOS)
```bash
# Clean and use ARM64-only preset
rm -rf build
cmake --preset debug  # Uses arm64 only
```

### Missing Headers (Language Server)
```bash
# Regenerate compile commands
cmake --preset debug
cp build/compile_commands.json .
# Restart VS Code
```

### CI Build Failures
Use minimal build configuration that matches CI:
```bash
cmake -B build -DYAZE_MINIMAL_BUILD=ON -DYAZE_ENABLE_UI_TESTS=OFF
cmake --build build
```

### Common Error Solutions

**"CMake Deprecation Warning" from submodules:**
- This is automatically handled by the project's CMake configuration
- If you see these warnings, they can be safely ignored

**"pthread_create not found" on Windows:**
- The project automatically sets `THREADS_PREFER_PTHREAD_FLAG=OFF`
- This is normal for Windows builds

**"Abseil C++ std propagation" warnings:**
- Automatically handled with `ABSL_PROPAGATE_CXX_STD=ON`
- Ensures proper C++ standard handling

**Visual Studio "file not found" errors:**
- Run `python scripts/generate-vs-projects.py` to regenerate project files
- Ensure CMake configuration completed successfully first

**CI/CD Build Failures:**
- Check if vcpkg download failed (network issues)
- The workflows automatically fall back to minimal builds
- For persistent issues, check the workflow logs for specific error messages

**Test Executable Issues:**
- Ensure the correct test executable is being used (CI vs development)
- Check that test filters are properly configured
- Verify that ROM-dependent tests are excluded in CI builds