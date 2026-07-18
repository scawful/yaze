[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ArchivePath,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedArchive = (Resolve-Path -LiteralPath $ArchivePath).Path
if ([IO.Path]::GetExtension($resolvedArchive) -ne ".zip") {
    throw "Windows package smoke test requires a ZIP archive: $resolvedArchive"
}

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

    $stdoutPath = Join-Path $extractPath "z3ed-help.stdout.txt"
    $stderrPath = Join-Path $extractPath "z3ed-help.stderr.txt"
    $process = Start-Process `
        -FilePath $z3edPath `
        -ArgumentList "--help" `
        -WorkingDirectory $packageRoot `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        throw "z3ed.exe --help did not exit within $TimeoutSeconds seconds"
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
        throw "z3ed.exe --help exited with code $($process.ExitCode)"
    }
    if ([string]::IsNullOrWhiteSpace($stdout) -and [string]::IsNullOrWhiteSpace($stderr)) {
        throw "z3ed.exe --help produced no output"
    }

    # GitHub-hosted Windows runners do not provide a reliable interactive
    # desktop. Keep yaze.exe launch as the documented manual pre-tag check.
    Write-Host "Windows package smoke test passed (GUI launch remains a manual pre-tag check)."
}
finally {
    if (Test-Path -LiteralPath $extractPath) {
        Remove-Item -LiteralPath $extractPath -Recurse -Force
    }
}
