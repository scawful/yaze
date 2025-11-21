# Pre-Push Checklist

This checklist ensures your changes are ready for CI and won't break the build. Follow this before every `git push`.

**Time Budget**: ~2 minutes
**Success Rate**: Catches 90% of CI failures

---

## Quick Start

```bash
# Unix/macOS
./scripts/pre-push-test.sh

# Windows PowerShell
.\scripts\pre-push-test.ps1
```

If all checks pass, you're good to push!

---

## Detailed Checklist

### ☐ Level 0: Static Analysis (< 1 second)

#### Code Formatting
```bash
cmake --build build --target yaze-format-check
```

**If it fails**:
```bash
# Auto-format your code
cmake --build build --target yaze-format

# Verify it passes now
cmake --build build --target yaze-format-check
```

**What it catches**: Formatting violations, inconsistent style

---

### ☐ Level 1: Configuration Validation (< 10 seconds)

#### CMake Configuration
```bash
# Test your preset
cmake --preset mac-dbg  # or lin-dbg, win-dbg
```

**If it fails**:
- Check `CMakeLists.txt` syntax
- Verify all required dependencies are available
- Check `CMakePresets.json` for typos

**What it catches**: CMake syntax errors, missing dependencies, invalid presets

---

### ☐ Level 2: Smoke Compilation (< 2 minutes)

#### Quick Compilation Test
```bash
./scripts/pre-push-test.sh --smoke-only
```

**What it compiles**:
- `src/app/rom.cc` (core ROM handling)
- `src/app/gfx/bitmap.cc` (graphics system)
- `src/zelda3/overworld/overworld.cc` (game logic)
- `src/cli/service/resources/resource_catalog.cc` (CLI)

**If it fails**:
- Check for missing `#include` directives
- Verify header paths are correct
- Check for platform-specific compilation issues
- Run full build to see all errors: `cmake --build build -v`

**What it catches**: Missing headers, include path issues, preprocessor errors

---

### ☐ Level 3: Symbol Validation (< 30 seconds)

#### Symbol Conflict Detection
```bash
./scripts/verify-symbols.sh
```

**If it fails**:
Look for these common issues:

1. **FLAGS symbol conflicts**:
   ```
   ✗ FLAGS symbol conflict: FLAGS_verbose
       → libyaze_cli.a
       → libyaze_app.a
   ```
   **Fix**: Define `FLAGS_*` in exactly one `.cc` file, not in headers

2. **Duplicate function definitions**:
   ```
   ⚠ Duplicate symbol: MyClass::MyFunction()
       → libyaze_foo.a
       → libyaze_bar.a
   ```
   **Fix**: Use `inline` for header-defined functions or move to `.cc` file

3. **Template instantiation conflicts**:
   ```
   ⚠ Duplicate symbol: std::vector<MyType>::resize()
       → libyaze_foo.a
       → libyaze_bar.a
   ```
   **Fix**: This is usually safe (templates), but if it causes link errors, use explicit instantiation

**What it catches**: ODR violations, duplicate symbols, FLAGS conflicts

---

### ☐ Level 4: Unit Tests (< 30 seconds)

#### Run Unit Tests
```bash
./build/bin/yaze_test --unit
```

**If it fails**:
1. Read the failure message carefully
2. Run the specific failing test:
   ```bash
   ./build/bin/yaze_test "TestSuite.TestName"
   ```
3. Debug with verbose output:
   ```bash
   ./build/bin/yaze_test --verbose "TestSuite.TestName"
   ```
4. Fix the issue in your code
5. Re-run tests

**Common issues**:
- Logic errors in new code
- Breaking changes to existing APIs
- Missing test updates after refactoring
- Platform-specific test failures

**What it catches**: Logic errors, API breakage, regressions

---

## Platform-Specific Checks

### macOS Developers

**Additional checks**:
```bash
# Test Linux-style strict linking (if Docker available)
docker run --rm -v $(pwd):/workspace yaze-linux-builder \
  ./scripts/pre-push-test.sh
```

**Why**: Linux linker is stricter about ODR violations

### Linux Developers

