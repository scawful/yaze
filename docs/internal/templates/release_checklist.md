# Release Checklist Template

**Release Version**: vX.Y.Z
**Release Coordinator**: [Agent/Developer Name]
**Target Branch**: develop → master
**Target Date**: YYYY-MM-DD
**Status**: PLANNING | IN_PROGRESS | READY | RELEASED
**Last Updated**: YYYY-MM-DD

---

## Pre-Release Testing Requirements

### 1. Platform Build Validation

All platforms must build successfully with zero errors and minimal warnings.

#### Windows Build

- [ ] **Debug build passes**: `cmake --preset win-dbg && cmake --build build`
- [ ] **Release build passes**: `cmake --preset win-rel && cmake --build build`
- [ ] **AI build passes**: `cmake --preset win-ai && cmake --build build --target z3ed`
- [ ] **No new warnings**: Compare warning count to previous release
- [ ] **Smoke test**: `pwsh -File scripts/agents/windows-smoke-build.ps1 -Preset win-rel -Target yaze`
- [ ] **Blocker Status**: NONE | [Description if blocked]

#### Linux Build

- [ ] **Debug build passes**: `cmake --preset lin-dbg && cmake --build build`
- [ ] **Release build passes**: `cmake --preset lin-rel && cmake --build build`
- [ ] **AI build passes**: `cmake --preset lin-ai && cmake --build build --target z3ed`
- [ ] **No new warnings**: Compare warning count to previous release
- [ ] **Smoke test**: `scripts/agents/smoke-build.sh lin-rel yaze`
- [ ] **Blocker Status**: NONE | [Description if blocked]

#### macOS Build

- [ ] **Debug build passes**: `cmake --preset mac-dbg && cmake --build build`
- [ ] **Release build passes**: `cmake --preset mac-rel && cmake --build build`
- [ ] **AI build passes**: `cmake --preset mac-ai && cmake --build build --target z3ed`
- [ ] **Universal binary passes**: `cmake --preset mac-uni && cmake --build build`
- [ ] **No new warnings**: Compare warning count to previous release
- [ ] **Smoke test**: `scripts/agents/smoke-build.sh mac-rel yaze`
- [ ] **Blocker Status**: NONE | [Description if blocked]

### 2. Test Suite Validation

All test suites must pass on all platforms.

#### Unit Tests

- [ ] **Windows**: `./build/bin/yaze_test --unit` (100% pass)
- [ ] **Linux**: `./build/bin/yaze_test --unit` (100% pass)
- [ ] **macOS**: `./build/bin/yaze_test --unit` (100% pass)
- [ ] **Zero regressions**: No new test failures vs previous release
- [ ] **Coverage maintained**: >80% coverage for critical paths

#### Integration Tests

- [ ] **Windows**: `./build/bin/yaze_test --integration` (100% pass)
- [ ] **Linux**: `./build/bin/yaze_test --integration` (100% pass)
- [ ] **macOS**: `./build/bin/yaze_test --integration` (100% pass)
- [ ] **ROM-dependent tests**: All pass with reference ROM
- [ ] **Zero regressions**: No new test failures vs previous release

#### E2E Tests

- [ ] **Windows**: `./build/bin/yaze_test --e2e` (100% pass)
- [ ] **Linux**: `./build/bin/yaze_test --e2e` (100% pass)
- [ ] **macOS**: `./build/bin/yaze_test --e2e` (100% pass)
- [ ] **GUI workflows validated**: Editor smoke tests pass
- [ ] **Zero regressions**: No new test failures vs previous release

#### Performance Benchmarks

- [ ] **Graphics benchmarks**: No >10% regression vs previous release
- [ ] **Load time benchmarks**: ROM loading <3s on reference hardware
- [ ] **Memory benchmarks**: No memory leaks detected
- [ ] **Profile results**: No new performance hotspots

### 3. CI/CD Validation

All CI jobs must pass successfully.

- [ ] **Build job (Linux)**: ✅ SUCCESS
- [ ] **Build job (macOS)**: ✅ SUCCESS
- [ ] **Build job (Windows)**: ✅ SUCCESS
- [ ] **Test job (Linux)**: ✅ SUCCESS
- [ ] **Test job (macOS)**: ✅ SUCCESS
- [ ] **Test job (Windows)**: ✅ SUCCESS
- [ ] **Code Quality job**: ✅ SUCCESS (clang-format, cppcheck, clang-tidy)
- [ ] **z3ed Agent job**: ✅ SUCCESS (optional, if AI features included)
- [ ] **Security scan**: ✅ PASS (no critical vulnerabilities)

**CI Run URL**: [Insert GitHub Actions URL]

### 4. Code Quality Checks

- [ ] **clang-format**: All code formatted correctly
- [ ] **clang-tidy**: No critical issues
- [ ] **cppcheck**: No new warnings
- [ ] **No dead code**: Unused code removed
- [ ] **No TODOs in critical paths**: All critical TODOs resolved
- [ ] **Copyright headers**: All files have correct headers
- [ ] **License compliance**: All dependencies have compatible licenses

