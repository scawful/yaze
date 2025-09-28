# PowerShell script to set up Windows development environment for YAZE
# This script helps developers get started with building YAZE on Windows

param(
    [switch]$SkipVcpkg = $false,
    [switch]$SkipVS = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "YAZE Windows Development Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check if we're in the right directory
if (-not (Test-Path "YAZE.sln")) {
    Write-Error "YAZE.sln not found. Please run this script from the project root directory."
    exit 1
}

Write-Host "✓ Found YAZE project files" -ForegroundColor Green

# Check for Visual Studio
if (-not $SkipVS) {
    Write-Host "Checking for Visual Studio..." -ForegroundColor Yellow
    
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            Write-Host "✓ Visual Studio found at: $vsInstall" -ForegroundColor Green
            
            # Check for MSBuild
            $msbuildPath = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $msbuildPath) {
                Write-Host "✓ MSBuild found" -ForegroundColor Green
            } else {
                Write-Warning "MSBuild not found. Please ensure C++ development workload is installed."
            }
        } else {
            Write-Warning "Visual Studio 2022 with C++ workload not found."
            Write-Host "Please install Visual Studio 2022 with the following workloads:" -ForegroundColor Yellow
            Write-Host "  - Desktop development with C++" -ForegroundColor White
            Write-Host "  - Game development with C++" -ForegroundColor White
        }
    } else {
        Write-Warning "Visual Studio Installer not found. Please install Visual Studio 2022."
    }
} else {
    Write-Host "Skipping Visual Studio check" -ForegroundColor Yellow
}

# Check for Git
Write-Host "Checking for Git..." -ForegroundColor Yellow
$gitFound = $false
try {
    $null = Get-Command git -ErrorAction Stop
    $gitVersion = & git --version
    Write-Host "✓ Git found: $gitVersion" -ForegroundColor Green
    $gitFound = $true
} catch {
    Write-Warning "Git not found. Please install Git for Windows."
    Write-Host "Download from: https://git-scm.com/download/win" -ForegroundColor Yellow
}

# Check for Python
Write-Host "Checking for Python..." -ForegroundColor Yellow
$pythonFound = $false
try {
    $null = Get-Command python -ErrorAction Stop
    $pythonVersion = & python --version
    Write-Host "✓ Python found: $pythonVersion" -ForegroundColor Green
    $pythonFound = $true
} catch {
    Write-Warning "Python not found. Please install Python 3.8 or later."
    Write-Host "Download from: https://www.python.org/downloads/" -ForegroundColor Yellow
}

# Set up vcpkg
if (-not $SkipVcpkg) {
    Write-Host "Setting up vcpkg..." -ForegroundColor Yellow
    
    if (-not (Test-Path "vcpkg")) {
        Write-Host "Cloning vcpkg..." -ForegroundColor Yellow
        if ($gitFound) {
            try {
                & git clone https://github.com/Microsoft/vcpkg.git vcpkg
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "✓ vcpkg cloned successfully" -ForegroundColor Green
                } else {
                    Write-Error "Failed to clone vcpkg"
                    exit 1
                }
            } catch {
                Write-Error "Failed to clone vcpkg: $_"
                exit 1
            }
        } else {
            Write-Error "Git is required to clone vcpkg. Please install Git first."
            exit 1
        }
    } else {
        Write-Host "✓ vcpkg directory already exists" -ForegroundColor Green
    }
    
    # Bootstrap vcpkg
    $vcpkgExe = "vcpkg\vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
        try {
            Push-Location vcpkg
            & .\bootstrap-vcpkg.bat
            if ($LASTEXITCODE -eq 0) {
                Write-Host "✓ vcpkg bootstrapped successfully" -ForegroundColor Green
            } else {
                Write-Error "Failed to bootstrap vcpkg"
                Pop-Location
                exit 1
            }
            Pop-Location
        } catch {
            Pop-Location
            Write-Error "Failed to bootstrap vcpkg: $_"
            exit 1
        }
    } else {
        Write-Host "✓ vcpkg already bootstrapped" -ForegroundColor Green
    }
    
    # Install dependencies
    Write-Host "Installing dependencies with vcpkg..." -ForegroundColor Yellow
    try {
        & $vcpkgExe install --triplet x64-windows
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Dependencies installed successfully" -ForegroundColor Green
        } else {
            Write-Warning "Some dependencies may not have installed correctly"
        }
    } catch {
        Write-Warning "Failed to install dependencies: $_"
    }
} else {
    Write-Host "Skipping vcpkg setup" -ForegroundColor Yellow
}

# Generate Visual Studio project files
Write-Host "Generating Visual Studio project files..." -ForegroundColor Yellow
if ($pythonFound) {
    try {
        & python scripts/generate-vs-projects.py
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Visual Studio project files generated" -ForegroundColor Green
        } else {
            Write-Warning "Failed to generate project files"
        }
    } catch {
        Write-Warning "Failed to generate project files: $_"
        Write-Host "You can manually run: python scripts/generate-vs-projects.py" -ForegroundColor Yellow
    }
} else {
    Write-Warning "Python is required to generate project files. Please install Python first."
    Write-Host "You can manually run: python scripts/generate-vs-projects.py" -ForegroundColor Yellow
}

# Test build
Write-Host "Testing build..." -ForegroundColor Yellow
if (Test-Path "scripts\build-windows.ps1") {
    try {
        & .\scripts\build-windows.ps1 -Configuration Release -Platform x64
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Test build successful" -ForegroundColor Green
        } else {
            Write-Warning "Test build failed, but setup is complete"
        }
    } catch {
        Write-Warning "Test build failed: $_"
        Write-Host "You can manually run: .\scripts\build-windows.ps1" -ForegroundColor Yellow
    }
} else {
    Write-Warning "Build script not found. You can manually build using Visual Studio."
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✓ YAZE Windows development setup complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open YAZE.sln in Visual Studio 2022" -ForegroundColor White
Write-Host "2. Select your desired configuration (Debug/Release) and platform (x64/x86/ARM64)" -ForegroundColor White
Write-Host "3. Build the solution (Ctrl+Shift+B)" -ForegroundColor White
Write-Host ""
Write-Host "Or use the command line:" -ForegroundColor Yellow
Write-Host "  .\scripts\build-windows.ps1 -Configuration Release -Platform x64" -ForegroundColor White
Write-Host ""
Write-Host "For more information, see docs/02-build-instructions.md" -ForegroundColor Yellow