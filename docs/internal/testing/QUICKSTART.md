# Matrix Testing Quick Start

**Want to test configurations locally before pushing?** You're in the right place.

## One-Minute Version

```bash
# Before pushing your code, run:
./scripts/test-config-matrix.sh

# Result: Green checkmarks = safe to push
```

That's it! It will test 7 key configurations on your platform.

## Want More Control?

### Test specific configuration
```bash
./scripts/test-config-matrix.sh --config minimal
./scripts/test-config-matrix.sh --config full-ai
```

### See what's being tested
```bash
./scripts/test-config-matrix.sh --verbose
```

### Quick "configure only" test (30 seconds)
```bash
./scripts/test-config-matrix.sh --smoke
```

### Parallel jobs (speed it up)
```bash
MATRIX_JOBS=8 ./scripts/test-config-matrix.sh
```

## Available Configurations

These are the 7 key configurations tested:

| Config | What It Tests | When You Care |
|--------|---------------|---------------|
| `minimal` | No AI, no gRPC | Making sure core editor works |
| `grpc-only` | gRPC without automation | Server-side features |
| `full-ai` | All features enabled | Complete feature testing |
| `cli-no-grpc` | CLI-only, no networking | Headless workflows |
| `http-api` | REST API endpoints | External integration |
| `no-json` | Ollama mode (no JSON) | Minimal dependencies |
| `all-off` | Library only | Embedded usage |

## Reading Results

### Success
```
[INFO] Configuring CMake...
[✓] Configuration successful
[✓] Build successful
[✓] Unit tests passed
✓ minimal: PASSED
```

### Failure
```
[INFO] Configuring CMake...
[✗] Configuration failed for minimal
Build logs: ./build_matrix/minimal/config.log
```

If a test fails, check the error log:
```bash
tail -50 build_matrix/<config>/config.log
tail -50 build_matrix/<config>/build.log
```

## Common Errors & Fixes

### "cmake: command not found"
**Fix**: Install CMake
```bash
# macOS
brew install cmake

# Ubuntu/Debian
sudo apt-get install cmake

# Windows
choco install cmake  # or download from cmake.org
```

### "Preset not found"
**Problem**: You're on Windows trying to run a Linux preset
**Fix**: Script auto-detects platform, but you can override:
```bash
./scripts/test-config-matrix.sh --platform linux  # Force Linux presets
```

### "Build failed - missing dependencies"
**Problem**: A library isn't installed
**Solution**: Follow the main README.md to install all dependencies

## Continuous Integration (GitHub Actions)

Matrix tests also run automatically:

- **Nightly**: 2 AM UTC, tests all Tier 2 configurations on all platforms
- **On-demand**: Include `[matrix]` in your commit message to trigger immediately
- **Results**: Check GitHub Actions tab for full report

## For Maintainers

Adding a new configuration to test?

1. Edit `/scripts/test-config-matrix.sh` - add to `CONFIGS` array
2. Test locally: `./scripts/test-config-matrix.sh --config new-config`
3. Update matrix test workflow: `/.github/workflows/matrix-test.yml`
4. Document in `/docs/internal/configuration-matrix.md`

## Full Documentation

For deep dives:
- **Configuration reference**: See `docs/internal/configuration-matrix.md`
- **Testing strategy**: See `docs/internal/testing/matrix-testing-strategy.md`
- **CI workflow**: See `.github/workflows/matrix-test.yml`

## Questions?

- Check existing logs: `./build_matrix/<config>/*.log`
- Run with `--verbose` for detailed output
- See `./scripts/test-config-matrix.sh --help`
