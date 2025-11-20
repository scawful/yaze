# Testing Quick Start - Before You Push

**Target Audience**: Developers contributing to yaze
**Goal**: Ensure your changes pass tests before pushing to remote

## The 5-Minute Pre-Push Checklist

Before pushing changes to the repository, run these commands to catch issues early:

### 1. Build Tests (30 seconds)

```bash
# Build the test executable
cmake --build build --target yaze_test
```

### 2. Run Fast Tests (<2 minutes)

```bash
# Run unit tests only (fastest)
./build/bin/yaze_test --unit

# Or run all stable tests (unit + non-ROM integration)
./build/bin/yaze_test
```

### 3. Platform-Specific Quick Check

**macOS**:
```bash
scripts/agents/smoke-build.sh mac-dbg yaze
```

**Linux**:
```bash
scripts/agents/smoke-build.sh lin-dbg yaze
```

**Windows (PowerShell)**:
```powershell
pwsh -File scripts/agents/windows-smoke-build.ps1 -Preset win-dbg -Target yaze
```

### 4. Check for Format Issues (optional but recommended)

```bash
# Check if code is formatted correctly
cmake --build build --target format-check

# Auto-fix formatting issues
cmake --build build --target format
```

## When to Run Full Test Suite

Run the **complete test suite** before pushing if:

- You modified core systems (ROM, graphics, editor base classes)
- You changed CMake configuration or build system
- You're preparing a pull request
- CI previously failed on your branch

### Full Test Suite Commands

```bash
# Run all tests (may take 5+ minutes)
./build/bin/yaze_test

# Include ROM-dependent tests (requires zelda3.sfc)
./build/bin/yaze_test --rom-dependent --rom-path /path/to/zelda3.sfc

# Run E2E GUI tests (headless)
./build/bin/yaze_test --e2e

# Run E2E with visible GUI (for debugging)
./build/bin/yaze_test --e2e --show-gui
```

## Common Test Failures and Fixes

### 1. Compilation Errors

**Symptom**: `cmake --build build --target yaze_test` fails

**Fix**:
```bash
# Clean and reconfigure
rm -rf build
cmake --preset mac-dbg  # or lin-dbg, win-dbg
cmake --build build --target yaze_test
```

### 2. Unit Test Failures

**Symptom**: `./build/bin/yaze_test --unit` shows failures

**Fix**:
- Read the error message carefully
- Check if you broke contracts in modified code
- Verify test expectations match your changes
- Update tests if behavior change was intentional

### 3. ROM-Dependent Test Failures

**Symptom**: Tests fail with "ROM file not found"

**Fix**:
```bash
# Set environment variable
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Or pass directly to test runner
./build/bin/yaze_test --rom-path /path/to/zelda3.sfc
```

### 4. E2E/GUI Test Failures

**Symptom**: E2E tests fail or hang

**Fix**:
- Check if SDL is initialized properly
- Run with `--show-gui` to see what's happening visually
- Verify ImGui Test Engine is enabled in build
- Check test logs for specific assertion failures

### 5. Platform-Specific Failures

**Symptom**: Tests pass locally but fail in CI

**Solution**:
1. Check which platform failed in CI logs
2. If Windows: ensure you're using the `win-*` preset
3. If Linux: check for case-sensitive path issues
4. If macOS: verify you're testing on compatible macOS version

## Test Categories Explained

| Category | What It Tests | When to Run | Duration |
|----------|---------------|-------------|----------|
| **Unit** | Individual functions/classes | Before every commit | <10s |
| **Integration** | Component interactions | Before every push | <30s |
| **E2E** | Full user workflows | Before PRs | 1-5min |
| **ROM-Dependent** | ROM data loading/saving | Before ROM changes | Variable |

## Recommended Workflows

### For Small Changes (typos, docs, minor fixes)

```bash
# Just build to verify no compile errors
cmake --build build --target yaze
```

### For Code Changes (new features, bug fixes)

```bash
# Build and run unit tests
cmake --build build --target yaze_test
./build/bin/yaze_test --unit

# If tests pass, push
git push
```

### For Core System Changes (ROM, graphics, editors)

```bash
# Run full test suite
cmake --build build --target yaze_test
./build/bin/yaze_test

# If all tests pass, push
git push
```

### For Pull Requests

```bash
# Run everything including ROM tests and E2E
./build/bin/yaze_test --rom-dependent --rom-path zelda3.sfc
./build/bin/yaze_test --e2e

# Check code formatting
cmake --build build --target format-check

# If all pass, create PR
git push origin feature-branch
```

## IDE Integration

