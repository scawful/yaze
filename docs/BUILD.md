# YAZE Build Guide

## Quick Start

### Prerequisites

- **CMake 3.16+**
- **C++20 compatible compiler** (GCC 12+, Clang 14+, MSVC 19.30+)
- **Ninja** (recommended) or Make
- **Git** (for submodules)

### 3-Command Build

```bash
# 1. Clone and setup
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# 2. Configure
cmake --preset dev

# 3. Build
cmake --build build
```

That's it! The build system will automatically:
- Download and build all dependencies using CPM.cmake
- Configure the project with optimal settings
- Build the main `yaze` executable and libraries

## Platform-Specific Setup

### Linux (Ubuntu 22.04+)

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential ninja-build pkg-config ccache \
  libsdl2-dev libyaml-cpp-dev libgtk-3-dev libglew-dev

# Build
cmake --preset dev
cmake --build build
```

### macOS (14+)

```bash
# Install dependencies
brew install cmake ninja pkg-config ccache sdl2 yaml-cpp

# Build
cmake --preset dev
cmake --build build
```

### Windows (10/11)

```powershell
# Install dependencies via vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install packages
.\vcpkg install sdl2 yaml-cpp

# Build
cmake --preset dev
cmake --build build
```

## Build Presets

YAZE provides several CMake presets for different use cases:

| Preset | Description | Use Case |
|--------|-------------|----------|
| `dev` | Full development build | Local development |
| `ci` | CI build | Continuous integration |
| `release` | Optimized release | Production builds |
| `minimal` | Minimal build | CI without gRPC/AI |
| `coverage` | Debug with coverage | Code coverage analysis |
| `sanitizer` | Debug with sanitizers | Memory debugging |
| `verbose` | Verbose warnings | Development debugging |

### Examples

```bash
# Development build (default)
cmake --preset dev
cmake --build build

# Release build
cmake --preset release
cmake --build build

# Minimal build (no gRPC/AI)
cmake --preset minimal
cmake --build build

# Coverage build
cmake --preset coverage
cmake --build build
```

## Feature Flags

YAZE supports several build-time feature flags:

| Flag | Default | Description |
|------|---------|-------------|
| `YAZE_BUILD_GUI` | ON | Build GUI application |
| `YAZE_BUILD_CLI` | ON | Build CLI tools (z3ed) |
| `YAZE_BUILD_EMU` | ON | Build emulator components |
| `YAZE_BUILD_LIB` | ON | Build static library |
| `YAZE_BUILD_TESTS` | ON | Build test suite |
| `YAZE_ENABLE_GRPC` | ON | Enable gRPC agent support |
| `YAZE_ENABLE_JSON` | ON | Enable JSON support |
| `YAZE_ENABLE_AI` | ON | Enable AI agent features |
| `YAZE_ENABLE_LTO` | OFF | Enable link-time optimization |
| `YAZE_ENABLE_SANITIZERS` | OFF | Enable AddressSanitizer/UBSanitizer |
| `YAZE_ENABLE_COVERAGE` | OFF | Enable code coverage |
| `YAZE_MINIMAL_BUILD` | OFF | Minimal build for CI |

### Custom Configuration

```bash
# Custom build with specific features
cmake -B build -G Ninja \
  -DYAZE_ENABLE_GRPC=OFF \
  -DYAZE_ENABLE_AI=OFF \
  -DYAZE_ENABLE_LTO=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

## Testing

### Run All Tests

```bash
# Build with tests
cmake --preset dev
cmake --build build

# Run all tests
cd build
ctest --output-on-failure
```

### Run Specific Test Suites

```bash
# Stable tests only
ctest -L stable

# Unit tests only
ctest -L unit

# Integration tests only
ctest -L integration

# Experimental tests (requires ROM)
ctest -L experimental
```

### Test with ROM

```bash
# Set ROM path
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Run ROM-dependent tests
ctest -L experimental
```

## Code Quality

### Formatting

```bash
# Format code
cmake --build build --target yaze-format

# Check formatting
cmake --build build --target yaze-format-check
```

