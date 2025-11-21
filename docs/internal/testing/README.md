# Testing Documentation Hub

For comprehensive testing information, see the public developer documentation:

## Primary Testing References

- **[Testing Guide](../../public/developer/testing-guide.md)** - Complete testing reference
  - Test categories (unit, integration, e2e, ROM-dependent)
  - Writing new tests
  - Running tests locally and in CI
  - Troubleshooting common test failures

- **[Testing Quick Start](../../public/developer/testing-quick-start.md)** - Pre-push checklist
  - Fast local validation (< 2 minutes)
  - Common test failures and fixes
  - Platform-specific quick checks

- **[Build & Test Quick Reference](../../public/build/quick-reference.md)** - Build system reference
  - CMake presets for testing
  - Test execution commands
  - Environment variables

## Historical Testing Infrastructure

Archived testing infrastructure documentation from various testing initiatives (matrix testing, symbol conflict detection, CI improvements) can be found in:

- **[Testing Archive](../legacy/testing-archive/)** - Historical testing docs

These documents were part of infrastructure experiments and improvement initiatives. While they contain valuable historical context, the current authoritative testing documentation is in the public developer guides linked above.

## Quick Test Commands

```bash
# Build tests
cmake --build --preset mac-dbg --target yaze_test

# Run all tests
./build/bin/yaze_test

# Run specific categories
./build/bin/yaze_test --unit
./build/bin/yaze_test --integration
./build/bin/yaze_test --e2e --show-gui

# Run with ROM
./build/bin/yaze_test --rom-dependent --rom-path zelda3.sfc
```

## Environment Variables

- `YAZE_TEST_ROM_PATH` - Default ROM for ROM-dependent tests
- `YAZE_SKIP_ROM_TESTS` - Skip tests requiring ROM files
- `YAZE_ENABLE_UI_TESTS` - Enable expensive UI tests

## CI Testing

See [CI/CD Pipeline documentation](../../public/build/quick-reference.md#testing) for information about:
- Test execution in GitHub Actions
- Platform-specific test suites
- Test result reporting
- Artifact collection on failures
