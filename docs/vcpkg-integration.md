# vcpkg Integration for Windows Builds

> **Note**: This document provides detailed vcpkg information. For the most up-to-date build instructions, see [Build Instructions](02-build-instructions.md).

This document describes how to use vcpkg for Windows builds in Visual Studio with YAZE.

## Overview

vcpkg is Microsoft's C++ package manager that simplifies dependency management for Windows builds. YAZE now includes full vcpkg integration with manifest mode support for automatic dependency resolution.

## Features

- **Manifest Mode**: Dependencies are automatically managed via `vcpkg.json`
- **Visual Studio Integration**: Seamless integration with Visual Studio 2022
- **Generated Project Files**: Visual Studio project files with proper vcpkg integration
- **CMake Presets**: Pre-configured build presets for Windows
- **Automatic Setup**: Setup scripts for easy vcpkg installation

## Quick Start

### 1. Setup vcpkg

Run the automated setup script:
```powershell
# PowerShell (recommended)
.\scripts\setup-windows-dev.ps1
```

This will:
- Set up vcpkg
- Install dependencies (SDL2)
- Generate Visual Studio project files with proper vcpkg integration

### 2. Build with Visual Studio

```powershell
# Generate project files (if not already done)
python scripts/generate-vs-projects.py

# Open YAZE.sln in Visual Studio 2022
# Select configuration and platform, then build
```

### 3. Alternative: Build with CMake

Use the Windows presets in CMakePresets.json:

```cmd
# Debug build
cmake --preset windows-debug
cmake --build build --preset windows-debug

# Release build
cmake --preset windows-release
cmake --build build --preset windows-release
```

## Configuration Details

### vcpkg.json Manifest

The `vcpkg.json` file defines all dependencies:

```json
{
  "name": "yaze",
  "version": "0.3.1",
  "description": "Yet Another Zelda3 Editor",
  "dependencies": [
    {
      "name": "zlib",
      "platform": "!uwp"
    },
    {
    {
      "name": "sdl2",
      "platform": "!uwp",
      "features": ["vulkan"]
    }
  ],
  "builtin-baseline": "2024.12.12"
}
```

### CMake Configuration

vcpkg integration is handled in several files:

- **CMakeLists.txt**: Automatic toolchain detection
- **cmake/vcpkg.cmake**: vcpkg-specific settings
- **CMakePresets.json**: Windows build presets

### Build Presets

Available Windows presets:

- `windows-debug`: Debug build with vcpkg
- `windows-release`: Release build with vcpkg

## Dependencies

vcpkg automatically installs these dependencies:

- **sdl2**: Graphics and input handling (with Vulkan support)

**Note**: Abseil and gtest are now built from source via CMake rather than through vcpkg to avoid compatibility issues.

## Environment Variables

Set `VCPKG_ROOT` to point to your vcpkg installation:

```cmd
set VCPKG_ROOT=C:\path\to\vcpkg
```

## Troubleshooting

### Common Issues

1. **vcpkg not found**: Ensure `VCPKG_ROOT` is set or vcpkg is in the project directory
2. **Dependencies not installing**: Check internet connection and vcpkg bootstrap
3. **Visual Studio integration**: Run `vcpkg integrate install` from vcpkg directory

### Manual Setup

If automated setup fails:

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
```

## Benefits

- **Consistent Dependencies**: Same versions across development environments
- **Easy Updates**: Update dependencies via vcpkg.json
- **CI/CD Friendly**: Reproducible builds
- **Visual Studio Integration**: Native IntelliSense support
- **No Manual Downloads**: Automatic dependency resolution

## Advanced Usage

### Custom Triplets

Override the default x64-windows triplet:

```cmd
cmake --preset windows-debug -DVCPKG_TARGET_TRIPLET=x86-windows
```

### Static Linking

For static builds, modify `cmake/vcpkg.cmake`:

```cmake
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CRT_LINKAGE static)
```
