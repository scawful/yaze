# YAZE Build Scripts

This directory contains build automation and maintenance scripts for the YAZE project.

## fetch_usdasm.sh

Fetch the usdasm disassembly on demand (not vendored in the repo).

```bash
scripts/fetch_usdasm.sh
# or: USDASM_DIR=/path/to/usdasm scripts/fetch_usdasm.sh
```

## install-nightly.sh

Builds and installs an isolated nightly build in a separate clone (no dev build overlap)
and creates wrapper commands (`yaze-nightly`, `yaze-nightly-grpc`, `z3ed-nightly`,
`yaze-mcp-nightly`) under `~/.local/bin`.

```bash
scripts/install-nightly.sh
YAZE_NIGHTLY_REPO="$HOME/.yaze/nightly/repo" YAZE_NIGHTLY_BUILD_TYPE=RelWithDebInfo scripts/install-nightly.sh
```

### What it does
- Clones `origin` into `$YAZE_NIGHTLY_REPO` (default `~/.yaze/nightly/repo`) and keeps it clean.
- Builds into `$YAZE_NIGHTLY_BUILD_DIR` (default `~/.yaze/nightly/repo/build-nightly`).
- Installs into `$YAZE_NIGHTLY_PREFIX/releases/<timestamp>` and updates `.../current`.
- Writes wrapper scripts to `$YAZE_NIGHTLY_BIN_DIR` (default `~/.local/bin`).
- On macOS, creates a stable app link at `~/Applications/Yaze Nightly.app` for menu launchers.

### Environment Overrides
- `YAZE_NIGHTLY_REPO` (default `~/.yaze/nightly/repo`)
- `YAZE_NIGHTLY_BRANCH` (default `master`)
- `YAZE_NIGHTLY_BUILD_DIR` (default `$YAZE_NIGHTLY_REPO/build-nightly`)
- `YAZE_NIGHTLY_BUILD_TYPE` (default `RelWithDebInfo`)
- `YAZE_NIGHTLY_PREFIX` (default `~/.local/yaze/nightly`)
- `YAZE_NIGHTLY_BIN_DIR` (default `~/.local/bin`)
- `YAZE_NIGHTLY_APP_DIR` (default `~/Applications`, macOS only)
- `YAZE_GRPC_HOST`/`YAZE_GRPC_PORT` (for `yaze-mcp-nightly`, defaults `localhost:50051`)
- `YAZE_MCP_REPO` (default `~/.yaze/yaze-mcp`)

### Typical usage
```bash
yaze-nightly-grpc     # start GUI with gRPC on 50051
yaze-mcp-nightly      # start MCP server against the same port
z3ed-nightly          # run CLI without touching dev build
```

### Removal
```bash
rm -rf ~/.local/yaze/nightly ~/.local/bin/yaze-nightly* ~/.local/bin/z3ed-nightly ~/.local/bin/yaze-mcp-nightly
```

See `docs/public/build/quick-reference.md` for environment overrides and runtime notes.

## build-ios.sh

Builds iOS static libraries via CMake and generates a thin Xcode project via XcodeGen.

```bash
scripts/build-ios.sh           # defaults to ios-debug
scripts/build-ios.sh ios-release
```

Requirements:
- Xcode (iOS SDK installed)
- XcodeGen (`brew install xcodegen`)

Outputs:
- `build-ios/ios/libyaze_ios_bundle.a`
- `src/ios/yaze_ios.xcodeproj`

## xcodebuild-ios.sh

Builds (and optionally archives/exports) the iOS app via `xcodebuild`.

```bash
# Simulator build (no signing)
scripts/xcodebuild-ios.sh ios-sim-debug build

# Device build (requires signing)
scripts/xcodebuild-ios.sh ios-debug build

# Device archive + export development .ipa
scripts/xcodebuild-ios.sh ios-debug ipa

# Build + install directly to paired device (defaults to "Baby Pad")
scripts/xcodebuild-ios.sh ios-debug deploy
scripts/xcodebuild-ios.sh ios-debug deploy "Baby Pad"
```

If you see `No Accounts` or provisioning errors when building on a headless/CI
machine, pass an App Store Connect authentication key (avoids Xcode Accounts):

```bash
export XCODE_AUTH_KEY_PATH=/path/to/AuthKey_XXXXXX.p8
export XCODE_AUTH_KEY_ID=XXXXXX
export XCODE_AUTH_KEY_ISSUER_ID=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
scripts/xcodebuild-ios.sh ios-debug ipa
```

If you need to sign with your own Team / bundle ID (recommended for local device
builds), override these build settings:

