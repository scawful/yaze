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
            $script:issuesFound += "Empty submodule: $submodule (run git submodule update)"
            $allPresent = $false
        } elseif ($Verbose) {
            Write-Status "Submodule found: $submodule" "Success"
        }
    }
    return $allPresent
}

function Test-CMakeCache {
    $buildDirs = @("build", "build_test", "build-grpc-test", "out/build")
    $cacheIssues = $false
    
    foreach ($dir in $buildDirs) {
        $cachePath = Join-Path $PSScriptRoot ".." $dir "CMakeCache.txt"
        if (Test-Path $cachePath) {
            $cacheAge = (Get-Date) - (Get-Item $cachePath).LastWriteTime
            if ($cacheAge.TotalDays -gt 7) {
                Write-Status "CMake cache in '$dir' is $([math]::Round($cacheAge.TotalDays)) days old" "Warning"
                $script:warnings += "Old CMake cache in $dir (consider cleaning)"
                $cacheIssues = $true
            } elseif ($Verbose) {
                Write-Status "CMake cache in '$dir' is recent" "Success"
            }
        }
    }
    return -not $cacheIssues
}

function Test-DependencyCompatibility {
    Write-Status "Testing dependency configuration..." "Step"
    
    # Check for potential conflicts
    $conflicts = @()
    
    # Check if grpc is enabled but might conflict with system packages
    $grpcPath = Join-Path $PSScriptRoot ".." "cmake" "grpc.cmake"
    if (Test-Path $grpcPath) {
        $grpcContent = Get-Content $grpcPath -Raw
        if ($grpcContent -match "CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE") {
            Write-Status "gRPC isolation configured correctly" "Success"
        } else {
            Write-Status "gRPC may conflict with system protobuf" "Warning"
            $script:warnings += "gRPC not properly isolated from system packages"
        }
    }
    
    # Check httplib configuration
    $httplibPath = Join-Path $PSScriptRoot ".." "third_party" "httplib" "CMakeLists.txt"
    if (Test-Path $httplibPath) {
        Write-Status "httplib found in third_party" "Success"
        $script:success += "httplib header-only library available"
    }
    
    # Check json library
    $jsonPath = Join-Path $PSScriptRoot ".." "third_party" "json" "include"
    if (Test-Path $jsonPath) {
        Write-Status "nlohmann/json found in third_party" "Success"
        $script:success += "nlohmann/json header-only library available"
    }
    
    return $conflicts.Count -eq 0
}

function Clean-CMakeCache {
    Write-Status "Cleaning CMake cache..." "Step"
    
    $buildDirs = @("build", "build_test", "build-grpc-test")
    foreach ($dir in $buildDirs) {
        $fullPath = Join-Path $PSScriptRoot ".." $dir
        if (Test-Path $fullPath) {
            Write-Status "Removing $dir..." "Info"
            Remove-Item -Recurse -Force $fullPath -ErrorAction SilentlyContinue
        }
    }
    
    # Also clean Visual Studio CMake cache
    $vsCache = Join-Path $PSScriptRoot ".." "out"
    if (Test-Path $vsCache) {
        Write-Status "Removing Visual Studio CMake cache (out/)..." "Info"
        Remove-Item -Recurse -Force $vsCache -ErrorAction SilentlyContinue
    }
    
    Write-Status "CMake cache cleaned" "Success"
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

function Test-CMakeConfiguration {
    Write-Status "Testing CMake configuration..." "Step"
    
    $testBuildDir = Join-Path $PSScriptRoot ".." "build_test_config"
    
    try {
        # Test basic CMake configuration
        Write-Status "Configuring CMake (this may take a moment)..." "Info"
        $output = & cmake -B $testBuildDir -S (Join-Path $PSScriptRoot "..") `
            -DCMAKE_BUILD_TYPE=Debug `
            -DYAZE_MINIMAL_BUILD=ON `
            -DYAZE_BUILD_TESTS=OFF `
            2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Status "CMake configuration successful" "Success"
            $script:success += "CMake configuration test passed"
            
            # Cleanup test build
            if (Test-Path $testBuildDir) {
                Remove-Item -Recurse -Force $testBuildDir -ErrorAction SilentlyContinue
            }
            return $true
        } else {
            Write-Status "CMake configuration failed (exit code: $LASTEXITCODE)" "Error"
            if ($Verbose) {
                Write-Status "CMake output:" "Info"
                $output | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
            }
            $script:issuesFound += "CMake configuration test failed (see output with -Verbose)"
            return $false
        }
    } catch {
        Write-Status "CMake configuration test failed: $_" "Error"
        $script:issuesFound += "CMake test exception: $_"
        return $false
    }
}

# ============================================================================
# Main Verification Process
# ============================================================================

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  YAZE Build Environment Verification for Visual Studio CMake  ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$startTime = Get-Date

