# Configuration Matrix Documentation

This document defines all CMake configuration flags, their interactions, and the tested configuration combinations for the yaze project.

**Last Updated**: 2025-11-20
**Owner**: CLAUDE_MATRIX_TEST (Platform Matrix Testing Specialist)

## 1. CMake Configuration Flags

### Core Build Options

| Flag | Default | Purpose | Notes |
|------|---------|---------|-------|
| `YAZE_BUILD_GUI` | ON | Build GUI application (ImGui-based editor) | Required for desktop users |
| `YAZE_BUILD_CLI` | ON | Build CLI tools (shared libraries) | Needed for z3ed CLI |
| `YAZE_BUILD_Z3ED` | ON | Build z3ed CLI executable | Requires `YAZE_BUILD_CLI=ON` |
| `YAZE_BUILD_EMU` | ON | Build emulator components | Optional; adds ~50MB to binary |
| `YAZE_BUILD_LIB` | ON | Build static library (`libyaze.a`) | For library consumers |
| `YAZE_BUILD_TESTS` | ON | Build test suite | Required for CI validation |

### Feature Flags

| Flag | Default | Purpose | Dependencies |
|------|---------|---------|--------------|
| `YAZE_ENABLE_GRPC` | ON | Enable gRPC agent support | Requires protobuf, gRPC libraries |
| `YAZE_ENABLE_JSON` | ON | Enable JSON support (nlohmann) | Used by AI services |
| `YAZE_ENABLE_AI` | ON | Enable AI agent features (legacy) | **Deprecated**: use `YAZE_ENABLE_AI_RUNTIME` |
| `YAZE_ENABLE_REMOTE_AUTOMATION` | depends on `YAZE_ENABLE_GRPC` | Enable remote GUI automation (gRPC servers) | Requires `YAZE_ENABLE_GRPC=ON` |
| `YAZE_ENABLE_AI_RUNTIME` | depends on `YAZE_ENABLE_AI` | Enable AI runtime (Gemini/Ollama, advanced routing) | Requires `YAZE_ENABLE_AI=ON` |
| `YAZE_BUILD_AGENT_UI` | depends on `YAZE_BUILD_GUI` | Build ImGui agent/chat panels in GUI | Requires `YAZE_BUILD_GUI=ON` |
| `YAZE_ENABLE_AGENT_CLI` | depends on `YAZE_BUILD_CLI` | Build conversational agent CLI stack | Auto-enabled if `YAZE_BUILD_CLI=ON` or `YAZE_BUILD_Z3ED=ON` |
| `YAZE_ENABLE_HTTP_API` | depends on `YAZE_ENABLE_AGENT_CLI` | Enable HTTP REST API server | Requires `YAZE_ENABLE_AGENT_CLI=ON` |

### Optimization & Debug Flags

| Flag | Default | Purpose | Notes |
|------|---------|---------|-------|
| `YAZE_ENABLE_LTO` | OFF | Link-time optimization | Increases build time by ~30% |
| `YAZE_ENABLE_SANITIZERS` | OFF | AddressSanitizer/UBSanitizer | For memory safety debugging |
| `YAZE_ENABLE_COVERAGE` | OFF | Code coverage tracking | For testing metrics |
| `YAZE_UNITY_BUILD` | OFF | Unity (Jumbo) builds | May hide include issues |

### Development & CI Options

| Flag | Default | Purpose | Notes |
|------|---------|---------|-------|
| `YAZE_ENABLE_ROM_TESTS` | OFF | Enable ROM-dependent tests | Requires vanilla ROM (`alttp_vanilla.sfc`) via `YAZE_TEST_ROM_VANILLA` |
| `YAZE_MINIMAL_BUILD` | OFF | Minimal CI build (skip optional features) | Used in resource-constrained CI |
| `YAZE_SUPPRESS_WARNINGS` | ON | Suppress compiler warnings | Use OFF for verbose builds |

## 2. Flag Interactions & Constraints

### Automatic Constraint Resolution

The CMake configuration automatically enforces these constraints:

```cmake
# REMOTE_AUTOMATION forces GRPC
if(YAZE_ENABLE_REMOTE_AUTOMATION AND NOT YAZE_ENABLE_GRPC)
  set(YAZE_ENABLE_GRPC ON CACHE BOOL ... FORCE)
endif()

# Disabling REMOTE_AUTOMATION forces GRPC OFF
if(NOT YAZE_ENABLE_REMOTE_AUTOMATION)
  set(YAZE_ENABLE_GRPC OFF CACHE BOOL ... FORCE)
endif()

# AI_RUNTIME forces AI enabled
if(YAZE_ENABLE_AI_RUNTIME AND NOT YAZE_ENABLE_AI)
  set(YAZE_ENABLE_AI ON CACHE BOOL ... FORCE)
endif()

# Disabling AI_RUNTIME forces AI OFF
if(NOT YAZE_ENABLE_AI_RUNTIME)
  set(YAZE_ENABLE_AI OFF CACHE BOOL ... FORCE)
endif()

# BUILD_CLI or BUILD_Z3ED forces AGENT_CLI ON
if((YAZE_BUILD_CLI OR YAZE_BUILD_Z3ED) AND NOT YAZE_ENABLE_AGENT_CLI)
  set(YAZE_ENABLE_AGENT_CLI ON CACHE BOOL ... FORCE)
endif()

# HTTP_API forces AGENT_CLI ON
if(YAZE_ENABLE_HTTP_API AND NOT YAZE_ENABLE_AGENT_CLI)
  set(YAZE_ENABLE_AGENT_CLI ON CACHE BOOL ... FORCE)
endif()

# AGENT_UI requires BUILD_GUI
if(YAZE_BUILD_AGENT_UI AND NOT YAZE_BUILD_GUI)
  set(YAZE_BUILD_AGENT_UI OFF CACHE BOOL ... FORCE)
endif()
```

