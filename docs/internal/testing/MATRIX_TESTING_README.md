# Matrix Testing System for yaze

## What's This?

A comprehensive **platform/configuration matrix testing system** that validates CMake flag combinations across all platforms.

**Before**: Only tested default configurations, missed dangerous flag interactions.
**After**: 7 local configurations + 14 nightly configurations = catch issues before they reach users.

## Quick Start (30 seconds)

### For Developers

Before pushing your code:

```bash
./scripts/test-config-matrix.sh
```

If all tests pass (green checkmarks), you're good to push.

### For CI/CD

Tests run automatically:
- Every night at 2 AM UTC (comprehensive matrix)
- On-demand with `[matrix]` in commit message
- Results in GitHub Actions

## What Gets Tested?

### Tier 1: Core Configurations (Every Commit)
3 standard presets everyone knows about:
- Linux (gRPC + Agent CLI)
- macOS (gRPC + Agent UI + Agent CLI)
- Windows (gRPC core features)

### Tier 2: Feature Combinations (Nightly)
Strategic testing of dangerous interactions:

**Linux**:
- `minimal` - No AI, no gRPC
- `grpc-only` - gRPC without automation
- `full-ai` - All features enabled
- `cli-no-grpc` - CLI without networking
- `http-api` - REST endpoints
- `no-json` - Ollama mode (no JSON parsing)

**macOS**:
- `minimal` - GUI, no AI
- `full-ai` - All features
- `agent-ui` - Agent UI panels only
- `universal` - ARM64 + x86_64 binary

**Windows**:
- `minimal` - No AI
- `full-ai` - All features
- `grpc-remote` - gRPC + automation
- `z3ed-cli` - CLI executable

### Tier 3: Platform-Specific (As Needed)
Architecture-specific tests when issues arise.

## The Problem It Solves

Matrix testing catches **cross-configuration issues** that single preset testing misses:

### Example 1: gRPC Without Automation
```bash
cmake -B build -DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_REMOTE_AUTOMATION=OFF
# Before: Silent link error (gRPC headers but no server code)
# After: CMake auto-enforces constraint, matrix tests validate
```

### Example 2: HTTP API Without CLI Stack
```bash
cmake -B build -DYAZE_ENABLE_HTTP_API=ON -DYAZE_ENABLE_AGENT_CLI=OFF
# Before: Runtime error (endpoints defined but no dispatcher)
# After: CMake auto-enforces, matrix tests validate
```

### Example 3: AI Runtime Without JSON
```bash
cmake -B build -DYAZE_ENABLE_AI_RUNTIME=ON -DYAZE_ENABLE_JSON=OFF
# Before: Compile error (Gemini needs JSON)
# After: Matrix test `no-json` catches this edge case
```

**All 6 known problematic patterns are now documented and tested.**

## Files & Usage

### For Getting Started (5 min)
📄 **`/docs/internal/testing/QUICKSTART.md`**
- One-page quick reference
- Common commands
- Error troubleshooting

### For Understanding Strategy (20 min)
📄 **`/docs/internal/testing/matrix-testing-strategy.md`**
- Why we test this way
- Real bug examples
- Philosophy behind smart matrix testing
- Monitoring and maintenance

### For Complete Reference (30 min)
📄 **`/docs/internal/configuration-matrix.md`**
- All 18 CMake flags documented
- Dependency graph
- Complete tested matrix
- Problematic combinations and fixes

### For Hands-On Use
🔧 **`/scripts/test-config-matrix.sh`**
```bash
./scripts/test-config-matrix.sh              # Test all
./scripts/test-config-matrix.sh --config minimal  # Specific
./scripts/test-config-matrix.sh --smoke      # Quick 30s test
./scripts/test-config-matrix.sh --verbose    # Detailed output
./scripts/test-config-matrix.sh --help       # All options
```

🔧 **`/scripts/validate-cmake-config.sh`**
```bash
./scripts/validate-cmake-config.sh \
  -DYAZE_ENABLE_GRPC=ON \
  -DYAZE_ENABLE_HTTP_API=ON
# Warns about problematic combinations before build
```

## Integration with Your Workflow

### Before Pushing (Recommended)
```bash
# Make your changes
git add src/...

# Test locally
./scripts/test-config-matrix.sh

# If green, commit and push
git commit -m "feature: your change"
git push
```

### In CI/CD (Automatic)
- Standard tests run on every push (Tier 1)
- Comprehensive tests run nightly (Tier 2)
- Can trigger with `[matrix]` in commit message

### When Adding New Features
1. Update `cmake/options.cmake` (define flag + constraints)
2. Document in `/docs/internal/configuration-matrix.md`
3. Add test config to `/scripts/test-config-matrix.sh`
4. Add CI job to `/.github/workflows/matrix-test.yml`

## Real Examples

### Example: Testing a Configuration Change

