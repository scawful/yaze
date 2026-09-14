[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ExpectedVersion,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ExpectedGitSha,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-BoundedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory `
        -PassThru

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        throw "$Label did not exit within $TimeoutSeconds seconds"
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "$Label exited with code $($process.ExitCode)"
    }
}

function Invoke-CapturedPackageCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string]$Argument,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$OutputDirectory
    )

    $outputStem = $Label.ToLowerInvariant() -replace '[^a-z0-9]+', '-'
    $stdoutPath = Join-Path $OutputDirectory "$outputStem.stdout.txt"
    $stderrPath = Join-Path $OutputDirectory "$outputStem.stderr.txt"
    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Argument `
        -WorkingDirectory $WorkingDirectory `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        throw "$Label did not exit within $TimeoutSeconds seconds"
    }
    $process.WaitForExit()

    $stdout = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    $stderr = Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue
    if ($stdout) {
        Write-Host ($stdout.TrimEnd())
    }
    if ($stderr) {
        Write-Host ($stderr.TrimEnd())
    }
    if ($process.ExitCode -ne 0) {
        throw "$Label exited with code $($process.ExitCode)"
    }
    if ([string]::IsNullOrWhiteSpace($stdout) -and [string]::IsNullOrWhiteSpace($stderr)) {
        throw "$Label produced no output"
    }

    return (($stdout, $stderr) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [Environment]::NewLine
}

function Normalize-Version {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Version
    )

    $normalized = $Version.Trim()
    if ($normalized.StartsWith("v", [StringComparison]::OrdinalIgnoreCase)) {
        $normalized = $normalized.Substring(1)
    }
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        throw "Expected version is empty after normalization"
    }
    return $normalized
}

function Get-YazeInstallRegistrations {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RegistryKeyName
    )

    $subKeyPath = "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$RegistryKeyName"
    $results = [System.Collections.Generic.List[object]]::new()
    foreach ($view in @(
        [Microsoft.Win32.RegistryView]::Registry64,
        [Microsoft.Win32.RegistryView]::Registry32
    )) {
        $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
            [Microsoft.Win32.RegistryHive]::LocalMachine,
            $view
        )
        try {
            $key = $baseKey.OpenSubKey($subKeyPath)
            if ($null -ne $key) {
                try {
                    $results.Add([pscustomobject]@{
                        View = $view.ToString()
                        UninstallString = [string]$key.GetValue("UninstallString")
                    })
                }
                finally {
                    $key.Dispose()
                }
            }
        }
        finally {
            $baseKey.Dispose()
        }
    }
    return $results.ToArray()
}

function Get-ExecutablePathFromCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandLine
    )

    $trimmed = $CommandLine.Trim()
    if ($trimmed.StartsWith('"')) {
        $closingQuote = $trimmed.IndexOf('"', 1)
        if ($closingQuote -le 1) {
            throw "Invalid quoted uninstall command: $CommandLine"
        }
        return $trimmed.Substring(1, $closingQuote - 1)
    }

    $match = [regex]::Match(
        $trimmed,
        '^(?<path>.+?\.exe)(?:\s|$)',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase
    )
    if (-not $match.Success) {
        throw "Unable to parse executable path from uninstall command: $CommandLine"
    }
    return $match.Groups['path'].Value
}

function Test-PathInsideRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root)
    $rootPrefix = $fullRoot.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar
    ) + [IO.Path]::DirectorySeparatorChar
    return $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)
}

$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath).Path
if ([IO.Path]::GetExtension($resolvedInstaller) -ne ".exe") {
    throw "Windows installer smoke test requires an EXE: $resolvedInstaller"
}

$normalizedExpectedVersion = Normalize-Version -Version $ExpectedVersion
$normalizedExpectedGitSha = $ExpectedGitSha.Trim().ToLowerInvariant()
if ($normalizedExpectedGitSha -notmatch '^[0-9a-f]{7,64}$') {
    throw "Expected Git SHA must contain 7-64 hexadecimal characters"
}

$registryKeyName = "yaze $normalizedExpectedVersion"
$preexistingRegistrations = @(Get-YazeInstallRegistrations -RegistryKeyName $registryKeyName)
if ($preexistingRegistrations.Count -gt 0) {
    $views = ($preexistingRegistrations | ForEach-Object { $_.View }) -join ", "
    throw "Yaze $normalizedExpectedVersion is already registered in $views. Run this smoke test only in a clean disposable Windows environment."
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("yaze-release-smoke-" + [guid]::NewGuid())
$installPath = Join-Path $tempRoot "install"
$logsPath = Join-Path $tempRoot "logs"
$uninstallCompleted = $false

try {
    New-Item -ItemType Directory -Path $installPath, $logsPath | Out-Null
    Write-Host "Silently installing: $resolvedInstaller"
    Invoke-BoundedProcess `
        -FilePath $resolvedInstaller `
        -Arguments @("/S", "/D=$installPath") `
        -WorkingDirectory (Split-Path -Parent $resolvedInstaller) `
        -Label "NSIS install"

    $yazeExecutables = @(Get-ChildItem -LiteralPath $installPath -Recurse -File -Filter "yaze.exe")
    if ($yazeExecutables.Count -ne 1) {
        throw "Expected exactly one installed yaze.exe, found $($yazeExecutables.Count)"
    }

    $packageRoot = $yazeExecutables[0].Directory.FullName
    $z3edPath = Join-Path $packageRoot "z3ed.exe"
    $assetsPath = Join-Path $packageRoot "assets"
    if (-not (Test-Path -LiteralPath $z3edPath -PathType Leaf)) {
        throw "Missing installed z3ed.exe next to yaze.exe in $packageRoot"
    }
    if (-not (Test-Path -LiteralPath $assetsPath -PathType Container)) {
        throw "Missing installed assets directory next to yaze.exe in $packageRoot"
    }
    $assetFiles = @(Get-ChildItem -LiteralPath $assetsPath -Recurse -File)
    if ($assetFiles.Count -eq 0) {
        throw "Installed assets directory is empty: $assetsPath"
    }
    $msvcpRuntimes = @(Get-ChildItem -LiteralPath $packageRoot -File -Filter "msvcp*.dll")
    $vcRuntimes = @(Get-ChildItem -LiteralPath $packageRoot -File -Filter "vcruntime*.dll")
    if ($msvcpRuntimes.Count -eq 0 -or $vcRuntimes.Count -eq 0) {
        throw "Installed package is missing app-local MSVC runtime DLLs"
    }

    $manifests = @(Get-ChildItem -LiteralPath $installPath -Recurse -File -Filter "manifest.json")
    if ($manifests.Count -ne 1) {
        throw "Expected exactly one installed manifest.json, found $($manifests.Count)"
    }
    if ($manifests[0].Directory.FullName -ne $packageRoot) {
        throw "Installed manifest.json is not next to yaze.exe: $($manifests[0].FullName)"
    }

    try {
        $manifest = Get-Content -LiteralPath $manifests[0].FullName -Raw | ConvertFrom-Json
    }
    catch {
        throw "Installed manifest.json is invalid JSON: $_"
    }
    foreach ($requiredField in @("name", "version", "git_sha", "features")) {
        if ($null -eq $manifest.PSObject.Properties[$requiredField]) {
            throw "Installed manifest.json is missing '$requiredField'"
        }
    }
    if ($null -eq $manifest.features -or $manifest.features -is [string] -or $manifest.features -is [array]) {
        throw "Installed manifest.json 'features' must be an object"
    }

    $actualVersion = Normalize-Version -Version ([string]$manifest.version)
    if ($actualVersion -ne $normalizedExpectedVersion) {
        throw "Installed manifest version mismatch: expected $normalizedExpectedVersion, found $actualVersion"
    }
    $actualGitSha = ([string]$manifest.git_sha).Trim().ToLowerInvariant()
    if ($actualGitSha -notmatch '^[0-9a-f]{7,64}$' -or -not $normalizedExpectedGitSha.StartsWith($actualGitSha)) {
        throw "Installed manifest Git SHA mismatch: expected prefix of $normalizedExpectedGitSha, found $actualGitSha"
    }

    $yazeVersionOutput = Invoke-CapturedPackageCommand `
        -FilePath $yazeExecutables[0].FullName `
        -Argument "--version" `
        -WorkingDirectory $packageRoot `
        -Label "installed yaze.exe --version" `
        -OutputDirectory $logsPath
    if ($yazeVersionOutput.Trim() -ne "yaze $normalizedExpectedVersion") {
        throw "Installed yaze.exe version mismatch: expected 'yaze $normalizedExpectedVersion', found '$($yazeVersionOutput.Trim())'"
    }

    $z3edSelfTestOutput = Invoke-CapturedPackageCommand `
        -FilePath $z3edPath `
        -Argument "--self-test" `
        -WorkingDirectory $packageRoot `
        -Label "installed z3ed.exe --self-test" `
        -OutputDirectory $logsPath

    $uninstallers = @(Get-ChildItem -LiteralPath $installPath -Recurse -File -Filter "Uninstall*.exe")
    if ($uninstallers.Count -ne 1) {
        throw "Expected exactly one NSIS uninstaller, found $($uninstallers.Count)"
    }

    $registrations = @(Get-YazeInstallRegistrations -RegistryKeyName $registryKeyName)
    if ($registrations.Count -ne 1) {
        throw "Expected exactly one Yaze registry entry after install, found $($registrations.Count)"
    }
    if ([string]::IsNullOrWhiteSpace($registrations[0].UninstallString)) {
        throw "Installed Yaze registry entry has no UninstallString"
    }
    $registeredUninstaller = Get-ExecutablePathFromCommand -CommandLine $registrations[0].UninstallString
    if (-not (Test-PathInsideRoot -Path $registeredUninstaller -Root $installPath)) {
        throw "Registered uninstaller is outside isolated install root: $registeredUninstaller"
    }
    if (-not (Test-Path -LiteralPath $registeredUninstaller -PathType Leaf)) {
        throw "Registered uninstaller does not exist: $registeredUninstaller"
    }
    if ([IO.Path]::GetFullPath($registeredUninstaller) -ne [IO.Path]::GetFullPath($uninstallers[0].FullName)) {
        throw "Registry UninstallString does not match the installed uninstaller"
    }

    Write-Host "Silently uninstalling: $registeredUninstaller"
    Invoke-BoundedProcess `
        -FilePath $registeredUninstaller `
        -Arguments @("/S", "_?=$installPath") `
        -WorkingDirectory $installPath `
        -Label "NSIS uninstall"
    Remove-Item -LiteralPath $registeredUninstaller -Force -ErrorAction SilentlyContinue

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $remainingPayload = @()
    $remainingRegistrations = @()
    do {
        $remainingPayload = if (Test-Path -LiteralPath $installPath) {
            @(Get-ChildItem -LiteralPath $installPath -Force)
        } else {
            @()
        }
        $remainingRegistrations = @(Get-YazeInstallRegistrations -RegistryKeyName $registryKeyName)
        if ($remainingPayload.Count -eq 0 -and $remainingRegistrations.Count -eq 0) {
            break
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    if ($remainingPayload.Count -gt 0) {
        $leftovers = Get-ChildItem -LiteralPath $installPath -Recurse -Force | ForEach-Object {
            [IO.Path]::GetRelativePath($installPath, $_.FullName)
        }
        throw "NSIS uninstall left packaged payload behind in $installPath`: $($leftovers -join ', ')"
    }
    if ($remainingRegistrations.Count -gt 0) {
        $views = ($remainingRegistrations | ForEach-Object { $_.View }) -join ", "
        throw "NSIS uninstall left registry entries behind in: $views"
    }
    if (Test-Path -LiteralPath $installPath) {
        Remove-Item -LiteralPath $installPath -Force
    }

    $uninstallCompleted = $true
    Write-Host "Windows installer smoke test passed: $resolvedInstaller"
}
finally {
    if (-not $uninstallCompleted -and (Test-Path -LiteralPath $installPath)) {
        $remainingUninstallers = @(
            Get-ChildItem -LiteralPath $installPath -Recurse -File -Filter "Uninstall*.exe" -ErrorAction SilentlyContinue
        )
        if ($remainingUninstallers.Count -gt 0) {
            try {
                Invoke-BoundedProcess `
                    -FilePath $remainingUninstallers[0].FullName `
                    -Arguments @("/S", "_?=$($remainingUninstallers[0].Directory.FullName)") `
                    -WorkingDirectory $remainingUninstallers[0].Directory.FullName `
                    -Label "NSIS cleanup uninstall"
            }
            catch {
                Write-Warning "Cleanup uninstaller failed: $_"
            }
        }
    }
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
