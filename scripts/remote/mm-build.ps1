# mm-build.ps1 - Windows side of scripts/remote/mm-test.sh (PowerShell 7).
#
# mm-test.sh uploads this file, job.txt and an optional git bundle into
# <root>\jobs\<id>\, then runs:
#   pwsh -NoProfile -File <root>\jobs\<id>\mm-build.ps1 -JobDir <root>\jobs\<id>
#
# Layout (everything under the remote user's profile, no admin rights needed):
#   %USERPROFILE%\src\yaze            clone that receives bundles (refs/mm/<slug>)
#   %USERPROFILE%\src\yw\<slug>       one worktree per branch, reused
#   %USERPROFILE%\src\yaze-remote     jobs\, mods\ (submodule mirrors), cpm\,
#                                     ccache\, stamps\, build.lock
#   %USERPROFILE%\tools\ninja, \ccache  portable ninja.exe and ccache.exe
#
# Exit codes: 0 pass, 1 tests failed, 2 configure/build failed, 3 setup error.
param([Parameter(Mandatory = $true)][string]$JobDir)

$ErrorActionPreference = 'Continue'
$PSStyle.OutputRendering = 'PlainText'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)

function Say([string]$m) { [Console]::Out.WriteLine("[mm] $m"); [Console]::Out.Flush() }
function Fail([int]$code, [string]$m) { Say "ERROR: $m"; exit $code }

# Runs a native command, appending stdout+stderr to $Log. Returns the exit code.
function Run([string]$Log, [string]$Exe, [string[]]$Argv) {
  "`n>>> $Exe $($Argv -join ' ')" | Out-File -Append -FilePath $Log
  & $Exe @Argv *>> $Log
  return $LASTEXITCODE
}
function GitOk([string[]]$Argv) {
  $rc = Run $script:GitLog 'git' $Argv
  if ($rc -ne 0) { Fail 3 "git $($Argv -join ' ') failed ($rc); see out\git.log" }
}

$JobDir = (Resolve-Path $JobDir).Path
$Out = Join-Path $JobDir 'out'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$script:GitLog = Join-Path $Out 'git.log'

# --- job parameters -----------------------------------------------------------
$job = @{}
foreach ($line in Get-Content (Join-Path $JobDir 'job.txt')) {
  if ($line -match '^([a-z_]+)=(.*)$') { $job[$Matches[1]] = $Matches[2].Trim() }
}
$checks = @{
  sha = '^[0-9a-f]{40}$'; slug = '^[A-Za-z0-9._-]{1,48}$'; mode = '^(unit|stable|filter|build)$'
  preset = '^[A-Za-z0-9_-]+$'; config = '^[A-Za-z]+$'; targets = '^[A-Za-z0-9_.+ -]+$'
  filter = '^[A-Za-z0-9_*?:./-]*$'; gtest_args = '^[A-Za-z0-9_=,.:/ -]*$'
  jobs = '^[0-9]*$'; clean = '^[01]$'; ccache = '^[01]?$'
  bundle = '^[A-Za-z0-9._-]*$'; bundle_ref = '^(refs/[A-Za-z0-9._/-]+)?$'
  origin = '^(https://[A-Za-z0-9._/-]+)?$'; max_worktrees = '^[0-9]+$'; lock_wait_min = '^[0-9]+$'
}
foreach ($k in $checks.Keys) {
  if (-not $job.ContainsKey($k)) { $job[$k] = '' }
  if ($job[$k] -notmatch $checks[$k]) { Fail 3 "job.txt: invalid $k='$($job[$k])'" }
}
$Home_ = $env:USERPROFILE
if ($Home_ -match '\s') { Fail 3 "USERPROFILE contains whitespace: $Home_" }
$Root = Split-Path (Split-Path $JobDir)           # ...\yaze-remote
$Repo = Join-Path $Home_ 'src\yaze'
$WtRoot = Join-Path $Home_ 'src\yw'
$Tools = Join-Path $Home_ 'tools'
$Wt = Join-Path $WtRoot $job.slug
$BuildDir = Join-Path $Wt "build\presets\$($job.preset)"
$times = [ordered]@{}

