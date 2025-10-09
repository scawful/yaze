# YAZE Build Environment Verification Script for Windows/Visual Studio CMake Users
# This script verifies the build environment is properly configured and ready to build
# Run this before building to catch common configuration issues early

param(
    [switch]$Verbose,
    [switch]$FixIssues,
    [switch]$CleanCache
)

$ErrorActionPreference = "Continue"
$script:issuesFound = @()
$script:warnings = @()
$script:success = @()

function Write-Status {
    param($Message, $Type = "Info")
    $timestamp = Get-Date -Format "HH:mm:ss"
    switch ($Type) {
        "Success" { Write-Host "[$timestamp] ✓ " -ForegroundColor Green -NoNewline; Write-Host $Message }
        "Error"   { Write-Host "[$timestamp] ✗ " -ForegroundColor Red -NoNewline; Write-Host $Message }
        "Warning" { Write-Host "[$timestamp] ⚠ " -ForegroundColor Yellow -NoNewline; Write-Host $Message }
        "Info"    { Write-Host "[$timestamp] ℹ " -ForegroundColor Cyan -NoNewline; Write-Host $Message }
        "Step"    { Write-Host "`n[$timestamp] ▶ " -ForegroundColor Blue -NoNewline; Write-Host $Message -ForegroundColor White }
    }
}

function Test-Command {
    param($Command)
    try {
        if (Get-Command $Command -ErrorAction SilentlyContinue) {
            return $true
        }
    } catch {
        return $false
    }
    return $false
}

function Get-CMakeVersion {
    try {
        $output = & cmake --version 2>&1 | Select-Object -First 1
        # Handle various CMake version output formats
        if ($output -match "cmake version ([0-9]+\.[0-9]+\.[0-9]+)") {
            return $matches[1]
        }
        # Try alternative format (some versions print differently)
        if ($output -match "([0-9]+\.[0-9]+\.[0-9]+)") {
            return $matches[1]
        }
    } catch {
        return $null
    }
    return $null
}

function Test-GitSubmodules {
    $submodules = @(
        "src/lib/SDL",
        "src/lib/abseil-cpp",
        "src/lib/asar",
        "src/lib/imgui",
        "third_party/json",
        "third_party/httplib"
    )
    
    $allPresent = $true
    foreach ($submodule in $submodules) {
        $path = Join-Path $PSScriptRoot ".." $submodule
        if (-not (Test-Path $path)) {
            Write-Status "Submodule missing: $submodule" "Error"
            $script:issuesFound += "Missing submodule: $submodule"
            $allPresent = $false
        } elseif ((Get-ChildItem $path -Force | Measure-Object).Count -eq 0) {
            Write-Status "Submodule empty: $submodule" "Error"
            $script:issuesFound += "Empty submodule: $submodule"
            $allPresent = $false
        } elseif ($Verbose) {
            Write-Status "Submodule found: $submodule" "Success"
        }
    }
    return $allPresent
}

function Test-GitConfig {
    Write-Status "Checking Git for Windows configuration..." "Step"
    $gitOk = $true

    # Check for core.autocrlf
    try {
        $autocrlf = git config --get core.autocrlf
        if ($autocrlf -ne "false") {
            Write-Status "Git 'core.autocrlf' is '$autocrlf'. Recommended setting is 'false' for cross-platform projects." "Warning"
            $script:warnings += "Git 'core.autocrlf' is not 'false'. This can cause line ending issues."
            if ($FixIssues) {
                Write-Status "Attempting to set 'core.autocrlf' to 'false' globally..." "Info"
                git config --global core.autocrlf false
                if ($LASTEXITCODE -eq 0) {
                    Write-Status "Successfully set 'core.autocrlf' to 'false'." "Success"
                } else {
                    Write-Status "Failed to set 'core.autocrlf'." "Error"
                    $script:issuesFound += "Failed to automatically set 'core.autocrlf'."
                }
            }
            $gitOk = $false
        } else {
            Write-Status "'core.autocrlf' is correctly set to 'false'." "Success"
        }
    } catch {
        Write-Status "Could not check Git 'core.autocrlf' setting." "Warning"
    }

    # Check for core.longpaths
    try {
        $longpaths = git config --get core.longpaths
        if ($longpaths -ne "true") {
            Write-Status "Git 'core.longpaths' is not 'true'. This can cause build failures with deep file paths." "Warning"
            $script:warnings += "Git 'core.longpaths' is not 'true'. This is highly recommended on Windows."
            if ($FixIssues) {
                Write-Status "Attempting to set 'core.longpaths' to 'true' globally..." "Info"
                git config --global core.longpaths true
                if ($LASTEXITCODE -eq 0) {
                    Write-Status "Successfully set 'core.longpaths' to 'true'." "Success"
                } else {
                    Write-Status "Failed to set 'core.longpaths'." "Error"
                    $script:issuesFound += "Failed to automatically set 'core.longpaths'."
                }
            }
            $gitOk = $false
        } else {
            Write-Status "'core.longpaths' is correctly set to 'true'." "Success"
        }
    } catch {
        Write-Status "Could not check Git 'core.longpaths' setting." "Warning"
    }

    return $gitOk
}

