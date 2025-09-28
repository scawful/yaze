# Setup script for Windows development environment
# This script installs the necessary tools for YAZE development on Windows

param(
    [switch]$Force = $false
)

Write-Host "Setting up Windows development environment for YAZE..." -ForegroundColor Green

# Check if we're on Windows
if ($env:OS -ne "Windows_NT") {
    Write-Host "This script is designed for Windows only." -ForegroundColor Red
    exit 1
}

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Host "This script requires administrator privileges to install software." -ForegroundColor Yellow
    Write-Host "Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
    exit 1
}

# Install Chocolatey if not present
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Chocolatey package manager..." -ForegroundColor Yellow
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    
    # Refresh environment variables
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    
    Write-Host "Chocolatey installed successfully" -ForegroundColor Green
} else {
    Write-Host "Chocolatey already installed" -ForegroundColor Green
}

# Install required tools
$tools = @(
    @{Name="cmake"; Description="CMake build system"},
    @{Name="git"; Description="Git version control"},
    @{Name="ninja"; Description="Ninja build system"},
    @{Name="python3"; Description="Python 3 for scripts"}
)

foreach ($tool in $tools) {
    Write-Host "Checking $($tool.Description)..." -ForegroundColor Cyan
    
    $installed = $false
    if ($tool.Name -eq "cmake") {
        $installed = Get-Command cmake -ErrorAction SilentlyContinue
    } elseif ($tool.Name -eq "git") {
        $installed = Get-Command git -ErrorAction SilentlyContinue
    } elseif ($tool.Name -eq "ninja") {
        $installed = Get-Command ninja -ErrorAction SilentlyContinue
    } elseif ($tool.Name -eq "python3") {
        $installed = Get-Command python3 -ErrorAction SilentlyContinue
    }
    
    if (-not $installed -or $Force) {
        Write-Host "Installing $($tool.Description)..." -ForegroundColor Yellow
        choco install -y $tool.Name
        if ($LASTEXITCODE -eq 0) {
            Write-Host "$($tool.Description) installed successfully" -ForegroundColor Green
        } else {
            Write-Host "Failed to install $($tool.Description)" -ForegroundColor Red
        }
    } else {
        Write-Host "$($tool.Description) already installed" -ForegroundColor Green
    }
}

# Refresh environment variables
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

# Verify installations
Write-Host "`nVerifying installations..." -ForegroundColor Cyan

$toolsToVerify = @("cmake", "git", "ninja", "python3")
foreach ($tool in $toolsToVerify) {
    $path = Get-Command $tool -ErrorAction SilentlyContinue
    if ($path) {
        Write-Host "✓ $tool found at: $($path.Source)" -ForegroundColor Green
    } else {
        Write-Host "✗ $tool not found" -ForegroundColor Red
    }
}

# Check for Visual Studio
Write-Host "`nChecking for Visual Studio..." -ForegroundColor Cyan
$vsWhere = Get-Command "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -ErrorAction SilentlyContinue
if (-not $vsWhere) {
    $vsWhere = Get-Command vswhere -ErrorAction SilentlyContinue
}

if ($vsWhere) {
    $vsInstallPath = & $vsWhere.Source -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInstallPath) {
        Write-Host "✓ Visual Studio found at: $vsInstallPath" -ForegroundColor Green
    } else {
        Write-Host "✗ Visual Studio 2022 with C++ workload not found" -ForegroundColor Red
        Write-Host "Please install Visual Studio 2022 with the 'Desktop development with C++' workload" -ForegroundColor Yellow
    }
} else {
    Write-Host "✗ vswhere not found - cannot detect Visual Studio" -ForegroundColor Red
}

Write-Host "`n🎉 Windows development environment setup complete!" -ForegroundColor Green
Write-Host "`nNext steps:" -ForegroundColor Cyan
Write-Host "1. Run the Visual Studio project generation script:" -ForegroundColor White
Write-Host "   .\scripts\generate-vs-projects.ps1" -ForegroundColor Gray
Write-Host "2. Or use CMake presets:" -ForegroundColor White
Write-Host "   cmake --preset windows-debug" -ForegroundColor Gray
Write-Host "3. Open YAZE.sln in Visual Studio and build" -ForegroundColor White
