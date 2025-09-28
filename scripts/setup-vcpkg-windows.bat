@echo off
REM Setup script for vcpkg on Windows
REM This script helps set up vcpkg for YAZE Windows builds

echo Setting up vcpkg for YAZE Windows builds...

REM Check if vcpkg directory exists
if not exist "vcpkg" (
    echo Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    if errorlevel 1 (
        echo Error: Failed to clone vcpkg repository
        pause
        exit /b 1
    )
)

REM Bootstrap vcpkg
cd vcpkg
if not exist "vcpkg.exe" (
    echo Bootstrapping vcpkg...
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo Error: Failed to bootstrap vcpkg
        pause
        exit /b 1
    )
)

REM Integrate vcpkg with Visual Studio (optional)
echo Integrating vcpkg with Visual Studio...
vcpkg integrate install

REM Set environment variable for this session
set VCPKG_ROOT=%CD%
echo VCPKG_ROOT set to: %VCPKG_ROOT%

cd ..

echo.
echo vcpkg setup complete!
echo.
echo To use vcpkg with YAZE:
echo 1. Use the Windows presets in CMakePresets.json:
echo    - windows-debug (Debug build)
echo    - windows-release (Release build)
echo.
echo 2. Or set VCPKG_ROOT environment variable:
echo    set VCPKG_ROOT=%CD%\vcpkg
echo.
echo 3. Dependencies will be automatically installed via vcpkg manifest mode
echo.

pause