```bash
export YAZE_IOS_TEAM_ID=YOUR_TEAM_ID
export YAZE_IOS_BUNDLE_ID=com.yourcompany.yaze-ios
export YAZE_ICLOUD_CONTAINER_ID="iCloud.${YAZE_IOS_BUNDLE_ID}"
scripts/xcodebuild-ios.sh ios-debug build
```

If you see iCloud/CloudKit entitlement errors, ensure the App ID has iCloud
capability enabled and the referenced iCloud container exists under your team.

Tip: put your exports in `scripts/signing.env` (gitignored). `scripts/xcodebuild-ios.sh`
will auto-source it.

Deploy notes:

- `deploy` installs with `xcrun devicectl device install app`.
- Device resolution order: `DEVICE` arg -> `$YAZE_IOS_DEVICE` -> `"Baby Pad"`.
- Set `YAZE_IOS_LAUNCH_AFTER_DEPLOY=0` to skip automatic app launch.

## build_cleaner.py

Automates CMake source list maintenance and header include management with IWYU-style analysis.

### Features

1. **CMake Source List Maintenance**: Automatically updates source file lists in CMake files
2. **Self-Header Includes**: Ensures source files include their corresponding headers
3. **IWYU-Style Analysis**: Suggests missing headers based on symbol usage
4. **.gitignore Support**: Respects .gitignore patterns when scanning files
5. **Auto-Discovery**: Can discover CMake libraries that opt-in to auto-maintenance

### Usage

```bash
# Dry-run to see what would change (recommended first step)
python3 scripts/build_cleaner.py --dry-run

# Update CMake source lists and header includes
python3 scripts/build_cleaner.py

# Run IWYU-style header analysis
python3 scripts/build_cleaner.py --iwyu

# Auto-discover CMake libraries marked for auto-maintenance
python3 scripts/build_cleaner.py --auto-discover

# Update only CMake source lists
python3 scripts/build_cleaner.py --cmake-only

# Update only header includes
python3 scripts/build_cleaner.py --includes-only
```

### Opting-In to Auto-Maintenance

By default, the script only auto-maintains source lists that are explicitly marked. To mark a CMake variable for auto-maintenance, add a comment above the `set()` statement:

```cmake
# This list is auto-maintained by scripts/build_cleaner.py
set(
  YAZE_APP_EMU_SRC
  app/emu/audio/apu.cc
  app/emu/cpu/cpu.cc
  # ... more files
)
```

The script looks for comments containing "auto-maintain" (case-insensitive) within 3 lines above the `set()` statement.

### Excluding Files from Processing

To exclude a specific file from all processing (CMake lists, header includes, IWYU), add this token near the top of the file:

```cpp
// build_cleaner:ignore
```

### .gitignore Support

The script automatically respects `.gitignore` patterns. To enable this feature, install the `pathspec` dependency:

```bash
pip3 install -r scripts/requirements.txt
# or
pip3 install pathspec
```

### IWYU Configuration

The script includes basic IWYU-style analysis that suggests headers based on symbol prefixes. To customize which headers are suggested, edit the `COMMON_HEADERS` dictionary in the script:

```python
COMMON_HEADERS = {
    'std::': ['<memory>', '<string>', '<vector>', ...],
    'absl::': ['<absl/status/status.h>', ...],
    'ImGui::': ['<imgui.h>'],
    'SDL_': ['<SDL.h>'],
}
```

**Note**: The IWYU analysis is conservative and may suggest headers that are already transitively included. Use with care and review suggestions before applying.

### Integration with CMake

The script is integrated into the CMake build system:

```bash
# Run as a CMake target
cmake --build build --target build_cleaner
```

### Dependencies

- Python 3.7+
- `pathspec` (optional, for .gitignore support): `pip3 install pathspec`

### How It Works

1. **CMake Maintenance**: Scans directories specified in the configuration and updates `set(VAR_NAME ...)` blocks with the current list of source files
2. **Self-Headers**: For each `.cc`/`.cpp` file, ensures it includes its corresponding `.h` file
3. **IWYU Analysis**: Scans source files for symbols and suggests appropriate headers based on prefix matching

### Current Auto-Maintained Variables

**All 20 library source lists are now auto-maintained by default:**

- Core: `YAZE_APP_EMU_SRC`, `YAZE_APP_CORE_SRC`, `YAZE_APP_EDITOR_SRC`, `YAZE_APP_ZELDA3_SRC`, `YAZE_NET_SRC`, `YAZE_UTIL_SRC`
- GFX: `GFX_TYPES_SRC`, `GFX_BACKEND_SRC`, `GFX_RESOURCE_SRC`, `GFX_CORE_SRC`, `GFX_UTIL_SRC`, `GFX_RENDER_SRC`, `GFX_DEBUG_SRC`
- GUI: `GUI_CORE_SRC`, `CANVAS_SRC`, `GUI_WIDGETS_SRC`, `GUI_AUTOMATION_SRC`, `GUI_APP_SRC`
- Other: `YAZE_AGENT_SOURCES`, `YAZE_TEST_SOURCES`

