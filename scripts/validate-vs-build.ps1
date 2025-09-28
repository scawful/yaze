# PowerShell script to validate Visual Studio project builds
# This script ensures that the .vcxproj files work correctly with vcpkg

param(
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Debug",
    
    [Parameter(Mandatory=$false)]
    [string]$Platform = "x64",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "YAZE Visual Studio Build Validation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "Platform: $Platform" -ForegroundColor Yellow
Write-Host "Clean Build: $Clean" -ForegroundColor Yellow
Write-Host ""

# Check if we're in the right directory
if (-not (Test-Path "yaze.sln")) {
    Write-Error "yaze.sln not found. Please run this script from the project root directory."
    exit 1
}

# Check if vcpkg is available
if (-not $env:VCPKG_ROOT) {
    Write-Error "VCPKG_ROOT environment variable is not set. Please install vcpkg and set the environment variable."
    exit 1
}

if (-not (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    Write-Error "vcpkg.exe not found at $env:VCPKG_ROOT\vcpkg.exe"
    exit 1
}

Write-Host "✓ vcpkg found at: $env:VCPKG_ROOT" -ForegroundColor Green

# Check if required dependencies are installed
Write-Host "Checking vcpkg dependencies..." -ForegroundColor Yellow
$dependencies = @("zlib:$Platform-windows", "libpng:$Platform-windows", "sdl2[vulkan]:$Platform-windows", "abseil:$Platform-windows")

foreach ($dep in $dependencies) {
    $result = & "$env:VCPKG_ROOT\vcpkg.exe" list $dep 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $result) {
        Write-Host "Installing missing dependency: $dep" -ForegroundColor Yellow
        & "$env:VCPKG_ROOT\vcpkg.exe" install $dep
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to install dependency: $dep"
            exit 1
        }
    } else {
        Write-Host "✓ $dep is installed" -ForegroundColor Green
    }
}

# Clean build if requested
if ($Clean) {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Ensure build directory exists
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Build using MSBuild
Write-Host "Building with MSBuild..." -ForegroundColor Yellow
$msbuildArgs = @(
    "yaze.sln"
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    "/p:VcpkgEnabled=true"
    "/p:VcpkgManifestInstall=true"
    "/m"  # Multi-processor build
)

if ($Verbose) {
    $msbuildArgs += "/verbosity:detailed"
}

Write-Host "MSBuild command: msbuild $($msbuildArgs -join ' ')" -ForegroundColor Gray
& msbuild @msbuildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "MSBuild failed with exit code $LASTEXITCODE"
    exit 1
}

Write-Host "✓ Build completed successfully" -ForegroundColor Green

# Verify executable was created
$exePath = "build\bin\$Configuration\yaze.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at expected path: $exePath"
    exit 1
}

Write-Host "✓ Executable created: $exePath" -ForegroundColor Green

# Verify assets were copied
$assetsPath = "build\bin\$Configuration\assets"
if (-not (Test-Path $assetsPath)) {
    Write-Error "Assets directory not found at expected path: $assetsPath"
    exit 1
}

Write-Host "✓ Assets copied to: $assetsPath" -ForegroundColor Green

# Test that the executable runs (basic test)
Write-Host "Testing executable startup..." -ForegroundColor Yellow
$testResult = & $exePath --help 2>&1
$exitCode = $LASTEXITCODE

# Check if it's the test main or app main
if ($testResult -match "Google Test" -or $testResult -match "gtest") {
    Write-Error "Executable is running test main instead of app main!"
    Write-Host "Output: $testResult" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Executable runs correctly (exit code: $exitCode)" -ForegroundColor Green

# Display file info
$exeInfo = Get-Item $exePath
Write-Host ""
Write-Host "Build Summary:" -ForegroundColor Cyan
Write-Host "  Executable: $($exeInfo.FullName)" -ForegroundColor White
Write-Host "  Size: $([math]::Round($exeInfo.Length / 1MB, 2)) MB" -ForegroundColor White
Write-Host "  Created: $($exeInfo.CreationTime)" -ForegroundColor White

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✓ Visual Studio build validation PASSED" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
