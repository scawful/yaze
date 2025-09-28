# PowerShell script to build YAZE on Windows
# This script sets up the environment and builds the project using Visual Studio

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$Clean = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "YAZE Windows Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check if we're in the right directory
if (-not (Test-Path "YAZE.sln")) {
    Write-Error "YAZE.sln not found. Please run this script from the project root directory."
    exit 1
}

Write-Host "✓ YAZE.sln found" -ForegroundColor Green

# Check for Visual Studio
Write-Host "Checking for Visual Studio..." -ForegroundColor Yellow
try {
    $msbuildPath = Get-Command msbuild -ErrorAction Stop
    Write-Host "✓ MSBuild found at: $($msbuildPath.Source)" -ForegroundColor Green
} catch {
    Write-Error "MSBuild not found. Please install Visual Studio 2022 or later with the C++ development workload."
    exit 1
}

# Check for vcpkg
Write-Host "Checking for vcpkg..." -ForegroundColor Yellow
if (-not (Test-Path "vcpkg.json")) {
    Write-Error "vcpkg.json not found. Please ensure vcpkg is properly configured."
    exit 1
}

Write-Host "✓ vcpkg.json found" -ForegroundColor Green

# Display build configuration
Write-Host "Build Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "Build Platform: $Platform" -ForegroundColor Yellow

# Create build directories
Write-Host "Creating build directories..." -ForegroundColor Yellow
$directories = @("build", "build\bin", "build\obj")
foreach ($dir in $directories) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "✓ Created directory: $dir" -ForegroundColor Green
    }
}

# Clean build if requested
if ($Clean) {
    Write-Host "Cleaning build directories..." -ForegroundColor Yellow
    if (Test-Path "build\bin") {
        Remove-Item -Recurse -Force "build\bin\*" -ErrorAction SilentlyContinue
    }
    if (Test-Path "build\obj") {
        Remove-Item -Recurse -Force "build\obj\*" -ErrorAction SilentlyContinue
    }
    Write-Host "✓ Build directories cleaned" -ForegroundColor Green
}

# Generate yaze_config.h if it doesn't exist
if (-not (Test-Path "yaze_config.h")) {
    Write-Host "Generating yaze_config.h..." -ForegroundColor Yellow
    if (Test-Path "src\yaze_config.h.in") {
        Copy-Item "src\yaze_config.h.in" "yaze_config.h"
        (Get-Content 'yaze_config.h') -replace '@yaze_VERSION_MAJOR@', '0' -replace '@yaze_VERSION_MINOR@', '3' -replace '@yaze_VERSION_PATCH@', '1' | Set-Content 'yaze_config.h'
        Write-Host "✓ Generated yaze_config.h" -ForegroundColor Green
    } else {
        Write-Warning "yaze_config.h.in not found, creating basic config"
        @"
// yaze config file
#define YAZE_VERSION_MAJOR 0
#define YAZE_VERSION_MINOR 3
#define YAZE_VERSION_PATCH 1
"@ | Out-File -FilePath "yaze_config.h" -Encoding UTF8
    }
}

# Build using MSBuild
Write-Host "Building with MSBuild..." -ForegroundColor Yellow

$msbuildArgs = @(
    "YAZE.sln"
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    "/p:VcpkgEnabled=true"
    "/p:VcpkgManifestInstall=true"
    "/m"  # Multi-processor build
)

if ($Verbose) {
    $msbuildArgs += "/verbosity:detailed"
} else {
    $msbuildArgs += "/verbosity:minimal"
}

$msbuildCommand = "msbuild $($msbuildArgs -join ' ')"
Write-Host "Command: $msbuildCommand" -ForegroundColor Gray

try {
    & msbuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
    Write-Host "✓ Build completed successfully" -ForegroundColor Green
} catch {
    Write-Error "Build failed: $_"
    exit 1
}

# Verify executable was created
$exePath = "build\bin\$Configuration\yaze.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at expected path: $exePath"
    exit 1
}

Write-Host "✓ Executable created: $exePath" -ForegroundColor Green

# Test that the executable runs (basic test)
Write-Host "Testing executable startup..." -ForegroundColor Yellow
try {
    $testResult = & $exePath --help 2>&1
    $exitCode = $LASTEXITCODE
    
    # Check if it's the test main or app main
    if ($testResult -match "Google Test|gtest") {
        Write-Error "Executable is running test main instead of app main!"
        Write-Host "Output: $testResult" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "✓ Executable runs correctly (exit code: $exitCode)" -ForegroundColor Green
} catch {
    Write-Warning "Could not test executable: $_"
}

# Display file info
$exeInfo = Get-Item $exePath
$fileSizeMB = [math]::Round($exeInfo.Length / 1MB, 2)
Write-Host "Executable size: $fileSizeMB MB" -ForegroundColor Cyan

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✓ YAZE Windows build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Build Configuration: $Configuration" -ForegroundColor White
Write-Host "Build Platform: $Platform" -ForegroundColor White
Write-Host "Executable: $exePath" -ForegroundColor White
Write-Host ""
Write-Host "To run YAZE:" -ForegroundColor Yellow
Write-Host "  $exePath" -ForegroundColor White
Write-Host ""
Write-Host "To build other configurations:" -ForegroundColor Yellow
Write-Host "  .\scripts\build-windows.ps1 -Configuration Debug -Platform x64" -ForegroundColor White
Write-Host "  .\scripts\build-windows.ps1 -Configuration Release -Platform x86" -ForegroundColor White
Write-Host "  .\scripts\build-windows.ps1 -Configuration RelWithDebInfo -Platform ARM64" -ForegroundColor White
Write-Host "  .\scripts\build-windows.ps1 -Clean" -ForegroundColor White
Write-Host ""
