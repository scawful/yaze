# Matrix Testing Strategy

**Owner**: CLAUDE_MATRIX_TEST (Platform Matrix Testing Specialist)
**Last Updated**: 2025-11-20
**Status**: ACTIVE

## Executive Summary

This document defines the strategy for comprehensive platform/configuration matrix testing to catch issues across CMake flag combinations, platforms, and build configurations.

**Key Goals**:
- Catch cross-configuration issues before they reach production
- Prevent "works on my machine" problems
- Document problematic flag combinations
- Make matrix testing accessible to developers locally
- Minimize CI time while maximizing coverage

**Quick Links**:
- Configuration reference: `/docs/internal/configuration-matrix.md`
- GitHub Actions workflow: `/.github/workflows/matrix-test.yml`
- Local test script: `/scripts/test-config-matrix.sh`

## 1. Problem Statement

### Current Gaps

Before this initiative, yaze only tested:
1. **Default configurations**: `ci-linux`, `ci-macos`, `ci-windows` presets
2. **Single feature toggles**: One dimension at a time
3. **No interaction testing**: Missing edge cases like "GRPC=ON but REMOTE_AUTOMATION=OFF"

### Real Bugs Caught by Matrix Testing

Examples of issues a configuration matrix would catch:

**Example 1: GRPC Without Automation**
```cmake
# Broken: User enables gRPC but disables remote automation
cmake -B build -DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_REMOTE_AUTOMATION=OFF
# Result: gRPC headers included but server code never compiled → link errors
```

**Example 2: HTTP API Without CLI Stack**
```cmake
# Broken: User wants HTTP API but disables agent CLI
cmake -B build -DYAZE_ENABLE_HTTP_API=ON -DYAZE_ENABLE_AGENT_CLI=OFF
# Result: REST endpoints defined but no command dispatcher → runtime errors
```

**Example 3: AI Runtime Without JSON**
```cmake
# Broken: User enables AI with Gemini but disables JSON
cmake -B build -DYAZE_ENABLE_AI_RUNTIME=ON -DYAZE_ENABLE_JSON=OFF
# Result: Gemini parser requires JSON but it's not available → compile errors
```

**Example 4: Windows GRPC Version Mismatch**
```cmake
# Broken on Windows: gRPC version incompatible with MSVC ABI
cmake -B build (with gRPC <1.67.1)
# Result: Symbol errors, linker failures on Visual Studio
```

## 2. Matrix Testing Approach

### Strategy: Smart, Not Exhaustive

Instead of testing all 2^18 = 262,144 combinations:

1. **Baseline**: Default configuration (most common user scenario)
2. **Extremes**: All ON, All OFF (catch hidden assumptions)
3. **Interactions**: Known problematic combinations
4. **Tiers**: Progressive validation by feature complexity
5. **Platforms**: Run critical tests on each OS

### Testing Tiers

#### Tier 1: Core Platforms (Every Commit)

**When**: On push to `master` or `develop`, every PR
**What**: The three critical presets that users will actually use
**Time**: ~15 minutes total

```
ci-linux (gRPC + Agent, Linux)
ci-macos (gRPC + Agent UI + Agent, macOS)
ci-windows (gRPC, Windows)
```

**Why**: These reflect real user workflows. If they break, users are impacted immediately.

#### Tier 2: Feature Combinations (Nightly / On-Demand)

**When**: Nightly at 2 AM UTC, manual dispatch, or `[matrix]` in commit message
**What**: 6-8 specific flag combinations per platform
**Time**: ~45 minutes total (parallel across 3 platforms × 7 configs)

```
Linux:        minimal, grpc-only, full-ai, cli-no-grpc, http-api, no-json
macOS:        minimal, full-ai, agent-ui, universal
Windows:      minimal, full-ai, grpc-remote, z3ed-cli
```

**Why**: Tests dangerous interactions without exponential explosion. Each config tests a realistic user workflow.

