# Setup script for vcpkg on Windows (PowerShell)
# This script helps set up vcpkg for YAZE Windows builds

Write-Host "Setting up vcpkg for YAZE Windows builds..." -ForegroundColor Green

# Check if vcpkg directory exists
if (-not (Test-Path "vcpkg")) {
    Write-Host "Cloning vcpkg..." -ForegroundColor Yellow
    git clone https://github.com/Microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to clone vcpkg repository" -ForegroundColor Red
        Read-Host "Press Enter to continue"
        exit 1
    }
}

# Bootstrap vcpkg
Set-Location vcpkg
if (-not (Test-Path "vcpkg.exe")) {
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    & .\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to bootstrap vcpkg" -ForegroundColor Red
        Read-Host "Press Enter to continue"
        exit 1
    }
}

# Integrate vcpkg with Visual Studio (optional)
Write-Host "Integrating vcpkg with Visual Studio..." -ForegroundColor Yellow
& .\vcpkg.exe integrate install

# Set environment variable for this session
$vcpkgRoot = Get-Location
$env:VCPKG_ROOT = $vcpkgRoot.Path
Write-Host "VCPKG_ROOT set to: $($env:VCPKG_ROOT)" -ForegroundColor Green

Set-Location ..

Write-Host ""
Write-Host "vcpkg setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "To use vcpkg with YAZE:" -ForegroundColor Cyan
Write-Host "1. Use the Windows presets in CMakePresets.json:" -ForegroundColor White
Write-Host "   - windows-debug (Debug build)" -ForegroundColor Gray
Write-Host "   - windows-release (Release build)" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Or set VCPKG_ROOT environment variable:" -ForegroundColor White
Write-Host "   `$env:VCPKG_ROOT = `"$vcpkgRoot`"" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Dependencies will be automatically installed via vcpkg manifest mode" -ForegroundColor White
Write-Host ""

Read-Host "Press Enter to continue"