### Dependency Graph

```
YAZE_ENABLE_REMOTE_AUTOMATION
  ├─ Requires: YAZE_ENABLE_GRPC
  └─ Requires: gRPC libraries, protobuf

YAZE_ENABLE_AI_RUNTIME
  ├─ Requires: YAZE_ENABLE_AI
  ├─ Requires: yaml-cpp, OpenSSL
  └─ Requires: Gemini/Ollama HTTP clients

YAZE_BUILD_AGENT_UI
  ├─ Requires: YAZE_BUILD_GUI
  └─ Requires: ImGui bindings

YAZE_ENABLE_AGENT_CLI
  ├─ Requires: YAZE_BUILD_CLI OR YAZE_BUILD_Z3ED
  └─ Requires: ftxui, various CLI handlers

YAZE_ENABLE_HTTP_API
  ├─ Requires: YAZE_ENABLE_AGENT_CLI
  └─ Requires: cpp-httplib

YAZE_ENABLE_JSON
  ├─ Requires: nlohmann_json
  └─ Used by: Gemini AI service, HTTP API
```

## 3. Tested Configuration Matrix

### Rationale

Testing all 2^N combinations is infeasible (18 flags = 262,144 combinations). Instead, we test:
1. **Baseline**: All defaults (realistic user scenario)
2. **Extremes**: All ON, All OFF (catch hidden assumptions)
3. **Interactions**: Known problematic combinations
4. **CI Presets**: Predefined workflows (dev, ci, minimal, release)
5. **Platform-specific**: Windows GRPC, macOS universal binary, Linux GCC

### Matrix Definition

#### Tier 1: Core Platform Builds (CI Standard)

These run on every PR and push:

| Name | Platform | GRPC | AI | AGENT_UI | CLI | Tests | Purpose |
|------|----------|------|----|-----------|----|-------|---------|
| `ci-linux` | Linux | ON | OFF | OFF | ON | ON | Server-side agent |
| `ci-macos` | macOS | ON | OFF | ON | ON | ON | Agent UI + CLI |
| `ci-windows` | Windows | ON | OFF | OFF | ON | ON | Core Windows build |

#### Tier 2: Feature Combination Tests (Nightly or On-Demand)

These test specific flag combinations:

| Name | GRPC | REMOTE_AUTO | JSON | AI | AI_RUNTIME | AGENT_UI | HTTP_API | Tests |
|------|------|-------------|------|----|-----------  |----------|----------|-------|
| `minimal` | OFF | OFF | ON | OFF | OFF | OFF | OFF | ON |
| `grpc-only` | ON | OFF | ON | OFF | OFF | OFF | OFF | ON |
| `full-ai` | ON | ON | ON | ON | ON | ON | ON | ON |
| `cli-only` | ON | ON | ON | ON | ON | OFF | ON | ON |
| `gui-only` | OFF | OFF | ON | OFF | OFF | ON | OFF | ON |
| `http-api` | ON | ON | ON | ON | ON | OFF | ON | ON |
| `no-json` | ON | ON | OFF | ON | OFF | OFF | OFF | ON |
| `all-off` | OFF | OFF | OFF | OFF | OFF | OFF | OFF | ON |

#### Tier 3: Platform-Specific Builds

| Name | Platform | Configuration | Special Notes |
|------|----------|----------------|-----------------|
| `win-ai` | Windows | Full AI + gRPC | CI Windows-specific preset |
| `win-arm` | Windows ARM64 | Debug, no AI | ARM64 architecture test |
| `mac-uni` | macOS | Universal binary | ARM64 + x86_64 |
| `lin-ai` | Linux | Full AI + gRPC | Server-side full stack |

## 4. Problematic Combinations

### Known Issue Patterns

#### Pattern A: GRPC Without REMOTE_AUTOMATION

**Status**: FIXED IN CMAKE
**Symptom**: gRPC headers included but no automation server compiled
**Why it matters**: Causes link errors if server code missing
**Resolution**: REMOTE_AUTOMATION now forces GRPC=ON via CMake constraint

#### Pattern B: HTTP_API Without AGENT_CLI

**Status**: FIXED IN CMAKE
**Symptom**: HTTP API endpoints defined but no CLI handler context
**Why it matters**: REST API has no command dispatcher
**Resolution**: HTTP_API now forces AGENT_CLI=ON via CMake constraint

