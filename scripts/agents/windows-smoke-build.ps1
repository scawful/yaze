param(
    [Parameter(Mandatory = $true)]
    [string]$Preset,
    [string]$Target = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot/.."
Set-Location $repoRoot

function Get-GeneratorAndConfig {
    param([string]$PresetName)

    $jsonPath = Join-Path $repoRoot "CMakePresets.json"
    $data = Get-Content $jsonPath -Raw | ConvertFrom-Json
    $configurePresets = @{}
    foreach ($preset in $data.configurePresets) {
        $configurePresets[$preset.name] = $preset
    }

    $buildPresets = @{}
    foreach ($preset in $data.buildPresets) {
        $buildPresets[$preset.name] = $preset
    }

    function Resolve-Generator([string]$name, [hashtable]$seen) {
        if ($seen.ContainsKey($name)) { return $null }
        $seen[$name] = $true
        if (-not $configurePresets.ContainsKey($name)) { return $null }
        $entry = $configurePresets[$name]
        if ($entry.generator) { return $entry.generator }
        $inherits = $entry.inherits
        if ($inherits -is [string]) { $inherits = @($inherits) }
        foreach ($parent in $inherits) {
            $gen = Resolve-Generator $parent $seen
            if ($gen) { return $gen }
        }
        return $null
    }

    $generator = Resolve-Generator $PresetName @{}

    $config = $null
    if ($buildPresets.ContainsKey($PresetName) -and $buildPresets[$PresetName].configuration) {
        $config = $buildPresets[$PresetName].configuration
    } elseif ($configurePresets.ContainsKey($PresetName)) {
        $cache = $configurePresets[$PresetName].cacheVariables
        if ($cache.CMAKE_BUILD_TYPE) { $config = $cache.CMAKE_BUILD_TYPE }
    }

    return @{ Generator = $generator; Configuration = $config }
}

Write-Host "Configuring preset: $Preset"
cmake --preset $Preset

$info = Get-GeneratorAndConfig -PresetName $Preset
$buildCmd = @("cmake", "--build", "--preset", $Preset)
if ($Target) { $buildCmd += @("--target", $Target) }
if ($info.Generator -like "*Visual Studio*" -and $info.Configuration) {
    $buildCmd += @("--config", $info.Configuration)
}

Write-Host "Building preset: $Preset"
Write-Host "+ $($buildCmd -join ' ')"
& $buildCmd

Write-Host "Smoke build completed for preset: $Preset"