# --- lock: one build at a time on this machine ---------------------------------
# An exclusive file handle; Windows releases it when this process exits.
$lockPath = Join-Path $Root 'build.lock'
$ownerPath = Join-Path $Root 'build.lock.owner'
$deadline = (Get-Date).AddMinutes([int]$job.lock_wait_min)
$lock = $null; $announced = $false
while (-not $lock) {
  try { $lock = [IO.File]::Open($lockPath, 'OpenOrCreate', 'ReadWrite', 'None') }
  catch {
    if (-not $announced) {
      $owner = Get-Content $ownerPath -ErrorAction SilentlyContinue
      Say "waiting for build lock (held by: $owner)"; $announced = $true
    }
    if ((Get-Date) -gt $deadline) { Fail 3 "timed out after $($job.lock_wait_min) min waiting for build lock" }
    Start-Sleep -Seconds 5
  }
}
"$($job.who) $($job.slug) $($job.sha.Substring(0,12)) since $(Get-Date -Format s) pid=$PID" | Set-Content $ownerPath
# Stay responsive for whoever is using the PC.
(Get-Process -Id $PID).PriorityClass = 'BelowNormal'

# --- toolchain -----------------------------------------------------------------
$cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue).Source   # before vcvars puts VS's copy first
foreach ($t in @("$Tools\ninja\ninja.exe", "$Tools\ccache\ccache.exe")) {
  if (-not (Test-Path $t)) { Fail 3 "missing $t (see scripts/remote/README.md, one-time setup)" }
}
if (-not $cmakeExe) { Fail 3 'cmake not found on PATH' }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { Fail 3 'no Visual Studio with the C++ x64 toolset found' }
$envCmd = Join-Path $JobDir 'vcvars-env.cmd'
"@call `"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>&1`r`n@set" | Set-Content -Encoding ascii $envCmd
foreach ($line in (& cmd.exe /d /c $envCmd)) {
  $i = $line.IndexOf('=')
  if ($i -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0, $i), $line.Substring($i + 1)) }
}
$clangCl = "$vs\VC\Tools\Llvm\x64\bin\clang-cl.exe"
if (-not (Test-Path $clangCl)) { Fail 3 "clang-cl not found at $clangCl (VS component 'C++ Clang tools for Windows')" }
# CI (ci.yml setup-build) builds Windows with clang-cl from the MSVC dev environment.
$env:CC = $clangCl; $env:CXX = $clangCl
$env:PATH = "$Tools\ninja;$Tools\ccache;$(Split-Path $cmakeExe);$env:PATH"
$env:CPM_SOURCE_CACHE = Join-Path $Root 'cpm'
$env:CCACHE_DIR = Join-Path $Root 'ccache'
# Worktrees share hits through relative paths. Keep the CPM cache out of base_dir:
# relativized CPM include paths overflow MAX_PATH in ninja's /showIncludes parsing.
$env:CCACHE_BASEDIR = $WtRoot
$env:CCACHE_NOHASHDIR = '1'
$env:CCACHE_MAXSIZE = '40G'
$env:CCACHE_SLOPPINESS = 'pch_defines,time_macros'
if ($job.ccache -eq '0') { $env:CCACHE_DISABLE = '1' }

# --- sync sources ----------------------------------------------------------------
$sw = [Diagnostics.Stopwatch]::StartNew()
if (-not (Test-Path (Join-Path $Repo '.git'))) {
  if (-not $job.origin) { Fail 3 "$Repo missing and no origin URL given" }
  Say "cloning $($job.origin) -> $Repo (one-time)"
  GitOk @('clone', '--quiet', '--no-checkout', $job.origin, $Repo)
}
GitOk @('-C', $Repo, 'config', 'core.longpaths', 'true')
$ref = "refs/mm/$($job.slug)"
if ($job.bundle) {
  $bundlePath = Join-Path $JobDir $job.bundle
  GitOk @('-C', $Repo, 'fetch', '--quiet', '--no-write-fetch-head', $bundlePath, "+$($job.bundle_ref):$ref")
  Remove-Item $bundlePath -Force
} else {
  GitOk @('-C', $Repo, 'update-ref', $ref, $job.sha)
}
& git -C $Repo cat-file -e "$($job.sha)^{commit}" 2>$null
if ($LASTEXITCODE -ne 0) { Fail 3 "commit $($job.sha) not present after fetch" }

if (Test-Path (Join-Path $Wt '.git')) {
  GitOk @('-C', $Wt, 'checkout', '--quiet', '--detach', '--force', $job.sha)
  GitOk @('-C', $Wt, 'clean', '-ffdq')          # untracked files; ignored build/ is kept
} else {
  New-Item -ItemType Directory -Force -Path $WtRoot | Out-Null
  GitOk @('-C', $Repo, 'worktree', 'prune')
  Say "creating worktree $Wt"
  GitOk @('-C', $Repo, 'worktree', 'add', '--quiet', '--detach', '--force', $Wt, $job.sha)
}