function Test-Vcpkg {
    $vcpkgPath = Join-Path $PSScriptRoot ".." "vcpkg"
    $vcpkgExe = Join-Path $vcpkgPath "vcpkg.exe"
    
    if (Test-Path $vcpkgPath) {
        if (Test-Path $vcpkgExe) {
            Write-Status "vcpkg found and bootstrapped" "Success"
            $script:success += "vcpkg available for dependency management"
            
            try {
                $vcpkgVersion = & $vcpkgExe version 2>&1 | Select-Object -First 1
                Write-Status "vcpkg version: $vcpkgVersion" "Info"
            } catch {
                Write-Status "vcpkg executable found but version check failed" "Warning"
            }
            return $true
        } else {
            Write-Status "vcpkg directory exists but not bootstrapped" "Warning"
            $script:warnings += "vcpkg not bootstrapped - run: vcpkg\bootstrap-vcpkg.bat"
            return $false
        }
    } else {
        Write-Status "vcpkg not found (optional, but recommended for gRPC)" "Info"
        $script:warnings += "vcpkg not installed. For faster builds with gRPC, consider running scripts\setup-vcpkg-windows.ps1"
        return $false
    }
}

function Test-CMakeCache {
    $buildDirs = @("build", "build-windows", "build-test", "build-ai", "out/build")
    $cacheIssues = $false
    
    foreach ($dir in $buildDirs) {
        $cachePath = Join-Path $PSScriptRoot ".." $dir "CMakeCache.txt"
        if (Test-Path $cachePath) {
            $cacheAge = (Get-Date) - (Get-Item $cachePath).LastWriteTime
            if ($cacheAge.TotalDays -gt 7) {
                Write-Status "CMake cache in '$dir' is $([math]::Round($cacheAge.TotalDays)) days old" "Warning"
                $script:warnings += "Old CMake cache in '$dir'"
                $cacheIssues = $true
            } elseif ($Verbose) {
                Write-Status "CMake cache in '$dir' is recent" "Success"
            }
        }
    }
    return -not $cacheIssues
}

function Clean-CMakeCache {
    param([switch]$Force)
    
    Write-Status "Cleaning CMake cache and build directories..." "Step"
    
    $buildDirs = @("build", "build_test", "build-ai", "build_rooms", "out")
    $cleaned = $false
    
    foreach ($dir in $buildDirs) {
        $fullPath = Join-Path $PSScriptRoot ".." $dir
        if (Test-Path $fullPath) {
            Write-Status "Removing '$fullPath'..." "Info"
            try {
                Remove-Item -Recurse -Force $fullPath -ErrorAction Stop
                $cleaned = $true
                Write-Status "  ✓ Removed '$dir'" "Success"
            } catch {
                Write-Status "  ✗ Failed to remove '$dir`: $_" "Error"
                $script:warnings += "Could not fully clean '$dir' (some files may be locked)"
            }
        }
    }
    
    # Clean CMake generated files in root
    $cmakeFiles = @(
        "CMakeCache.txt",
        "cmake_install.cmake",
        "compile_commands.json"
    )
    
    foreach ($file in $cmakeFiles) {
        $fullPath = Join-Path $PSScriptRoot ".." $file
        if (Test-Path $fullPath) {
            Write-Status "Removing root '$file'..." "Info"
            Remove-Item -Force $fullPath -ErrorAction SilentlyContinue
        }
    }
    
    if ($cleaned) {
        Write-Status "CMake cache cleaned successfully" "Success"
        $script:success += "Build directories cleaned. A fresh build is recommended."
    } else {
        Write-Status "No build directories found to clean" "Info"
    }
}

