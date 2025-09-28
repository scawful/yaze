# Test CMake configuration for YAZE
# This script tests if CMake can configure the project without errors

param(
    [string]$Architecture = "x64",
    [switch]$Clean = $false
)

Write-Host "Testing CMake configuration for YAZE..." -ForegroundColor Green

# Check if we're on Windows
if ($env:OS -ne "Windows_NT") {
    Write-Host "This script is designed for Windows only." -ForegroundColor Red
    exit 1
}

# Check if CMake is available
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakePath) {
    Write-Host "CMake not found. Please run setup-windows-dev.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "CMake found: $($cmakePath.Source)" -ForegroundColor Green

# Set up paths
$SourceDir = Split-Path -Parent $PSScriptRoot
$TestBuildDir = Join-Path $SourceDir "build-test"

Write-Host "Source directory: $SourceDir" -ForegroundColor Cyan
Write-Host "Test build directory: $TestBuildDir" -ForegroundColor Cyan

# Clean test build directory if requested
if ($Clean -and (Test-Path $TestBuildDir)) {
    Write-Host "Cleaning test build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $TestBuildDir
}

# Create test build directory
if (-not (Test-Path $TestBuildDir)) {
    New-Item -ItemType Directory -Path $TestBuildDir | Out-Null
}

# Test CMake configuration with minimal settings
Write-Host "Testing CMake configuration..." -ForegroundColor Yellow

$TestArgs = @(
    "-B", $TestBuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", $Architecture,
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DCMAKE_POLICY_VERSION_MAXIMUM=3.28",
    "-DCMAKE_WARN_DEPRECATED=OFF",
    "-DABSL_PROPAGATE_CXX_STD=ON",
    "-DTHREADS_PREFER_PTHREAD_FLAG=OFF",
    "-DYAZE_BUILD_TESTS=OFF",
    "-DYAZE_BUILD_APP=ON",
    "-DYAZE_BUILD_LIB=OFF",
    "-DYAZE_BUILD_EMU=OFF",
    "-DYAZE_BUILD_Z3ED=OFF",
    "-DYAZE_ENABLE_ROM_TESTS=OFF",
    "-DYAZE_ENABLE_EXPERIMENTAL_TESTS=OFF",
    "-DYAZE_ENABLE_UI_TESTS=OFF",
    "-DYAZE_INSTALL_LIB=OFF",
    "-DYAZE_MINIMAL_BUILD=ON"
)

Write-Host "CMake command: cmake $($TestArgs -join ' ') $SourceDir" -ForegroundColor Gray

& cmake @TestArgs $SourceDir

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ CMake configuration test PASSED!" -ForegroundColor Green
    Write-Host "The project can be configured successfully." -ForegroundColor Green
    
    # Clean up test build directory
    if (Test-Path $TestBuildDir) {
        Remove-Item -Recurse -Force $TestBuildDir
        Write-Host "Test build directory cleaned up." -ForegroundColor Gray
    }
    
    Write-Host "`nYou can now run the full project generation script:" -ForegroundColor Cyan
    Write-Host ".\scripts\generate-vs-projects.ps1" -ForegroundColor White
} else {
    Write-Host "❌ CMake configuration test FAILED!" -ForegroundColor Red
    Write-Host "Please check the error messages above." -ForegroundColor Red
    exit 1
}