### 5. Symbol Conflict Verification

- [ ] **No duplicate symbols**: `scripts/check-symbols.sh` passes (if available)
- [ ] **No ODR violations**: All targets link cleanly
- [ ] **Flag definitions unique**: No FLAGS_* conflicts
- [ ] **Library boundaries clean**: No unintended cross-dependencies

### 6. Configuration Matrix Coverage

Test critical preset combinations:

- [ ] **Minimal build**: `cmake --preset minimal` (no gRPC, no AI)
- [ ] **Dev build**: `cmake --preset dev` (all features, ROM tests)
- [ ] **CI build**: `cmake --preset ci-*` (matches CI environment)
- [ ] **Release build**: `cmake --preset *-rel` (optimized, no tests)
- [ ] **AI build**: `cmake --preset *-ai` (gRPC + AI runtime)

### 7. Feature-Specific Validation

#### GUI Application (yaze)

- [ ] **Launches successfully**: No crashes on startup
- [ ] **ROM loading works**: Can load reference ROM
- [ ] **Editors functional**: All editors (Overworld, Dungeon, Graphics, etc.) open
- [ ] **Saving works**: ROM modifications persist correctly
- [ ] **No memory leaks**: Valgrind/sanitizer clean (Linux/macOS)
- [ ] **UI responsive**: No freezes or lag during normal operation

#### CLI Tool (z3ed)

- [ ] **Launches successfully**: `z3ed --help` works
- [ ] **Basic commands work**: `z3ed rom info zelda3.sfc`
- [ ] **AI features work**: `z3ed agent simple-chat` (if enabled)
- [ ] **HTTP API works**: `z3ed --http-port=8080` serves endpoints (if enabled)
- [ ] **TUI functional**: Terminal UI renders correctly

#### Asar Integration

- [ ] **Patch application works**: Can apply .asm patches
- [ ] **Symbol extraction works**: Symbols loaded from ROM
- [ ] **Error reporting clear**: Patch errors show helpful messages

#### ZSCustomOverworld Support

- [ ] **v3 detection works**: Correctly identifies ZSCustomOverworld ROMs
- [ ] **Upgrade path works**: Can upgrade from v2 to v3
- [ ] **Extended features work**: Multi-area maps, custom sizes

### 8. Documentation Validation

- [ ] **README.md up to date**: Reflects current version and features
- [ ] **CHANGELOG.md updated**: All changes since last release documented
- [ ] **Build docs accurate**: Instructions work on all platforms
- [ ] **API docs current**: Doxygen builds without errors
- [ ] **User guides updated**: New features documented
- [ ] **Migration guide**: Breaking changes documented (if any)
- [ ] **Release notes drafted**: User-facing summary of changes

### 9. Dependency and License Checks

- [ ] **Dependencies up to date**: No known security vulnerabilities
- [ ] **License files current**: All dependencies listed in LICENSES.txt
- [ ] **Third-party notices**: THIRD_PARTY_NOTICES.md updated
- [ ] **Submodules pinned**: All submodules at stable commits
- [ ] **vcpkg versions locked**: CMake dependency versions specified

### 10. Backward Compatibility

- [ ] **ROM format compatible**: Existing ROMs load correctly
- [ ] **Save format compatible**: Old saves work in new version
- [ ] **Config file compatible**: Settings from previous version preserved
- [ ] **Plugin API stable**: External plugins still work (if applicable)
- [ ] **Breaking changes documented**: Migration path clear

---

## Release Process

### Pre-Release

1. **Branch Preparation**
   - [ ] All features merged to `develop` branch
   - [ ] All tests passing on `develop`
   - [ ] Version number updated in:
     - [ ] `VERSION` (source of truth for builds)
     - [ ] `README.md` (current release)
     - [ ] `docs/public/reference/changelog.md` (new entry)
     - [ ] `docs/public/release-notes.md` (new section)
     - [ ] `docs/public/release/README.md` (release header)
   - [ ] CHANGELOG.md updated with release notes
   - [ ] Documentation updated

2. **Final Testing**
   - [ ] Run full test suite on all platforms
   - [ ] Run smoke builds on all platforms
   - [ ] Verify CI passes on `develop` branch
   - [ ] Manual testing of critical workflows
   - [ ] Performance regression check

3. **Code Freeze**
   - [ ] Announce code freeze on coordination board
   - [ ] No new features merged
   - [ ] Only critical bug fixes allowed
   - [ ] Final commit message: "chore: prepare for vX.Y.Z release"

### Release

4. **Merge to Master**
   - [ ] Create merge commit: `git checkout master && git merge develop --no-ff`
   - [ ] Tag release: `git tag -a vX.Y.Z -m "Release vX.Y.Z - [Brief Description]"`
   - [ ] Push to remote: `git push origin master develop --tags`