#### Tier 3: Platform-Specific (As Needed)

**When**: When platform-specific issues arise
**What**: Architecture-specific builds (ARM64, universal binary, etc.)
**Time**: ~20 minutes

```
Windows ARM64:     Debug + Release
macOS Universal:   arm64 + x86_64
Linux ARM:         Cross-compile tests
```

**Why**: Catches architecture-specific issues that only appear on target platforms.

### Configuration Selection Rationale

#### Why "Minimal"?

Tests the smallest viable configuration:
- Validates core ROM reading/writing works without extras
- Ensures build system doesn't have "feature X requires feature Y" errors
- Catches over-linked libraries

#### Why "gRPC Only"?

Tests server-side automation without AI:
- Validates gRPC infrastructure
- Tests GUI automation system
- Ensures protocol buffer compilation
- Minimal dependencies for headless servers

#### Why "Full AI Stack"?

Tests maximum feature complexity:
- All AI features enabled
- Both Gemini and Ollama paths
- Remote automation + Agent UI
- Catches subtle linking issues with yaml-cpp, OpenSSL, etc.

#### Why "No JSON"?

Tests optional JSON dependency:
- Ensures Ollama works without JSON
- Validates graceful degradation
- Catches hardcoded JSON assumptions

#### Why Platform-Specific?

Each platform has unique constraints:
- **Windows**: MSVC ABI compatibility, gRPC version pinning
- **macOS**: Universal binary (arm64 + x86_64), Homebrew dependencies
- **Linux**: GCC version, glibc compatibility, system library versions

## 3. Problematic Flag Combinations

### Pattern 1: Hidden Dependencies (Fixed)

**Configuration**:
```cmake
YAZE_ENABLE_GRPC=ON
YAZE_ENABLE_REMOTE_AUTOMATION=OFF  # ← Inconsistent!
```

**Problem**: gRPC headers included, but no automation server compiled → link errors

**Fix**: CMake now forces:
```cmake
if(YAZE_ENABLE_REMOTE_AUTOMATION AND NOT YAZE_ENABLE_GRPC)
  set(YAZE_ENABLE_GRPC ON ... FORCE)
endif()
```

**Matrix Test**: `grpc-only` configuration validates this constraint.

### Pattern 2: Orphaned Features (Fixed)

**Configuration**:
```cmake
YAZE_ENABLE_HTTP_API=ON
YAZE_ENABLE_AGENT_CLI=OFF  # ← HTTP API needs a CLI context!
```

**Problem**: REST endpoints defined but no command dispatcher

**Fix**: CMake forces:
```cmake
if(YAZE_ENABLE_HTTP_API AND NOT YAZE_ENABLE_AGENT_CLI)
  set(YAZE_ENABLE_AGENT_CLI ON ... FORCE)
endif()
```

**Matrix Test**: `http-api` configuration validates this.

### Pattern 3: Optional Dependency Breakage

**Configuration**:
```cmake
YAZE_ENABLE_AI_RUNTIME=ON
YAZE_ENABLE_JSON=OFF  # ← Gemini requires JSON!
```

**Problem**: Gemini service can't parse responses

**Status**: Currently relies on developer discipline
**Matrix Test**: `no-json` + `full-ai` would catch this

### Pattern 4: Platform-Specific ABI Mismatch

**Configuration**: Windows with gRPC <1.67.1

**Problem**: MSVC ABI differences, symbol mismatch

**Status**: Documented in `ci-windows` preset
**Matrix Test**: `grpc-remote` on Windows validates gRPC version

### Pattern 5: Architecture-Specific Issues

**Configuration**: macOS universal binary with platform-specific dependencies

**Problem**: Homebrew packages may not have arm64 support

**Status**: Requires dependency audit
**Matrix Test**: `universal` on macOS tests both arm64 and x86_64

## 4. Matrix Testing Tools

### Local Testing: `scripts/test-config-matrix.sh`

