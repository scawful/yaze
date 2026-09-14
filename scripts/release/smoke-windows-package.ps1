[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ArchivePath,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ExpectedVersion,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-PackageCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$OutputDirectory,

        [string]$ExpectedOutput = ""
    )

    $outputStem = $Label.ToLowerInvariant() -replace '[^a-z0-9]+', '-'
    $stdoutPath = Join-Path $OutputDirectory "$outputStem.stdout.txt"
    $stderrPath = Join-Path $OutputDirectory "$outputStem.stderr.txt"
    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Arguments `
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
    $output = (($stdout, $stderr) | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    }) -join [Environment]::NewLine
    if ([string]::IsNullOrWhiteSpace($output)) {
        throw "$Label produced no output"
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedOutput) -and
        $output.Trim() -cne $ExpectedOutput) {
        throw "$Label output mismatch: expected '$ExpectedOutput', found '$($output.Trim())'"
    }
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

$resolvedArchive = (Resolve-Path -LiteralPath $ArchivePath).Path
if ([IO.Path]::GetExtension($resolvedArchive) -ne ".zip") {
    throw "Windows package smoke test requires a ZIP archive: $resolvedArchive"
}
$normalizedExpectedVersion = Normalize-Version -Version $ExpectedVersion

$extractPath = Join-Path ([IO.Path]::GetTempPath()) ("yaze-release-smoke-" + [guid]::NewGuid())

try {
    New-Item -ItemType Directory -Path $extractPath | Out-Null
    Expand-Archive -LiteralPath $resolvedArchive -DestinationPath $extractPath

    $yazeExecutables = @(Get-ChildItem -LiteralPath $extractPath -Recurse -File -Filter "yaze.exe")
    if ($yazeExecutables.Count -ne 1) {
        throw "Expected exactly one yaze.exe in the package, found $($yazeExecutables.Count)"
    }

    $packageRoot = $yazeExecutables[0].Directory.FullName
    $z3edPath = Join-Path $packageRoot "z3ed.exe"
    $assetsPath = Join-Path $packageRoot "assets"

    if (-not (Test-Path -LiteralPath $z3edPath -PathType Leaf)) {
        throw "Missing z3ed.exe next to yaze.exe in $packageRoot"
    }
    if (-not (Test-Path -LiteralPath $assetsPath -PathType Container)) {
        throw "Missing assets directory next to yaze.exe in $packageRoot"
    }

    $assetFiles = @(Get-ChildItem -LiteralPath $assetsPath -Recurse -File)
    if ($assetFiles.Count -eq 0) {
        throw "Assets directory is empty: $assetsPath"
    }

    Write-Host "Unpacked Windows package: $resolvedArchive"
    Write-Host "Found yaze.exe, z3ed.exe, and $($assetFiles.Count) asset files"

    Invoke-PackageCommand `
        -FilePath $yazeExecutables[0].FullName `
        -Arguments "--version" `
        -WorkingDirectory $packageRoot `
        -Label "yaze.exe --version" `
        -OutputDirectory $extractPath `
        -ExpectedOutput "yaze $normalizedExpectedVersion"

    Invoke-PackageCommand `
        -FilePath $z3edPath `
        -Arguments "--self-test" `
        -WorkingDirectory $packageRoot `
        -Label "z3ed.exe --self-test" `
        -OutputDirectory $extractPath

    # GitHub-hosted Windows runners do not provide a reliable interactive
    # desktop. Keep interactive editor use as the documented manual pre-tag check.
    Write-Host "Windows package smoke test passed (interactive GUI use remains a manual pre-tag check)."
}
finally {
    if (Test-Path -LiteralPath $extractPath) {
        Remove-Item -LiteralPath $extractPath -Recurse -Force
    }
}
