# Run with PowerShell 7. Only the production report function is evaluated.
param(
  [string]$RunnerPath = (Join-Path $PSScriptRoot '..\mm-build.ps1'),
  [string]$FixtureRoot = (Join-Path ([IO.Path]::GetTempPath()) ("yaze-mm-retention-" + [guid]::NewGuid().ToString('N')))
)

$ErrorActionPreference = 'Stop'
$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
  [IO.Path]::GetFullPath($RunnerPath), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) { throw "Runner parse failed: $parseErrors" }
$functions = @($ast.FindAll({
  param($node)
  $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Write-RetentionReport'
}, $true))
if ($functions.Count -ne 1) { throw 'Expected exactly one Write-RetentionReport function' }
# Do not dot-source the runner: its main body syncs sources and starts builds.
. ([scriptblock]::Create($functions[0].Extent.Text))

function Assert-True([bool]$Condition, [string]$Message) {
  if (-not $Condition) { throw $Message }
}

function Write-FixtureFile([string]$Path, [string]$Content) {
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
  [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
}

function Get-Snapshot([string]$Path) {
  $items = @(Get-Item -LiteralPath $Path -Force) +
    @(Get-ChildItem -LiteralPath $Path -Force -Recurse)
  $records = @($items | Sort-Object FullName | ForEach-Object {
    $record = [ordered]@{
      path = [IO.Path]::GetRelativePath($Path, $_.FullName)
      directory = $_.PSIsContainer
      attributes = [int]$_.Attributes
      created = $_.CreationTimeUtc.Ticks
      modified = $_.LastWriteTimeUtc.Ticks
    }
    if (-not $_.PSIsContainer) {
      $record.bytes = [Convert]::ToBase64String([IO.File]::ReadAllBytes($_.FullName))
    }
    [pscustomobject]$record
  })
  # Access times are excluded: reading fixture bytes can update them itself.
  return (ConvertTo-Json -InputObject $records -Depth 5 -Compress)
}

function Get-StableSnapshot([string]$Path) {
  # NTFS can publish directory creation timestamps after fixture writes return.
  # Settle only the baseline; never hide a change after the report executes.
  $previous = Get-Snapshot $Path
  for ($attempt = 0; $attempt -lt 20; $attempt++) {
    Start-Sleep -Milliseconds 50
    $current = Get-Snapshot $Path
    if ($previous -ceq $current) { return $current }
    $previous = $current
  }
  throw "Fixture metadata did not stabilize before report: $Path"
}

function New-Fixture([string]$CasePath) {
  $root = Join-Path $CasePath 'remote'
  $wtRoot = Join-Path $CasePath 'worktrees'
  $jobDir = Join-Path $root 'jobs\current'
  foreach ($path in @((Join-Path $root 'stamps'), $wtRoot, $jobDir)) {
    [IO.Directory]::CreateDirectory($path) | Out-Null
  }
  return [pscustomobject]@{
    CasePath = $CasePath; Root = $root; WtRoot = $wtRoot; JobDir = $jobDir
    Now = [datetime]::Parse('2026-01-10T12:00:00Z').ToUniversalTime()
  }
}

function Add-Worktree($Fixture, [string]$Name, [int]$DaysOld) {
  $path = Join-Path $Fixture.WtRoot $Name
  Write-FixtureFile (Join-Path $path '.git') 'gitdir: synthetic-only-no-repository'
  Write-FixtureFile (Join-Path $path '.gitignore') "ignored.sfc`nbuild/`n"
  Write-FixtureFile (Join-Path $path 'ignored.sfc') 'synthetic sentinel, not ROM data'
  Write-FixtureFile (Join-Path $path 'source.cc') 'preserve untracked source sentinel'
  Write-FixtureFile (Join-Path $path 'build\cache.txt') 'preserve pending build sentinel'
  $stamp = Join-Path $Fixture.Root "stamps\$Name"
  Write-FixtureFile $stamp ('a' * 40)
  (Get-Item -LiteralPath $stamp).LastWriteTimeUtc = $Fixture.Now.AddDays(-$DaysOld)
  return $path
}

function Add-Job($Fixture, [string]$Name, [int]$DaysOld) {
  $path = Join-Path $Fixture.Root "jobs\$Name"
  Write-FixtureFile (Join-Path $path 'testhome\appdata\settings.json') '{"synthetic":true}'
  Write-FixtureFile (Join-Path $path 'testhome\documents\saved.sfc') 'synthetic saved-file sentinel'
  Write-FixtureFile (Join-Path $path 'out\test.log') 'preserve failure evidence'
  Write-FixtureFile (Join-Path $path 'job.txt') 'synthetic job metadata'
  (Get-Item -LiteralPath $path).LastWriteTimeUtc = $Fixture.Now.AddDays(-$DaysOld)
  return $path
}

function Invoke-PreservedReport($Fixture, [int]$Limit) {
  $before = Get-StableSnapshot $Fixture.CasePath
  $report = @(Write-RetentionReport -Root $Fixture.Root -WtRoot $Fixture.WtRoot `
    -JobDir $Fixture.JobDir -MaxWorktrees $Limit -Now $Fixture.Now)
  $after = Get-Snapshot $Fixture.CasePath
  if ($before -cne $after) {
    $oldRecords = @{}
    $newRecords = @{}
    foreach ($record in (ConvertFrom-Json -InputObject $before)) { $oldRecords[$record.path] = $record }
    foreach ($record in (ConvertFrom-Json -InputObject $after)) { $newRecords[$record.path] = $record }
    $differences = 0
    foreach ($path in (@($oldRecords.Keys) + @($newRecords.Keys) | Sort-Object -Unique)) {
      if (-not $oldRecords.ContainsKey($path) -or -not $newRecords.ContainsKey($path)) {
        [Console]::Out.WriteLine("SNAPSHOT path changed: $path (before=$($oldRecords.ContainsKey($path)), after=$($newRecords.ContainsKey($path)))")
        $differences++
      } else {
        foreach ($field in $oldRecords[$path].PSObject.Properties.Name) {
          if ($oldRecords[$path].$field -cne $newRecords[$path].$field) {
            [Console]::Out.WriteLine("SNAPSHOT field changed: $path / $field (before=$($oldRecords[$path].$field), after=$($newRecords[$path].$field))")
            $differences++
          }
        }
      }
      if ($differences -ge 8) { break }
    }
    # Synthetic snapshots are small; retain exact evidence in the caller's log
    # even after the owned fixture tree is removed.
    [Console]::Out.WriteLine("SNAPSHOT before: $before")
    [Console]::Out.WriteLine("SNAPSHOT after: $after")
    throw 'Report changed fixture paths, bytes, attributes or timestamps'
  }
  Assert-True ($report -contains 'retention: report only; worktrees, stamps and job evidence are preserved') `
    'Report did not state the preservation contract'
  return $report
}

$cases = [ordered]@{
  'over-limit worktrees and stamps retain every byte and metadata' = {
    param($f)
    $old = Add-Worktree $f 'old-held' 8
    $new = Add-Worktree $f 'new-held' 1
    $report = @(Invoke-PreservedReport $f 1)
    Assert-True ($report -contains "retention review worktree: $old") 'Missing oldest worktree review candidate'
    Assert-True ($report -notcontains "retention review worktree: $new") 'Newest worktree incorrectly selected'
  }
  'stale jobs preserve testhome and logs while current job is excluded' = {
    param($f)
    $old = Add-Job $f 'old-held' 4
    $current = Add-Job $f 'current' 5
    $recent = Add-Job $f 'recent' 1
    $report = @(Invoke-PreservedReport $f 8)
    Assert-True ($report -contains "retention review job: $old") 'Missing stale job review candidate'
    Assert-True ($report -notcontains "retention review job: $current") 'Current job incorrectly selected'
    Assert-True ($report -notcontains "retention review job: $recent") 'Recent job incorrectly selected'
  }
  'zero threshold reports all stamped worktrees without removal' = {
    param($f)
    $one = Add-Worktree $f 'first' 4
    $two = Add-Worktree $f 'second' 1
    $report = @(Invoke-PreservedReport $f 0)
    Assert-True ($report -contains "retention review worktree: $one") 'Zero threshold missed first worktree'
    Assert-True ($report -contains "retention review worktree: $two") 'Zero threshold missed second worktree'
  }
  'below threshold leaves worktrees out of the review list' = {
    param($f)
    Add-Worktree $f 'only' 8 | Out-Null
    $report = @(Invoke-PreservedReport $f 8)
    Assert-True (@($report | Where-Object { $_ -like 'retention review worktree: *' }).Count -eq 0) `
      'Below-threshold worktree incorrectly selected'
  }
  'missing scan directories are not created or treated as a failure' = {
    param($f)
    $f.Root = Join-Path $f.CasePath 'absent remote'
    $f.WtRoot = Join-Path $f.CasePath 'absent worktrees'
    $f.JobDir = Join-Path $f.Root 'jobs\current'
    $report = @(Invoke-PreservedReport $f 0)
    Assert-True (@($report | Where-Object { $_ -match '^retention review (worktree|job): ' }).Count -eq 0) `
      'Missing directories produced an existing review candidate'
  }
  'missing worktree retains its stamp for later review' = {
    param($f)
    $stamp = Join-Path $f.Root 'stamps\missing-held'
    Write-FixtureFile $stamp 'preserve orphan stamp evidence'
    Invoke-PreservedReport $f 0 | Out-Null
    Assert-True (Test-Path -LiteralPath $stamp -PathType Leaf) 'Missing worktree lost its stamp'
  }
  'literal paths and canonical current-job aliases are preserved' = {
    param($f)
    $old = Add-Worktree $f 'held' 7
    Add-Job $f 'current' 7 | Out-Null
    $f.JobDir = (Join-Path $f.JobDir '.').ToUpperInvariant() + [IO.Path]::DirectorySeparatorChar
    $report = @(Invoke-PreservedReport $f 0)
    Assert-True ($report -contains "retention review worktree: $old") 'Literal fixture path was not scanned'
    Assert-True (@($report | Where-Object { $_ -like 'retention review job: *' }).Count -eq 0) `
      'Canonical current-job alias incorrectly selected'
  }
}

$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
if (Test-Path -LiteralPath $FixtureRoot) { throw "FixtureRoot must not exist: $FixtureRoot" }
[IO.Directory]::CreateDirectory($FixtureRoot) | Out-Null
$marker = Join-Path $FixtureRoot '.synthetic-retention-fixture'
$markerValue = [guid]::NewGuid().ToString('N')
Write-FixtureFile $marker $markerValue
$passed = 0
$failed = 0
try {
  foreach ($case in $cases.GetEnumerator()) {
    # Keep ordinary cases free of wildcards so the destructive baseline reaches
    # their sentinels. One separate case exercises literal-path handling.
    $name = if ($case.Key -like 'literal paths*') { 'case [literal]' } else { "case-$($passed + $failed)" }
    $casePath = Join-Path $FixtureRoot $name
    $fixture = New-Fixture $casePath
    try {
      & $case.Value $fixture
      $passed++
      Write-Output "PASS: $($case.Key)"
    } catch {
      $failed++
      Write-Output "FAIL: $($case.Key): $($_.Exception.Message)"
    }
  }
} finally {
  # Only this absent-on-entry, marked, synthetic tree is eligible for cleanup.
  if ((Test-Path -LiteralPath $marker -PathType Leaf) -and
      [IO.File]::ReadAllText($marker) -ceq $markerValue) {
    Remove-Item -LiteralPath $FixtureRoot -Recurse -Force
  } else {
    throw "Fixture cleanup refused: ownership marker changed at $FixtureRoot"
  }
}
Write-Output "selected=$($cases.Count) passed=$passed failed=$failed skipped=0"
if ($failed -ne 0 -or $passed -ne $cases.Count) { exit 1 }
exit 0