The script intelligently preserves conditional blocks (if/endif) and excludes conditional files from the main source list.

## verify-build-environment.\*

`verify-build-environment.ps1` (Windows) and `verify-build-environment.sh` (macOS/Linux) are the primary diagnostics for contributors. They now:

- Check for `clang-cl`, Ninja, NASM, Visual Studio workloads, and VS Code (optional).
- Validate vcpkg bootstrap status plus `vcpkg/installed` cache contents.
- Warn about missing ROM assets (`roms/alttp_vanilla.sfc`, etc.).
- Offer `-FixIssues` and `-CleanCache` switches to repair Git config, resync submodules, and wipe stale build directories.

Run the script once per machine (and rerun after major toolchain updates) to ensure presets such as `win-dbg`, `win-ai`, `mac-ai`, and `ci-windows-ai` have everything they need.

## setup-vcpkg-windows.ps1

Automates the vcpkg bootstrap flow on Windows:

1. Clones and bootstraps vcpkg (if not already present).
2. Verifies that `git`, `clang-cl`, and Ninja are available, printing friendly instructions when they are missing.
3. Installs the default triplet (`x64-windows` or `arm64-windows` when detected) and confirms that `vcpkg/installed/<triplet>` is populated.
4. Reminds you to rerun `.\scripts\verify-build-environment.ps1 -FixIssues` to double-check the environment.

Use it immediately after cloning the repository or whenever you need to refresh your local dependency cache before running `win-ai` or `ci-windows-ai` presets.

## CMake Validation Tools

A comprehensive toolkit for validating CMake configuration and catching dependency issues early. These tools help prevent build failures by detecting configuration problems before compilation.

### validate-cmake-config.cmake

Validates CMake configuration by checking targets, flags, and platform-specific settings.

```bash
# Validate default build directory
cmake -P scripts/validate-cmake-config.cmake

# Validate specific build directory
cmake -P scripts/validate-cmake-config.cmake build
```

**What it checks:**
- Required targets exist
- Feature flag consistency (AI requires gRPC, etc.)
- Compiler settings (C++23, MSVC runtime on Windows)
- Abseil configuration on Windows (prevents missing include issues)
- Output directories
- Common configuration mistakes

### check-include-paths.sh

Validates include paths in compile_commands.json to catch missing includes before build.

```bash
# Check default build directory
./scripts/check-include-paths.sh

# Check specific build
./scripts/check-include-paths.sh build

# Verbose mode (show all include dirs)
VERBOSE=1 ./scripts/check-include-paths.sh build
```

**Requires:** `jq` for better parsing (optional but recommended): `brew install jq`

**What it checks:**
- Common dependencies (SDL2, ImGui, yaml-cpp)
- Platform-specific includes
- Abseil includes from gRPC build (critical on Windows)
- Suspicious configurations (missing -I flags, relative paths)

### visualize-deps.py

Generates dependency graphs and detects circular dependencies.

```bash
# Generate GraphViz diagram
python3 scripts/visualize-deps.py build --format graphviz -o deps.dot
dot -Tpng deps.dot -o deps.png

# Generate Mermaid diagram
python3 scripts/visualize-deps.py build --format mermaid -o deps.mmd

# Show statistics
python3 scripts/visualize-deps.py build --stats
```

**Formats:**
- **graphviz**: DOT format for rendering with `dot` command
- **mermaid**: For embedding in Markdown/documentation
- **text**: Simple text tree for quick overview

**Features:**
- Detects circular dependencies (highlighted in red)
- Shows dependency statistics
- Color-coded targets (executables, libraries, etc.)

### test-cmake-presets.sh

Tests that all CMake presets can configure successfully.

```bash
# Test all presets for current platform
./scripts/test-cmake-presets.sh

# Test specific preset
./scripts/test-cmake-presets.sh --preset mac-ai

# Test in parallel (faster)
./scripts/test-cmake-presets.sh --platform mac --parallel 4

# Quick mode (don't clean between tests)
./scripts/test-cmake-presets.sh --quick
```

**Options:**
- `--parallel N`: Test N presets in parallel (default: 4)
- `--preset NAME`: Test only specific preset
- `--platform PLATFORM`: Test only mac/win/lin presets
- `--quick`: Skip cleaning between tests
- `--verbose`: Show full CMake output

