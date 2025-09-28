@echo off
REM YAZE Windows Build Script (Batch Version)
REM This script builds the YAZE project on Windows using MSBuild

setlocal enabledelayedexpansion

REM Parse command line arguments
set BUILD_CONFIG=Release
set BUILD_PLATFORM=x64
set CLEAN_BUILD=0
set VERBOSE=0

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="Debug" set BUILD_CONFIG=Debug
if /i "%~1"=="Release" set BUILD_CONFIG=Release
if /i "%~1"=="RelWithDebInfo" set BUILD_CONFIG=RelWithDebInfo
if /i "%~1"=="MinSizeRel" set BUILD_CONFIG=MinSizeRel
if /i "%~1"=="x64" set BUILD_PLATFORM=x64
if /i "%~1"=="x86" set BUILD_PLATFORM=x86
if /i "%~1"=="ARM64" set BUILD_PLATFORM=ARM64
if /i "%~1"=="clean" set CLEAN_BUILD=1
if /i "%~1"=="verbose" set VERBOSE=1
shift
goto :parse_args

:args_done

echo ========================================
echo YAZE Windows Build Script
echo ========================================

REM Check if we're in the right directory
if not exist "YAZE.sln" (
    echo ERROR: YAZE.sln not found. Please run this script from the project root directory.
    pause
    exit /b 1
)

echo ✓ YAZE.sln found

REM Check for MSBuild
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MSBuild not found. Please install Visual Studio 2022 or later.
    echo Make sure to install the C++ development workload.
    pause
    exit /b 1
)

echo ✓ MSBuild found

REM Check for vcpkg
if not exist "vcpkg.json" (
    echo WARNING: vcpkg.json not found. vcpkg integration may not work properly.
)

echo Build Configuration: %BUILD_CONFIG%
echo Build Platform: %BUILD_PLATFORM%

REM Create build directories
echo Creating build directories...
if not exist "build" mkdir build
if not exist "build\bin" mkdir build\bin
if not exist "build\obj" mkdir build\obj

REM Clean build if requested
if %CLEAN_BUILD%==1 (
    echo Cleaning build directories...
    if exist "build\bin" rmdir /s /q "build\bin" 2>nul
    if exist "build\obj" rmdir /s /q "build\obj" 2>nul
    if not exist "build\bin" mkdir build\bin
    if not exist "build\obj" mkdir build\obj
    echo ✓ Build directories cleaned
)

REM Generate yaze_config.h if it doesn't exist
if not exist "yaze_config.h" (
    echo Generating yaze_config.h...
    if exist "src\yaze_config.h.in" (
        copy "src\yaze_config.h.in" "yaze_config.h" >nul
        powershell -Command "(Get-Content 'yaze_config.h') -replace '@yaze_VERSION_MAJOR@', '0' -replace '@yaze_VERSION_MINOR@', '3' -replace '@yaze_VERSION_PATCH@', '1' | Set-Content 'yaze_config.h'"
        echo ✓ Generated yaze_config.h
    ) else (
        echo WARNING: yaze_config.h.in not found, creating basic config
        echo // yaze config file > yaze_config.h
        echo #define YAZE_VERSION_MAJOR 0 >> yaze_config.h
        echo #define YAZE_VERSION_MINOR 3 >> yaze_config.h
        echo #define YAZE_VERSION_PATCH 1 >> yaze_config.h
    )
)

REM Build using MSBuild
echo Building with MSBuild...

set MSBUILD_ARGS=YAZE.sln /p:Configuration=%BUILD_CONFIG% /p:Platform=%BUILD_PLATFORM% /p:VcpkgEnabled=true /p:VcpkgManifestInstall=true /m

if %VERBOSE%==1 (
    set MSBUILD_ARGS=%MSBUILD_ARGS% /verbosity:detailed
) else (
    set MSBUILD_ARGS=%MSBUILD_ARGS% /verbosity:minimal
)

echo Command: msbuild %MSBUILD_ARGS%

msbuild %MSBUILD_ARGS%

if %errorlevel% neq 0 (
    echo ERROR: Build failed with exit code %errorlevel%
    pause
    exit /b 1
)

echo ✓ Build completed successfully

REM Verify executable was created
set EXE_PATH=build\bin\%BUILD_CONFIG%\yaze.exe
if not exist "%EXE_PATH%" (
    echo ERROR: Executable not found at expected path: %EXE_PATH%
    pause
    exit /b 1
)

echo ✓ Executable created: %EXE_PATH%

REM Test that the executable runs (basic test)
echo Testing executable startup...
"%EXE_PATH%" --help >nul 2>&1
set EXIT_CODE=%errorlevel%

REM Check if it's the test main or app main
"%EXE_PATH%" --help 2>&1 | findstr /i "Google Test" >nul
if %errorlevel% equ 0 (
    echo ERROR: Executable is running test main instead of app main!
    pause
    exit /b 1
)

echo ✓ Executable runs correctly (exit code: %EXIT_CODE%)

REM Display file info
for %%A in ("%EXE_PATH%") do set FILE_SIZE=%%~zA
set /a FILE_SIZE_MB=%FILE_SIZE% / 1024 / 1024
echo Executable size: %FILE_SIZE_MB% MB

echo ========================================
echo ✓ YAZE Windows build completed successfully!
echo ========================================
echo.
echo Build Configuration: %BUILD_CONFIG%
echo Build Platform: %BUILD_PLATFORM%
echo Executable: %EXE_PATH%
echo.
echo To run YAZE:
echo   %EXE_PATH%
echo.
echo To build other configurations:
echo   %~nx0 Debug x64
echo   %~nx0 Release x86
echo   %~nx0 RelWithDebInfo ARM64
echo   %~nx0 clean
echo.

pause