Developers run this before pushing to validate all critical configurations locally.

#### Quick Start
```bash
# Test all configurations on current platform
./scripts/test-config-matrix.sh

# Test specific configuration
./scripts/test-config-matrix.sh --config minimal

# Smoke test (configure only, no build)
./scripts/test-config-matrix.sh --smoke

# Verbose with timing
./scripts/test-config-matrix.sh --verbose
```

#### Features
- **Fast feedback**: ~2-3 minutes for all configurations
- **Smoke mode**: Configure without building (30 seconds)
- **Platform detection**: Automatically runs platform-appropriate presets
- **Result tracking**: Clear pass/fail summary
- **Debug logging**: Full CMake/build output in `build_matrix/<config>/`

#### Output Example
```
Config: minimal
  Status: PASSED
  Description: No AI, no gRPC
  Build time: 2.3s

Config: full-ai
  Status: PASSED
  Description: All features enabled
  Build time: 45.2s

============
2/2 configs passed
============
```

### CI Testing: `.github/workflows/matrix-test.yml`

Automated nightly testing across all three platforms.

#### Execution
- **Trigger**: Nightly (2 AM UTC) + manual dispatch + `[matrix]` in commit message
- **Platforms**: Linux (ubuntu-22.04), macOS (14), Windows (2022)
- **Configurations per platform**: 6-7 distinct flag combinations
- **Total runtime**: ~45 minutes (all jobs in parallel)
- **Report**: Pass/fail summary + artifact upload on failure

#### What It Tests

**Linux (6 configs)**:
1. `minimal` - No AI, no gRPC
2. `grpc-only` - gRPC without automation
3. `full-ai` - All features
4. `cli-no-grpc` - CLI only
5. `http-api` - REST endpoints
6. `no-json` - Ollama mode

**macOS (4 configs)**:
1. `minimal` - GUI, no AI
2. `full-ai` - All features
3. `agent-ui` - Agent UI panels only
4. `universal` - arm64 + x86_64 binary

**Windows (4 configs)**:
1. `minimal` - No AI
2. `full-ai` - All features
3. `grpc-remote` - gRPC + automation
4. `z3ed-cli` - CLI executable

## 5. Integration with Development Workflow

### For Developers

Before pushing code to `develop` or `master`:

```bash
# 1. Make changes
git add src/...

# 2. Test locally
./scripts/test-config-matrix.sh

# 3. If all pass, commit
git commit -m "feature: add new thing"

# 4. Push
git push
```

### For CI/CD

**On every push to develop/master**:
1. Standard CI runs (Tier 1 tests)
2. Code quality checks
3. If green, wait for nightly matrix test

**Nightly**:
1. All Tier 2 combinations run in parallel
2. Failures trigger alerts
3. Success confirms no new cross-configuration issues

### For Pull Requests

Option A: **Include `[matrix]` in commit message**
```bash
git commit -m "fix: handle edge case [matrix]"
git push  # Triggers matrix test immediately
```

Option B: **Manual dispatch**
- Go to `.github/workflows/matrix-test.yml`
- Click "Run workflow"
- Select desired tier

## 6. Monitoring & Maintenance

### What to Watch

**Daily**: Check nightly matrix test results
- Link: GitHub Actions > `Configuration Matrix Testing`
- Alert if any configuration fails

**Weekly**: Review failure patterns
- Are certain flag combinations always failing?
- Is a platform having consistent issues?
- Do dependencies need version updates?

**Monthly**: Audit the matrix configuration
- Do new flags need testing?
- Are deprecated flags still tested?
- Can any Tier 2 configs be combined?

### Adding New Configurations

When adding a new feature flag:

1. **Update `cmake/options.cmake`**
   - Define the option
   - Document dependencies
   - Add constraint enforcement

2. **Update `/docs/internal/configuration-matrix.md`**
   - Add to Section 1 (flags)
   - Update Section 2 (constraints)
   - Add to relevant Tier in Section 3

