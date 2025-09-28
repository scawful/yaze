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

# Generate Visual Studio projects
.\scripts\generate-vs-projects.ps1

# Or use CMake directly
cmake --preset windows-debug
cmake --build build --preset windows-debug
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

# Generate Visual Studio project files
.\scripts\generate-vs-projects.ps1

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
- zlib (compression)
- libpng (PNG support)
- sdl2 (graphics/input)
- abseil (Google C++ libraries)
- gtest (testing framework)

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
# Generate project files
.\scripts\generate-vs-projects.ps1

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
- Automatic vcpkg dependency management (zlib, libpng, SDL2)
- Multi-platform support (x64, x86, ARM64)
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
- Multi-architecture support (x64, x86, ARM64)
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
vcpkg install --triplet x64-windows zlib libpng sdl2 abseil gtest

# Check installed packages
vcpkg list
```

**Visual Studio Integration:**
```cmd
# Re-integrate vcpkg
cd C:\vcpkg
.\vcpkg.exe integrate install
```

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
- Run `.\scripts\generate-vs-projects.ps1` to regenerate project files
- Ensure CMake configuration completed successfully first