### Static Analysis

```bash
# Run clang-tidy
find src -name "*.cc" | xargs clang-tidy --header-filter='src/.*\.(h|hpp)$'

# Run cppcheck
cppcheck --enable=warning,style,performance src/
```

## Packaging

### Create Packages

```bash
# Build release
cmake --preset release
cmake --build build

# Create packages
cd build
cpack
```

### Platform-Specific Packages

| Platform | Package Types | Command |
|----------|---------------|---------|
| Linux | DEB, TGZ | `cpack -G DEB -G TGZ` |
| macOS | DMG | `cpack -G DragNDrop` |
| Windows | NSIS, ZIP | `cpack -G NSIS -G ZIP` |

## Troubleshooting

### Common Issues

#### 1. CMake Not Found

```bash
# Ubuntu/Debian
sudo apt install cmake

# macOS
brew install cmake

# Windows
# Download from https://cmake.org/download/
```

#### 2. Compiler Not Found

```bash
# Ubuntu/Debian
sudo apt install build-essential

# macOS
xcode-select --install

# Windows
# Install Visual Studio Build Tools
```

#### 3. Dependencies Not Found

```bash
# Clear CPM cache and rebuild
rm -rf ~/.cpm-cache
rm -rf build
cmake --preset dev
cmake --build build
```

#### 4. Build Failures

```bash
# Clean build
rm -rf build
cmake --preset dev
cmake --build build --verbose

# Check logs
cmake --build build 2>&1 | tee build.log
```

#### 5. gRPC Build Issues

```bash
# Use minimal build (no gRPC)
cmake --preset minimal
cmake --build build

# Or disable gRPC explicitly
cmake -B build -DYAZE_ENABLE_GRPC=OFF
cmake --build build
```

### Debug Build

```bash
# Debug build with verbose output
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DYAZE_VERBOSE_BUILD=ON

cmake --build build --verbose
```

### Memory Debugging

```bash
# AddressSanitizer build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DYAZE_ENABLE_SANITIZERS=ON

cmake --build build

# Run with sanitizer
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./build/bin/yaze
```

## Performance Optimization

### Release Build

```bash
# Optimized release build
cmake --preset release
cmake --build build
```

### Link-Time Optimization

```bash
# LTO build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DYAZE_ENABLE_LTO=ON

cmake --build build
```

### Unity Builds

```bash
# Unity build (faster compilation)
cmake -B build -G Ninja \
  -DYAZE_UNITY_BUILD=ON

cmake --build build
```

## CI/CD

### Local CI Testing

```bash
# Test CI build locally
cmake --preset ci
cmake --build build

# Run CI tests
cd build
ctest -L stable
```

### GitHub Actions

The project includes comprehensive GitHub Actions workflows:

- **CI Pipeline**: Builds and tests on Linux, macOS, Windows
- **Code Quality**: Formatting, linting, static analysis
- **Security**: CodeQL, dependency scanning
- **Release**: Automated packaging and release creation

## Advanced Configuration

### Custom Toolchain

```bash
# Use specific compiler
cmake -B build -G Ninja \
  -DCMAKE_C_COMPILER=gcc-12 \
  -DCMAKE_CXX_COMPILER=g++-12

cmake --build build
```

### Cross-Compilation

```bash
# Cross-compile for different architecture
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-gcc.cmake

cmake --build build
```

### Custom Dependencies

```bash
# Use system packages instead of CPM
cmake -B build -G Ninja \
  -DYAZE_USE_SYSTEM_DEPS=ON

cmake --build build
```

## Getting Help

- **Issues**: [GitHub Issues](https://github.com/scawful/yaze/issues)
- **Discussions**: [GitHub Discussions](https://github.com/scawful/yaze/discussions)
- **Documentation**: [docs/](docs/)
- **CI Status**: [GitHub Actions](https://github.com/scawful/yaze/actions)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `cmake --build build --target yaze-format-check`
5. Submit a pull request

For more details, see [CONTRIBUTING.md](CONTRIBUTING.md).
