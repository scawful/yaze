# YAZE vcpkg Setup Script
# This script sets up vcpkg for YAZE development on Windows

param(
    [string]$Triplet = "x64-windows"
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