# Build Instructions

YAZE uses CMake 3.16+ with modern target-based configuration. For VSCode users, install the CMake extensions:
- https://marketplace.visualstudio.com/items?itemName=twxs.cmake
- https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

## Quick Build

```bash
# Development build
cmake --preset dev
cmake --build --preset dev

# CI build (minimal dependencies)
cmake --preset ci
cmake --build --preset ci
```

## Dependencies

### Core Dependencies
- **SDL2**: Graphics and input library
- **ImGui**: Immediate mode GUI library with docking support
- **Abseil**: Modern C++ utilities library
- **libpng**: Image processing library

### v0.3.0 Additions
- **Asar**: 65816 assembler for ROM patching
- **ftxui**: Terminal UI library for CLI
- **GoogleTest**: Comprehensive testing framework

## Platform-Specific Setup

### Windows

#### vcpkg (Recommended)
```json
{
  "dependencies": [
    "abseil", "sdl2", "libpng"
  ]
}
```

#### msys2 (Advanced)
Install packages: `mingw-w64-x86_64-gcc`, `mingw-w64-x86_64-cmake`, `mingw-w64-x86_64-sdl2`, `mingw-w64-x86_64-libpng`, `mingw-w64-x86_64-abseil-cpp`

### macOS
```bash
brew install cmake sdl2 zlib libpng abseil boost-python3
```

### Linux
Use your package manager to install the same dependencies as macOS.

### iOS
Xcode required. The xcodeproject file is in the `ios` directory. Link `SDL2.framework` and `libpng.a`.

## Build Targets

- **yaze**: Desktop GUI application
- **z3ed**: Enhanced CLI tool
- **yaze_c**: C library for extensions
- **yaze_test**: Test suite executable
- **yaze_emu**: Standalone SNES emulator
