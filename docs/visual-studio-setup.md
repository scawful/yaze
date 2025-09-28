# Visual Studio Setup Guide

This guide will help Visual Studio users set up and build the yaze project on Windows.

## Prerequisites

### Required Software
1. **Visual Studio 2022** (Community, Professional, or Enterprise)
   - Install with "Desktop development with C++" workload
   - Ensure CMake tools are included
   - Install Git for Windows (or use built-in Git support)

2. **vcpkg** (Package Manager)
   - Download from: https://github.com/Microsoft/vcpkg
   - Follow installation instructions to integrate with Visual Studio

3. **CMake** (3.16 or later)
   - Usually included with Visual Studio 2022
   - Verify with: `cmake --version`

### Environment Setup

1. **Set up vcpkg environment variable:**
   ```cmd
   set VCPKG_ROOT=C:\vcpkg
   ```

2. **Integrate vcpkg with Visual Studio:**
   ```cmd
   cd C:\vcpkg
   .\vcpkg integrate install
   ```

## Project Setup

### 1. Clone the Repository
```cmd
git clone --recursive https://github.com/your-username/yaze.git
cd yaze
```

### 2. Install Dependencies via vcpkg
The project uses `vcpkg.json` for automatic dependency management. Dependencies will be installed automatically during CMake configuration.

Manual installation (if needed):
```cmd
vcpkg install zlib:x64-windows
vcpkg install libpng:x64-windows
vcpkg install sdl2[vulkan]:x64-windows
vcpkg install abseil:x64-windows
vcpkg install gtest:x64-windows
```

### 3. Configure Build System

#### Option A: Using Visual Studio Project File (Easiest)
1. Open Visual Studio 2022
2. Select "Open a project or solution"
3. Navigate to the yaze project folder and open `yaze.sln`
4. The project is pre-configured with vcpkg integration and proper dependencies
5. Select your desired build configuration (Debug/Release) and platform (x64/x86)
6. Press F5 to build and run, or Ctrl+Shift+B to build only

#### Option B: Using CMake with Visual Studio (Recommended for developers)
1. Open Visual Studio 2022
2. Select "Open a local folder" and navigate to the yaze project folder
3. Visual Studio will automatically detect the CMake project
4. Wait for CMake configuration to complete (check Output window)

#### Option C: Using Command Line
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

### 4. Build Configuration

#### Using Visual Studio Project File (.vcxproj)
- **Debug Build:** Select "Debug" configuration and press F5 or Ctrl+Shift+B
- **Release Build:** Select "Release" configuration and press F5 or Ctrl+Shift+B
- **Platform:** Choose x64 (recommended) or x86 from the platform dropdown

#### Using CMake (Command Line)
```cmd
# For Development (Debug Build)
cmake --build . --config Debug --target yaze

# For Release Build
cmake --build . --config Release --target yaze

# For Testing (Optional)
cmake --build . --config Debug --target yaze_test
```

## Common Issues and Solutions

### Issue 1: zlib Import Errors
**Problem:** `fatal error C1083: Cannot open include file: 'zlib.h'`

**Solution:**
1. Ensure vcpkg is properly integrated with Visual Studio
2. Verify the vcpkg toolchain file is set:
   ```cmd
   cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
   ```
3. Check that zlib is installed:
   ```cmd
   vcpkg list zlib
   ```

### Issue 2: Executable Runs Tests Instead of Main App
**Problem:** Running `yaze.exe` starts the test framework instead of the application

**Solution:** This has been fixed in the latest version. The issue was caused by linking `gtest_main` to the main executable. The fix removes `gtest_main` from the main application while keeping `gtest` for testing capabilities.

### Issue 3: SDL2 Configuration Issues
**Problem:** SDL2 not found or linking errors

**Solution:**
1. Install SDL2 with vcpkg:
   ```cmd
   vcpkg install sdl2[vulkan]:x64-windows
   ```
2. Ensure the project uses the vcpkg toolchain file

### Issue 4: Build Errors with Abseil
**Problem:** Missing Abseil symbols or linking issues

**Solution:**
1. Install Abseil via vcpkg:
   ```cmd
   vcpkg install abseil:x64-windows
   ```
2. The project is configured to use Abseil 20240116.2 (see vcpkg.json overrides)

## Visual Studio Configuration

### CMake Settings
Create or modify `.vscode/settings.json` or use Visual Studio's CMake settings:

```json
{
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "-Dyaze_BUILD_TESTS=ON",
        "-Dyaze_BUILD_APP=ON",
        "-Dyaze_BUILD_LIB=ON"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

### Build Presets
The project includes CMake presets in `CMakePresets.json`. Use these in Visual Studio:

1. **Debug Build:** `debug` preset
2. **Release Build:** `release` preset  
3. **Development Build:** `dev` preset (includes ROM testing)

## Running the Application

### Using Visual Studio Project File
1. Open `yaze.sln` in Visual Studio
2. Set `yaze` as the startup project (should be default)
3. Configure command line arguments in Project Properties > Debugging > Command Arguments
   - Example: `--rom_file=C:\path\to\your\zelda3.sfc`
4. Press F5 to build and run, or Ctrl+F5 to run without debugging

### Command Line
```cmd
cd build/bin/Debug  # or Release
yaze.exe --rom_file=path/to/your/zelda3.sfc
```

### Visual Studio (CMake)
1. Set `yaze` as the startup project
2. Configure command line arguments in Project Properties > Debugging
3. Press F5 to run

## Testing

### Run Unit Tests
```cmd
cd build
ctest --build-config Debug
```

### Run Specific Test Suite
```cmd
cd build/bin/Debug
yaze_test.exe
```

## Troubleshooting

### Clean Build
If you encounter persistent issues:
```cmd
rmdir /s build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Debug
```

### Check Dependencies
Verify all dependencies are properly installed:
```cmd
vcpkg list
```

### CMake Cache Issues
Clear CMake cache:
```cmd
del CMakeCache.txt
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

## Visual Studio Project File Features

The included `yaze.vcxproj` and `yaze.sln` files provide:

### **Automatic Dependency Management**
- **vcpkg Integration:** Automatically installs and links dependencies from `vcpkg.json`
- **Platform Support:** Pre-configured for both x64 and x86 builds
- **Library Linking:** Automatically links SDL2, zlib, libpng, and system libraries

### **Build Configuration**
- **Debug Configuration:** Includes debugging symbols and runtime checks
- **Release Configuration:** Optimized for performance with full optimizations
- **C++23 Standard:** Uses modern C++ features and standard library

### **Asset Management**
- **Automatic Asset Copying:** Post-build events copy themes and assets to output directory
- **ROM File Handling:** Automatically copies `zelda3.sfc` if present in project root
- **Resource Organization:** Properly structures output directory for distribution

### **Development Features**
- **IntelliSense Support:** Full code completion and error detection
- **Debugging Integration:** Native Visual Studio debugging support
- **Project Properties:** Easy access to compiler and linker settings

## Additional Notes

- The project supports both x64 and x86 builds (use appropriate vcpkg triplets)
- For ARM64 Windows builds, use `arm64-windows` triplet
- The CI/CD pipeline uses minimal builds to avoid dependency issues
- Development builds include additional debugging features and ROM testing capabilities
- The `.vcxproj` file provides the easiest setup for Visual Studio users who prefer traditional project files over CMake

## Support

If you encounter issues not covered in this guide:
1. Check the project's GitHub issues
2. Verify your Visual Studio and vcpkg installations
3. Ensure all dependencies are properly installed via vcpkg
4. Try a clean build following the troubleshooting steps above