5. **Build Release Artifacts**
   - [ ] Trigger release workflow: `.github/workflows/release.yml`
   - [ ] Verify Windows binary builds
   - [ ] Verify macOS binary builds (x64 + ARM64)
   - [ ] Verify Linux binary builds
   - [ ] Verify all artifacts uploaded to GitHub Release

6. **Create GitHub Release**
   - [ ] Go to https://github.com/scawful/yaze/releases/new
   - [ ] Select tag `vX.Y.Z`
   - [ ] Title: "yaze vX.Y.Z - [Brief Description]"
   - [ ] Description: Copy from CHANGELOG.md + add highlights
   - [ ] Attach binaries (if not auto-uploaded)
   - [ ] Mark as "Latest Release"
   - [ ] Publish release

### Post-Release

7. **Verification**
   - [ ] Download binaries from GitHub Release
   - [ ] Test Windows binary on clean machine
   - [ ] Test macOS binary on clean machine (both Intel and ARM)
   - [ ] Test Linux binary on clean machine
   - [ ] Verify all download links work

8. **Announcement**
   - [ ] Update project website (if applicable)
   - [ ] Post announcement in GitHub Discussions
   - [ ] Update social media (if applicable)
   - [ ] Notify contributors
   - [ ] Update coordination board with release completion

9. **Cleanup**
   - [ ] Archive release branch (if used)
   - [ ] Close completed milestones
   - [ ] Update project board
   - [ ] Plan next release cycle

---

## GO/NO-GO Decision Criteria

### ✅ GREEN LIGHT (READY TO RELEASE)

**All of the following must be true**:
- ✅ All platform builds pass (Windows, Linux, macOS)
- ✅ All test suites pass on all platforms (unit, integration, e2e)
- ✅ CI/CD pipeline fully green
- ✅ No critical bugs open
- ✅ No unresolved blockers
- ✅ Documentation complete and accurate
- ✅ Release artifacts build successfully
- ✅ Manual testing confirms functionality
- ✅ Release coordinator approval

### ❌ RED LIGHT (NOT READY)

**Any of the following triggers a NO-GO**:
- ❌ Platform build failure
- ❌ Test suite regression
- ❌ Critical bug discovered
- ❌ Security vulnerability found
- ❌ Unresolved blocker
- ❌ CI/CD pipeline failure
- ❌ Documentation incomplete
- ❌ Release artifacts fail to build
- ❌ Manual testing reveals issues

---

## Rollback Plan

If critical issues are discovered post-release:

1. **Immediate**: Unlist GitHub Release (mark as pre-release)
2. **Assess**: Determine severity and impact
3. **Fix**: Create hotfix branch from `master`
4. **Test**: Validate fix with full test suite
5. **Release**: Tag hotfix as vX.Y.Z+1 and release
6. **Document**: Update CHANGELOG with hotfix notes

---

## Blockers and Issues

### Active Blockers

| Blocker | Severity | Description | Owner | Status | ETA |
|---------|----------|-------------|-------|--------|-----|
| [Add blockers as discovered] | | | | | |

### Resolved Issues

| Issue | Resolution | Date |
|-------|------------|------|
| [Add resolved issues] | | |

---

## Platform-Specific Notes

### Windows

- **Compiler**: MSVC 2022 (Visual Studio 17)
- **Generator**: Ninja Multi-Config
- **Known Issues**: [List any Windows-specific considerations]
- **Verification**: Test on Windows 10 and Windows 11

### Linux

- **Compiler**: GCC 12+ or Clang 16+
- **Distros**: Ubuntu 22.04, Fedora 38+ (primary targets)
- **Known Issues**: [List any Linux-specific considerations]
- **Verification**: Test on Ubuntu 22.04 LTS

### macOS

- **Compiler**: Apple Clang 15+
- **Architectures**: x86_64 (Intel) and arm64 (Apple Silicon)
- **macOS Versions**: macOS 12+ (Monterey and later)
- **Known Issues**: [List any macOS-specific considerations]
- **Verification**: Test on both Intel and Apple Silicon Macs

---

## References

- **Testing Infrastructure**: [docs/internal/testing/README.md](testing/README.md)
- **Build Quick Reference**: [docs/public/build/quick-reference.md](../public/build/quick-reference.md)
- **Testing Quick Start**: [docs/public/developer/testing-quick-start.md](../public/developer/testing-quick-start.md)
- **Coordination Board**: [docs/internal/agents/coordination-board.md](agents/coordination-board.md)
- **CI/CD Pipeline**: [.github/workflows/ci.yml](../../.github/workflows/ci.yml)
- **Release Workflow**: [.github/workflows/release.yml](../../.github/workflows/release.yml)

---

**Last Review**: YYYY-MM-DD
**Next Review**: YYYY-MM-DD (after release)