# Step 1: Check CMake
Write-Status "Checking CMake installation..." "Step"
if (Test-Command "cmake") {
    $cmakeVersion = Get-CMakeVersion
    if ($cmakeVersion) {
        Write-Status "CMake found: version $cmakeVersion" "Success"
        
        # Parse version components
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
} else {
    Write-Status "Git not found in PATH" "Error"
    $script:issuesFound += "Git not installed or not in PATH"
}

# Step 3: Check Visual Studio
Write-Status "Checking Visual Studio installation..." "Step"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstances = & $vswhere -latest -format json | ConvertFrom-Json
    if ($vsInstances) {
        # Handle both single instance and array returns
        $vsInstance = if ($vsInstances -is [array]) { $vsInstances[0] } else { $vsInstances }
        $vsVersion = $vsInstance.installationVersion
        $vsPath = $vsInstance.installationPath
        Write-Status "Visual Studio found: version $vsVersion" "Success"
        Write-Status "  Path: $vsPath" "Info"
        $script:success += "Visual Studio detected (version $vsVersion)"
    }
} else {
    Write-Status "Visual Studio not found (vswhere.exe missing)" "Warning"
    $script:warnings += "Could not detect Visual Studio installation"
}

# Step 4: Check Git Submodules
Write-Status "Checking git submodules..." "Step"
$submodulesOk = Test-GitSubmodules
if ($submodulesOk) {
    Write-Status "All required submodules present" "Success"
} else {
    Write-Status "Some submodules are missing or empty" "Error"
    if ($FixIssues) {
        Sync-GitSubmodules
        # Re-check after sync
        $submodulesOk = Test-GitSubmodules
        if (-not $submodulesOk) {
            Write-Status "Submodule sync completed but some issues remain" "Warning"
        }
    } else {
        # Auto-fix without confirmation
        Write-Status "Automatically syncing submodules..." "Info"
        if (Sync-GitSubmodules) {
            Write-Status "Submodules synced successfully" "Success"
            # Re-check after sync
            $submodulesOk = Test-GitSubmodules
        } else {
            Write-Status "Failed to sync submodules automatically" "Error"
            Write-Status "Run with -FixIssues to try again, or manually run: git submodule update --init --recursive" "Info"
        }
    }
}

# Step 5: Check CMake Cache
Write-Status "Checking CMake cache..." "Step"
$cacheOk = Test-CMakeCache
if ($cacheOk) {
    Write-Status "CMake cache is up to date" "Success"
} else {
    if ($CleanCache) {
        Clean-CMakeCache
    } elseif ($FixIssues) {
        # Ask for confirmation before cleaning cache
        Write-Host "`nCMake cache is older than 7 days. Clean it?" -ForegroundColor Yellow
        Write-Host "This will remove build/, build_test/, build-grpc-test/, and out/ directories." -ForegroundColor Gray
        $response = Read-Host "Continue? (Y/n)"
        if ($response -eq "" -or $response -match "^[Yy]") {
            Clean-CMakeCache
        } else {
            Write-Status "Skipping cache clean" "Info"
        }
    } else {
        Write-Status "CMake cache is older than 7 days (consider cleaning)" "Warning"
        Write-Status "Run with -CleanCache to remove old cache files" "Info"
    }
}

# Step 6: Check Dependencies
Test-DependencyCompatibility

# Step 7: Test CMake Configuration (if requested)
if ($Verbose -or $FixIssues) {
    Test-CMakeConfiguration
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
    
    Write-Host "Recommended Actions:" -ForegroundColor Yellow
    Write-Host "  1. Run: .\scripts\verify-build-environment.ps1 -FixIssues" -ForegroundColor Cyan
    Write-Host "  2. Install missing dependencies" -ForegroundColor Cyan
    Write-Host "  3. Check build instructions: docs/02-build-instructions.md" -ForegroundColor Cyan
    Write-Host ""
    
    exit 1
} else {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║          ✓ Build Environment Ready for Development!           ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green
    
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "  Visual Studio CMake Workflow:" -ForegroundColor White
    Write-Host "    1. Open Visual Studio 2022" -ForegroundColor Gray
    Write-Host "    2. File → Open → Folder" -ForegroundColor Gray
    Write-Host "    3. Select the yaze directory" -ForegroundColor Gray
    Write-Host "    4. Visual Studio will detect CMakeLists.txt" -ForegroundColor Gray
    Write-Host "    5. Select Debug/Release from toolbar" -ForegroundColor Gray
    Write-Host "    6. Press F5 to build and run" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Command Line (Alternative):" -ForegroundColor White
    Write-Host "    cmake -B build -DCMAKE_BUILD_TYPE=Debug" -ForegroundColor Gray
    Write-Host "    cmake --build build --config Debug" -ForegroundColor Gray
    Write-Host ""
    
    exit 0
}
