# Windows Build Quick Start Guide

## 🚀 Fastest Build Method (Recommended)

### Prerequisites
- Visual Studio 2022 with C++ development tools
- Git
- CMake 3.16+

### Option 1: vcpkg (5-10 minute build)

```powershell
# 1. Clone vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# 2. Install dependencies (cached for future builds)
.\vcpkg install grpc:x64-windows sdl2[vulkan]:x64-windows yaml-cpp:x64-windows

# 3. Clone and build yaze
git clone https://github.com/scawful/yaze.git
cd yaze
git submodule update --init --recursive

# 4. Configure with vcpkg
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# 5. Build (5-10 minutes)
cmake --build build --config Release --parallel
```

**Result:** Full-featured build in ~10 minutes

---

## 🔧 Alternative Methods

### Option 2: FetchContent (45 minute first build)

```powershell
# Clone and build - CMake downloads everything
git clone https://github.com/scawful/yaze.git
cd yaze
git submodule update --init --recursive

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

**First build:** 45 minutes (compiles gRPC from source)  
**Subsequent builds:** 5 minutes (cached)

### Option 3: Minimal Build (5 minute build, limited features)

```powershell
# Fast build without gRPC/AI features
cmake -B build -G "Visual Studio 17 2022" -A x64 -DYAZE_MINIMAL_BUILD=ON
cmake --build build --config Release --parallel
```

**Limitations:** No networking, no AI agent (JSON support only)

---

## 📋 Feature Comparison

| Build Type | gRPC | AI Agent | JSON | Build Time | Use Case |
|------------|------|----------|------|------------|----------|
| vcpkg | ✅ | ✅ | ✅ | 10 min | **Recommended** |
| FetchContent | ✅ | ✅ | ✅ | 45 min (first) | No vcpkg |
| Minimal | ❌ | ❌ | ✅ | 5 min | Development/Testing |

---

## 🐛 Troubleshooting

### "gRPC not found" Error
```powershell
# Install via vcpkg
C:\vcpkg\vcpkg install grpc:x64-windows

# Or use minimal build
cmake -B build -DYAZE_MINIMAL_BUILD=ON
```

### "vcpkg integration failed"
```powershell
# Set toolchain file explicitly
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### "Link errors with protobuf"
```powershell
# Clean and rebuild
cmake --build build --target clean
cmake --build build --config Release
```

### Build Takes Too Long
```powershell
# Use parallel builds
cmake --build build --config Release --parallel

# Or use minimal build
cmake -B build -DYAZE_MINIMAL_BUILD=ON
```

---

## 🎯 CI/CD Notes

GitHub Actions CI uses:
- **vcpkg** for Windows release builds (fast)
- **Minimal build** for Windows CI testing (fastest)
- **Pinned vcpkg version** for reproducibility

See `.github/workflows/ci.yml` for configuration details.

---

## 📚 More Information

- Full documentation: `docs/BUILD-IMPROVEMENTS.md`
- Build instructions: `docs/B1-build-instructions.md`
- CMake presets: `CMakePresets.json`
