# Build Presets Guide

CMake presets for development workflow and architecture-specific builds.

## 🍎 macOS ARM64 Presets (Recommended for Apple Silicon)

### For Development Work:
```bash
# ARM64-only development build with ROM testing
cmake --preset macos-dev
cmake --build --preset macos-dev

# ARM64-only debug build 
cmake --preset macos-debug
cmake --build --preset macos-debug

# ARM64-only release build
cmake --preset macos-release  
cmake --build --preset macos-release
```

### For Distribution:
```bash
# Universal binary (ARM64 + x86_64) - use only when needed for distribution
cmake --preset macos-debug-universal
cmake --build --preset macos-debug-universal

cmake --preset macos-release-universal
cmake --build --preset macos-release-universal
```

## 🔧 Why This Fixes Architecture Errors

**Problem**: The original presets used `CMAKE_OSX_ARCHITECTURES: "x86_64;arm64"` which forced CMake to build universal binaries. This caused issues because:
- Dependencies like Abseil tried to use x86 SSE instructions (`-msse4.1`) 
- These instructions don't exist on ARM64 processors
- Build failed with "unsupported option '-msse4.1' for target 'arm64-apple-darwin'"

**Solution**: The new ARM64-only presets use `CMAKE_OSX_ARCHITECTURES: "arm64"` which:
- ✅ Only targets ARM64 architecture  
- ✅ Prevents x86-specific instruction usage
- ✅ Uses ARM64 optimizations instead
- ✅ Builds much faster (no cross-compilation)

## 📋 Available Presets

| Preset Name | Architecture | Purpose | ROM Tests |
|-------------|-------------|---------|-----------|
| `macos-dev` | ARM64 only | Development | ✅ Enabled |
| `macos-debug` | ARM64 only | Debug builds | ❌ Disabled |
| `macos-release` | ARM64 only | Release builds | ❌ Disabled |
| `macos-debug-universal` | Universal | Distribution debug | ❌ Disabled |
| `macos-release-universal` | Universal | Distribution release | ❌ Disabled |

## 🚀 Quick Start

For most development work on Apple Silicon:

```bash
# Clean build
rm -rf build

# Configure for ARM64 development
cmake --preset macos-dev

# Build
cmake --build --preset macos-dev

# Run tests  
cmake --build --preset macos-dev --target test
```

## 🛠️ IDE Integration

### VS Code with CMake Tools:
1. Open Command Palette (`Cmd+Shift+P`)
2. Run "CMake: Select Configure Preset"
3. Choose `macos-dev` or `macos-debug`

### CLion:
1. Go to Settings → Build, Execution, Deployment → CMake
2. Add new profile with preset `macos-dev`

### Xcode:
```bash
# Generate Xcode project
cmake --preset macos-debug -G Xcode
open build/yaze.xcodeproj
```

## 🔍 Troubleshooting

If you still get architecture errors:
1. **Clean completely**: `rm -rf build`
2. **Check preset**: Ensure you're using an ARM64 preset (not universal)
3. **Verify configuration**: Check that `CMAKE_OSX_ARCHITECTURES` shows only `arm64`

```bash
# Verify architecture setting
cmake --preset macos-debug
grep -A 5 -B 5 "CMAKE_OSX_ARCHITECTURES" build/CMakeCache.txt
```

## 📝 Notes

- **ARM64 presets**: Fast builds, no architecture conflicts
- **Universal presets**: Slower builds, for distribution only
- **Deployment target**: ARM64 presets use macOS 11.0+ (when Apple Silicon was introduced)
- **Universal presets**: Still support macOS 10.15+ for backward compatibility
