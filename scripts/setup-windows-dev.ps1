# YAZE Windows Development Setup Script
# This script sets up a complete Windows development environment for YAZE

param(
    [switch]$SkipVcpkg,
    [switch]$SkipVS,
    [switch]$SkipBuild
)

# Set error handling
$ErrorActionPreference = "Continue"

# Colors for output
$Colors = @{
    Success = "Green"
    Warning = "Yellow" 
    Error = "Red"
    Info = "Cyan"
    White = "White"
}

function Write-Status {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Colors[$Color]
}

function Test-Command {
    param([string]$Command)
    try {
        $null = Get-Command $Command -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Test-VisualStudio {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $msbuildPath = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $msbuildPath) {
                return $true
            }
        }
    }
    return $false
}

# Main script
Write-Status "========================================" "Info"
Write-Status "YAZE Windows Development Setup" "Info"
Write-Status "========================================" "Info"

# Check if we're in the right directory
if (-not (Test-Path "YAZE.sln")) {
    Write-Status "ERROR: YAZE.sln not found. Please run this script from the project root directory." "Error"
    exit 1
}

Write-Status "✓ Found YAZE project files" "Success"

# Check Visual Studio
if (-not $SkipVS) {
    Write-Status "Checking Visual Studio..." "Warning"
    if (Test-VisualStudio) {
        Write-Status "✓ Visual Studio 2022 with C++ workload found" "Success"
    } else {
        Write-Status "⚠ Visual Studio 2022 with C++ workload not found" "Warning"
        Write-Status "Please install Visual Studio 2022 with 'Desktop development with C++' workload" "Info"
    }
} else {
    Write-Status "Skipping Visual Studio check" "Warning"
}

# Check Git
Write-Status "Checking Git..." "Warning"
if (Test-Command "git") {
    $gitVersion = & git --version
    Write-Status "✓ Git found: $gitVersion" "Success"
} else {
    Write-Status "⚠ Git not found" "Warning"
    Write-Status "Please install Git for Windows from: https://git-scm.com/download/win" "Info"
}

# Check Python
Write-Status "Checking Python..." "Warning"
if (Test-Command "python") {
    $pythonVersion = & python --version
    Write-Status "✓ Python found: $pythonVersion" "Success"
} else {
    Write-Status "⚠ Python not found" "Warning"
    Write-Status "Please install Python 3.8+ from: https://www.python.org/downloads/" "Info"
}

# Setup vcpkg
if (-not $SkipVcpkg) {
    Write-Status "Setting up vcpkg..." "Warning"
    
    # Clone vcpkg if needed
    if (-not (Test-Path "vcpkg")) {
        if (Test-Command "git") {
            Write-Status "Cloning vcpkg..." "Warning"
            & git clone https://github.com/Microsoft/vcpkg.git vcpkg
            if ($LASTEXITCODE -eq 0) {
                Write-Status "✓ vcpkg cloned successfully" "Success"
            } else {
                Write-Status "✗ Failed to clone vcpkg" "Error"
                exit 1
            }
        } else {
            Write-Status "✗ Git is required to clone vcpkg" "Error"
            exit 1
        }
    } else {
        Write-Status "✓ vcpkg directory already exists" "Success"
    }
    
    # Bootstrap vcpkg
    $vcpkgExe = "vcpkg\vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Status "Bootstrapping vcpkg..." "Warning"
        Push-Location vcpkg
        & .\bootstrap-vcpkg.bat
        if ($LASTEXITCODE -eq 0) {
            Write-Status "✓ vcpkg bootstrapped successfully" "Success"
        } else {
            Write-Status "✗ Failed to bootstrap vcpkg" "Error"
            Pop-Location
            exit 1
        }
        Pop-Location
    } else {
        Write-Status "✓ vcpkg already bootstrapped" "Success"
    }
    
    # Install dependencies
    Write-Status "Installing dependencies..." "Warning"
    & $vcpkgExe install --triplet x64-windows
    if ($LASTEXITCODE -eq 0) {
        Write-Status "✓ Dependencies installed successfully" "Success"
    } else {
        Write-Status "⚠ Some dependencies may not have installed correctly" "Warning"
    }
} else {
    Write-Status "Skipping vcpkg setup" "Warning"
}

# Generate project files
Write-Status "Generating Visual Studio project files..." "Warning"
if (Test-Command "python") {
    & python scripts/generate-vs-projects.py
    if ($LASTEXITCODE -eq 0) {
        Write-Status "✓ Project files generated successfully" "Success"
    } else {
        Write-Status "⚠ Failed to generate project files" "Warning"
    }
} else {
    Write-Status "⚠ Python required to generate project files" "Warning"
}

# Test build
if (-not $SkipBuild) {
    Write-Status "Testing build..." "Warning"
    if (Test-Path "scripts\build-windows.ps1") {
        & .\scripts\build-windows.ps1 -Configuration Release -Platform x64
        if ($LASTEXITCODE -eq 0) {
            Write-Status "✓ Test build successful" "Success"
        } else {
            Write-Status "⚠ Test build failed, but setup is complete" "Warning"
        }
    } else {
        Write-Status "⚠ Build script not found" "Warning"
    }
} else {
    Write-Status "Skipping test build" "Warning"
}

# Final instructions
Write-Status "========================================" "Info"
Write-Status "✓ YAZE Windows development setup complete!" "Success"
Write-Status "========================================" "Info"
Write-Status ""
Write-Status "Next steps:" "Warning"
Write-Status "1. Open YAZE.sln in Visual Studio 2022" "White"
Write-Status "2. Select configuration (Debug/Release) and platform (x64/x86/ARM64)" "White"
Write-Status "3. Build the solution (Ctrl+Shift+B)" "White"
Write-Status ""
Write-Status "Or use command line:" "Warning"
Write-Status "  .\scripts\build-windows.ps1 -Configuration Release -Platform x64" "White"
Write-Status ""
Write-Status "For more information, see docs/windows-development-guide.md" "Info"