function Sync-GitSubmodules {
    Write-Status "Syncing git submodules..." "Step"
    
    Push-Location (Join-Path $PSScriptRoot "..")
    try {
        Write-Status "Running: git submodule sync --recursive" "Info"
        $syncOutput = git submodule sync --recursive 2>&1
        
        Write-Status "Running: git submodule update --init --recursive" "Info"
        $updateOutput = git submodule update --init --recursive 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Status "Submodules synced successfully" "Success"
            return $true
        } else {
            Write-Status "Git submodule commands completed with warnings" "Warning"
            if ($Verbose) {
                Write-Host $updateOutput -ForegroundColor Gray
            }
            return $true  # Still return true as submodules may be partially synced
        }
    } catch {
        Write-Status "Failed to sync submodules: $_" "Error"
        return $false
    } finally {
        Pop-Location
    }
}

# ============================================================================
# Main Verification Process
# ============================================================================

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  YAZE Build Environment Verification for Visual Studio CMake  ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$startTime = Get-Date

# Step 0: Handle Cache Cleaning
if ($CleanCache) {
    Clean-CMakeCache
    Write-Status "Cache cleaning complete. Please re-run without -CleanCache to verify." "Info"
    exit 0
}

# Step 1: Check CMake
Write-Status "Checking CMake installation..." "Step"
if (Test-Command "cmake") {
    $cmakeVersion = Get-CMakeVersion
    if ($cmakeVersion) {
        Write-Status "CMake found: version $cmakeVersion" "Success"
        
        try {
            $versionParts = $cmakeVersion.Split('.')
            $major = [int]$versionParts[0]
            $minor = [int]$versionParts[1]
            
            if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 16)) {
                Write-Status "CMake version too old (need 3.16+)" "Error"
                $script:issuesFound += "CMake version $cmakeVersion is below minimum 3.16"
            }
        } catch {
            Write-Status "Could not parse CMake version: $cmakeVersion" "Warning"
            $script:warnings += "Unable to verify CMake version requirement (need 3.16+)"
        }
    } else {
        Write-Status "CMake found but version could not be determined" "Warning"
        $script:warnings += "CMake version could not be parsed - ensure version 3.16+ is installed"
    }
} else {
    Write-Status "CMake not found in PATH" "Error"
    $script:issuesFound += "CMake not installed or not in PATH"
}

# Step 2: Check Git
Write-Status "Checking Git installation..." "Step"
if (Test-Command "git") {
    $gitVersion = (& git --version) -replace "git version ", ""
    Write-Status "Git found: version $gitVersion" "Success"
    Test-GitConfig
} else {
    Write-Status "Git not found in PATH" "Error"
    $script:issuesFound += "Git not installed or not in PATH"
}

# Step 3: Check Visual Studio
Write-Status "Checking Visual Studio installation..." "Step"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstances = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -format json | ConvertFrom-Json
    if ($vsInstances) {
        $vsInstance = if ($vsInstances -is [array]) { $vsInstances[0] } else { $vsInstances }
        $vsVersion = $vsInstance.installationVersion
        $vsPath = $vsInstance.installationPath
        Write-Status "Visual Studio with C++ Desktop workload found: version $vsVersion" "Success"
        Write-Status "  Path: $vsPath" "Info"
        $script:success += "Visual Studio C++ workload detected (version $vsVersion)"
    } else {
        Write-Status "Visual Studio found, but 'Desktop development with C++' workload is missing." "Error"
        $script:issuesFound += "Visual Studio 'Desktop development with C++' workload not installed."
    }
} else {
    Write-Status "Visual Studio not found (vswhere.exe missing)" "Error"
    $script:issuesFound += "Visual Studio installation not detected."
}

# Step 4: Check vcpkg
Test-Vcpkg | Out-Null

# Step 5: Check Git Submodules
Write-Status "Checking git submodules..." "Step"
$submodulesOk = Test-GitSubmodules
if ($submodulesOk) {
    Write-Status "All required submodules appear to be present." "Success"
} else {
    if ($FixIssues) {
        Sync-GitSubmodules
        Write-Status "Re-checking submodules after sync..." "Info"
        $submodulesOk = Test-GitSubmodules
        if ($submodulesOk) {
            $script:success += "Submodules were successfully synced."
        } else {
            $script:issuesFound += "Submodule sync completed but some issues remain."
        }
    }
}

