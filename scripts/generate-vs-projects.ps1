# Configure Visual Studio project files for YAZE
# This script configures CMake build system to work with existing Visual Studio project files

param(
    [string]$Configuration = "Debug",
    [string]$Architecture = "x64",
    [switch]$Clean = $false,
    [switch]$UseVcpkg = $false,
    [switch]$Help = $false
)

# Show help if requested
if ($Help) {
    Write-Host "Usage: .\generate-vs-projects.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Configuration <config>  Build configuration (Debug, Release, RelWithDebInfo, MinSizeRel)"
    Write-Host "  -Architecture <arch>     Target architecture (x64, x86, ARM64)"
    Write-Host "  -Clean                   Clean build directory before configuring"
    Write-Host "  -UseVcpkg               Use vcpkg for dependency management"
    Write-Host "  -Help                   Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\generate-vs-projects.ps1                    # Default: Debug x64"
    Write-Host "  .\generate-vs-projects.ps1 -Configuration Release -Architecture x86"
    Write-Host "  .\generate-vs-projects.ps1 -Clean -UseVcpkg"
    Write-Host ""
    Write-Host "Note: This script configures CMake to work with existing YAZE.sln and YAZE.vcxproj files"
    exit 0
}

# Validate architecture parameter
$ValidArchitectures = @("x64", "x86", "ARM64")
if ($Architecture -notin $ValidArchitectures) {
    Write-Host "Invalid architecture: $Architecture" -ForegroundColor Red
    Write-Host "Valid architectures: $($ValidArchitectures -join ', ')" -ForegroundColor Yellow
    exit 1
}

Write-Host "Generating Visual Studio project files for YAZE..." -ForegroundColor Green

# Check if we're on Windows
if ($env:OS -ne "Windows_NT") {
    Write-Host "This script is designed for Windows. Use CMake presets on other platforms." -ForegroundColor Yellow
    Write-Host "Available presets:" -ForegroundColor Cyan
    Write-Host "  - windows-debug" -ForegroundColor Gray
    Write-Host "  - windows-release" -ForegroundColor Gray
    Write-Host "  - windows-dev" -ForegroundColor Gray
    exit 1
}

# Check if CMake is available
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakePath) {
    # Try common CMake installation paths
    $commonPaths = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\CMake\bin\cmake.exe",
        "C:\cmake\bin\cmake.exe"
    )
    
    foreach ($path in $commonPaths) {
        if (Test-Path $path) {
            Write-Host "Found CMake at: $path" -ForegroundColor Green
            $env:Path += ";$(Split-Path $path)"
            $cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
            if ($cmakePath) {
                break
            }
        }
    }
}

if (-not $cmakePath) {
    Write-Host "CMake not found in PATH. Attempting to install..." -ForegroundColor Yellow
    
    # Try to install CMake via Chocolatey
    if (Get-Command choco -ErrorAction SilentlyContinue) {
        Write-Host "Installing CMake via Chocolatey..." -ForegroundColor Yellow
        choco install -y cmake
        if ($LASTEXITCODE -eq 0) {
            Write-Host "CMake installed successfully" -ForegroundColor Green
            # Refresh PATH
            $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        } else {
            Write-Host "Failed to install CMake via Chocolatey" -ForegroundColor Red
        }
    } else {
        Write-Host "Chocolatey not found. Please install CMake manually:" -ForegroundColor Red
        Write-Host "1. Download from: https://cmake.org/download/" -ForegroundColor Yellow
        Write-Host "2. Or install Chocolatey first: https://chocolatey.org/install" -ForegroundColor Yellow
        Write-Host "3. Then run: choco install cmake" -ForegroundColor Yellow
        exit 1
    }
    
    # Check again after installation
    $cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakePath) {
        Write-Host "CMake still not found after installation. Please restart your terminal or add CMake to PATH manually." -ForegroundColor Red
        exit 1
    }
}

Write-Host "CMake found: $($cmakePath.Source)" -ForegroundColor Green

# Check if Visual Studio is available
$vsWhere = Get-Command "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -ErrorAction SilentlyContinue
if (-not $vsWhere) {
    $vsWhere = Get-Command vswhere -ErrorAction SilentlyContinue
}

if ($vsWhere) {
    $vsInstallPath = & $vsWhere.Source -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInstallPath) {
        Write-Host "Visual Studio found at: $vsInstallPath" -ForegroundColor Green
    } else {
        Write-Host "Visual Studio 2022 not found. Please install Visual Studio 2022 with C++ workload." -ForegroundColor Yellow
    }
} else {
    Write-Host "vswhere not found. Assuming Visual Studio is available." -ForegroundColor Yellow
}

# Set up paths
$SourceDir = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $SourceDir "build-vs"

Write-Host "Source directory: $SourceDir" -ForegroundColor Cyan
Write-Host "Build directory: $BuildDir" -ForegroundColor Cyan

# Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Check if vcpkg is available
$VcpkgPath = Join-Path $SourceDir "vcpkg\scripts\buildsystems\vcpkg.cmake"
$UseVcpkg = Test-Path $VcpkgPath

if ($UseVcpkg) {
    Write-Host "Using vcpkg toolchain: $VcpkgPath" -ForegroundColor Green
} else {
    Write-Host "vcpkg not found, using system libraries" -ForegroundColor Yellow
}

# Determine generator and architecture
$Generator = "Visual Studio 17 2022"
$ArchFlag = if ($Architecture -eq "x64") { "-A x64" } else { "-A Win32" }

# Build CMake command
$CmakeArgs = @(
    "-B", $BuildDir,
    "-G", "`"$Generator`"",
    $ArchFlag,
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DCMAKE_POLICY_VERSION_MAXIMUM=3.28",
    "-DCMAKE_WARN_DEPRECATED=OFF",
    "-DABSL_PROPAGATE_CXX_STD=ON",
    "-DTHREADS_PREFER_PTHREAD_FLAG=OFF",
    "-DYAZE_BUILD_TESTS=ON",
    "-DYAZE_BUILD_APP=ON",
    "-DYAZE_BUILD_LIB=ON",
    "-DYAZE_BUILD_EMU=ON",
    "-DYAZE_BUILD_Z3ED=ON",
    "-DYAZE_ENABLE_ROM_TESTS=OFF",
    "-DYAZE_ENABLE_EXPERIMENTAL_TESTS=ON",
    "-DYAZE_ENABLE_UI_TESTS=ON",
    "-DYAZE_INSTALL_LIB=OFF"
)

if ($UseVcpkg) {
    $CmakeArgs += @(
        "-DCMAKE_TOOLCHAIN_FILE=`"$VcpkgPath`"",
        "-DVCPKG_TARGET_TRIPLET=$Architecture-windows",
        "-DVCPKG_MANIFEST_MODE=ON"
    )
}

# Configure CMake to generate build files (but don't overwrite existing project files)
Write-Host "Configuring CMake for build system..." -ForegroundColor Yellow
Write-Host "Command: cmake $($CmakeArgs -join ' ')" -ForegroundColor Gray

& cmake @CmakeArgs $SourceDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Check if the existing solution file is present and valid
$ExistingSolutionFile = Join-Path $SourceDir "YAZE.sln"
if (Test-Path $ExistingSolutionFile) {
    Write-Host "✅ Using existing Visual Studio solution: $ExistingSolutionFile" -ForegroundColor Green
    
    # Verify the solution file is properly structured
    $SolutionContent = Get-Content $ExistingSolutionFile -Raw
    if ($SolutionContent -match "YAZE\.vcxproj") {
        Write-Host "✅ Solution file references YAZE.vcxproj correctly" -ForegroundColor Green
    } else {
        Write-Host "⚠️  Warning: Solution file may not reference YAZE.vcxproj properly" -ForegroundColor Yellow
    }
    
    # Open solution in Visual Studio if available
    if (Get-Command "devenv" -ErrorAction SilentlyContinue) {
        Write-Host "Opening solution in Visual Studio..." -ForegroundColor Yellow
        & devenv $ExistingSolutionFile
    } elseif (Get-Command "code" -ErrorAction SilentlyContinue) {
        Write-Host "Opening solution in VS Code..." -ForegroundColor Yellow
        & code $ExistingSolutionFile
    } else {
        Write-Host "Visual Studio solution ready: $ExistingSolutionFile" -ForegroundColor Cyan
    }
} else {
    Write-Host "❌ Existing solution file not found: $ExistingSolutionFile" -ForegroundColor Red
    Write-Host "Please ensure YAZE.sln exists in the project root" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "🎉 Visual Studio project configuration complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Open YAZE.sln in Visual Studio" -ForegroundColor White
Write-Host "2. Select configuration: $Configuration" -ForegroundColor White
Write-Host "3. Select platform: $Architecture" -ForegroundColor White
Write-Host "4. Build the solution (Ctrl+Shift+B)" -ForegroundColor White
Write-Host ""
Write-Host "Available configurations:" -ForegroundColor Cyan
Write-Host "  - Debug (with debugging symbols)" -ForegroundColor Gray
Write-Host "  - Release (optimized)" -ForegroundColor Gray
Write-Host "  - RelWithDebInfo (optimized with debug info)" -ForegroundColor Gray
Write-Host "  - MinSizeRel (minimum size)" -ForegroundColor Gray
Write-Host ""
Write-Host "Available architectures:" -ForegroundColor Cyan
Write-Host "  - x64 (64-bit Intel/AMD)" -ForegroundColor Gray
Write-Host "  - x86 (32-bit Intel/AMD)" -ForegroundColor Gray
Write-Host "  - ARM64 (64-bit ARM)" -ForegroundColor Gray
