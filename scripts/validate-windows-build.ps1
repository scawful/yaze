# YAZE Windows Build Validation Script
# This script validates that the Windows build environment is properly set up

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
Write-Status "YAZE Windows Build Validation" "Info"
Write-Status "========================================" "Info"

$allGood = $true

# Check if we're in the right directory
if (-not (Test-Path "YAZE.sln")) {
    Write-Status "✗ YAZE.sln not found" "Error"
    $allGood = $false
} else {
    Write-Status "✓ YAZE.sln found" "Success"
}

# Check for vcpkg.json
if (-not (Test-Path "vcpkg.json")) {
    Write-Status "✗ vcpkg.json not found" "Error"
    $allGood = $false
} else {
    Write-Status "✓ vcpkg.json found" "Success"
}

# Check for Visual Studio
if (Test-VisualStudio) {
    Write-Status "✓ Visual Studio 2022 with C++ workload found" "Success"
} else {
    Write-Status "✗ Visual Studio 2022 with C++ workload not found" "Error"
    $allGood = $false
}

# Check for Git
if (Test-Command "git") {
    $gitVersion = & git --version
    Write-Status "✓ Git found: $gitVersion" "Success"
} else {
    Write-Status "✗ Git not found" "Error"
    $allGood = $false
}

# Check for Python
if (Test-Command "python") {
    $pythonVersion = & python --version
    Write-Status "✓ Python found: $pythonVersion" "Success"
} else {
    Write-Status "✗ Python not found" "Error"
    $allGood = $false
}

# Check for vcpkg
if (Test-Path "vcpkg\vcpkg.exe") {
    Write-Status "✓ vcpkg found and bootstrapped" "Success"
} else {
    Write-Status "✗ vcpkg not found or not bootstrapped" "Error"
    $allGood = $false
}

# Check for generated project files
if (Test-Path "YAZE.vcxproj") {
    Write-Status "✓ Visual Studio project file found" "Success"
} else {
    Write-Status "✗ Visual Studio project file not found" "Error"
    Write-Status "  Run: python scripts/generate-vs-projects.py" "Info"
    $allGood = $false
}

# Check for config file
if (Test-Path "yaze_config.h") {
    Write-Status "✓ yaze_config.h found" "Success"
} else {
    Write-Status "⚠ yaze_config.h not found (will be generated during build)" "Warning"
}

Write-Status "========================================" "Info"
if ($allGood) {
    Write-Status "✓ All checks passed! Build environment is ready." "Success"
    Write-Status ""
    Write-Status "You can now build YAZE using:" "Warning"
    Write-Status "  .\scripts\build-windows.ps1" "White"
    Write-Status "  or" "White"
    Write-Status "  .\scripts\build-windows.bat" "White"
} else {
    Write-Status "✗ Some checks failed. Please fix the issues above." "Error"
    Write-Status ""
    Write-Status "Run the setup script to fix issues:" "Warning"
    Write-Status "  .\scripts\setup-windows-dev.ps1" "White"
}
Write-Status "========================================" "Info"
