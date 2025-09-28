# Build Instructions

YAZE uses CMake 3.16+ with modern target-based configuration. For VSCode users, install the CMake extensions:
- https://marketplace.visualstudio.com/items?itemName=twxs.cmake
- https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

## Quick Start

### macOS (Apple Silicon)
```bash
cmake --preset debug
cmake --build build
```

### Linux / Windows
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Minimal Build
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
**Option 1 - Minimal (Recommended for CI):**
- Visual Studio 2019+ with C++ CMake tools
- No additional dependencies needed (all bundled)

**Option 2 - Full Development with vcpkg (Recommended):**
- Visual Studio 2019+ with C++ CMake tools
- Install vcpkg and dependencies from `vcpkg.json`

#### vcpkg Setup (Option 2)
Run the setup script to automatically configure vcpkg:
```cmd
# Command Prompt
scripts\setup-vcpkg-windows.bat

# PowerShell
.\scripts\setup-vcpkg-windows.ps1
```

Or manually:
```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
set VCPKG_ROOT=%CD%
```

#### Windows Build Commands
```cmd
# Debug build with vcpkg (minimal, no tests)
cmake --preset windows-debug
cmake --build build --preset windows-debug

# Development build with vcpkg (includes Google Test)
cmake --preset windows-dev
cmake --build build --preset windows-dev

# Release build with vcpkg (optimized, no tests)
cmake --preset windows-release
cmake --build build --preset windows-release
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

## Troubleshooting

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
