# Release Checklist - feat/http-api-phase2 → master

**Release Coordinator**: CLAUDE_RELEASE_COORD
**Target Commit**: 662687dd8a - "fix(windows): Complete four-level OpenSSL fix - add guards to z3ed_network_client.cc"
**CI Run**: #499 - https://github.com/scawful/yaze/actions/runs/19545334652
**Status**: BLOCKED (Windows/Linux setup failing before builds start)
**Last Updated**: 2025-11-20 10:20 PST

## Critical Context
- Windows std::filesystem build has been BROKEN for 2+ weeks
- Latest fix simplifies approach: apply /std:c++latest unconditionally on Windows
- Multiple platform-specific fixes merged into feat/http-api-phase2 branch
- User demands: "we absolutely need a release soon"

## Platform Build Status

### Windows Build
- **Status**: ❌ FAILED (CI Run #19546656961 – Windows build now gets past OpenSSL but hits the SDK `ERROR` macro conflict in `http_server.cc`.)
- **Previous Failures**: std::filesystem compilation errors (runs #480-484), missing httplib OpenSSL guard in z3ed_network_client.cc (run #498), and SDK macro collision (run #500/19546656961).
- **Fix Applied**:
  - `.github/actions/setup-build` marks the sccache installer as `continue-on-error` so transient tarball issues stop blocking setup.
  - Source files `websocket_client.cc` and `z3ed_network_client.cc` explicitly `#undef CPPHTTPLIB_OPENSSL_SUPPORT` on Windows (four-level OpenSSL fix validated in run #19546656961).
  - Logging enum renamed from `LogLevel::ERROR` to `LogLevel::ERR` (and macros updated) so Windows SDK’s `#define ERROR 0` no longer breaks compilation.
- **Blocker**: Need CI run #501 to confirm Windows now builds/tests cleanly once the ERROR rename + formatting changes land in CI.
- **Owner**: CLAUDE_AIINF (Windows build) with CODEX covering the guard + logging patches.
- **Test Command**: `cmake --preset win-dbg && cmake --build build`
- **CI Job Status**: Run #19546656961 failed in `http_server.cc` before rename; rerun required after latest patches.

### Linux Build
- **Status**: ❌ FAILED (CI Run #19545334652 – Setup build environment dies during the sccache install for the same tarball issue; CI Run #19543878000 reached gRPC and hit `No space left on device` while creating `libgrpc_unsecure.a`.)
- **Previous Failures**:
  - Circular dependency resolved (commit 0812a84a22) ✅
  - FLAGS symbol conflicts in run #19528789779 ✅ (no longer reproduces after agent library refactor)
- **Known Issues**: gRPC Debug builds consume >14 GB; when CPM caches + Android images stay on disk the runner runs out of space.
- **Fix Applied**: Added a Linux-only “Reclaim disk space” step to `.github/actions/setup-build` that deletes `/usr/share/dotnet`, `/usr/local/lib/android`, `/opt/ghc`, unused tool caches, and prunes Docker layers before configuring.
- **Blocker**: Need CI run #501 to verify the cleanup frees enough space for the gRPC static libraries to link; may still need additional pruning if `_deps/grpc-build` spills beyond 14 GB.
- **Owner**: CLAUDE_LIN_BUILD (space reclamation) with CODEX assisting on workflow patch.
- **Test Command**: `cmake --preset lin-dbg && cmake --build build`
- **CI Job Status**: Setup failing in run #499 (`hendrikmuhs/ccache-action` tarball returned HTML); run #498 failed with `/usr/bin/ar: ... No space left on device` while linking `_deps/grpc-build/libgrpc_unsecure.a`.

### macOS Build
- **Status**: ✅ PASSED (CI Run #19545334652 – Jobs "Build/Test - macOS 14")
- **Previous Fixes**: z3ed linker error resolved (commit 9c562df277) ✅
- **Previous Run**: PASSED in run #19528789779 ✅
- **Known Issues**: None active
- **Blocker**: None
- **Owner**: CLAUDE_MAC_BUILD (specialist agent confirmed stable)
- **Test Command**: `cmake --preset mac-dbg && cmake --build build`
- **CI Job Status**: Passed

## HTTP API Validation

### Phase 2 Implementation Status
- **Status**: ✅ COMPLETE (validated locally on macOS)
- **Scope**: cmake/options.cmake, src/cli/cli_main.cc, src/cli/service/api/
- **Endpoints Tested**:
  - ✅ GET /api/v1/health → 200 OK
  - ✅ GET /api/v1/models → 200 OK (empty list expected)
- **CI Testing**: ⏳ PENDING (enable_http_api_tests=false for this run)
- **Documentation**: ✅ Complete (src/cli/service/api/README.md)
- **Owner**: CLAUDE_AIINF

## Test Execution Status

### Unit Tests
- **Status**: ⏳ BLOCKED (CI Run #499 – Windows/Linux jobs never reached the test phase; macOS unit tests passed)
- **Expected**: All pass (no unit test changes in this branch)

### Integration Tests
- **Status**: ⏳ BLOCKED (CI Run #499 – Windows/Linux jobs failed during setup; macOS integration suite green)
- **Expected**: All pass (platform fixes shouldn't break integration)

### E2E Tests
- **Status**: ⏳ BLOCKED (CI Run #499 – HTTP API tests disabled; need rerun after builds succeed)
- **Expected**: All pass (no UI changes)

## GO/NO-GO Decision Criteria

### GREEN LIGHT (GO) Requirements
- ✅ All 3 platforms build successfully in CI
- ✅ All test suites pass on all platforms
- ✅ No new compiler warnings introduced
- ✅ HTTP API validated on at least one platform (already done: macOS)
- ✅ No critical security issues introduced
- ✅ All coordination board blockers resolved

### RED LIGHT (NO-GO) Triggers
- ❌ Any platform build failure
- ❌ Test regression on any platform
- ❌ New critical warnings/errors
- ❌ Security vulnerabilities detected
- ❌ Unresolved blocker from coordination board

## Current Blockers

### ACTIVE BLOCKERS

**BLOCKER #1: Windows SDK `ERROR` macro conflict**
- **Status**: ⏳ FIX APPLIED – waiting on CI run #501 (CI Run #19546656961 exposed issue)
- **First Seen**: CI Run #19546656961
- **Description**: Windows SDK defines `#define ERROR 0`, which rewrote `LogLevel::ERROR` in `http_server.cc`.
- **Impact**: Windows build failed again right after the OpenSSL fix succeeded.
- **Owner**: CODEX (rename) + CLAUDE_AIINF (Windows build follow-up)
- **Resolution Plan**: Enum and macros renamed to `LogLevel::ERR`; run #501 must confirm the build now succeeds.
- **Severity**: HIGH – blocks Windows release validation until CI confirms.

**BLOCKER #2: Linux gRPC build exhausts disk**
- **Status**: ❌ REPRODUCES (CI Run #19543878000 – waiting for new run to confirm cleanup)
- **First Seen**: CI Run #19543878000
- **Description**: `_deps/grpc-build/Debug/libgrpc_unsecure.a` failed because `/usr/bin/ar` hit `No space left on device`.
- **Impact**: Linux build fails mid-way; tests never execute.
- **Owner**: CLAUDE_LIN_BUILD with CODEX supplying workflow cleanup script.
- **Resolution Plan**: Reclaim disk space before Configure/Build (delete dotnet/android/GHC/toolcache + prune Docker) – implemented in `.github/actions/setup-build`; need CI run #501 to verify.
- **Severity**: CRITICAL – blocks Linux release validation.

**BLOCKER #3: Code Quality - clang-format violations**
- **Status**: ⏳ FIX APPLIED – waiting on CI run #501
- **First Seen**: CI Run #19545334652
- **Description**: clang-format flagged `chat_tui.cc`, `unified_layout.cc`, `tui.cc`, `command_handler.h`, `conversational_agent_service.h`, `tool_dispatcher.h`, and `z3ed_network_client.cc`.
- **Impact**: Non-blocking for release but keeps Code Quality job red.
- **Owner**: CODEX
- **Resolution Plan**: Ran `clang-format` locally on all affected files (plus the logging sources updated for the rename); CI run #501 should confirm the job passes.
- **Severity**: LOW – cosmetic but must pass before merge.

### RESOLVED BLOCKERS

**✅ Windows std::filesystem compilation** - Fixed in commit 43118254e6
**✅ Linux circular dependency** - Fixed in commit 0812a84a22
**✅ macOS z3ed linker error** - Fixed in commit 9c562df277

## Release Merge Plan

### When GREEN LIGHT Achieved:
1. **Verify CI run #501 passes all jobs**
2. **Run smoke build verification**: `scripts/agents/smoke-build.sh {preset} {target}` on all platforms
3. **Update coordination board** with final status
4. **Create merge commit**: `git checkout develop && git merge feat/http-api-phase2 --no-ff`
5. **Run final test suite**: `scripts/agents/run-tests.sh {preset}`
6. **Merge to master**: `git checkout master && git merge develop --no-ff`
7. **Tag release**: `git tag -a v0.x.x -m "Release v0.x.x - Windows std::filesystem fix + HTTP API Phase 2"`
8. **Push with tags**: `git push origin master develop --tags`
9. **Trigger release workflow**: CI will automatically build release artifacts

### If RED LIGHT (Failure):
1. **Identify failing job** in CI run #501
2. **Assign to specialized agent**:
   - Windows failures → CLAUDE_AIINF (Windows Build Specialist)
   - Linux failures → CLAUDE_AIINF (Linux Build Specialist)
   - macOS failures → CLAUDE_AIINF (macOS Build Specialist)
   - Test failures → CLAUDE_CORE (Test Specialist)
3. **Create emergency fix** on feat/http-api-phase2 branch
4. **Trigger new CI run** and update this checklist
5. **Repeat until GREEN LIGHT**

## Monitoring Protocol

**CLAUDE_RELEASE_COORD will check CI status every 5 minutes and update coordination board with:**
- Platform build progress (queued/in_progress/success/failure)
- Test execution status
- Any new blockers discovered
- ETA to GREEN LIGHT decision

## Next Steps

1. ⏳ Trigger CI run #501 (feat/http-api-phase2 with ERROR rename + formatting) – https://github.com/scawful/yaze/actions/runs/19546656961 is the previous run.
   - Use `scripts/agents/cancel-ci-runs.sh feat/http-api-phase2` to cancel any queued/in-progress runs first so the new run starts immediately.
2. ⏳ Verify Windows build/test succeed in run #501 now that `LogLevel::ERR` replaces the macro-conflicting value.
3. ⏳ Verify Linux build/test succeed with the new disk-cleanup step (no more “No space left on device” while linking `_deps/grpc-build`).
4. ⏳ Ensure macOS build/test remain green.
5. ⏳ Confirm Code Quality job passes (clang-format already applied locally).
6. ⏳ Make GO/NO-GO decision and execute merge plan if run #501 is green.

---

**Coordination Board**: `docs/internal/agents/coordination-board.md`
**Build Reference**: `docs/public/build/quick-reference.md`
**HTTP API Docs**: `src/cli/service/api/README.md`
