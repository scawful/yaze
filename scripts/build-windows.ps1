# YAZE Windows Build Script
# This script builds the YAZE project on Windows using MSBuild

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$Clean,
    [switch]$Verbose
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

function Get-MSBuildPath {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $msbuildPath = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $msbuildPath) {
                return $msbuildPath
            }
        }
    }
    return $null
}

# Main script
Write-Status "========================================" "Info"
Write-Status "YAZE Windows Build Script" "Info"
Write-Status "========================================" "Info"

# Validate parameters
$ValidConfigs = @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")
$ValidPlatforms = @("x64", "x86", "ARM64")

if ($ValidConfigs -notcontains $Configuration) {
    Write-Status "ERROR: Invalid configuration '$Configuration'. Valid options: $($ValidConfigs -join ', ')" "Error"
    exit 1
}

if ($ValidPlatforms -notcontains $Platform) {
    Write-Status "ERROR: Invalid platform '$Platform'. Valid options: $($ValidPlatforms -join ', ')" "Error"
    exit 1
}

Write-Status "Build Configuration: $Configuration" "Warning"
Write-Status "Build Platform: $Platform" "Warning"

# Check if we're in the right directory
if (-not (Test-Path "YAZE.sln")) {
    Write-Status "ERROR: YAZE.sln not found. Please run this script from the project root directory." "Error"
    exit 1
}

Write-Status "✓ Found YAZE.sln" "Success"

# Check for MSBuild
$msbuildPath = Get-MSBuildPath
if (-not $msbuildPath) {
    Write-Status "ERROR: MSBuild not found. Please install Visual Studio 2022 with C++ workload." "Error"
    exit 1
}

Write-Status "✓ MSBuild found at: $msbuildPath" "Success"

# Check for vcpkg
if (-not (Test-Path "vcpkg.json")) {
    Write-Status "WARNING: vcpkg.json not found. vcpkg integration may not work properly." "Warning"
}

# Create build directories
Write-Status "Creating build directories..." "Warning"
$directories = @("build", "build\bin", "build\obj")
foreach ($dir in $directories) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Status "✓ Created directory: $dir" "Success"
    }
}

# Clean build if requested
if ($Clean) {
    Write-Status "Cleaning build directories..." "Warning"
    if (Test-Path "build\bin") {
        Remove-Item -Recurse -Force "build\bin\*" -ErrorAction SilentlyContinue
    }
    if (Test-Path "build\obj") {
        Remove-Item -Recurse -Force "build\obj\*" -ErrorAction SilentlyContinue
    }
    Write-Status "✓ Build directories cleaned" "Success"
}

# Generate yaze_config.h if it doesn't exist
if (-not (Test-Path "yaze_config.h")) {
    Write-Status "Generating yaze_config.h..." "Warning"
    if (Test-Path "src\yaze_config.h.in") {
        Copy-Item "src\yaze_config.h.in" "yaze_config.h"
        $content = Get-Content "yaze_config.h" -Raw
        $content = $content -replace '@yaze_VERSION_MAJOR@', '0'
        $content = $content -replace '@yaze_VERSION_MINOR@', '3'
        $content = $content -replace '@yaze_VERSION_PATCH@', '1'
        Set-Content "yaze_config.h" $content
        Write-Status "✓ Generated yaze_config.h" "Success"
    } else {
        Write-Status "WARNING: yaze_config.h.in not found, creating basic config" "Warning"
        @"
// yaze config file
#define YAZE_VERSION_MAJOR 0
#define YAZE_VERSION_MINOR 3
#define YAZE_VERSION_PATCH 1
"@ | Out-File -FilePath "yaze_config.h" -Encoding UTF8
    }
}

# Build using MSBuild
Write-Status "Building with MSBuild..." "Warning"

$msbuildArgs = @(
    "YAZE.sln"
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    "/p:VcpkgEnabled=true"
    "/p:VcpkgManifestInstall=true"
    "/m"
)

if ($Verbose) {
    $msbuildArgs += "/verbosity:detailed"
} else {
    $msbuildArgs += "/verbosity:minimal"
}

$msbuildCommand = "& `"$msbuildPath`" $($msbuildArgs -join ' ')"
Write-Status "Command: $msbuildCommand" "Info"

try {
    & $msbuildPath @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
    Write-Status "✓ Build completed successfully" "Success"
} catch {
    Write-Status "✗ Build failed: $_" "Error"
    exit 1
}

# Verify executable was created
$exePath = "build\bin\$Configuration\yaze.exe"
if (-not (Test-Path $exePath)) {
    Write-Status "ERROR: Executable not found at expected path: $exePath" "Error"
    exit 1
}

Write-Status "✓ Executable created: $exePath" "Success"

# Test that the executable runs
Write-Status "Testing executable..." "Warning"
try {
    $testResult = & $exePath --help 2>&1
    $exitCode = $LASTEXITCODE
    
    # Check if it's the test main or app main
    if ($testResult -match "Google Test|gtest") {
        Write-Status "ERROR: Executable is running test main instead of app main!" "Error"
        Write-Status "Output: $testResult" "Error"
        exit 1
    }
    
    Write-Status "✓ Executable runs correctly (exit code: $exitCode)" "Success"
} catch {
    Write-Status "WARNING: Could not test executable: $_" "Warning"
}

# Display file info
$exeInfo = Get-Item $exePath
$fileSizeMB = [math]::Round($exeInfo.Length / 1MB, 2)
Write-Status "Executable size: $fileSizeMB MB" "Info"

Write-Status "========================================" "Info"
Write-Status "✓ YAZE Windows build completed successfully!" "Success"
Write-Status "========================================" "Info"
Write-Status ""
Write-Status "Build Configuration: $Configuration" "White"
Write-Status "Build Platform: $Platform" "White"
Write-Status "Executable: $exePath" "White"
Write-Status ""
Write-Status "To run YAZE:" "Warning"
Write-Status "  $exePath" "White"
Write-Status ""
Write-Status "To build other configurations:" "Warning"
Write-Status "  .\scripts\build-windows.ps1 -Configuration Debug -Platform x64" "White"
Write-Status "  .\scripts\build-windows.ps1 -Configuration Release -Platform x86" "White"
Write-Status "  .\scripts\build-windows.ps1 -Configuration RelWithDebInfo -Platform ARM64" "White"
Write-Status "  .\scripts\build-windows.ps1 -Clean" "White"
Write-Status ""