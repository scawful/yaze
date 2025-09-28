@echo off
echo ========================================
echo yaze Visual Studio Setup Script
echo ========================================
echo.

REM Check if vcpkg is installed
if not exist "%VCPKG_ROOT%" (
    echo ERROR: VCPKG_ROOT environment variable is not set!
    echo Please install vcpkg and set the VCPKG_ROOT environment variable.
    echo Example: set VCPKG_ROOT=C:\vcpkg
    echo.
    echo Download vcpkg from: https://github.com/Microsoft/vcpkg
    echo After installation, run: .\vcpkg integrate install
    pause
    exit /b 1
)

echo VCPKG_ROOT is set to: %VCPKG_ROOT%
echo.

REM Check if vcpkg.exe exists
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg.exe not found at %VCPKG_ROOT%\vcpkg.exe
    echo Please verify your vcpkg installation.
    pause
    exit /b 1
)

echo Installing dependencies via vcpkg...
echo This may take several minutes on first run.
echo.

REM Install dependencies for x64-windows
echo Installing x64-windows dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install zlib:x64-windows
"%VCPKG_ROOT%\vcpkg.exe" install libpng:x64-windows
"%VCPKG_ROOT%\vcpkg.exe" install sdl2[vulkan]:x64-windows
"%VCPKG_ROOT%\vcpkg.exe" install abseil:x64-windows
"%VCPKG_ROOT%\vcpkg.exe" install gtest:x64-windows

echo.
echo Installing x86-windows dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install zlib:x86-windows
"%VCPKG_ROOT%\vcpkg.exe" install libpng:x86-windows
"%VCPKG_ROOT%\vcpkg.exe" install sdl2[vulkan]:x86-windows
"%VCPKG_ROOT%\vcpkg.exe" install abseil:x86-windows
"%VCPKG_ROOT%\vcpkg.exe" install gtest:x86-windows

echo.
echo Integrating vcpkg with Visual Studio...
"%VCPKG_ROOT%\vcpkg.exe" integrate install

echo.
echo ========================================
echo Setup Complete!
echo ========================================
echo.
echo You can now:
echo 1. Open yaze.sln in Visual Studio 2022
echo 2. Select Debug or Release configuration
echo 3. Choose x64 or x86 platform
echo 4. Press F5 to build and run
echo.
echo If you have a Zelda 3 ROM file, place it in the project root
echo and name it 'zelda3.sfc' for automatic copying.
echo.
pause
