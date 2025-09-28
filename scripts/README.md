# YAZE Build Scripts

This directory contains build and setup scripts for YAZE development on different platforms.

## Windows Scripts

### Setup Scripts
- **`setup-windows-dev.ps1`** - Complete Windows development environment setup (PowerShell)
- **`setup-vcpkg-windows.ps1`** - vcpkg setup only (PowerShell)
- **`setup-vcpkg-windows.bat`** - vcpkg setup only (Batch)

### Build Scripts
- **`build-windows.ps1`** - Build YAZE on Windows (PowerShell)
- **`build-windows.bat`** - Build YAZE on Windows (Batch)

### Validation Scripts
- **`validate-windows-build.ps1`** - Validate Windows build environment

### Project Generation
- **`generate-vs-projects.py`** - Generate Visual Studio project files (Cross-platform Python)

## Windows Compiler Recommendations

### ⚠️ Important: MSVC vs Clang on Windows

**We strongly recommend using Clang on Windows** due to compatibility issues with MSVC and Abseil's int128 and type_traits features:

#### Why Clang is Recommended:
- ✅ **Better C++23 Support**: Full support for modern C++23 features
- ✅ **Abseil Compatibility**: No issues with `absl::int128` and type traits
- ✅ **Cross-Platform Consistency**: Same compiler across all platforms
- ✅ **Better Error Messages**: More helpful diagnostic messages
- ✅ **Faster Compilation**: Generally faster than MSVC

#### MSVC Issues:
- ❌ **C++23 Deprecation Warnings**: Abseil int128 triggers numerous deprecation warnings
- ❌ **Type Traits Problems**: Some Abseil type traits don't work correctly with MSVC
- ❌ **Int128 Limitations**: MSVC's int128 support is incomplete
- ❌ **Build Complexity**: Requires additional workarounds and flags

### Compiler Setup Options

#### Option 1: Clang (Recommended)
```powershell
# Install LLVM/Clang via winget
winget install LLVM.LLVM

# Or download from: https://releases.llvm.org/
# Make sure to add Clang to PATH during installation

# Verify installation
clang --version
```

#### Option 2: MSVC with Workarounds
If you must use MSVC, the build system includes workarounds:
- Abseil int128 is automatically disabled on Windows
- C++23 deprecation warnings are silenced
- Additional compatibility flags are applied

However, you may still encounter issues with some Abseil features.

## Quick Start (Windows)

### Option 1: Automated Setup (Recommended)
```powershell
.\scripts\setup-windows-dev.ps1
```

### Option 2: Manual Setup
```powershell
# 1. Setup vcpkg
.\scripts\setup-vcpkg-windows.ps1

# 2. Generate project files
python scripts/generate-vs-projects.py

# 3. Build
.\scripts\build-windows.ps1
```

### Option 3: Using Batch Scripts
```batch
REM Setup vcpkg
.\scripts\setup-vcpkg-windows.bat

REM Generate project files
python scripts/generate-vs-projects.py

REM Build
.\scripts\build-windows.bat
```

## Script Options

### setup-windows-dev.ps1
- `-SkipVcpkg` - Skip vcpkg setup
- `-SkipVS` - Skip Visual Studio check
- `-SkipBuild` - Skip test build

### build-windows.ps1
- `-Configuration` - Build configuration (Debug, Release, RelWithDebInfo, MinSizeRel)
- `-Platform` - Target platform (x64, x86, ARM64)
- `-Compiler` - Compiler to use (clang, msvc, auto)
- `-Clean` - Clean build directories before building
- `-Verbose` - Verbose build output

### build-windows.bat
- First argument: Configuration (Debug, Release, RelWithDebInfo, MinSizeRel)
- Second argument: Platform (x64, x86, ARM64)
- Third argument: Compiler (clang, msvc, auto)
- `clean` - Clean build directories
- `verbose` - Verbose build output

## Examples

```powershell
# Build Release x64 with Clang (recommended)
.\scripts\build-windows.ps1 -Compiler clang

# Build Release x64 with MSVC (with workarounds)
.\scripts\build-windows.ps1 -Compiler msvc

# Build Debug x64 with Clang
.\scripts\build-windows.ps1 -Configuration Debug -Platform x64 -Compiler clang

# Build Release x86 with auto-detection
.\scripts\build-windows.ps1 -Configuration Release -Platform x86 -Compiler auto

# Clean build with Clang
.\scripts\build-windows.ps1 -Clean -Compiler clang

# Verbose build with MSVC
.\scripts\build-windows.ps1 -Verbose -Compiler msvc

# Validate environment
.\scripts\validate-windows-build.ps1
```

```batch
REM Build Release x64 with Clang (recommended)
.\scripts\build-windows.bat Release x64 clang

REM Build Debug x64 with MSVC
.\scripts\build-windows.bat Debug x64 msvc

REM Build Release x86 with auto-detection
.\scripts\build-windows.bat Release x86 auto

REM Clean build with Clang
.\scripts\build-windows.bat clean clang
```

## Troubleshooting

### Common Issues

1. **PowerShell Execution Policy**
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
   ```

2. **MSBuild Not Found**
   - Install Visual Studio 2022 with C++ workload
   - Or add MSBuild to PATH

3. **vcpkg Issues**
   - Run `.\scripts\setup-vcpkg-windows.ps1` to reinstall
   - Check internet connection for dependency downloads

4. **Python Not Found**
   - Install Python 3.8+ from python.org
   - Make sure Python is in PATH

5. **MSVC Compilation Errors**
   - **Abseil int128 errors**: Use Clang instead (`-Compiler clang`)
   - **C++23 deprecation warnings**: These are silenced automatically, but Clang is cleaner
   - **Type traits issues**: Switch to Clang for better compatibility
   - **Solution**: Install Clang and use `.\scripts\build-windows.ps1 -Compiler clang`

6. **Clang Not Found**
   - Install LLVM/Clang: `winget install LLVM.LLVM`
   - Or download from: https://releases.llvm.org/
   - Make sure Clang is in PATH: `clang --version`

7. **Compiler Detection Issues**
   - Use explicit compiler selection: `-Compiler clang` or `-Compiler msvc`
   - Check available compilers: `where clang` and `where cl`

### Getting Help

1. Run validation script: `.\scripts\validate-windows-build.ps1`
2. Check the [Windows Development Guide](../docs/windows-development-guide.md)
3. Review build output for specific error messages

## Other Scripts

- **`create_release.sh`** - Create GitHub releases (Linux/macOS)
- **`extract_changelog.py`** - Extract changelog for releases
- **`quality_check.sh`** - Code quality checks (Linux/macOS)
- **`create-macos-bundle.sh`** - Create macOS application bundle for releases