#### Pattern C: AGENT_UI Without BUILD_GUI

**Status**: FIXED IN CMAKE
**Symptom**: ImGui panels compiled for headless build
**Why it matters**: Wastes space, may cause UI binding issues
**Resolution**: AGENT_UI now disabled if BUILD_GUI=OFF

#### Pattern D: AI_RUNTIME Without JSON

**Status**: TESTING
**Symptom**: Gemini service requires JSON parsing
**Why it matters**: Gemini HTTPS support needs JSON deserialization
**Resolution**: Gemini only linked when both AI_RUNTIME AND JSON enabled

#### Pattern E: Windows + GRPC + gRPC v1.67.1

**Status**: DOCUMENTED
**Symptom**: MSVC compatibility issues with older gRPC versions
**Why it matters**: gRPC <1.68.0 has MSVC ABI mismatches
**Resolution**: ci-windows preset pins to tested stable version

#### Pattern F: macOS ARM64 + Unknown Dependencies

**Status**: DOCUMENTED
**Symptom**: Homebrew brew dependencies may not have arm64 support
**Why it matters**: Cross-architecture builds fail silently
**Resolution**: mac-uni preset tests both architectures

## 5. Test Coverage by Configuration

### What Each Configuration Validates

#### Minimal Build
- Core editor functionality without AI/CLI
- Smallest binary size
- Most compatible (no gRPC, no network)
- Target users: GUI-only, offline users

#### gRPC Only
- Server-side agent without AI services
- GUI automation without language model
- Useful for: Headless automation

#### Full AI Stack
- All features enabled
- Gemini + Ollama support
- Advanced routing + proposal planning
- Target users: AI-assisted ROM hacking

#### CLI Only
- z3ed command-line tool
- No GUI components
- Server-side focused
- Target users: Scripting, CI/CD integration

#### GUI Only
- Traditional desktop editor
- No network services
- Suitable for: Casual players

#### HTTP API
- REST endpoints for external tools
- Integration with other ROM editors
- JSON-based communication

#### No JSON
- Validates JSON is truly optional
- Tests Ollama-only mode (no Gemini)
- Smaller binary alternative

#### All Off
- Validates minimum viable configuration
- Basic ROM reading/writing only
- Edge case handling

## 6. Running Configuration Matrix Tests

### Local Testing

```bash
# Run entire local matrix
./scripts/test-config-matrix.sh

# Run specific configuration
./scripts/test-config-matrix.sh --config minimal
./scripts/test-config-matrix.sh --config full-ai

# Smoke test only (no full build)
./scripts/test-config-matrix.sh --smoke

# Verbose output
./scripts/test-config-matrix.sh --verbose
```

### CI Testing

Matrix tests run nightly via `.github/workflows/matrix-test.yml`:

```yaml
# Automatic testing of all Tier 2 combinations on all platforms
# Run time: ~45 minutes (parallel execution)
# Triggered: On schedule (2 AM UTC daily) or manual dispatch
```

### Building Specific Preset

```bash
# Linux
cmake --preset ci-linux -B build_ci -DYAZE_ENABLE_GRPC=ON
cmake --build build_ci

# Windows
cmake --preset ci-windows -B build_ci
cmake --build build_ci --config RelWithDebInfo

# macOS Universal
cmake --preset mac-uni -B build_uni -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build_uni
```

## 7. Configuration Dependencies Reference

### For Pull Requests

Use this checklist when modifying CMake configuration:

- [ ] Added new `option()`? Document in Section 1 above
- [ ] New dependency? Document in Section 2 (Dependency Graph)
- [ ] New feature flag? Add to relevant Tier in Section 3
- [ ] Problematic combination? Document in Section 4
- [ ] Update test matrix script if testing approach changes

### For Developers

Quick reference when debugging build issues:

1. **gRPC link errors?** Check: `YAZE_ENABLE_GRPC=ON` requires `YAZE_ENABLE_REMOTE_AUTOMATION=ON` (auto-enforced)
2. **Gemini compile errors?** Verify: `YAZE_ENABLE_AI_RUNTIME=ON AND YAZE_ENABLE_JSON=ON`
3. **Agent UI missing?** Check: `YAZE_BUILD_GUI=ON AND YAZE_BUILD_AGENT_UI=ON`
4. **CLI commands not found?** Verify: `YAZE_ENABLE_AGENT_CLI=ON` (auto-forced by `YAZE_BUILD_CLI=ON`)
5. **HTTP API endpoints undefined?** Check: `YAZE_ENABLE_HTTP_API=ON` forces `YAZE_ENABLE_AGENT_CLI=ON`

## 8. Future Improvements

Potential enhancements as project evolves:

- [ ] Separate AI_RUNTIME from ENABLE_AI (currently coupled)
- [ ] Add YAZE_ENABLE_GRPC_STRICT flag for stricter server-side validation
- [ ] Document platform-specific library version constraints
- [ ] Add automated configuration lint tool
- [ ] Track binary size impact per feature flag combination
- [ ] Add performance benchmarks for each Tier 2 configuration
