@echo off
REM Setup script for Windows development environment
REM This script installs the necessary tools for YAZE development on Windows

setlocal enabledelayedexpansion

set FORCE=false

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :args_done
if "%~1"=="--force" set FORCE=true
shift
goto :parse_args

:args_done

echo Setting up Windows development environment for YAZE...

REM Check if we're on Windows
if not "%OS%"=="Windows_NT" (
    echo This script is designed for Windows only.
    exit /b 1
)

REM Check if running as administrator
net session >nul 2>&1
if errorlevel 1 (
    echo This script requires administrator privileges to install software.
    echo Please run Command Prompt as Administrator and try again.
    exit /b 1
)

REM Install Chocolatey if not present
where choco >nul 2>&1
if errorlevel 1 (
    echo Installing Chocolatey package manager...
    powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
    
    REM Refresh environment variables
    call refreshenv
    
    echo Chocolatey installed successfully
) else (
    echo Chocolatey already installed
)

REM Install required tools
echo.
echo Installing required development tools...

REM CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo Installing CMake...
    choco install -y cmake
    if errorlevel 1 (
        echo Failed to install CMake
    ) else (
        echo CMake installed successfully
    )
) else (
    echo CMake already installed
)

REM Git
where git >nul 2>&1
if errorlevel 1 (
    echo Installing Git...
    choco install -y git
    if errorlevel 1 (
        echo Failed to install Git
    ) else (
        echo Git installed successfully
    )
) else (
    echo Git already installed
)

REM Ninja
where ninja >nul 2>&1
if errorlevel 1 (
    echo Installing Ninja...
    choco install -y ninja
    if errorlevel 1 (
        echo Failed to install Ninja
    ) else (
        echo Ninja installed successfully
    )
) else (
    echo Ninja already installed
)

REM Python 3
where python3 >nul 2>&1
if errorlevel 1 (
    echo Installing Python 3...
    choco install -y python3
    if errorlevel 1 (
        echo Failed to install Python 3
    ) else (
        echo Python 3 installed successfully
    )
) else (
    echo Python 3 already installed
)

REM Refresh environment variables
call refreshenv

REM Verify installations
echo.
echo Verifying installations...

where cmake >nul 2>&1
if errorlevel 1 (
    echo ✗ CMake not found
) else (
    echo ✓ CMake found
)

where git >nul 2>&1
if errorlevel 1 (
    echo ✗ Git not found
) else (
    echo ✓ Git found
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo ✗ Ninja not found
) else (
    echo ✓ Ninja found
)

where python3 >nul 2>&1
if errorlevel 1 (
    echo ✗ Python 3 not found
) else (
    echo ✓ Python 3 found
)

REM Check for Visual Studio
echo.
echo Checking for Visual Studio...
where vswhere >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" (
        set VSWHERE="C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    ) else (
        echo ✗ vswhere not found - cannot detect Visual Studio
        goto :setup_complete
    )
) else (
    set VSWHERE=vswhere
)

%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath >nul 2>&1
if errorlevel 1 (
    echo ✗ Visual Studio 2022 with C++ workload not found
    echo Please install Visual Studio 2022 with the 'Desktop development with C++' workload
) else (
    echo ✓ Visual Studio found
)

:setup_complete
echo.
echo 🎉 Windows development environment setup complete!
echo.
echo Next steps:
echo 1. Run the Visual Studio project generation script:
echo    .\scripts\generate-vs-projects.bat
echo 2. Or use CMake presets:
echo    cmake --preset windows-debug
echo 3. Open YAZE.sln in Visual Studio and build

pause
