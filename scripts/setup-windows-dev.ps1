# YAZE Windows Development Setup Script
# Function-based approach with minimal if-else statements

param(
    [switch]$SkipVcpkg,
    [switch]$SkipVS,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"

function Write-Status {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
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

function Check-ProjectDirectory {
    Write-Status "Checking project directory..." "Yellow"
    $projectValid = Test-Path "YAZE.sln"
    if ($projectValid) {
        Write-Status "✓ YAZE.sln found" "Green"
        return $true
    } else {
        Write-Status "✗ YAZE.sln not found" "Red"
        Write-Status "Please run this script from the YAZE project root directory" "Yellow"
        return $false
    }
}

function Check-VisualStudio {
    Write-Status "Checking Visual Studio..." "Yellow"
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsFound = $false
    if (Test-Path $vsWhere) {
        $vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $msbuildPath = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            $vsFound = Test-Path $msbuildPath
        }
    }
    
    if ($vsFound) {
        Write-Status "✓ Visual Studio 2022 with C++ workload found" "Green"
    } else {
        Write-Status "⚠ Visual Studio 2022 with C++ workload not found" "Yellow"
        Write-Status "Please install Visual Studio 2022 with 'Desktop development with C++' workload" "White"
    }
    return $vsFound
}

function Check-Git {
    Write-Status "Checking Git..." "Yellow"
    $gitFound = Test-Command "git"
    if ($gitFound) {
        $gitVersion = & git --version
        Write-Status "✓ Git found: $gitVersion" "Green"
    } else {
        Write-Status "⚠ Git not found" "Yellow"
        Write-Status "Please install Git for Windows from: https://git-scm.com/download/win" "White"
    }
    return $gitFound
}

function Check-Python {
    Write-Status "Checking Python..." "Yellow"
    $pythonFound = Test-Command "python"
    if ($pythonFound) {
        $pythonVersion = & python --version
        Write-Status "✓ Python found: $pythonVersion" "Green"
    } else {
        Write-Status "⚠ Python not found" "Yellow"
        Write-Status "Please install Python 3.8+ from: https://www.python.org/downloads/" "White"
    }
    return $pythonFound
}

function Setup-Vcpkg {
    Write-Status "Setting up vcpkg..." "Yellow"
    
    # Clone vcpkg
    $vcpkgExists = Test-Path "vcpkg"
    if (-not $vcpkgExists) {
        Write-Status "Cloning vcpkg..." "Yellow"
        $gitFound = Test-Command "git"
        if ($gitFound) {
            & git clone https://github.com/Microsoft/vcpkg.git vcpkg
            $cloneSuccess = ($LASTEXITCODE -eq 0)
            if ($cloneSuccess) {
                Write-Status "✓ vcpkg cloned successfully" "Green"
            } else {
                Write-Status "✗ Failed to clone vcpkg" "Red"
                exit 1
            }
        } else {
            Write-Status "✗ Git is required to clone vcpkg" "Red"
            exit 1
        }
    } else {
        Write-Status "✓ vcpkg directory already exists" "Green"
    }
    
    # Bootstrap vcpkg
    $vcpkgExe = "vcpkg\vcpkg.exe"
    $vcpkgBootstrapped = Test-Path $vcpkgExe
    if (-not $vcpkgBootstrapped) {
        Write-Status "Bootstrapping vcpkg..." "Yellow"
        Push-Location vcpkg
        & .\bootstrap-vcpkg.bat
        $bootstrapSuccess = ($LASTEXITCODE -eq 0)
        Pop-Location
        if ($bootstrapSuccess) {
            Write-Status "✓ vcpkg bootstrapped successfully" "Green"
        } else {
            Write-Status "✗ Failed to bootstrap vcpkg" "Red"
            exit 1
        }
    } else {
        Write-Status "✓ vcpkg already bootstrapped" "Green"
    }
    
    # Install dependencies
    Write-Status "Installing dependencies..." "Yellow"
    & $vcpkgExe install --triplet x64-windows
    $installSuccess = ($LASTEXITCODE -eq 0)
    if ($installSuccess) {
        Write-Status "✓ Dependencies installed successfully" "Green"
    } else {
        Write-Status "⚠ Some dependencies may not have installed correctly" "Yellow"
    }
}

function Generate-ProjectFiles {
    Write-Status "Generating Visual Studio project files..." "Yellow"
    $pythonFound = Test-Command "python"
    if ($pythonFound) {
        & python scripts/generate-vs-projects.py
        $generateSuccess = ($LASTEXITCODE -eq 0)
        if ($generateSuccess) {
            Write-Status "✓ Project files generated successfully" "Green"
        } else {
            Write-Status "⚠ Failed to generate project files" "Yellow"
        }
    } else {
        Write-Status "⚠ Python required to generate project files" "Yellow"
    }
}

function Test-Build {
    Write-Status "Testing build..." "Yellow"
    $buildScriptExists = Test-Path "scripts\build-windows.ps1"
    if ($buildScriptExists) {
        & .\scripts\build-windows.ps1 -Configuration Release -Platform x64
        $buildSuccess = ($LASTEXITCODE -eq 0)
        if ($buildSuccess) {
            Write-Status "✓ Test build successful" "Green"
        } else {
            Write-Status "⚠ Test build failed, but setup is complete" "Yellow"
        }
    } else {
        Write-Status "⚠ Build script not found" "Yellow"
    }
}

function Show-FinalInstructions {
    Write-Status "========================================" "Cyan"
    Write-Status "✓ YAZE Windows development setup complete!" "Green"
    Write-Status "========================================" "Cyan"
    Write-Status ""
    Write-Status "Next steps:" "Yellow"
    Write-Status "1. Open YAZE.sln in Visual Studio 2022" "White"
    Write-Status "2. Select configuration (Debug/Release) and platform (x64/x86/ARM64)" "White"
    Write-Status "3. Build the solution (Ctrl+Shift+B)" "White"
    Write-Status ""
    Write-Status "Or use command line:" "Yellow"
    Write-Status "  .\scripts\build-windows.ps1 -Configuration Release -Platform x64" "White"
    Write-Status ""
    Write-Status "For more information, see docs/windows-development-guide.md" "Cyan"
}

# Main execution
Write-Status "========================================" "Cyan"
Write-Status "YAZE Windows Development Setup" "Cyan"
Write-Status "========================================" "Cyan"

# Execute setup steps
$projectValid = Check-ProjectDirectory
if (-not $projectValid) { exit 1 }

if (-not $SkipVS) {
    Check-VisualStudio
}

Check-Git
Check-Python

if (-not $SkipVcpkg) {
    Setup-Vcpkg
} else {
    Write-Status "Skipping vcpkg setup" "Yellow"
}

Generate-ProjectFiles

if (-not $SkipBuild) {
    Test-Build
} else {
    Write-Status "Skipping test build" "Yellow"
}

Show-FinalInstructions