3. **Update `/scripts/test-config-matrix.sh`**
   - Add to `CONFIGS` array
   - Test locally: `./scripts/test-config-matrix.sh --config new-config`

4. **Update `/.github/workflows/matrix-test.yml`**
   - Add matrix job entries for each platform
   - Estimate runtime impact

## 7. Troubleshooting Common Issues

### Issue: "Configuration failed" locally

```bash
# Check the cmake log
tail -50 build_matrix/<config>/config.log

# Check if presets exist
cmake --list-presets
```

### Issue: "Build failed" locally

```bash
# Get full build output
./scripts/test-config-matrix.sh --config <name> --verbose

# Check for missing dependencies
# On macOS: brew list | grep <dep>
# On Linux: apt list --installed | grep <dep>
```

### Issue: Test passes locally but fails in CI

**Likely causes**:
1. Different CMake version (CI uses latest)
2. Different compiler (GCC vs Clang vs MSVC)
3. Missing system library

**Solutions**:
- Check `.github/actions/setup-build` for CI environment
- Match local compiler: `cmake --preset ci-linux -DCMAKE_CXX_COMPILER=gcc-13`
- Add dependency: Update `cmake/dependencies.cmake`

## 8. Future Improvements

### Short Term (Next Sprint)

- [ ] Add binary size tracking per configuration
- [ ] Add compile time benchmarks
- [ ] Auto-generate configuration compatibility matrix chart
- [ ] Add `--ci-mode` flag to local script (simulates GH Actions)

### Medium Term (Next Quarter)

- [ ] Integrate with release pipeline (validate all Tier 2 before release)
- [ ] Add performance regression tests per configuration
- [ ] Create configuration validator tool (warns on suspicious combinations)
- [ ] Document platform-specific dependency versions

### Long Term (Next Year)

- [ ] Separate `YAZE_ENABLE_AI` and `YAZE_ENABLE_AI_RUNTIME` (currently coupled)
- [ ] Add Tier 0 (smoke tests) that run on every commit
- [ ] Create web dashboard of matrix test results
- [ ] Add "configuration suggestion" tool (infer optimal flags for user's hardware)

## 9. Reference: Configuration Categories

### GUI User (Desktop)
```cmake
YAZE_BUILD_GUI=ON
YAZE_BUILD_AGENT_UI=ON
YAZE_ENABLE_GRPC=OFF           # No network overhead
YAZE_ENABLE_AI=OFF             # Unnecessary for GUI-only
```

### Server/Headless (Automation)
```cmake
YAZE_BUILD_GUI=OFF
YAZE_ENABLE_GRPC=ON
YAZE_ENABLE_REMOTE_AUTOMATION=ON
YAZE_ENABLE_AI=OFF             # Optional
```

### Full-Featured Developer
```cmake
YAZE_BUILD_GUI=ON
YAZE_BUILD_AGENT_UI=ON
YAZE_ENABLE_GRPC=ON
YAZE_ENABLE_REMOTE_AUTOMATION=ON
YAZE_ENABLE_AI_RUNTIME=ON
YAZE_ENABLE_HTTP_API=ON
```

### CLI-Only (z3ed Agent)
```cmake
YAZE_BUILD_GUI=OFF
YAZE_BUILD_Z3ED=ON
YAZE_ENABLE_GRPC=ON
YAZE_ENABLE_AI_RUNTIME=ON
YAZE_ENABLE_HTTP_API=ON
```

### Minimum (Embedded/Library)
```cmake
YAZE_BUILD_GUI=OFF
YAZE_BUILD_CLI=OFF
YAZE_BUILD_TESTS=OFF
YAZE_ENABLE_GRPC=OFF
YAZE_ENABLE_AI=OFF
```

---

**Questions?** Check `/docs/internal/configuration-matrix.md` or ask in coordination-board.md.
