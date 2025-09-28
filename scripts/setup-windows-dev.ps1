# YAZE Windows Development Setup Script
# Simple linear approach without complex if-else statements

param(
    [switch]$SkipVcpkg,
    [switch]$SkipVS,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "YAZE Windows Development Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Step 1: Check project directory
Write-Host "Step 1: Checking project directory..." -ForegroundColor Yellow
$projectValid = Test-Path "YAZE.sln"
if ($projectValid) {
    Write-Host "✓ YAZE.sln found" -ForegroundColor Green
} else {
    Write-Host "✗ YAZE.sln not found" -ForegroundColor Red
    Write-Host "Please run this script from the YAZE project root directory" -ForegroundColor Yellow
    exit 1
}

# Step 2: Check Visual Studio
Write-Host "Step 2: Checking Visual Studio..." -ForegroundColor Yellow
if ($SkipVS) {
    Write-Host "Skipping Visual Studio check" -ForegroundColor Yellow
} else {
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
        Write-Host "✓ Visual Studio 2022 with C++ workload found" -ForegroundColor Green
    } else {
        Write-Host "⚠ Visual Studio 2022 with C++ workload not found" -ForegroundColor Yellow
        Write-Host "Please install Visual Studio 2022 with 'Desktop development with C++' workload" -ForegroundColor White
    }
}

# Step 3: Check Git
Write-Host "Step 3: Checking Git..." -ForegroundColor Yellow
$gitFound = $false
try {
    $null = Get-Command git -ErrorAction Stop
    $gitFound = $true
} catch {
    $gitFound = $false
}

if ($gitFound) {
    $gitVersion = & git --version
    Write-Host "✓ Git found: $gitVersion" -ForegroundColor Green
} else {
    Write-Host "⚠ Git not found" -ForegroundColor Yellow
    Write-Host "Please install Git for Windows from: https://git-scm.com/download/win" -ForegroundColor White
}

# Step 4: Check Python
Write-Host "Step 4: Checking Python..." -ForegroundColor Yellow
$pythonFound = $false
try {
    $null = Get-Command python -ErrorAction Stop
    $pythonFound = $true
} catch {
    $pythonFound = $false
}

if ($pythonFound) {
    $pythonVersion = & python --version
    Write-Host "✓ Python found: $pythonVersion" -ForegroundColor Green
} else {
    Write-Host "⚠ Python not found" -ForegroundColor Yellow
    Write-Host "Please install Python 3.8+ from: https://www.python.org/downloads/" -ForegroundColor White
}

# Step 5: Setup vcpkg
Write-Host "Step 5: Setting up vcpkg..." -ForegroundColor Yellow
if ($SkipVcpkg) {
    Write-Host "Skipping vcpkg setup" -ForegroundColor Yellow
} else {
    # Clone vcpkg
    $vcpkgExists = Test-Path "vcpkg"
    if (-not $vcpkgExists) {
        Write-Host "Cloning vcpkg..." -ForegroundColor Yellow
        if ($gitFound) {
            & git clone https://github.com/Microsoft/vcpkg.git vcpkg
            $cloneSuccess = ($LASTEXITCODE -eq 0)
            if ($cloneSuccess) {
                Write-Host "✓ vcpkg cloned successfully" -ForegroundColor Green
            } else {
                Write-Host "✗ Failed to clone vcpkg" -ForegroundColor Red
                exit 1
            }
        } else {
            Write-Host "✗ Git is required to clone vcpkg" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "✓ vcpkg directory already exists" -ForegroundColor Green
    }
    
    # Bootstrap vcpkg
    $vcpkgExe = "vcpkg\vcpkg.exe"
    $vcpkgBootstrapped = Test-Path $vcpkgExe
    if (-not $vcpkgBootstrapped) {
        Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
        Push-Location vcpkg
        & .\bootstrap-vcpkg.bat
        $bootstrapSuccess = ($LASTEXITCODE -eq 0)
        Pop-Location
        if ($bootstrapSuccess) {
            Write-Host "✓ vcpkg bootstrapped successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Failed to bootstrap vcpkg" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "✓ vcpkg already bootstrapped" -ForegroundColor Green
    }
    
    # Install dependencies
    Write-Host "Installing dependencies..." -ForegroundColor Yellow
    & $vcpkgExe install --triplet x64-windows
    $installSuccess = ($LASTEXITCODE -eq 0)
    if ($installSuccess) {
        Write-Host "✓ Dependencies installed successfully" -ForegroundColor Green
    } else {
        Write-Host "⚠ Some dependencies may not have installed correctly" -ForegroundColor Yellow
    }
}

# Step 6: Generate project files
Write-Host "Step 6: Generating Visual Studio project files..." -ForegroundColor Yellow
if ($pythonFound) {
    & python scripts/generate-vs-projects.py
    $generateSuccess = ($LASTEXITCODE -eq 0)
    if ($generateSuccess) {
        Write-Host "✓ Project files generated successfully" -ForegroundColor Green
    } else {
        Write-Host "⚠ Failed to generate project files" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠ Python required to generate project files" -ForegroundColor Yellow
}

# Step 7: Test build
Write-Host "Step 7: Testing build..." -ForegroundColor Yellow
if ($SkipBuild) {
    Write-Host "Skipping test build" -ForegroundColor Yellow
} else {
    $buildScriptExists = Test-Path "scripts\build-windows.ps1"
    if ($buildScriptExists) {
        & .\scripts\build-windows.ps1 -Configuration Release -Platform x64
        $buildSuccess = ($LASTEXITCODE -eq 0)
        if ($buildSuccess) {
            Write-Host "✓ Test build successful" -ForegroundColor Green
        } else {
            Write-Host "⚠ Test build failed, but setup is complete" -ForegroundColor Yellow
        }
    } else {
        Write-Host "⚠ Build script not found" -ForegroundColor Yellow
    }
}

# Final instructions
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✓ YAZE Windows development setup complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open YAZE.sln in Visual Studio 2022" -ForegroundColor White
Write-Host "2. Select configuration (Debug/Release) and platform (x64/x86/ARM64)" -ForegroundColor White
Write-Host "3. Build the solution (Ctrl+Shift+B)" -ForegroundColor White
Write-Host ""
Write-Host "Or use command line:" -ForegroundColor Yellow
Write-Host "  .\scripts\build-windows.ps1 -Configuration Release -Platform x64" -ForegroundColor White
Write-Host ""
Write-Host "For more information, see docs/windows-development-guide.md" -ForegroundColor Cyan