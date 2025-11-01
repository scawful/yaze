# GitHub Actions - Composite Actions

This directory contains reusable composite actions for the YAZE CI/CD pipeline.

## Available Actions

### 1. `setup-build`
Sets up the build environment with dependencies and caching.

**Inputs:**
- `platform` (required): Target platform (linux, macos, windows)
- `preset` (required): CMake preset to use
- `cache-key` (optional): Cache key for dependencies

**What it does:**
- Configures CPM cache
- Installs platform-specific build dependencies
- Sets up sccache/ccache for faster builds

### 2. `build-project`
Builds the project with CMake and caching.

**Inputs:**
- `platform` (required): Target platform (linux, macos, windows)
- `preset` (required): CMake preset to use
- `build-type` (optional): Build type (Debug, Release, RelWithDebInfo)

**What it does:**
- Caches build artifacts
- Configures the project with CMake
- Builds the project with optimal parallel settings
- Shows build artifacts for verification

### 3. `run-tests`
Runs the test suite with appropriate filtering.

**Inputs:**
- `test-type` (required): Type of tests to run (stable, unit, integration, all)
- `preset` (optional): CMake preset to use (default: ci)

**What it does:**
- Runs the specified test suite(s)
- Generates JUnit XML test results
- Uploads test results as artifacts

## Usage

These composite actions are used in the main CI workflow (`.github/workflows/ci.yml`). They must be called after checking out the repository:

```yaml
steps:
  - name: Checkout code
    uses: actions/checkout@v4
    with:
      submodules: recursive

  - name: Setup build environment
    uses: ./.github/actions/setup-build
    with:
      platform: linux
      preset: ci
      cache-key: ${{ hashFiles('cmake/dependencies.lock') }}
```

## Important Notes

1. **Repository checkout required**: The repository must be checked out before calling any of these composite actions. They do not include a checkout step themselves.

2. **Platform-specific behavior**: Each action adapts to the target platform (Linux, macOS, Windows) and runs appropriate commands for that environment.

3. **Caching**: The actions use GitHub Actions caching to speed up builds by caching:
   - CPM dependencies (~/.cpm-cache)
   - Build artifacts (build/)
   - Compiler cache (sccache/ccache)

4. **Dependencies**: The Linux CI packages are listed in `.github/workflows/scripts/linux-ci-packages.txt`.

## Maintenance

When updating these actions:
- Test on all three platforms (Linux, macOS, Windows)
- Ensure shell compatibility (bash for Linux/macOS, pwsh for Windows)
- Update this README if inputs or behavior changes

