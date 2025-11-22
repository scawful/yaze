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
        "ext/SDL",
        "src/lib/abseil-cpp",
        "ext/asar",
        "ext/imgui",
        "ext/json",
        "ext/httplib",
        "ext/imgui_test_engine",
        "ext/nativefiledialog-extended"
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

function Test-VcpkgCache {
    $vcpkgPath = Join-Path $PSScriptRoot ".." "vcpkg"
    $installedDir = Join-Path $vcpkgPath "installed"

    if (-not (Test-Path $installedDir)) {
        return
    }

    $triplets = @("x64-windows", "arm64-windows")
    $hasPackages = $false

    foreach ($triplet in $triplets) {
        $tripletDir = Join-Path $installedDir $triplet
        if (Test-Path $tripletDir) {
            $count = (Get-ChildItem $tripletDir -Force | Measure-Object).Count
            if ($count -gt 0) {
                $hasPackages = $true
                Write-Status "vcpkg cache populated for $triplet" "Success"
            }
        }
    }

    if (-not $hasPackages) {
        Write-Status "vcpkg/installed is empty. Run scripts\\setup-vcpkg-windows.ps1 to prefetch dependencies." "Warning"
        $script:warnings += "vcpkg cache empty - builds may spend extra time compiling dependencies."
    } else {
        $script:success += "vcpkg cache ready for Windows presets"
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
                Write-Status "  ✗ Failed to remove '$dir': $_" "Error"
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

function Test-Ninja {
    Write-Status "Checking Ninja build system..." "Step"
    if (Test-Command "ninja") {
        try {
            $ninjaVersion = & ninja --version 2>&1
            Write-Status "Ninja found: version $ninjaVersion" "Success"
            $script:success += "Ninja build system available (required for win-dbg presets)"
            return $true
        } catch {
            Write-Status "Ninja command exists but version check failed" "Warning"
            return $true
        }
    } else {
        Write-Status "Ninja not found in PATH" "Warning"
        $script:warnings += "Ninja not installed. Required for win-dbg, win-rel, win-ai presets. Use win-vs-* presets instead or install Ninja."
        return $false
    }
}

function Test-ClangCL {
    Write-Status "Checking clang-cl compiler..." "Step"
    if (Test-Command "clang-cl") {
        try {
            $clangVersion = & clang-cl --version 2>&1 | Select-Object -First 1
            Write-Status "clang-cl found: $clangVersion" "Success"
            $script:success += "clang-cl available (recommended for win-* presets)"
            return $true
        } catch {
            Write-Status "clang-cl command exists but version check failed" "Warning"
            return $true
        }
    } else {
        Write-Status "clang-cl not found (LLVM toolset for MSVC missing?)" "Warning"
        $script:warnings += "Install the \"LLVM tools for Visual Studio\" component or enable clang-cl via Visual Studio Installer."
        return $false
    }
}

function Test-NASM {
    Write-Status "Checking NASM assembler..." "Step"
    if (Test-Command "nasm") {
        try {
            $nasmVersion = & nasm -version 2>&1 | Select-Object -First 1
            Write-Status "NASM found: $nasmVersion" "Success"
            $script:success += "NASM assembler available (needed for BoringSSL in gRPC)"
            return $true
        } catch {
            Write-Status "NASM command exists but version check failed" "Warning"
            return $true
        }
    } else {
        Write-Status "NASM not found in PATH (optional)" "Info"
        Write-Status "NASM is required for gRPC builds with BoringSSL. Install via: choco install nasm" "Info"
        return $false
    }
}

function Test-VSCode {
    Write-Status "Checking Visual Studio Code installation..." "Step"
    
    # Check for VSCode in common locations
    $vscodeLocations = @(
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code\Code.exe",
        "$env:ProgramFiles\Microsoft VS Code\Code.exe",
        "$env:ProgramFiles(x86)\Microsoft VS Code\Code.exe"
    )
    
    $vscodeFound = $false
    $vscodePath = $null
    
    foreach ($location in $vscodeLocations) {
        if (Test-Path $location) {
            $vscodeFound = $true
            $vscodePath = $location
            break
        }
    }
    
    if (-not $vscodeFound) {
        # Try to find it via command
        if (Test-Command "code") {
            $vscodeFound = $true
            $vscodePath = (Get-Command code).Source
        }
    }
    
    if ($vscodeFound) {
        Write-Status "VS Code found: $vscodePath" "Success"
        
        # Check for CMake Tools extension
        $extensionsOutput = & code --list-extensions 2>&1
        if ($extensionsOutput -match "ms-vscode.cmake-tools") {
            Write-Status "VS Code CMake Tools extension installed" "Success"
            $script:success += "VS Code with CMake Tools ready for development"
        } else {
            Write-Status "VS Code found but CMake Tools extension not installed" "Warning"
            $script:warnings += "Install CMake Tools extension: code --install-extension ms-vscode.cmake-tools"
        }
        return $true
    } else {
        Write-Status "VS Code not found (optional)" "Info"
        return $false
    }
}

function Test-RomAssets {
    Write-Status "Checking for local Zelda 3 ROM assets..." "Step"
    $romPaths = @(
        "zelda3.sfc",
        "assets/zelda3.sfc",
        "assets/zelda3.yaze",
        "Roms/zelda3.sfc"
    )

    foreach ($relativePath in $romPaths) {
        $fullPath = Join-Path $PSScriptRoot ".." $relativePath
        if (Test-Path $fullPath) {
            Write-Status "Found ROM asset at '$relativePath'" "Success"
            $script:success += "ROM asset available for GUI/editor smoke tests"
            return
        }
    }

    Write-Status "No ROM asset detected. Place a clean 'zelda3.sfc' in the repo root or assets/ directory." "Warning"
    $script:warnings += "ROM assets missing - GUI workflows that load ROMs will fail until one is provided."
}

function Test-CMakePresets {
    Write-Status "Validating CMakePresets.json..." "Step"
    
    $presetsPath = Join-Path $PSScriptRoot ".." "CMakePresets.json"
    
    if (-not (Test-Path $presetsPath)) {
        Write-Status "CMakePresets.json not found!" "Error"
        $script:issuesFound += "CMakePresets.json missing from repository"
        return $false
    }
    
    try {
        $presets = Get-Content $presetsPath -Raw | ConvertFrom-Json
        $configurePresets = $presets.configurePresets | Where-Object { $_.name -like "win-*" }
        
        if ($configurePresets.Count -eq 0) {
            Write-Status "No Windows presets found in CMakePresets.json" "Error"
            $script:issuesFound += "CMakePresets.json has no Windows presets (win-dbg, win-rel, etc.)"
            return $false
        }
        
        Write-Status "CMakePresets.json valid with $($configurePresets.Count) Windows presets" "Success"
        
        # List available presets if verbose
        if ($Verbose) {
            Write-Status "Available Windows presets:" "Info"
            foreach ($preset in $configurePresets) {
                Write-Host "    - $($preset.name): $($preset.description)" -ForegroundColor Gray
            }
        }
        
        $script:success += "CMakePresets.json contains Windows build configurations"
        return $true
        
    } catch {
        Write-Status "Failed to parse CMakePresets.json: $_" "Error"
        $script:issuesFound += "CMakePresets.json is invalid or corrupted"
        return $false
    }
}

function Test-VisualStudioComponents {
    Write-Status "Checking Visual Studio C++ components..." "Step"
    
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $false
    }
    
    # Check for specific components needed for C++
    $requiredComponents = @(
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "Microsoft.VisualStudio.Component.Windows10SDK"
    )
    
    $recommendedComponents = @(
        "Microsoft.VisualStudio.Component.VC.CMake.Project",
        "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
        "Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset"
    )
    
    $allComponentsPresent = $true
    
    foreach ($component in $requiredComponents) {
        $result = & $vswhere -latest -requires $component -format value -property instanceId 2>&1
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($result)) {
            Write-Status "Missing required component: $component" "Error"
            $script:issuesFound += "Visual Studio component not installed: $component"
            $allComponentsPresent = $false
        } elseif ($Verbose) {
            Write-Status "Component installed: $component" "Success"
        }
    }
    
    foreach ($component in $recommendedComponents) {
        $result = & $vswhere -latest -requires $component -format value -property instanceId 2>&1
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($result)) {
            Write-Status "Recommended component not installed: $component" "Info"
            if ($component -match "CMake") {
                $script:warnings += "Visual Studio CMake support not installed (recommended for IDE integration)"
            }
        } elseif ($Verbose) {
            Write-Status "Recommended component installed: $component" "Success"
        }
    }
    
    return $allComponentsPresent
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

