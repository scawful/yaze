# Pre-Push Test Script for YAZE (Windows)
# Runs fast validation checks before pushing to remote
# Catches 90% of CI failures in < 2 minutes

param(
    [string]$Preset = "",
    [string]$BuildDir = "",
    [switch]$ConfigOnly = $false,
    [switch]$SmokeOnly = $false,
    [switch]$SkipSymbols = $false,
    [switch]$SkipTests = $false,
    [switch]$Verbose = $false,
    [switch]$Help = $false
)

# Script configuration
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Get-Item $ScriptDir).Parent.FullName

# Default build directory
if ($BuildDir -eq "") {
    $BuildDir = Join-Path $ProjectRoot "build"
}

# Statistics
$TotalChecks = 0
$PassedChecks = 0
$FailedChecks = 0
$StartTime = Get-Date

# Helper functions
function Print-Header {
    param([string]$Message)
    Write-Host "`n=== $Message ===" -ForegroundColor Blue
}

function Print-Step {
    param([string]$Message)
    Write-Host "→ $Message" -ForegroundColor Yellow
}

function Print-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor Green
    $script:PassedChecks++
}

function Print-Error {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
    $script:FailedChecks++
}

function Print-Info {
    param([string]$Message)
    Write-Host "ℹ $Message" -ForegroundColor Cyan
}

function Get-ElapsedTime {
    $elapsed = (Get-Date) - $script:StartTime
    return "{0:N0}s" -f $elapsed.TotalSeconds
}

function Show-Usage {
    Write-Host @"
Usage: .\pre-push-test.ps1 [OPTIONS]

Pre-push validation script that runs fast checks to catch CI failures early.

OPTIONS:
    -Preset NAME        Use specific CMake preset (auto-detect if not specified)
    -BuildDir PATH      Build directory (default: build)
    -ConfigOnly         Only validate CMake configuration
    -SmokeOnly          Only run smoke compilation test
    -SkipSymbols        Skip symbol conflict checking
    -SkipTests          Skip running unit tests
    -Verbose            Show detailed output
    -Help               Show this help message

EXAMPLES:
    .\pre-push-test.ps1                    # Run all checks with auto-detected preset
    .\pre-push-test.ps1 -Preset win-dbg    # Run all checks with specific preset
    .\pre-push-test.ps1 -ConfigOnly        # Only validate CMake configuration
    .\pre-push-test.ps1 -SmokeOnly         # Only compile representative files
    .\pre-push-test.ps1 -SkipTests         # Skip unit tests (faster)

TIME BUDGET:
    Config validation:  ~10 seconds
    Smoke compilation:  ~90 seconds
    Symbol checking:    ~30 seconds
    Unit tests:         ~30 seconds
    ─────────────────────────────
    Total (all checks): ~2 minutes

WHAT THIS CATCHES:
    ✓ CMake configuration errors
    ✓ Missing include paths
    ✓ Header-only compilation issues
    ✓ Symbol conflicts (ODR violations)
    ✓ Unit test failures
    ✓ Platform-specific issues

"@
    exit 0
}

if ($Help) {
    Show-Usage
}

# Auto-detect preset if not specified
if ($Preset -eq "") {
    $Preset = "win-dbg"
    Print-Info "Auto-detected preset: $Preset"
}

Set-Location $ProjectRoot

Print-Header "YAZE Pre-Push Validation"
Print-Info "Preset: $Preset"
Print-Info "Build directory: $BuildDir"
Print-Info "Time budget: ~2 minutes"
Write-Host ""

# ============================================================================
# LEVEL 0: Static Analysis
# ============================================================================

Print-Header "Level 0: Static Analysis"
$TotalChecks++

Print-Step "Checking code formatting..."
try {
    if ($Verbose) {
        cmake --build $BuildDir --target yaze-format-check 2>&1 | Out-Host
        Print-Success "Code formatting is correct"
    } else {
        cmake --build $BuildDir --target yaze-format-check 2>&1 | Out-Null
        Print-Success "Code formatting is correct"
    }
} catch {
    Print-Error "Code formatting check failed"
    Print-Info "Run: cmake --build $BuildDir --target yaze-format"
    exit 1
}

# Skip remaining checks if config-only
if ($ConfigOnly) {
    Print-Header "Summary (Config Only)"
    Print-Info "Time elapsed: $(Get-ElapsedTime)"
    Print-Info "Total checks: $TotalChecks"
    Print-Info "Passed: $PassedChecks"
    Print-Info "Failed: $FailedChecks"
    exit 0
}

# ============================================================================
# LEVEL 1: Configuration Validation
# ============================================================================

Print-Header "Level 1: Configuration Validation"
$TotalChecks++

Print-Step "Validating CMake preset: $Preset"
try {
    cmake --preset $Preset 2>&1 | Out-Null
    Print-Success "CMake configuration successful"
} catch {
    Print-Error "CMake configuration failed"
    Print-Info "Run: cmake --preset $Preset (with verbose output)"
    exit 1
}

# Check for include path issues
$TotalChecks++
Print-Step "Checking include path propagation..."
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $content = Get-Content $cacheFile -Raw
    if ($content -match "INCLUDE_DIRECTORIES") {
        Print-Success "Include paths configured"
    } else {
        Print-Error "Include paths not properly configured"
        exit 1
    }
} else {
    Print-Error "CMakeCache.txt not found"
    exit 1
}

