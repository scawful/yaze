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
- **`generate-vs-projects.ps1`** - Generate Visual Studio project files (PowerShell)
- **`generate-vs-projects.bat`** - Generate Visual Studio project files (Batch)

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
- `-Clean` - Clean build directories before building
- `-Verbose` - Verbose build output

### build-windows.bat
- First argument: Configuration (Debug, Release, RelWithDebInfo, MinSizeRel)
- Second argument: Platform (x64, x86, ARM64)
- `clean` - Clean build directories
- `verbose` - Verbose build output

## Examples

```powershell
# Build Release x64 (default)
.\scripts\build-windows.ps1

# Build Debug x64
.\scripts\build-windows.ps1 -Configuration Debug -Platform x64

# Build Release x86
.\scripts\build-windows.ps1 -Configuration Release -Platform x86

# Clean build
.\scripts\build-windows.ps1 -Clean

# Verbose build
.\scripts\build-windows.ps1 -Verbose

# Validate environment
.\scripts\validate-windows-build.ps1
```

```batch
REM Build Release x64 (default)
.\scripts\build-windows.bat

REM Build Debug x64
.\scripts\build-windows.bat Debug x64

REM Build Release x86
.\scripts\build-windows.bat Release x86

REM Clean build
.\scripts\build-windows.bat clean
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

### Getting Help

1. Run validation script: `.\scripts\validate-windows-build.ps1`
2. Check the [Windows Development Guide](../docs/windows-development-guide.md)
3. Review build output for specific error messages

## Other Scripts

- **`create_release.sh`** - Create GitHub releases (Linux/macOS)
- **`extract_changelog.py`** - Extract changelog for releases
- **`quality_check.sh`** - Code quality checks (Linux/macOS)
- **`test_asar_integration.py`** - Test Asar integration
- **`agent.sh`** - AI agent helper script (Linux/macOS)