# Step 3: Check Build Tools (Ninja, clang-cl, NASM)
Test-Ninja | Out-Null
Test-ClangCL | Out-Null
Test-NASM | Out-Null

# Step 4: Check Visual Studio
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
        
        # Check for detailed components
        Test-VisualStudioComponents | Out-Null
    } else {
        Write-Status "Visual Studio found, but 'Desktop development with C++' workload is missing." "Error"
        $script:issuesFound += "Visual Studio 'Desktop development with C++' workload not installed."
    }
} else {
    Write-Status "Visual Studio not found (vswhere.exe missing)" "Error"
    $script:issuesFound += "Visual Studio installation not detected."
}

# Step 5: Check VSCode (optional)
Test-VSCode | Out-Null

# Step 6: Check CMakePresets.json
Test-CMakePresets | Out-Null

# Step 7: Check vcpkg
Test-Vcpkg | Out-Null
Test-VcpkgCache | Out-Null

# Step 8: Check Git Submodules
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

# Step 9: Check CMake Cache
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

# Step 10: Check ROM assets
Test-RomAssets | Out-Null

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
    if ($script:warnings -join ' ' -match 'Ninja') {
        Write-Host " • Ninja build system:" -ForegroundColor White
        Write-Host "   Ninja is required for win-dbg, win-rel, and win-ai presets." -ForegroundColor Gray
        Write-Host "   Install via Chocolatey: choco install ninja" -ForegroundColor Gray
        Write-Host "   Or use win-vs-* presets which use Visual Studio generator instead.`n"
    }
    if ($script:warnings -join ' ' -match 'CMake Tools') {
        Write-Host " • VS Code CMake Tools extension:" -ForegroundColor White
        Write-Host "   For VS Code integration, install the CMake Tools extension:" -ForegroundColor Gray
        Write-Host "   code --install-extension ms-vscode.cmake-tools" -ForegroundColor Gray
        Write-Host "   Or install manually from the Extensions panel.`n"
    }
    if ($script:issuesFound -join ' ' -match 'CMakePresets') {
        Write-Host " • CMakePresets.json missing or invalid:" -ForegroundColor White
        Write-Host "   This file is required for preset-based builds." -ForegroundColor Gray
        Write-Host "   Ensure you're in the yaze repository root and the file exists." -ForegroundColor Gray
        Write-Host "   Pull latest changes from git to get the updated presets.`n"
    }
    
    Write-Host "If problems persist, check the build instructions in 'docs/B1-build-instructions.md'`n" -ForegroundColor Cyan
    
    exit 1
} else {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║          ✓ Build Environment Ready for Development!           ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green
    
    # Determine which IDE and preset to recommend
    $hasVS = Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $hasVSCode = Test-Command "code"
    $hasNinja = Test-Command "ninja"
    
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host ""
    
    # Recommend presets based on available tools
    if ($hasVS -and -not $hasNinja) {
        Write-Host "  Recommended: Visual Studio Generator Presets" -ForegroundColor Yellow
        Write-Host "  (Ninja not found - use win-vs-* presets)" -ForegroundColor Gray
        Write-Host ""
    } elseif ($hasNinja) {
        Write-Host "  Recommended: Ninja Generator Presets (faster builds)" -ForegroundColor Yellow
        Write-Host "  (Ninja detected - use win-* presets)" -ForegroundColor Gray
        Write-Host ""
    }
    
    # Visual Studio instructions
    if ($hasVS) {
        Write-Host "  Option 1: Visual Studio 2022 (Full IDE)" -ForegroundColor White
        Write-Host "    1. Open Visual Studio 2022" -ForegroundColor Gray
        Write-Host "    2. Select 'File -> Open -> Folder...' and choose the 'yaze' directory" -ForegroundColor Gray
        if ($hasNinja) {
            Write-Host "    3. Select preset: 'win-dbg' (Ninja) or 'win-vs-dbg' (VS Generator)" -ForegroundColor Gray
        } else {
            Write-Host "    3. Select preset: 'win-vs-dbg' (install Ninja for win-dbg option)" -ForegroundColor Gray
        }
        Write-Host "    4. Press F5 to build and debug" -ForegroundColor Gray
        Write-Host ""
    }
    
    # VSCode instructions
    if ($hasVSCode) {
        Write-Host "  Option 2: Visual Studio Code (Lightweight)" -ForegroundColor White
        Write-Host "    1. Open folder in VS Code: code ." -ForegroundColor Gray
        Write-Host "    2. Install CMake Tools extension (if not installed)" -ForegroundColor Gray
        if ($hasNinja) {
            Write-Host "    3. Select CMake preset: 'win-dbg' from status bar" -ForegroundColor Gray
        } else {
            Write-Host "    3. Install Ninja first: choco install ninja" -ForegroundColor Gray
            Write-Host "       Then select preset: 'win-dbg'" -ForegroundColor Gray
        }
        Write-Host "    4. Press F7 to build, F5 to debug" -ForegroundColor Gray
        Write-Host ""
    }
    
    # Command line instructions
    Write-Host "  Option 3: Command Line" -ForegroundColor White
    if ($hasNinja) {
        Write-Host "    # Basic build (Ninja generator - fast)" -ForegroundColor Gray
        Write-Host "    cmake --preset win-dbg" -ForegroundColor Cyan
        Write-Host "    cmake --build --preset win-dbg" -ForegroundColor Cyan
        Write-Host "" 
        Write-Host "    # With AI features (gRPC + JSON)" -ForegroundColor Gray
        Write-Host "    cmake --preset win-ai" -ForegroundColor Cyan
        Write-Host "    cmake --build --preset win-ai" -ForegroundColor Cyan
    } else {
        Write-Host "    # Visual Studio generator (install Ninja for faster builds)" -ForegroundColor Gray
        Write-Host "    cmake --preset win-vs-dbg" -ForegroundColor Cyan
        Write-Host "    cmake --build --preset win-vs-dbg" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "    # Install Ninja for faster builds:" -ForegroundColor Yellow
        Write-Host "    choco install ninja" -ForegroundColor Gray
    }
    Write-Host ""
    
    # Available presets summary
    Write-Host "  Available Presets:" -ForegroundColor White
    if ($hasNinja) {
        Write-Host "    win-dbg      - Debug build (Ninja)" -ForegroundColor Gray
        Write-Host "    win-rel      - Release build (Ninja)" -ForegroundColor Gray
        Write-Host "    win-ai       - Debug with AI/gRPC features (Ninja)" -ForegroundColor Gray
    }
    Write-Host "    win-vs-dbg   - Debug build (Visual Studio)" -ForegroundColor Gray
    Write-Host "    win-vs-rel   - Release build (Visual Studio)" -ForegroundColor Gray
    Write-Host "    win-vs-ai    - Debug with AI/gRPC features (Visual Studio)" -ForegroundColor Gray
    Write-Host ""
    
    exit 0
}