# Submodules clone from local bare mirrors (<root>\mods) so a new worktree does not
# re-download SDL/imgui. The mirror URL lives in the shared repo config.
$mods = Join-Path $Root 'mods'
New-Item -ItemType Directory -Force -Path $mods | Out-Null
$subUrls = & git -C $Wt config -f .gitmodules --get-regexp '^submodule\..*\.url$'
foreach ($entry in $subUrls) {
  if ($entry -notmatch '^submodule\.(.+)\.url\s+(\S+)$') { continue }
  $name = $Matches[1]; $url = $Matches[2]
  $path = (& git -C $Wt config -f .gitmodules --get "submodule.$name.path")
  $pin = ((& git -C $Wt ls-tree HEAD $path) -split '\s+')[2]
  $mirror = Join-Path $mods (($name -replace '[\\/]', '_') + '.git')
  if (-not (Test-Path $mirror)) {
    Say "mirroring submodule $name (one-time)"
    GitOk @('clone', '--quiet', '--mirror', $url, $mirror)
  }
  if ($pin) {
    & git -C $mirror cat-file -e "$pin^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0) { GitOk @('-C', $mirror, 'fetch', '--quiet', 'origin') }
    & git -C $mirror cat-file -e "$pin^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0) { GitOk @('-C', $mirror, 'fetch', '--quiet', 'origin', "+$($pin):refs/pinned/$pin") }
  }
  GitOk @('-C', $Repo, 'config', "submodule.$name.url", ($mirror -replace '\\', '/'))
}
GitOk @('-C', $Wt, '-c', 'protocol.file.allow=always', 'submodule', 'update', '--init', '--recursive', '--force', '--quiet')
$times.sync_s = [math]::Round($sw.Elapsed.TotalSeconds, 1)
New-Item -ItemType Directory -Force -Path (Join-Path $Root 'stamps') | Out-Null
Set-Content (Join-Path $Root "stamps\$($job.slug)") $job.sha
Say "sync ok ($($times.sync_s)s): $($job.slug) @ $($job.sha.Substring(0,12))"

# --- configure + build -------------------------------------------------------------
$jobs = if ($job.jobs) { [int]$job.jobs } else {
  # ~1.2 GB per clang-cl job, capped by free RAM (the PC also runs other work).
  $freeGb = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB
  [math]::Max(4, [math]::Min([Environment]::ProcessorCount, [int][math]::Floor($freeGb / 1.2)))
}
if ($job.clean -eq '1' -and (Test-Path $BuildDir)) {
  Say "removing $BuildDir (--clean)"; Remove-Item -Recurse -Force $BuildDir
}
$buildLog = Join-Path $Out 'build.log'
& ccache -z *> $null
if (-not (Test-Path (Join-Path $BuildDir 'build.ninja'))) {   # new dir or failed configure
  $sw.Restart()
  Push-Location $Wt
  # /Z7 (embedded debug info) instead of /Zi so ccache can cache RelWithDebInfo objects.
  # OPENSSL_NO_ASM: gRPC's BoringSSL would need NASM (CI installs it with choco).
  $rc = Run $buildLog 'cmake' @('--preset', $job.preset, '-DCMAKE_POLICY_DEFAULT_CMP0141=NEW',
    '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded', '-DOPENSSL_NO_ASM=ON')
  Pop-Location
  $times.configure_s = [math]::Round($sw.Elapsed.TotalSeconds, 1)
  if ($rc -ne 0) {
    Select-String -Path $buildLog -Pattern 'CMake Error|error' | Select-Object -Last 25 | ForEach-Object { Say "  $($_.Line)" }
    Fail 2 "configure failed ($($times.configure_s)s); see out\build.log"
  }
  Say "configure ok ($($times.configure_s)s)"
}
$targets = @($job.targets -split '\s+' | Where-Object { $_ })
$sw.Restart()
$rc = Run $buildLog 'cmake' (@('--build', $BuildDir, '--config', $job.config, '--parallel', "$jobs", '--target') + $targets)
$times.build_s = [math]::Round($sw.Elapsed.TotalSeconds, 1)
$stats = (& ccache --print-stats) -join "`n"
$hits = 0; $miss = 0
if ($stats -match '(?m)^direct_cache_hit\s+(\d+)') { $hits += [int]$Matches[1] }
if ($stats -match '(?m)^preprocessed_cache_hit\s+(\d+)') { $hits += [int]$Matches[1] }
if ($stats -match '(?m)^cache_miss\s+(\d+)') { $miss = [int]$Matches[1] }
$ccacheLine = "ccache hits=$hits misses=$miss"
if ($rc -ne 0) {
  Select-String -Path $buildLog -Pattern 'FAILED:|error:|error C\d+|error LNK|fatal error' |
    Select-Object -First 40 | ForEach-Object { Say "  $($_.Line)" }
  Fail 2 "build failed ($($times.build_s)s, -j$jobs); see out\build.log"
}
Say "build ok ($($times.build_s)s, -j$jobs, $ccacheLine): $($targets -join ' ')"