```bash
# I want to test what happens with no JSON support
./scripts/test-config-matrix.sh --config no-json

# Output:
# [INFO] Testing: no-json
# [✓] Configuration successful
# [✓] Build successful
# [✓] Unit tests passed
# ✓ no-json: PASSED
```

### Example: Validating Flag Combination

```bash
# Is this combination valid?
./scripts/validate-cmake-config.sh \
  -DYAZE_ENABLE_HTTP_API=ON \
  -DYAZE_ENABLE_AGENT_CLI=OFF

# Output:
# ✗ ERROR: YAZE_ENABLE_HTTP_API=ON requires YAZE_ENABLE_AGENT_CLI=ON
```

### Example: Smoke Test Before Push

```bash
# Quick 30-second validation
./scripts/test-config-matrix.sh --smoke

# Output:
# [INFO] Testing: minimal
# [INFO] Running smoke test (configure only)
# [✓] Configuration successful
# Results: 7/7 passed
```

## Key Design Decisions

### 1. Smart Matrix, Not Exhaustive
- Testing all 2^18 combinations = 262,144 tests (impossible)
- Instead: 7 local configs + 14 nightly configs (practical)
- Covers: baselines, extremes, interactions, platforms

### 2. Automatic Constraint Enforcement
CMake automatically prevents invalid combinations:
```cmake
if(YAZE_ENABLE_REMOTE_AUTOMATION AND NOT YAZE_ENABLE_GRPC)
  set(YAZE_ENABLE_GRPC ON ... FORCE)
endif()
```

### 3. Tiered Execution
- **Tier 1** (3 configs): Every commit, ~15 min
- **Tier 2** (14 configs): Nightly, ~45 min
- **Tier 3** (architecture-specific): On-demand

### 4. Developer-Friendly
- Local testing before push (fast feedback)
- Clear pass/fail reporting
- Smoke mode for quick validation
- Helpful error messages

## Performance Impact

### Local Testing
```
Full test:  ~2-3 minutes (all 7 configs)
Smoke test: ~30 seconds (configure only)
Specific:   ~20-30 seconds (one config)
```

### CI/CD
- Tier 1 (standard CI): No change (~15 min as before)
- Tier 2 (nightly): New, but off the critical path (~45 min)
- No impact on PR merge latency

## Troubleshooting

### Test fails locally
```bash
# See detailed output
./scripts/test-config-matrix.sh --config <name> --verbose

# Check build log
tail -50 build_matrix/<name>/build.log

# Check cmake log
tail -50 build_matrix/<name>/config.log
```

### Don't have dependencies
```bash
# Install dependencies per platform
macOS:  brew install [dep]
Linux:  apt-get install [dep]
Windows: choco install [dep] or build with vcpkg
```

### Windows gRPC issues
```bash
# ci-windows preset uses stable gRPC 1.67.1
# If you use different version, you'll get ABI errors
# Solution: Use preset or update validation rules
```

## Monitoring

### Daily
Check nightly matrix test results in GitHub Actions

### Weekly
Review failure patterns and fix root causes

### Monthly
Audit matrix configuration and documentation

## Future Enhancements

- Binary size tracking per configuration
- Compile time benchmarks
- Performance regression detection
- Configuration recommendation tool
- Web dashboard of results

## Questions?

| Question | Answer |
|----------|--------|
| How do I use this? | Read `QUICKSTART.md` |
| What's tested? | See `configuration-matrix.md` Section 3 |
| Why this approach? | Read `matrix-testing-strategy.md` |
| How do I add a config? | Check `MATRIX_TESTING_IMPLEMENTATION.md` |

## Files Overview

```
Documentation:
  ✓ docs/internal/configuration-matrix.md
    → All flags, dependencies, tested matrix

  ✓ docs/internal/testing/matrix-testing-strategy.md
    → Philosophy, examples, integration guide

  ✓ docs/internal/testing/QUICKSTART.md
    → One-page reference for developers

  ✓ MATRIX_TESTING_IMPLEMENTATION.md
    → Complete implementation guide

  ✓ MATRIX_TESTING_CHECKLIST.md
    → Status, next steps, responsibilities

Automation:
  ✓ .github/workflows/matrix-test.yml
    → Nightly/on-demand CI testing

  ✓ scripts/test-config-matrix.sh
    → Local pre-push validation

  ✓ scripts/validate-cmake-config.sh
    → Flag combination validation
```

## Getting Started Now

1. **Read**: `docs/internal/testing/QUICKSTART.md` (5 min)
2. **Run**: `./scripts/test-config-matrix.sh` (2 min)
3. **Add to workflow**: Use before pushing (optional)
4. **Monitor**: Check nightly results in GitHub Actions

---

**Status**: Ready to use
**Local Testing**: `./scripts/test-config-matrix.sh`
**CI Testing**: Automatic nightly + on-demand
**Questions**: See the QUICKSTART guide

Last Updated: 2025-11-20
Owner: CLAUDE_MATRIX_TEST
