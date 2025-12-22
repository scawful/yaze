# Build and Test Quick Reference

This document is the single source of truth for configuring, building, and testing YAZE on all platforms. Other documentation files link here rather than duplicating these instructions.

---

## 1. Environment Setup

### Clone the Repository

```bash
git clone --recursive https://github.com/scawful/yaze.git
cd yaze
```

### Verify Your Environment

Run the verification script once per machine to check dependencies and fix common issues:

**macOS / Linux:**
```bash
./scripts/verify-build-environment.sh --fix
```

**Windows (PowerShell):**
```powershell
.\scripts\verify-build-environment.ps1 -FixIssues
```

---

## 2. Build Presets

YAZE uses CMake presets for consistent builds. Configure with `cmake --preset <name>`, then build with `cmake --build --preset <name>`.

### Available Presets

| Preset | Platform | Description |
|--------|----------|-------------|
| `mac-dbg`, `lin-dbg`, `win-dbg` | macOS / Linux / Windows | Standard debug builds with tests enabled |
| `mac-ai`, `lin-ai`, `win-ai` | macOS / Linux / Windows | Full AI stack: gRPC, agent UI, z3ed CLI, AI runtime |
| `mac-rel`, `lin-rel`, `win-rel` | macOS / Linux / Windows | Optimized release builds |
| `mac-dev`, `lin-dev`, `win-dev` | macOS / Linux / Windows | Development builds with ROM-dependent tests |
| `mac-uni` | macOS | Universal binary (ARM64 + x86_64) for distribution |
| `mac-test`, `lin-test`, `win-test` | All | Optimized builds for fast test iteration |
| `ci-*` | Platform-specific | CI/CD configurations (see CMakePresets.json) |

**Tip:** Add `-v` suffix (e.g., `mac-dbg-v`) to enable verbose compiler warnings.

---

## 3. Build Directory Policy

| Build Type | Default Directory |
|------------|-------------------|
| Native (desktop/CLI) | `build/` |
| WASM | `build-wasm/` |

If you need per-user or per-agent isolation, create a local `CMakeUserPresets.json` that points `binaryDir` to a custom path.

Example:
```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
export YAZE_BUILD_ROOT="$HOME/.cache/yaze"
cmake --preset dev-local
cmake --build --preset dev-local --target yaze
```

For AI-enabled builds, use the `*-ai` presets and specify only the targets you need:
```bash
cmake --build --preset mac-ai --target yaze z3ed
```

**Windows Helper Scripts:**
- Quick builds: `scripts/agents/windows-smoke-build.ps1`
- Test runs: `scripts/agents/run-tests.sh` (or PowerShell equivalent)

---

## 4. Common Build Commands

### Standard Debug Build

**macOS:**
```bash
cmake --preset mac-dbg
cmake --build --preset mac-dbg --target yaze
```

**Linux:**
```bash
cmake --preset lin-dbg
cmake --build --preset lin-dbg --target yaze
```

**Windows:**
```bash
cmake --preset win-dbg
cmake --build --preset win-dbg --target yaze
```

### AI-Enabled Build (with gRPC and z3ed CLI)

**macOS:**
```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze z3ed
```

**Linux:**
```bash
cmake --preset lin-ai
cmake --build --preset lin-ai --target yaze z3ed
```

**Windows:**
```bash
cmake --preset win-ai
cmake --build --preset win-ai --target yaze z3ed
```

---

## 5. Testing

YAZE uses CTest with GoogleTest. Tests are organized by category using labels.

### Quick Start

```bash
# Run stable tests (fast, no ROM required)
ctest --test-dir build -L stable -j4

# Run all enabled tests
ctest --test-dir build --output-on-failure

# Run tests matching a pattern
ctest --test-dir build -R "Dungeon"
```

### Test Categories

| Category | Command | Description |
|----------|---------|-------------|
| Stable | `ctest --test-dir build -L stable` | Core unit tests, always available |
| GUI | `ctest --test-dir build -L gui` | GUI smoke tests |
| ROM-dependent | `ctest --test-dir build -L rom_dependent` | Requires a Zelda 3 ROM |
| Experimental | `ctest --test-dir build -L experimental` | AI/experimental features |

### Enabling ROM-Dependent Tests

```bash
# Configure with ROM path
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Build and run
cmake --build --preset mac-dev --target yaze_test
ctest --test-dir build -L rom_dependent
```

### Test Coverage by Preset

| Preset | Stable | GUI | ROM-Dep | Experimental |
|--------|:------:|:---:|:-------:|:------------:|
| `*-dbg` | Yes | Yes | No | No |
| `*-ai` | Yes | Yes | No | Yes |
| `*-dev` | Yes | Yes | Yes | No |
| `*-rel` | No | No | No | No |

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `YAZE_TEST_ROM_PATH` | Path to ROM for ROM-dependent tests |
| `YAZE_SKIP_ROM_TESTS` | Skip ROM tests (useful for CI) |
| `YAZE_ENABLE_UI_TESTS` | Enable GUI tests (auto-detected if display available) |

---

## 6. Further Reading

- **[Build Troubleshooting](troubleshooting.md)** - Solutions for common build issues
- **[Platform Compatibility](platform-compatibility.md)** - Platform-specific notes and CI/CD details
- **[CMake Presets Guide](presets.md)** - Complete preset reference
- **[Testing Guide](../developer/testing-guide.md)** - Comprehensive testing documentation