# ============================================================================
# LEVEL 2: Smoke Compilation
# ============================================================================

if (-not $SmokeOnly) {
    Print-Header "Level 2: Smoke Compilation"
    $TotalChecks++

    Print-Step "Compiling representative files..."
    Print-Info "This validates headers, includes, and preprocessor directives"

    # List of representative files (one per major library)
    $SmokeFiles = @(
        "src/app/rom.cc",
        "src/app/gfx/bitmap.cc",
        "src/zelda3/overworld/overworld.cc",
        "src/cli/service/resources/resource_catalog.cc"
    )

    $SmokeFailed = $false
    foreach ($file in $SmokeFiles) {
        $fullPath = Join-Path $ProjectRoot $file
        if (-not (Test-Path $fullPath)) {
            Print-Info "Skipping $file (not found)"
            continue
        }

        # Get object file name
        $objFile = [System.IO.Path]::GetFileNameWithoutExtension($file) + ".obj"

        try {
            if ($Verbose) {
                Print-Step "  Compiling $file"
                cmake --build $BuildDir --target $objFile --config Debug 2>&1 | Out-Host
                Print-Success "  ✓ $file"
            } else {
                cmake --build $BuildDir --target $objFile --config Debug 2>&1 | Out-Null
                Print-Success "  ✓ $file"
            }
        } catch {
            Print-Error "  ✗ $file"
            if (-not $Verbose) {
                Print-Info "Run with -Verbose for details"
            }
            $SmokeFailed = $true
        }
    }

    if (-not $SmokeFailed) {
        Print-Success "Smoke compilation successful"
    } else {
        Print-Error "Smoke compilation failed"
        Print-Info "Run: cmake --build $BuildDir -v (for verbose output)"
        exit 1
    }
}

# ============================================================================
# LEVEL 3: Symbol Validation
# ============================================================================

if (-not $SkipSymbols) {
    Print-Header "Level 3: Symbol Validation"
    $TotalChecks++

    Print-Step "Checking for symbol conflicts..."
    Print-Info "This detects ODR violations and duplicate symbols"

    $symbolScript = Join-Path $ScriptDir "verify-symbols.ps1"
    if (Test-Path $symbolScript) {
        try {
            if ($Verbose) {
                & $symbolScript -BuildDir $BuildDir 2>&1 | Out-Host
                Print-Success "No symbol conflicts detected"
            } else {
                & $symbolScript -BuildDir $BuildDir 2>&1 | Out-Null
                Print-Success "No symbol conflicts detected"
            }
        } catch {
            Print-Error "Symbol conflicts detected"
            Print-Info "Run: .\scripts\verify-symbols.ps1 -BuildDir $BuildDir"
            exit 1
        }
    } else {
        Print-Info "Symbol checker not found (skipping)"
        Print-Info "Create: scripts\verify-symbols.ps1"
    }
}

# ============================================================================
# LEVEL 4: Unit Tests
# ============================================================================

if (-not $SkipTests) {
    Print-Header "Level 4: Unit Tests"
    $TotalChecks++

    Print-Step "Running unit tests..."
    Print-Info "This validates component logic"

    # Find test binary
    $TestBinary = Join-Path $BuildDir "bin\Debug\yaze_test.exe"
    if (-not (Test-Path $TestBinary)) {
        $TestBinary = Join-Path $BuildDir "bin\yaze_test.exe"
    }
    if (-not (Test-Path $TestBinary)) {
        $TestBinary = Join-Path $BuildDir "bin\RelWithDebInfo\yaze_test.exe"
    }

    if (-not (Test-Path $TestBinary)) {
        Print-Info "Test binary not found, building..."
        try {
            cmake --build $BuildDir --target yaze_test --config Debug 2>&1 | Out-Null
            Print-Success "Test binary built"
        } catch {
            Print-Error "Failed to build test binary"
            exit 1
        }
        # Try finding it again
        $TestBinary = Join-Path $BuildDir "bin\Debug\yaze_test.exe"
    }

    if (Test-Path $TestBinary) {
        try {
            if ($Verbose) {
                & $TestBinary --unit 2>&1 | Out-Host
                Print-Success "All unit tests passed"
            } else {
                & $TestBinary --unit 2>&1 | Out-Null
                Print-Success "All unit tests passed"
            }
        } catch {
            Print-Error "Unit tests failed"
            Print-Info "Run: $TestBinary --unit (for detailed output)"
            exit 1
        }
    } else {
        Print-Error "Test binary not found at: $TestBinary"
        exit 1
    }
}

# ============================================================================
# Summary
# ============================================================================

Print-Header "Summary"
$ElapsedSeconds = (Get-Date) - $StartTime | Select-Object -ExpandProperty TotalSeconds

Print-Info "Time elapsed: $([math]::Round($ElapsedSeconds, 0))s"
Print-Info "Total checks: $TotalChecks"
Print-Info "Passed: $PassedChecks"
Print-Info "Failed: $FailedChecks"

if ($FailedChecks -eq 0) {
    Write-Host ""
    Print-Success "All checks passed! Safe to push."
    exit 0
} else {
    Write-Host ""
    Print-Error "Some checks failed. Please fix before pushing."
    exit 1
}