### Usage in Development Workflow

**After configuring CMake:**
```bash
cmake --preset mac-ai
cmake -P scripts/validate-cmake-config.cmake build
./scripts/check-include-paths.sh build
```

**Before committing:**
```bash
# Test all platform presets configure successfully
./scripts/test-cmake-presets.sh --platform mac
```

**When adding new targets:**
```bash
# Check for circular dependencies
python3 scripts/visualize-deps.py build --stats
```

**For full details**, see [docs/internal/testing/cmake-validation.md](../docs/internal/testing/cmake-validation.md)

## Symbol Conflict Detection

Tools to detect One Definition Rule (ODR) violations and duplicate symbol definitions **before linking fails**.

### Quick Start

```bash
# Extract symbols from object files
./scripts/extract-symbols.sh

# Check for conflicts
./scripts/check-duplicate-symbols.sh

# Run tests
./scripts/test-symbol-detection.sh
```

### Scripts

#### extract-symbols.sh

Scans compiled object files and creates a JSON database of all symbols and their locations.

**Features:**
- Cross-platform: macOS/Linux (nm), Windows (dumpbin)
- Fast: ~2-3 seconds for typical builds
- Identifies duplicate definitions across object files
- Tracks symbol type (text, data, read-only, etc.)

**Usage:**
```bash
# Extract from build directory
./scripts/extract-symbols.sh build

# Custom output file
./scripts/extract-symbols.sh build symbols.json
```

**Output:** `build/symbol_database.json` - JSON with symbol conflicts listed

#### check-duplicate-symbols.sh

Analyzes symbol database and reports conflicts in developer-friendly format.

**Usage:**
```bash
# Check default database
./scripts/check-duplicate-symbols.sh

# Verbose output
./scripts/check-duplicate-symbols.sh --verbose

# Include fix suggestions
./scripts/check-duplicate-symbols.sh --fix-suggestions
```

**Exit codes:**
- `0` = No conflicts found
- `1` = Conflicts detected (fails in CI/pre-commit)

#### test-symbol-detection.sh

Integration test suite for symbol detection system.

**Usage:**
```bash
./scripts/test-symbol-detection.sh
```

**Validates:**
- Scripts are executable
- Build directory and object files exist
- Symbol extraction works correctly
- JSON database is valid
- Duplicate checker runs successfully
- Pre-commit hook is configured

### Git Hook Integration

**First-time setup:**
```bash
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

The pre-commit hook automatically runs symbol checks on changed files:
- Fast: ~1-2 seconds
- Only checks affected objects
- Warns about conflicts
- Can skip with `--no-verify` if needed

### CI/CD Integration

The `symbol-detection.yml` GitHub Actions workflow runs on:
- All pushes to `master` and `develop`
- All pull requests affecting C++ files
- Workflows can be triggered manually

**What it does:**
1. Builds project
2. Extracts symbols from all object files
3. Checks for conflicts
4. Uploads symbol database as artifact
5. Fails job if conflicts found

### Common Fixes

**Duplicate global variable:**
```cpp
// Bad - defined in both files
ABSL_FLAG(std::string, rom, "", "ROM path");

// Fix 1: Use static (internal linkage)
static ABSL_FLAG(std::string, rom, "", "ROM path");

// Fix 2: Use anonymous namespace
namespace {
  ABSL_FLAG(std::string, rom, "", "ROM path");
}
```

**Duplicate function:**
```cpp
// Bad - defined in both files
void ProcessData() { /* ... */ }

// Fix: Make inline or use static
inline void ProcessData() { /* ... */ }
```

### Performance

| Operation | Time |
|-----------|------|
| Extract (4000 objects, macOS) | ~3s |
| Extract (4000 objects, Windows) | ~5-7s |
| Check duplicates | <100ms |
| Pre-commit hook | ~1-2s |

### Documentation

Full documentation available in:
- [docs/internal/testing/symbol-conflict-detection.md](../docs/internal/testing/symbol-conflict-detection.md)
- [docs/internal/testing/sample-symbol-database.json](../docs/internal/testing/sample-symbol-database.json)

## AI Model Evaluation Suite

Tools for evaluating and comparing AI models used with the z3ed CLI agent system. Located in `scripts/ai/`.

### Quick Start

```bash
# Run a quick smoke test
./scripts/ai/run-model-eval.sh --quick

# Evaluate specific models
./scripts/ai/run-model-eval.sh --models llama3.2,qwen2.5-coder

# Evaluate all available models
./scripts/ai/run-model-eval.sh --all