# --- tests ---------------------------------------------------------------------------
$testRc = 0
$testLog = Join-Path $Out 'test.log'
# Keep tests away from the PC user's real files. test/yaze_test.cc sets these with
# SDL_setenv, which on Windows does not reach std::getenv, so without this the
# harness resolves %USERPROFILE%\Documents\Yaze and skips every test.
$testHome = Join-Path $JobDir 'testhome'
$env:TEMP = $env:TMP = $testHome          # the isolation test wants docs under TEMP
$env:YAZE_USER_DOCUMENTS_DIR = Join-Path $testHome 'documents'
$env:YAZE_APP_DATA_DIR = Join-Path $testHome 'appdata'
New-Item -ItemType Directory -Force -Path $env:YAZE_USER_DOCUMENTS_DIR, $env:YAZE_APP_DATA_DIR | Out-Null
$junit = Join-Path $Out 'junit.xml'
$sw.Restart()
if ($job.mode -eq 'unit' -or $job.mode -eq 'stable') {
  $label = if ($job.mode -eq 'unit') { '^unit$' } else { '^stable$' }
  $testRc = Run $testLog 'ctest' @('--test-dir', $BuildDir, '--build-config', $job.config,
    '--label-regex', $label, '--no-tests=error', '--output-on-failure', '--timeout', '300',
    '--parallel', "$jobs", '--output-junit', $junit)
} elseif ($job.mode -eq 'filter') {
  $exe = Get-ChildItem -Path (Join-Path $BuildDir 'bin') -Recurse -Filter 'yaze_test_unit.exe' |
    Select-Object -First 1
  if (-not $exe) { Fail 2 'yaze_test_unit.exe not found under the build dir' }
  # An empty --gtest_filter value matches nothing (gtest's default is "*", not
  # ""), so only pass it when --filter was actually given; a bare --gtest-args
  # run (e.g. --gtest_shuffle) then exercises the whole binary as one process,
  # which is what reproduces test-order-dependent failures.
  $testArgs = @()
  if ($job.filter) { $testArgs += "--gtest_filter=$($job.filter)" }
  if ($job.gtest_args) { $testArgs += ($job.gtest_args -split '\s+' | Where-Object { $_ }) }
  $testArgs += "--gtest_output=xml:$junit"
  Push-Location $BuildDir
  $testRc = Run $testLog $exe.FullName $testArgs
  Pop-Location
}
$times.test_s = [math]::Round($sw.Elapsed.TotalSeconds, 1)
if ($job.mode -ne 'build') {
  $tail = Select-String -Path $testLog -CaseSensitive -Pattern '% tests passed|\[  PASSED  \]|\[  FAILED  \] \d' |
    Select-Object -Last 1
  $skipped = (Select-String -Path $testLog -Pattern '\(Skipped\)\s*$').Count
  Say "tests $(if ($testRc -eq 0) { 'ok' } else { 'FAILED' }) ($($times.test_s)s): $($tail.Line) [ctest skipped: $skipped]"
}

# --- summary + housekeeping ----------------------------------------------------------
$rcFinal = if ($testRc -ne 0) { 1 } else { 0 }
$summary = [ordered]@{
  ref = $job.ref; sha = $job.sha; slug = $job.slug; preset = $job.preset; config = $job.config
  mode = $job.mode; targets = ($targets -join ' '); filter = $job.filter; jobs = $jobs; exit = $rcFinal
  ccache = $ccacheLine
}
foreach ($k in $times.Keys) { $summary[$k] = $times[$k] }
$summary.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" } | Set-Content (Join-Path $Out 'summary.txt')

# Keep the newest max_worktrees worktrees (by last use); drop older ones and old job dirs.
$stampDir = Join-Path $Root 'stamps'
Get-ChildItem $stampDir -File | Sort-Object LastWriteTime -Descending |
  Select-Object -Skip ([int]$job.max_worktrees) | ForEach-Object {
    $old = Join-Path $WtRoot $_.Name
    Say "pruning least-recently-used worktree $($_.Name)"
    & git -C $Repo worktree remove --force $old *> $null
    if (Test-Path $old) { Remove-Item -Recurse -Force $old -ErrorAction SilentlyContinue }
    Remove-Item $_.FullName -Force
  }
& git -C $Repo worktree prune *> $null
Get-ChildItem (Join-Path $Root 'jobs') -Directory |
  Where-Object { $_.FullName -ne $JobDir -and $_.LastWriteTime -lt (Get-Date).AddDays(-2) } |
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $ownerPath -Force -ErrorAction SilentlyContinue
$lock.Dispose()
exit $rcFinal