# Step 6: Check CMake Cache
Write-Status "Checking CMake cache..." "Step"
if (Test-CMakeCache) {
    Write-Status "CMake cache appears up to date." "Success"
} else {
    if ($FixIssues) {
        Write-Host "`nCMake cache is older than 7 days. Clean it?" -ForegroundColor Yellow
        Write-Host "This will remove all `build*` and `out` directories." -ForegroundColor Gray
        $response = Read-Host "Continue? (Y/n)"
        if ($response -eq "" -or $response -match "^[Yy]") {
            Clean-CMakeCache
        } else {
            Write-Status "Skipping cache clean." "Info"
        }
    }
}

# ============================================================================
# Summary Report
# ============================================================================

$duration = (Get-Date) - $startTime

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    Verification Summary                       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "Duration: $([math]::Round($duration.TotalSeconds, 2)) seconds`n"

if ($script:success.Count -gt 0) {
    Write-Host "✓ Successes ($($script:success.Count)):" -ForegroundColor Green
    foreach ($item in $script:success) {
        Write-Host "  • $item" -ForegroundColor Green
    }
    Write-Host ""
}

if ($script:warnings.Count -gt 0) {
    Write-Host "⚠ Warnings ($($script:warnings.Count)):" -ForegroundColor Yellow
    foreach ($item in $script:warnings) {
        Write-Host "  • $item" -ForegroundColor Yellow
    }
    Write-Host ""
}

if ($script:issuesFound.Count -gt 0) {
    Write-Host "✗ Issues Found ($($script:issuesFound.Count)):" -ForegroundColor Red
    foreach ($item in $script:issuesFound) {
        Write-Host "  • $item" -ForegroundColor Red
    }
    Write-Host ""
    
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "║                  Troubleshooting Steps                      ║" -ForegroundColor Yellow
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Yellow
    Write-Host "Some issues were found. Here are some common solutions:`n"
    
    if ($script:issuesFound -join ' ' -match 'submodule') {
        Write-Host " • Submodule problems:" -ForegroundColor White
        Write-Host "   Run 'git submodule update --init --recursive' in your terminal." -ForegroundColor Gray
        Write-Host "   Or, run this script again with the '-FixIssues' flag.`n"
    }
    if ($script:warnings -join ' ' -match 'cache') {
        Write-Host " • Stale build files:" -ForegroundColor White
        Write-Host "   Your build directory is old and may cause strange errors." -ForegroundColor Gray
        Write-Host "   Run this script with the '-CleanCache' flag to delete all build files and start fresh.`n"
    }
    if ($script:issuesFound -join ' ' -match 'Visual Studio') {
        Write-Host " • Visual Studio issues:" -ForegroundColor White
        Write-Host "   Open the 'Visual Studio Installer' and ensure the" -ForegroundColor Gray
        Write-Host "   'Desktop development with C++' workload is installed.`n"
    }
    if ($script:warnings -join ' ' -match 'Git') {
        Write-Host " • Git configuration:" -ForegroundColor White
        Write-Host "   Your Git settings might cause issues. For best results on Windows, run:" -ForegroundColor Gray
        Write-Host "   git config --global core.autocrlf false" -ForegroundColor Gray
        Write-Host "   git config --global core.longpaths true" -ForegroundColor Gray
        Write-Host "   Or, run this script again with the '-FixIssues' flag.`n"
    }
    
    Write-Host "If problems persist, check the build instructions in 'docs/B1-build-instructions.md'`n" -ForegroundColor Cyan
    
    exit 1
} else {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║          ✓ Build Environment Ready for Development!           ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green
    
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "  Visual Studio (Recommended):" -ForegroundColor White
    Write-Host "    1. Open Visual Studio 2022." -ForegroundColor Gray
    Write-Host "    2. Select 'File -> Open -> Folder...' and choose the 'yaze' directory." -ForegroundColor Gray
    Write-Host "    3. Select a Windows preset (e.g., 'win-dbg') from the dropdown." -ForegroundColor Gray
    Write-Host "    4. Press F5 to build and debug.`n" -ForegroundColor Gray
    
    Write-Host "  Command Line:" -ForegroundColor White
    Write-Host "    cmake --preset win-dbg" -ForegroundColor Gray
    Write-Host "    cmake --build --preset win-dbg`n" -ForegroundColor Gray
    
    exit 0
}