# Evaluate with comparison report
./scripts/ai/run-model-eval.sh --default --compare
```

### Components

#### run-model-eval.sh

Main entry point script. Handles prerequisites checking, model pulling, and orchestrates the evaluation.

**Options:**
- `--models, -m LIST` - Comma-separated list of models to evaluate
- `--all` - Evaluate all available Ollama models
- `--default` - Evaluate default models from config (llama3.2, qwen2.5-coder, etc.)
- `--tasks, -t LIST` - Task categories: rom_inspection, code_analysis, tool_calling, conversation
- `--timeout SEC` - Timeout per task (default: 120)
- `--quick` - Quick smoke test (single model, fewer tasks)
- `--compare` - Generate comparison report after evaluation
- `--dry-run` - Show what would run without executing

#### eval-runner.py

Python evaluation engine that runs tasks against models and scores responses.

**Features:**
- Multi-model evaluation
- Pattern-based accuracy scoring
- Response completeness analysis
- Tool usage detection
- Response time measurement
- JSON output for analysis

**Direct usage:**
```bash
python scripts/ai/eval-runner.py \
  --models llama3.2,qwen2.5-coder \
  --tasks all \
  --output results/eval-$(date +%Y%m%d).json
```

#### compare-models.py

Generates comparison reports from evaluation results.

**Formats:**
- `--format table` - ASCII table (default)
- `--format markdown` - Markdown with analysis
- `--format json` - Machine-readable JSON

**Usage:**
```bash
# Compare all recent evaluations
python scripts/ai/compare-models.py results/eval-*.json

# Generate markdown report
python scripts/ai/compare-models.py --format markdown --output report.md results/*.json

# Get best model name (for scripting)
BEST_MODEL=$(python scripts/ai/compare-models.py --best results/eval-*.json)
```

#### eval-tasks.yaml

Task definitions and scoring configuration. Categories:

| Category | Description | Example Tasks |
|----------|-------------|---------------|
| rom_inspection | ROM data structure queries | List dungeons, describe maps |
| code_analysis | Code understanding tasks | Explain functions, find bugs |
| tool_calling | Tool usage evaluation | File operations, build commands |
| conversation | Multi-turn dialog | Follow-ups, clarifications |

**Scoring dimensions:**
- **Accuracy** (40%): Pattern matching against expected responses
- **Completeness** (30%): Response depth and structure
- **Tool Usage** (20%): Appropriate tool selection
- **Response Time** (10%): Speed (normalized to 0-10)

### Output

Results are saved to `scripts/ai/results/`:
- `eval-YYYYMMDD-HHMMSS.json` - Individual evaluation results
- `comparison-YYYYMMDD-HHMMSS.md` - Comparison reports

**Sample output:**
```
┌──────────────────────────────────────────────────────────────────────┐
│                    YAZE AI Model Evaluation Report                   │
├──────────────────────────────────────────────────────────────────────┤
│ Model                    │ Accuracy   │ Tool Use   │ Speed   │ Runs │
├──────────────────────────────────────────────────────────────────────┤
│ qwen2.5-coder:7b         │     8.8/10 │     9.2/10 │    2.1s │     3 │
│ llama3.2:latest          │     7.9/10 │     7.5/10 │    2.3s │     3 │
│ codellama:7b             │     7.2/10 │     8.1/10 │    2.8s │     3 │
├──────────────────────────────────────────────────────────────────────┤
│ Recommended: qwen2.5-coder:7b (score: 8.7/10)                        │
└──────────────────────────────────────────────────────────────────────┘
```

### Prerequisites

- **Ollama**: Install from https://ollama.ai
- **Python 3.10+** with `requests` and `pyyaml`:
  ```bash
  pip install requests pyyaml
  ```
- **At least one model pulled**:
  ```bash
  ollama pull llama3.2
  ```

### Adding Custom Tasks

Edit `scripts/ai/eval-tasks.yaml` to add new evaluation tasks:

```yaml
categories:
  custom_category:
    description: "My custom tasks"
    tasks:
      - id: "my_task"
        name: "My Task Name"
        prompt: "What is the purpose of..."
        expected_patterns:
          - "expected|keyword|pattern"
        required_tool: null
        scoring:
          accuracy_criteria: "Must mention X, Y, Z"
          completeness_criteria: "Should include examples"
```

### Integration with CI

The evaluation suite can be integrated into CI pipelines:

```yaml
# .github/workflows/ai-eval.yml
- name: Run AI Evaluation
  run: |
    ollama serve &
    sleep 5
    ollama pull llama3.2
    ./scripts/ai/run-model-eval.sh --models llama3.2 --tasks tool_calling
```