### Visual Studio Code

Add this to `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build Tests",
      "type": "shell",
      "command": "cmake --build build --target yaze_test",
      "group": "build"
    },
    {
      "label": "Run Unit Tests",
      "type": "shell",
      "command": "./build/bin/yaze_test --unit",
      "group": "test",
      "dependsOn": "Build Tests"
    },
    {
      "label": "Run All Tests",
      "type": "shell",
      "command": "./build/bin/yaze_test",
      "group": "test",
      "dependsOn": "Build Tests"
    }
  ]
}
```

Then use `Cmd/Ctrl+Shift+B` to build tests or `Cmd/Ctrl+Shift+P` → "Run Test Task" to run them.

### CLion / Visual Studio

Both IDEs auto-detect CTest and provide built-in test runners:

- **CLion**: Tests appear in "Test Explorer" panel
- **Visual Studio**: Use "Test Explorer" window

Configure test presets in `CMakePresets.json` (already configured in this project).

## Environment Variables

Customize test behavior with these environment variables:

```bash
# Path to test ROM file
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Skip ROM-dependent tests entirely
export YAZE_SKIP_ROM_TESTS=1

# Enable UI tests (E2E)
export YAZE_ENABLE_UI_TESTS=1

# Verbose test output
export YAZE_TEST_VERBOSE=1
```

## Getting Test Output

### Verbose Test Output

```bash
# Show all test output (even passing tests)
./build/bin/yaze_test --gtest_output=verbose

# Show only failed test output
./build/bin/yaze_test --gtest_output=on_failure
```

### Specific Test Patterns

```bash
# Run only tests matching pattern
./build/bin/yaze_test --gtest_filter="*AsarWrapper*"

# Run tests in specific suite
./build/bin/yaze_test --gtest_filter="RomTest.*"

# Exclude specific tests
./build/bin/yaze_test --gtest_filter="-*SlowTest*"
```

### Repeat Tests for Flakiness

```bash
# Run tests 10 times to catch flakiness
./build/bin/yaze_test --gtest_repeat=10

# Stop on first failure
./build/bin/yaze_test --gtest_repeat=10 --gtest_break_on_failure
```

## CI/CD Testing

After pushing, CI will run tests on all platforms (Linux, macOS, Windows):

1. **Check CI status**: Look for green checkmark in GitHub
2. **If CI fails**: Click "Details" to see which platform/test failed
3. **Fix and push again**: CI re-runs automatically

**Pro tip**: Use remote workflow triggers to test in CI before pushing:

```bash
# Trigger CI remotely (requires gh CLI)
scripts/agents/run-gh-workflow.sh ci.yml -f enable_http_api_tests=true
```

See [GH Actions Remote Guide](../../internal/agents/gh-actions-remote.md) for setup.

## Advanced Topics

### Running Tests with CTest

```bash
# Run all stable tests via ctest
ctest --preset dev

# Run specific test suite
ctest -L unit

# Run with verbose output
ctest --preset dev --output-on-failure

# Run tests in parallel
ctest --preset dev -j8
```

### Debugging Failed Tests

```bash
# Run test under debugger (macOS/Linux)
lldb ./build/bin/yaze_test -- --gtest_filter="*FailingTest*"

# Run test under debugger (Windows)
devenv /debugexe ./build/bin/yaze_test.exe --gtest_filter="*FailingTest*"
```

### Writing New Tests

See [Testing Guide](testing-guide.md) for comprehensive guide on writing tests.

Quick template:

```cpp
#include <gtest/gtest.h>
#include "my_class.h"

namespace yaze {
namespace test {

TEST(MyClassTest, BasicFunctionality) {
  MyClass obj;
  EXPECT_TRUE(obj.DoSomething());
}

}  // namespace test
}  // namespace yaze
```

Add your test file to `test/CMakeLists.txt` in the appropriate suite.

## Help and Resources

- **Detailed Testing Guide**: [docs/public/developer/testing-guide.md](testing-guide.md)
- **Build Commands**: [docs/public/build/quick-reference.md](../build/quick-reference.md)
- **Testing Infrastructure**: [docs/internal/testing/README.md](../../internal/testing/README.md)
- **Troubleshooting**: [docs/public/build/troubleshooting.md](../build/troubleshooting.md)

## Questions?

1. Check [Testing Guide](testing-guide.md) for detailed explanations
2. Search existing issues: https://github.com/scawful/yaze/issues
3. Ask in discussions: https://github.com/scawful/yaze/discussions

---

**Remember**: Running tests before pushing saves time for everyone. A few minutes of local testing prevents hours of CI debugging.
