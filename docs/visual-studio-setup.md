# Visual Studio Setup Guide for YAZE

This guide will help Visual Studio users set up and build the YAZE project on Windows.

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

### 3. Configure CMake

#### Option A: Using Visual Studio (Recommended)
1. Open Visual Studio 2022
2. Select "Open a local folder" and navigate to the YAZE project folder
3. Visual Studio will automatically detect the CMake project
4. Wait for CMake configuration to complete (check Output window)

#### Option B: Using Command Line
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

### 4. Build Configuration

#### For Development (Debug Build)
```cmd
cmake --build . --config Debug --target yaze
```

#### For Release Build
```cmd
cmake --build . --config Release --target yaze
```

#### For Testing (Optional)
```cmd
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
        "-DYAZE_BUILD_TESTS=ON",
        "-DYAZE_BUILD_APP=ON",
        "-DYAZE_BUILD_LIB=ON"
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

### Command Line
```cmd
cd build/bin/Debug  # or Release
yaze.exe --rom_file=path/to/your/zelda3.sfc
```

### Visual Studio
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

## Additional Notes

- The project supports both x64 and x86 builds (use appropriate vcpkg triplets)
- For ARM64 Windows builds, use `arm64-windows` triplet
- The CI/CD pipeline uses minimal builds to avoid dependency issues
- Development builds include additional debugging features and ROM testing capabilities

## Support

If you encounter issues not covered in this guide:
1. Check the project's GitHub issues
2. Verify your Visual Studio and vcpkg installations
3. Ensure all dependencies are properly installed via vcpkg
4. Try a clean build following the troubleshooting steps above
