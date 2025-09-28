@echo off
REM YAZE vcpkg Setup Script (Batch Version)
REM This script sets up vcpkg for YAZE development on Windows

setlocal enabledelayedexpansion

echo ========================================
echo YAZE vcpkg Setup Script
echo ========================================

REM Check if we're in the right directory
if not exist "vcpkg.json" (
    echo ERROR: vcpkg.json not found. Please run this script from the project root directory.
    pause
    exit /b 1
)

echo ✓ vcpkg.json found

REM Check for Git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Git not found. Please install Git for Windows.
    echo Download from: https://git-scm.com/download/win
    pause
    exit /b 1
)

echo ✓ Git found

REM Clone vcpkg if needed
if not exist "vcpkg" (
    echo Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git vcpkg
    if %errorlevel% neq 0 (
        echo ERROR: Failed to clone vcpkg
        pause
        exit /b 1
    )
    echo ✓ vcpkg cloned successfully
) else (
    echo ✓ vcpkg directory already exists
)

REM Bootstrap vcpkg
if not exist "vcpkg\vcpkg.exe" (
    echo Bootstrapping vcpkg...
    cd vcpkg
    call bootstrap-vcpkg.bat
    if %errorlevel% neq 0 (
        echo ERROR: Failed to bootstrap vcpkg
        cd ..
        pause
        exit /b 1
    )
    cd ..
    echo ✓ vcpkg bootstrapped successfully
) else (
    echo ✓ vcpkg already bootstrapped
)

REM Install dependencies
echo Installing dependencies...
vcpkg\vcpkg.exe install --triplet x64-windows
if %errorlevel% neq 0 (
    echo WARNING: Some dependencies may not have installed correctly
) else (
    echo ✓ Dependencies installed successfully
)

echo ========================================
echo ✓ vcpkg setup complete!
echo ========================================
echo.
echo You can now build YAZE using:
echo   .\scripts\build-windows.ps1
echo   or
echo   .\scripts\build-windows.bat
echo.

pause