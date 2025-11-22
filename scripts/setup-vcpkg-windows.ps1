# yaze vcpkg Setup Script
# This script sets up vcpkg for YAZE development on Windows

param(
    [string]$Triplet = "x64-windows"
)

# Auto-detect architecture if not specified
if ($Triplet -eq "x64-windows") {
    $Architecture = $env:PROCESSOR_ARCHITECTURE
    if ($Architecture -eq "ARM64") {
        $Triplet = "arm64-windows"
        Write-Host "Auto-detected ARM64 architecture, using arm64-windows triplet" -ForegroundColor Yellow
    }
}

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

# Main script
Write-Status "========================================" "Info"
Write-Status "YAZE vcpkg Setup Script" "Info"
Write-Status "========================================" "Info"

Write-Status "Target triplet: $Triplet" "Warning"

# Check if we're in the right directory
if (-not (Test-Path "vcpkg.json")) {
    Write-Status "ERROR: vcpkg.json not found. Please run this script from the project root directory." "Error"
    exit 1
}

Write-Status "✓ Found vcpkg.json" "Success"

# Check for Git
if (-not (Test-Command "git")) {
    Write-Status "ERROR: Git not found. Please install Git for Windows." "Error"
    Write-Status "Download from: https://git-scm.com/download/win" "Info"
    exit 1
}

Write-Status "✓ Git found" "Success"

# Check for clang-cl
if (Test-Command "clang-cl") {
    $clangVersion = & clang-cl --version 2>&1 | Select-Object -First 1
    Write-Status "✓ clang-cl detected: $clangVersion" "Success"
} else {
    Write-Status "⚠ clang-cl not found. Install the \"LLVM tools for Visual Studio\" component for faster builds." "Warning"
}

# Check for Ninja
if (Test-Command "ninja") {
    $ninjaVersion = & ninja --version 2>&1
    Write-Status "✓ Ninja detected: version $ninjaVersion" "Success"
} else {
    Write-Status "⚠ Ninja not found. Install via: choco install ninja (required for win-dbg/win-ai presets)" "Warning"
}

# Clone vcpkg if needed
if (-not (Test-Path "vcpkg")) {
    Write-Status "Cloning vcpkg..." "Warning"
    & git clone https://github.com/Microsoft/vcpkg.git vcpkg
    if ($LASTEXITCODE -eq 0) {
        Write-Status "✓ vcpkg cloned successfully" "Success"
    } else {
        Write-Status "✗ Failed to clone vcpkg" "Error"
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
Write-Status "Installing dependencies for triplet: $Triplet" "Warning"
& $vcpkgExe install --triplet $Triplet
if ($LASTEXITCODE -eq 0) {
    Write-Status "✓ Dependencies installed successfully" "Success"
    $installedPath = "vcpkg\installed\$Triplet"
    if (Test-Path $installedPath) {
        Write-Status "✓ Cached packages under $installedPath" "Success"
    } else {
        Write-Status "⚠ vcpkg install folder missing (expected $installedPath). Builds may rebuild dependencies on first run." "Warning"
    }
} else {
    Write-Status "⚠ Some dependencies may not have installed correctly" "Warning"
}

Write-Status "========================================" "Info"
Write-Status "✓ vcpkg setup complete!" "Success"
Write-Status "========================================" "Info"
Write-Status ""
Write-Status "You can now build YAZE using:" "Warning"
Write-Status "  .\scripts\build-windows.ps1" "White"
Write-Status ""
Write-Status "For ongoing diagnostics run: .\scripts\verify-build-environment.ps1 -FixIssues" "Info"
Write-Status ""