**Additional checks**:
```bash
# Run with verbose warnings
cmake --preset lin-dbg-v
cmake --build build -v
```

**Why**: Catches more warnings that might fail on other platforms

### Windows Developers

**Additional checks**:
```powershell
# Test with clang-cl explicitly
cmake --preset win-dbg -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build
```

**Why**: Ensures compatibility with CI's clang-cl configuration

---

## Optional Checks (Recommended)

### Integration Tests (2-5 minutes)
```bash
./build/bin/yaze_test --integration
```

**When to run**: Before pushing major changes

### E2E Tests (5-10 minutes)
```bash
./build/bin/yaze_test --e2e
```

**When to run**: Before pushing UI changes

### Memory Sanitizer (10-20 minutes)
```bash
cmake --preset sanitizer
cmake --build build
./build/bin/yaze_test
```

**When to run**: Before pushing memory-related changes

---

## Troubleshooting

### "I don't have time for all this!"

**Minimum checks** (< 1 minute):
```bash
# Just format and unit tests
cmake --build build --target yaze-format-check && \
./build/bin/yaze_test --unit
```

### "Tests pass locally but fail in CI"

**Common causes**:
1. **Platform-specific**: Your change works on macOS but breaks Linux/Windows
   - **Solution**: Test with matching CI preset (`ci-linux`, `ci-macos`, `ci-windows`)

2. **Symbol conflicts**: Local linker is more permissive than CI
   - **Solution**: Run `./scripts/verify-symbols.sh`

3. **Include paths**: Your IDE finds headers that CI doesn't
   - **Solution**: Run smoke compilation test

4. **Cached build**: Your local build has stale artifacts
   - **Solution**: Clean rebuild: `rm -rf build && cmake --preset <preset> && cmake --build build`

### "Pre-push script is too slow"

**Speed it up**:
```bash
# Skip symbol checking (30s saved)
./scripts/pre-push-test.sh --skip-symbols

# Skip tests (30s saved)
./scripts/pre-push-test.sh --skip-tests

# Only check configuration (90% faster)
./scripts/pre-push-test.sh --config-only
```

**Warning**: Skipping checks increases risk of CI failures

### "My branch is behind develop"

**Update first**:
```bash
git fetch origin
git rebase origin/develop
# Re-run pre-push checks
./scripts/pre-push-test.sh
```

---

## Emergency Push (Use Sparingly)

If you absolutely must push without full validation:

1. **Push to a feature branch** (never directly to develop/master):
   ```bash
   git push origin feature/my-fix
   ```

2. **Create a PR immediately** to trigger CI

3. **Watch CI closely** and be ready to fix issues

4. **Don't merge until CI passes**

---

## CI-Matching Presets

Use these presets to match CI exactly:

| Platform | Local Preset | CI Preset | CI Job |
|----------|-------------|-----------|--------|
| Ubuntu 22.04 | `lin-dbg` | `ci-linux` | build/test |
| macOS 14 | `mac-dbg` | `ci-macos` | build/test |
| Windows 2022 | `win-dbg` | `ci-windows` | build/test |

**Usage**:
```bash
cmake --preset ci-linux    # Exactly matches CI
cmake --build build
./build/bin/yaze_test --unit
```

---

## Success Metrics

After running all checks:
- ✅ **0 format violations**
- ✅ **0 CMake errors**
- ✅ **0 compilation errors**
- ✅ **0 symbol conflicts**
- ✅ **0 test failures**

**Result**: ~90% chance of passing CI on first try

---

## Related Documentation

- **Testing Strategy**: `docs/internal/testing/testing-strategy.md`
- **Gap Analysis**: `docs/internal/testing/gap-analysis.md`
- **Build Quick Reference**: `docs/public/build/quick-reference.md`
- **Troubleshooting**: `docs/public/build/troubleshooting.md`

---

## Questions?

- Check test output carefully (most errors are self-explanatory)
- Review recent commits for similar fixes: `git log --oneline --since="7 days ago"`
- Read error messages completely (don't skim)
- When in doubt, clean rebuild: `rm -rf build && cmake --preset <preset> && cmake --build build`
