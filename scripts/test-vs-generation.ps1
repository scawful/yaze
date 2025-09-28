# Test script to verify Visual Studio project generation
# This script tests the generate-vs-projects.ps1 script with different configurations

param(
    [switch]$Clean = $false,
    [switch]$Verbose = $false
)

Write-Host "Testing Visual Studio project generation for YAZE..." -ForegroundColor Green

# Test configurations
$TestConfigs = @(
    @{ Config = "Debug"; Arch = "x64" },
    @{ Config = "Release"; Arch = "x64" }
)

$ScriptPath = Join-Path $PSScriptRoot "generate-vs-projects.ps1"

foreach ($testConfig in $TestConfigs) {
    Write-Host ""
    Write-Host "Testing configuration: $($testConfig.Config) $($testConfig.Arch)" -ForegroundColor Cyan
    
    $TestArgs = @(
        "-Configuration", $testConfig.Config,
        "-Architecture", $testConfig.Arch
    )
    
    if ($Clean) {
        $TestArgs += "-Clean"
    }
    
    if ($Verbose) {
        $TestArgs += "-Verbose"
    }
    
    Write-Host "Running: .\generate-vs-projects.ps1 $($TestArgs -join ' ')" -ForegroundColor Gray
    
    try {
        & $ScriptPath @TestArgs
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Configuration $($testConfig.Config) $($testConfig.Arch) passed" -ForegroundColor Green
        } else {
            Write-Host "❌ Configuration $($testConfig.Config) $($testConfig.Arch) failed" -ForegroundColor Red
        }
    } catch {
        Write-Host "❌ Configuration $($testConfig.Config) $($testConfig.Arch) failed with exception: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Test complete!" -ForegroundColor Green
Write-Host ""
Write-Host "If all tests passed, the Visual Studio project generation is working correctly." -ForegroundColor Cyan
Write-Host "You can now open YAZE.sln in Visual Studio and build the project." -ForegroundColor Cyan
