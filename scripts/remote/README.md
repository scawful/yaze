# Remote Windows build box (`mm-test.sh`)

`scripts/remote/mm-test.sh` builds a yaze branch or commit on a Windows PC over
SSH and runs its tests there, so agents do not compile on a laptop. It mirrors
the CI job "Test - Windows 2022 (Core)": preset `ci-windows`, config
`RelWithDebInfo`, clang-cl from the Visual Studio 2022 x64 developer
environment, Ninja Multi-Config, gRPC built from source through CPM. Two
differences from CI: `-DOPENSSL_NO_ASM=ON` (BoringSSL would need NASM, which CI
installs with choco) and `/Z7` debug info (see Caches).

```bash
scripts/remote/mm-test.sh <branch|commit>                        # ctest label "unit"
scripts/remote/mm-test.sh <branch> --filter 'DungeonRoom*'       # yaze_test_unit --gtest_filter
scripts/remote/mm-test.sh <branch> \
  --gtest-args '--gtest_shuffle --gtest_random_seed=1'           # whole binary, one process, shuffled
scripts/remote/mm-test.sh <branch> --full                        # ctest label "stable" (unit + integration)
scripts/remote/mm-test.sh <branch> --target z3ed                 # build only
scripts/remote/mm-test.sh <branch> --clean --no-ccache           # cold rebuild (timings)
scripts/remote/mm-test.sh --help
```

Run it from any yaze checkout on the Mac. The branch can be local-only: the
script asks the PC which commits it already has and sends the rest as a
`git bundle`. Results land in `build/remote-mm/<slug>/` (`summary.txt`,
`build.log`, `test.log`, `junit.xml`, `git.log`); the script prints a short
pass/fail summary with failing test names.

Two boxes are configured: `mm` (16 cores) and `eva-02` (8 cores). Each has
its own lock, clone, caches and worktrees, so two jobs run in parallel. The
script prints which box it picked; the result paths are the same either way.

Exit codes: `0` pass, `1` test failures (or every test skipped), `2` configure
or build failure, `3` setup/usage/SSH error or build-lock timeout. A cold first build of a new branch takes several
minutes; run it in the background rather than inside a short tool timeout.

## How it works

1. `ssh` lists the PC's refs; the Mac bundles only the missing commits.
2. `scp` uploads `mm-build.ps1`, `job.txt` and the bundle to
   `%USERPROFILE%\src\yaze-remote\jobs\<id>\`.
3. `mm-build.ps1` (PowerShell 7) takes the build lock, fetches the bundle into
   `%USERPROFILE%\src\yaze` as `refs/mm/<slug>`, checks out a clean worktree at
   `%USERPROFILE%\src\yw\<slug>` (one per branch, reused, so rebuilds are
   incremental), updates submodules from local mirrors, then configures (only
   when the build dir is new), builds, and runs the tests.
4. `scp` copies `out\` back; the Mac parses `junit.xml`.

Details:

- **Lock.** `yaze-remote\build.lock` is held with an exclusive file handle for
  the whole sync/build/test step; Windows releases it when the process exits.
  A second caller prints `waiting for build lock (held by: ...)` and waits up
  to `MM_LOCK_WAIT_MIN` minutes (default 120).
- **Caches.** ccache (`yaze-remote\ccache`, base dir `%USERPROFILE%\src\yw`,
  `hash_dir` off) is shared by all worktrees, and CPM sources (gRPC etc.) are
  shared in `yaze-remote\cpm`, so a new branch recompiles mostly from cache.
  The CPM cache stays outside base_dir: relativized CPM include paths overflow
  MAX_PATH in ninja's `/showIncludes` handling. Only third-party objects are
  cacheable: CMake passes `-clang:-std=c++23` to clang-cl for C++23, and ccache
  rejects that option, so yaze's own sources always compile (a new branch
  takes a few minutes; an edit in an existing worktree takes seconds). RelWithDebInfo
  uses `/Z7` (`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`) because ccache
  cannot cache `/Zi`.
- **Load.** The build runs at BelowNormal priority. Parallelism defaults to the
  core count capped by free RAM (about 1.2 GB per job); override with `--jobs`.
- **Test isolation.** Tests run with `YAZE_USER_DOCUMENTS_DIR`,
  `YAZE_APP_DATA_DIR`, `TEMP` and `TMP` pointing into the job dir.
  `test/yaze_test.cc` sets the first two with `SDL_setenv`, which on Windows
  calls `SetEnvironmentVariableA` and does not reach `std::getenv`; without the
  runner's values the harness resolves the PC user's real
  `Documents\Yaze\settings.json`, creates that folder, and skips every test.
  All test processes of one run share these dirs (the harness would give each
  process its own), so tests that write settings can interfere under `ctest -j`.
- **Cleanup.** The newest `MM_MAX_WORKTREES` worktrees (default 8, by last
  use) are kept; older ones and job dirs older than two days are removed.
  `--clean` deletes one branch's build dir before building.

## Configuration

| Variable | Default | Meaning |
|----------|---------|---------|
| `MM_HOST` | unset | SSH alias of one PC to use (key auth, default shell `cmd`); disables fallback |
| `MM_HOSTS` | `mm` | Boxes tried in order when `MM_HOST` is unset: the first ready box with a free build lock runs the job; if all are busy, it waits on the first reachable one |
| `MM_REMOTE_ROOT` | `src/yaze-remote` | Runner dir, relative to the remote home |
| `MM_MAX_WORKTREES` | `8` | Worktrees kept on the PC |
| `MM_LOCK_WAIT_MIN` | `120` | Minutes to wait for the build lock |

The PC-side clone (`%USERPROFILE%\src\yaze`) and worktree root
(`%USERPROFILE%\src\yw`) are fixed in `mm-build.ps1`.

## One-time setup on the PC (user scope, no admin)

Needs: Git for Windows, CMake, PowerShell 7 (`pwsh`), Visual Studio 2022 with
the C++ x64 toolset and "C++ Clang tools for Windows" (clang-cl). Then put
portable tools under `%USERPROFILE%\tools`:

| Tool | Source | Install to |
|------|--------|------------|
| Ninja 1.13.2 | `https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip` | `tools\ninja\ninja.exe` |
| ccache 4.14.1 | `https://github.com/ccache/ccache/releases/download/v4.14.1/ccache-4.14.1-windows-x86_64.zip` | `tools\ccache\ccache.exe` |

The yaze clone is created on first use from the Mac checkout's `origin` URL
(HTTPS only). Submodule mirrors (`yaze-remote\mods`) are created on first use.

## Limits

- Windows only exercises the Windows build. macOS app, iOS and Mesen work
  still need the Mac.
- `--filter` accepts `[A-Za-z0-9_*?:./-]` only (it crosses `cmd.exe`).
- `--gtest-args` accepts `[A-Za-z0-9_=,.:/ -]` only, is split on whitespace and
  appended verbatim to a direct `yaze_test_unit` invocation. It implies filter
  mode; without `--filter` too, no `--gtest_filter` is passed, so gtest's own
  default (`*`) runs every test in the binary as a single process — needed to
  reproduce a failure that only shows up when other tests share the process
  (e.g. `--gtest_shuffle --gtest_random_seed=N`), since ctest's per-fixture
  shards never do that.
- Worktree slugs come from the branch name; `rev-<sha>` for a bare commit.
