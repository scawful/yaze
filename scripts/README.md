# yaze Build Scripts

This directory contains build and setup scripts for YAZE development on different platforms.

## Windows Scripts

### vcpkg Setup (Optional)
- **`setup-vcpkg-windows.ps1`** - Setup vcpkg for SDL2 dependency (PowerShell)
- **`setup-vcpkg-windows.bat`** - Setup vcpkg for SDL2 dependency (Batch)

**Note**: vcpkg is optional. YAZE uses bundled dependencies by default.

## Windows Build Workflow

### Recommended: Visual Studio CMake Mode

**YAZE uses Visual Studio's native CMake support - no project generation needed.**

1. **Install Visual Studio 2022** with "Desktop development with C++" workload
2. **Open Visual Studio 2022**
3. **File → Open → Folder**
4. **Navigate to yaze directory**
5. **Select configuration** (Debug/Release) from toolbar
6. **Press F5** to build and run

### Command Line Build

```powershell
# Standard CMake workflow
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# Run the application
.\build\bin\Debug\yaze.exe
```

### Compiler Notes

The CMake configuration is designed to work with **MSVC** (Visual Studio's compiler). The build system includes automatic workarounds for C++23 compatibility:
- Abseil int128 is automatically excluded on Windows
- C++23 deprecation warnings are properly silenced
- Platform-specific definitions are handled automatically

## Quick Start (Windows)

### Option 1: Visual Studio (Recommended)
1. Open Visual Studio 2022
2. File → Open → Folder
3. Navigate to yaze directory
4. Select configuration (Debug/Release)
5. Press F5 to build and run

### Option 2: Command Line
```powershell
# Standard CMake build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Option 3: With vcpkg (Optional)
```powershell
# Setup vcpkg for SDL2
.\scripts\setup-vcpkg-windows.ps1

# Then build normally in Visual Studio or command line
```

## Troubleshooting

### Common Issues

1. **PowerShell Execution Policy**
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
   ```

2. **Visual Studio Not Detecting CMakeLists.txt**
   - Ensure you opened the folder (not a solution file)
   - Check that CMakeLists.txt exists in the root directory
   - Try: File → Close Folder, then File → Open → Folder

3. **vcpkg Issues (if using vcpkg)**
   - Run `.\scripts\setup-vcpkg-windows.ps1` to reinstall
   - Check internet connection for dependency downloads
   - Note: vcpkg is optional; bundled dependencies work by default

4. **CMake Configuration Errors**
   - Delete the `build/` directory
   - Close and reopen the folder in Visual Studio
   - Or run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug`

5. **Missing Git Submodules**
   ```powershell
   git submodule update --init --recursive
   ```

### Getting Help

1. Check the [Build Instructions](../docs/02-build-instructions.md)
2. Review CMake output for specific error messages
3. Ensure all prerequisites are installed (Visual Studio 2022 with C++ workload)

## Other Scripts

- **`create_release.sh`** - Create GitHub releases (Linux/macOS)
- **`extract_changelog.py`** - Extract changelog for releases
- **`quality_check.sh`** - Code quality checks (Linux/macOS)
- **`create-macos-bundle.sh`** - Create macOS application bundle for releases

## Build Environment Verification

This directory also contains build environment verification scripts.

### `verify-build-environment.ps1` / `.sh`

A comprehensive script that checks:

- ✅ **CMake Installation** - Version 3.16+ required
- ✅ **Git Installation** - With submodule support
- ✅ **C++ Compiler** - GCC 13+, Clang 16+, or MSVC 2019+
- ✅ **Platform Tools** - Xcode (macOS), Visual Studio (Windows), build-essential (Linux)
- ✅ **Git Submodules** - All dependencies synchronized

### Usage

**Windows (PowerShell):**
```powershell
.\scripts\verify-build-environment.ps1
```

**macOS/Linux:**
```bash
./scripts/verify-build-environment.sh
```
