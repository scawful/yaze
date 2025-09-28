@echo off
REM Generate Visual Studio project files for YAZE
REM This script creates proper Visual Studio solution and project files

setlocal enabledelayedexpansion

REM Default values
set CONFIGURATION=Debug
set ARCHITECTURE=x64
set CLEAN=false

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :args_done
if "%~1"=="--clean" set CLEAN=true
if "%~1"=="--release" set CONFIGURATION=Release
if "%~1"=="--x86" set ARCHITECTURE=Win32
if "%~1"=="--x64" set ARCHITECTURE=x64
if "%~1"=="--arm64" set ARCHITECTURE=ARM64
shift
goto :parse_args

:args_done

REM Validate architecture
if not "%ARCHITECTURE%"=="x64" if not "%ARCHITECTURE%"=="Win32" if not "%ARCHITECTURE%"=="ARM64" (
    echo Invalid architecture: %ARCHITECTURE%
    echo Valid architectures: x64, Win32, ARM64
    exit /b 1
)

echo Generating Visual Studio project files for YAZE...

REM Check if we're on Windows
if not "%OS%"=="Windows_NT" (
    echo This script is designed for Windows. Use CMake presets on other platforms.
    echo Available presets:
    echo   - windows-debug
    echo   - windows-release  
    echo   - windows-dev
    exit /b 1
)

REM Check if CMake is available
where cmake >nul 2>&1
if errorlevel 1 (
    REM Try common CMake installation paths
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        echo Found CMake at: C:\Program Files\CMake\bin\cmake.exe
        set "PATH=%PATH%;C:\Program Files\CMake\bin"
        goto :cmake_found
    )
    if exist "C:\Program Files (x86)\CMake\bin\cmake.exe" (
        echo Found CMake at: C:\Program Files (x86)\CMake\bin\cmake.exe
        set "PATH=%PATH%;C:\Program Files (x86)\CMake\bin"
        goto :cmake_found
    )
    if exist "C:\cmake\bin\cmake.exe" (
        echo Found CMake at: C:\cmake\bin\cmake.exe
        set "PATH=%PATH%;C:\cmake\bin"
        goto :cmake_found
    )
    
    REM If we get here, CMake is not found
    echo CMake not found in PATH. Attempting to install...
    
    REM Try to install CMake via Chocolatey
    where choco >nul 2>&1
    if not errorlevel 1 (
        echo Installing CMake via Chocolatey...
        choco install -y cmake
        if errorlevel 1 (
            echo Failed to install CMake via Chocolatey
        ) else (
            echo CMake installed successfully
            REM Refresh PATH
            call refreshenv
        )
    ) else (
        echo Chocolatey not found. Please install CMake manually:
        echo 1. Download from: https://cmake.org/download/
        echo 2. Or install Chocolatey first: https://chocolatey.org/install
        echo 3. Then run: choco install cmake
        exit /b 1
    )
    
    REM Check again after installation
    where cmake >nul 2>&1
    if errorlevel 1 (
        echo CMake still not found after installation. Please restart your terminal or add CMake to PATH manually.
        exit /b 1
    )
)

:cmake_found
echo CMake found and ready to use

REM Set up paths
set SOURCE_DIR=%~dp0..
set BUILD_DIR=%SOURCE_DIR%\build-vs

echo Source directory: %SOURCE_DIR%
echo Build directory: %BUILD_DIR%

REM Clean build directory if requested
if "%CLEAN%"=="true" (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Check if vcpkg is available
set VCPKG_PATH=%SOURCE_DIR%\vcpkg\scripts\buildsystems\vcpkg.cmake
if exist "%VCPKG_PATH%" (
    echo Using vcpkg toolchain: %VCPKG_PATH%
    set USE_VCPKG=true
) else (
    echo vcpkg not found, using system libraries
    set USE_VCPKG=false
)

REM Build CMake command
set CMAKE_ARGS=-B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A %ARCHITECTURE% -DCMAKE_BUILD_TYPE=%CONFIGURATION% -DYAZE_BUILD_TESTS=ON -DYAZE_BUILD_APP=ON -DYAZE_BUILD_LIB=ON -DYAZE_BUILD_EMU=ON -DYAZE_BUILD_Z3ED=ON -DYAZE_ENABLE_ROM_TESTS=OFF -DYAZE_ENABLE_EXPERIMENTAL_TESTS=ON -DYAZE_ENABLE_UI_TESTS=ON -DYAZE_INSTALL_LIB=OFF

if "%USE_VCPKG%"=="true" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_TOOLCHAIN_FILE="%VCPKG_PATH%" -DVCPKG_TARGET_TRIPLET=%ARCHITECTURE%-windows -DVCPKG_MANIFEST_MODE=ON
)

REM Run CMake configuration
echo Configuring CMake...
echo Command: cmake %CMAKE_ARGS% "%SOURCE_DIR%"

cmake %CMAKE_ARGS% "%SOURCE_DIR%"

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Check if solution file was created
set SOLUTION_FILE=%BUILD_DIR%\YAZE.sln
if exist "%SOLUTION_FILE%" (
    echo ✅ Visual Studio solution created: %SOLUTION_FILE%
    
    REM Copy solution file to root directory for convenience
    set ROOT_SOLUTION_FILE=%SOURCE_DIR%\YAZE.sln
    copy "%SOLUTION_FILE%" "%ROOT_SOLUTION_FILE%" >nul
    echo ✅ Solution file copied to root directory
    
    REM Try to open solution in Visual Studio
    where devenv >nul 2>&1
    if not errorlevel 1 (
        echo Opening solution in Visual Studio...
        start "" devenv "%ROOT_SOLUTION_FILE%"
    ) else (
        echo Visual Studio solution ready: %ROOT_SOLUTION_FILE%
    )
) else (
    echo ❌ Solution file not created. Check CMake output for errors.
    exit /b 1
)

echo.
echo 🎉 Visual Studio project generation complete!
echo.
echo Next steps:
echo 1. Open YAZE.sln in Visual Studio
echo 2. Select configuration: %CONFIGURATION%
echo 3. Select platform: %ARCHITECTURE%
echo 4. Build the solution (Ctrl+Shift+B)
echo.
echo Available configurations:
echo   - Debug (with debugging symbols)
echo   - Release (optimized)
echo   - RelWithDebInfo (optimized with debug info)
echo   - MinSizeRel (minimum size)
echo.
echo Available architectures:
echo   - x64 (64-bit Intel/AMD)
echo   - x86 (32-bit Intel/AMD)
echo   - ARM64 (64-bit ARM)

pause
