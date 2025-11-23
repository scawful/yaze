# App Development Agent Tools Specification

**Document Version**: 1.0
**Date**: 2025-11-22
**Author**: CLAUDE_AIINF
**Purpose**: Define tools that enable AI agents to assist with yaze C++ development

## Executive Summary

This document specifies new tools for the yaze AI agent system that enable agents to assist with C++ application development. These tools complement existing ROM manipulation and editor tools by providing build system interaction, code analysis, debugging assistance, and editor integration capabilities.

## Tool Architecture Overview

### Integration Points
- **ToolDispatcher**: Central routing via `src/cli/service/agent/tool_dispatcher.cc`
- **CommandHandler Pattern**: All tools inherit from `resources::CommandHandler`
- **Output Formatting**: JSON and text formats via `resources::OutputFormatter`
- **Security Model**: Sandboxed execution, project-restricted access
- **Async Support**: Long-running operations use background execution

## Tool Specifications

### 1. Build System Tools

#### 1.1 build_configure
**Purpose**: Configure CMake build with appropriate presets and options
**Priority**: P0 (Critical for MVP)

**Parameters**:
```cpp
struct BuildConfigureParams {
  std::string preset;        // e.g., "mac-dbg", "lin-ai", "win-rel"
  std::string build_dir;     // e.g., "build_ai" (default: "build")
  bool clean_build;          // Remove existing build directory first
  std::vector<std::string> options; // Additional CMake options
};
```

**Returns**:
```json
{
  "status": "success",
  "preset": "mac-dbg",
  "build_directory": "/Users/scawful/Code/yaze/build_ai",
  "cmake_version": "3.28.0",
  "compiler": "AppleClang 15.0.0.15000100",
  "options_applied": ["YAZE_ENABLE_AI=ON", "YAZE_ENABLE_GRPC=ON"]
}
```

**Implementation Approach**:
- Execute `cmake --preset <preset>` via subprocess
- Parse CMakeCache.txt for configuration details
- Validate preset exists in CMakePresets.json
- Support parallel build directories for agent isolation

---

#### 1.2 build_compile
**Purpose**: Trigger compilation of specific targets or entire project
**Priority**: P0 (Critical for MVP)

**Parameters**:
```cpp
struct BuildCompileParams {
  std::string build_dir;      // Build directory (default: "build")
  std::string target;          // Specific target or "all"
  int jobs;                    // Parallel jobs (default: CPU count)
  bool verbose;                // Show detailed compiler output
  bool continue_on_error;      // Continue building after errors
};
```

**Returns**:
```json
{
  "status": "failed",
  "target": "yaze",
  "errors": [
    {
      "file": "src/app/editor/overworld_editor.cc",
      "line": 234,
      "column": 15,
      "severity": "error",
      "message": "use of undeclared identifier 'LoadGraphics'",
      "context": "  LoadGraphics();"
    }
  ],
  "warnings_count": 12,
  "build_time_seconds": 45.3,
  "artifacts": ["bin/yaze", "bin/z3ed"]
}
```

**Implementation Approach**:
- Execute `cmake --build <dir> --target <target> -j<jobs>`
- Parse compiler output with regex patterns for errors/warnings
- Track build timing and resource usage
- Support incremental builds and error recovery

---

#### 1.3 build_test
**Purpose**: Execute test suites with filtering and result parsing
**Priority**: P0 (Critical for MVP)

**Parameters**:
```cpp
struct BuildTestParams {
  std::string build_dir;       // Build directory
  std::string suite;            // Test suite: "unit", "integration", "e2e", "all"
  std::string filter;           // Test name filter (gtest pattern)
  bool rom_dependent;           // Include ROM-dependent tests
  std::string rom_path;         // Path to ROM file (if rom_dependent)
  bool show_output;             // Display test output
};
```

**Returns**:
```json
{
  "status": "failed",
  "suite": "unit",
  "tests_run": 156,
  "tests_passed": 154,
  "tests_failed": 2,
  "failures": [
    {
      "test_name": "SnesColorTest.ConvertRgbToSnes",
      "file": "test/unit/gfx/snes_color_test.cc",
      "line": 45,
      "failure_message": "Expected: 0x7FFF\n  Actual: 0x7FFE"
    }
  ],
  "execution_time_seconds": 12.4,
  "coverage_percent": 78.3
}
```

**Implementation Approach**:
- Execute ctest with appropriate labels and filters
- Parse test output XML (if available) or stdout
- Support test discovery and listing
- Handle timeouts and crashes gracefully

---

#### 1.4 build_status
**Purpose**: Query current build system state and configuration
**Priority**: P1 (Important for debugging)

**Parameters**:
```cpp
struct BuildStatusParams {
  std::string build_dir;       // Build directory to inspect
  bool show_cache;              // Include CMakeCache variables
  bool show_targets;            // List available build targets
};
```

**Returns**:
```json
{
  "configured": true,
  "preset": "mac-dbg",
  "last_build": "2025-11-22T10:30:00Z",
  "targets_available": ["yaze", "z3ed", "yaze_test", "format"],
  "configuration": {
    "CMAKE_BUILD_TYPE": "Debug",
    "YAZE_ENABLE_AI": "ON",
    "YAZE_ENABLE_GRPC": "ON"
  },
  "dirty_files": ["src/app/editor/overworld_editor.cc"],
  "build_dependencies_outdated": false
}
```

---

### 2. Code Analysis Tools

#### 2.1 find_symbol
**Purpose**: Locate class, function, or variable definitions in codebase
**Priority**: P0 (Critical for navigation)

**Parameters**:
```cpp
struct FindSymbolParams {
  std::string symbol_name;     // Name to search for
  std::string symbol_type;     // "class", "function", "variable", "any"
  std::string scope;           // Directory scope (default: "src/")
  bool include_declarations;   // Include forward declarations
};
```

**Returns**:
```json
{
  "symbol": "OverworldEditor",
  "type": "class",
  "locations": [
    {
      "file": "src/app/editor/overworld/overworld_editor.h",
      "line": 45,
      "kind": "definition",
      "context": "class OverworldEditor : public Editor {"
    },
    {
      "file": "src/app/editor/overworld/overworld_editor.cc",
      "line": 23,
      "kind": "implementation",
      "context": "OverworldEditor::OverworldEditor() {"
    }
  ],
  "base_classes": ["Editor"],
  "derived_classes": [],
  "namespace": "yaze::app::editor"
}
```

**Implementation Approach**:
- Use ctags/cscope database if available
- Fall back to intelligent grep patterns
- Parse include guards and namespace blocks
- Cache symbol database for performance

---

#### 2.2 get_call_hierarchy
**Purpose**: Analyze function call relationships
**Priority**: P1 (Important for refactoring)

**Parameters**:
```cpp
struct CallHierarchyParams {
  std::string function_name;   // Function to analyze
  std::string direction;       // "callers", "callees", "both"
  int max_depth;                // Recursion depth (default: 3)
  bool include_virtual;         // Track virtual function calls
};
```

**Returns**:
```json
{
  "function": "Rom::LoadFromFile",
  "callers": [
    {
      "function": "EditorManager::OpenRom",
      "file": "src/app/editor/editor_manager.cc",
      "line": 156,
      "call_sites": [{"line": 162, "context": "rom_->LoadFromFile(path)"}]
    }
  ],
  "callees": [
    {
      "function": "Rom::ReadAllGraphicsData",
      "file": "src/app/rom.cc",
      "line": 234,
      "is_virtual": false
    }
  ],
  "complexity_score": 12
}
```

---

#### 2.3 get_class_members
**Purpose**: List all methods and fields of a class
**Priority**: P1 (Important for understanding)

**Parameters**:
```cpp
struct ClassMembersParams {
  std::string class_name;      // Class to analyze
  bool include_inherited;      // Include base class members
  bool include_private;        // Include private members
  std::string filter;          // Filter by member name pattern
};
```

**Returns**:
```json
{
  "class": "OverworldEditor",
  "namespace": "yaze::app::editor",
  "members": {
    "methods": [
      {
        "name": "Update",
        "signature": "absl::Status Update() override",
        "visibility": "public",
        "is_virtual": true,
        "line": 67
      }
    ],
    "fields": [
      {
        "name": "current_map_",
        "type": "int",
        "visibility": "private",
        "line": 234,
        "has_getter": true,
        "has_setter": false
      }
    ]
  },
  "base_classes": ["Editor"],
  "total_methods": 42,
  "total_fields": 18
}
```

---

#### 2.4 analyze_includes
**Purpose**: Show include dependency graph
**Priority**: P2 (Useful for optimization)

**Parameters**:
```cpp
struct AnalyzeIncludesParams {
  std::string file_path;       // File to analyze
  std::string direction;       // "includes", "included_by", "both"
  bool show_system;             // Include system headers
  int max_depth;                // Recursion depth
};
```

**Returns**:
```json
{
  "file": "src/app/editor/overworld_editor.cc",
  "direct_includes": [
    {"file": "overworld_editor.h", "is_system": false},
    {"file": "app/rom.h", "is_system": false},
    {"file": <vector>", "is_system": true}
  ],
  "included_by": [
    "src/app/editor/editor_manager.cc"
  ],
  "include_depth": 3,
  "circular_dependencies": [],
  "suggestions": ["Consider forward declaration for 'Rom' class"]
}
```

---

### 3. Debug Tools

#### 3.1 parse_crash_log
**Purpose**: Extract actionable information from crash dumps
**Priority**: P0 (Critical for debugging)

**Parameters**:
```cpp
struct ParseCrashLogParams {
  std::string log_path;        // Path to crash log or stdin
  std::string platform;        // "macos", "linux", "windows", "auto"
  bool symbolicate;            // Attempt to resolve symbols
};
```

**Returns**:
```json
{
  "crash_type": "SIGSEGV",
  "crash_address": "0x00000000",
  "crashed_thread": 0,
  "stack_trace": [
    {
      "frame": 0,
      "address": "0x10234abcd",
      "symbol": "yaze::app::editor::OverworldEditor::RenderMap",
      "file": "src/app/editor/overworld_editor.cc",
      "line": 456,
      "is_user_code": true
    }
  ],
  "likely_cause": "Null pointer dereference in RenderMap",
  "suggested_fixes": [
    "Check if 'current_map_data_' is initialized before use",
    "Add null check at line 456"
  ],
  "similar_crashes": ["#1234 - Fixed in commit abc123"]
}
```

**Implementation Approach**:
- Parse platform-specific crash formats (lldb, gdb, Windows dumps)
- Symbolicate addresses using debug symbols
- Identify patterns (null deref, stack overflow, etc.)
- Search issue tracker for similar crashes

---

#### 3.2 get_memory_profile
**Purpose**: Analyze memory usage and detect leaks
**Priority**: P2 (Useful for optimization)

**Parameters**:
```cpp
struct MemoryProfileParams {
  std::string process_name;    // Process to analyze or PID
  std::string profile_type;    // "snapshot", "leaks", "allocations"
  int duration_seconds;         // For allocation profiling
};
```

**Returns**:
```json
{
  "total_memory_mb": 234.5,
  "heap_size_mb": 180.2,
  "largest_allocations": [
    {
      "size_mb": 45.6,
      "location": "gfx::Arena::LoadAllGraphics",
      "count": 223,
      "type": "gfx::Bitmap"
    }
  ],
  "potential_leaks": [
    {
      "size_bytes": 1024,
      "allocation_site": "CreateTempBuffer at editor.cc:123",
      "leak_confidence": 0.85
    }
  ],
  "memory_growth_rate_mb_per_min": 2.3
}
```

---

#### 3.3 analyze_performance
**Purpose**: Profile performance hotspots
**Priority**: P2 (Useful for optimization)

**Parameters**:
```cpp
struct PerformanceAnalysisParams {
  std::string target;           // Binary or test to profile
  std::string scenario;         // Specific scenario to profile
  int duration_seconds;         // Profiling duration
  std::string metric;           // "cpu", "memory", "io", "all"
};
```

**Returns**:
```json
{
  "hotspots": [
    {
      "function": "gfx::Bitmap::ApplyPalette",
      "cpu_percent": 23.4,
      "call_count": 1000000,
      "avg_duration_us": 12.3,
      "file": "src/app/gfx/bitmap.cc",
      "line": 234
    }
  ],
  "bottlenecks": [
    "Graphics rendering taking 65% of frame time",
    "Excessive allocations in tile loading"
  ],
  "optimization_suggestions": [
    "Cache palette conversions",
    "Use SIMD for pixel operations"
  ]
}
```

---

### 4. Editor Integration Tools

#### 4.1 get_canvas_state
**Purpose**: Query current canvas/editor state for context
**Priority**: P1 (Important for automation)

**Parameters**:
```cpp
struct CanvasStateParams {
  std::string editor_type;     // "overworld", "dungeon", "graphics"
  bool include_selection;      // Include selected entities
  bool include_viewport;       // Include camera/zoom info
};
```

**Returns**:
```json
{
  "editor": "overworld",
  "current_map": 0x00,
  "map_name": "Hyrule Field",
  "viewport": {
    "x": 0,
    "y": 0,
    "width": 512,
    "height": 512,
    "zoom": 2.0
  },
  "selection": {
    "type": "entrance",
    "id": 0x03,
    "position": {"x": 256, "y": 128}
  },
  "tool": "select",
  "modified": true,
  "undo_stack_size": 15
}
```

**Implementation Approach**:
- Query EditorManager for active editor
- Use Canvas automation API for state extraction
- Serialize entity selections and properties
- Include modification tracking

---

#### 4.2 simulate_user_action
**Purpose**: Trigger UI actions programmatically
**Priority**: P1 (Important for automation)

**Parameters**:
```cpp
struct SimulateActionParams {
  std::string action_type;     // "click", "drag", "key", "menu"
  nlohmann::json parameters;   // Action-specific parameters
  std::string editor_context;  // Which editor to target
};
```

**Returns**:
```json
{
  "action": "click",
  "target": "tile_palette",
  "position": {"x": 100, "y": 50},
  "result": "success",
  "new_selection": {
    "tile_id": 0x42,
    "tile_type": "grass"
  },
  "side_effects": ["Tool changed to 'paint'"]
}
```

---

#### 4.3 capture_screenshot
**Purpose**: Capture editor visuals for verification or documentation
**Priority**: P2 (Useful for testing)

**Parameters**:
```cpp
struct ScreenshotParams {
  std::string output_path;     // Where to save screenshot
  std::string target;          // "full", "canvas", "window"
  std::string format;          // "png", "jpg", "bmp"
  bool include_ui;              // Include UI overlays
};
```

**Returns**:
```json
{
  "status": "success",
  "file_path": "/tmp/screenshot_2025_11_22_103045.png",
  "dimensions": {"width": 1920, "height": 1080},
  "file_size_kb": 234,
  "metadata": {
    "editor": "overworld",
    "map_id": "0x00",
    "timestamp": "2025-11-22T10:30:45Z"
  }
}
```

---

## Implementation Roadmap

### Phase 1: MVP (Week 1)
**Priority P0 tools only**
1. `build_compile` - Essential for development iteration
2. `build_test` - Required for validation
3. `find_symbol` - Core navigation capability
4. `parse_crash_log` - Critical debugging tool

### Phase 2: Enhanced (Week 2)
**Priority P1 tools**
1. `build_configure` - Build system management
2. `build_status` - State inspection
3. `get_call_hierarchy` - Code understanding
4. `get_class_members` - API exploration
5. `get_canvas_state` - Editor integration
6. `simulate_user_action` - Automation capability

### Phase 3: Complete (Week 3)
**Priority P2 tools**
1. `analyze_includes` - Optimization support
2. `get_memory_profile` - Memory debugging
3. `analyze_performance` - Performance tuning
4. `capture_screenshot` - Visual verification

## Integration with Existing Infrastructure

### ToolDispatcher Integration

Add to `tool_dispatcher.h`:
```cpp
enum class ToolCallType {
  // ... existing types ...

  // Build Tools
  kBuildConfigure,
  kBuildCompile,
  kBuildTest,
  kBuildStatus,

  // Code Analysis
  kCodeFindSymbol,
  kCodeGetCallHierarchy,
  kCodeGetClassMembers,
  kCodeAnalyzeIncludes,

  // Debug Tools
  kDebugParseCrashLog,
  kDebugGetMemoryProfile,
  kDebugAnalyzePerformance,

  // Editor Integration
  kEditorGetCanvasState,
  kEditorSimulateAction,
  kEditorCaptureScreenshot,
};
```

### Handler Implementation Pattern

Each tool follows the CommandHandler pattern:

```cpp
class BuildCompileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "build-compile"; }

  std::string GetUsage() const override {
    return "build-compile --build-dir <dir> [--target <name>] "
           "[--jobs <n>] [--verbose] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    // Validate required arguments
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override {
    // Implementation
    return absl::OkStatus();
  }

  bool RequiresLabels() const override { return false; }
};
```

### Security Considerations

1. **Build System Tools**:
   - Restrict to project directory
   - Validate presets against CMakePresets.json
   - Sanitize compiler flags
   - Limit parallel jobs to prevent DoS

2. **Code Analysis Tools**:
   - Read-only operations only
   - Cache results to prevent excessive parsing
   - Timeout long-running analyses

3. **Debug Tools**:
   - Sanitize crash log paths
   - Limit profiling duration
   - Prevent access to system processes

4. **Editor Integration**:
   - Rate limit UI actions
   - Validate action parameters
   - Prevent infinite loops in automation

## Testing Strategy

### Unit Tests
- Mock subprocess execution for build tools
- Test error parsing with known compiler outputs
- Verify security restrictions (path traversal, etc.)

### Integration Tests
- Test with real CMake builds (small test projects)
- Verify symbol finding with known codebase structure
- Test crash parsing with sample logs

### End-to-End Tests
- Full development workflow automation
- Build → Test → Debug cycle
- Editor automation scenarios

## Performance Considerations

1. **Caching**:
   - Symbol database caching (5-minute TTL)
   - Build status caching (invalidate on file changes)
   - Compiler error pattern cache

2. **Async Operations**:
   - Long builds run in background
   - Profiling operations are async
   - Support streaming output for progress

3. **Resource Limits**:
   - Max parallel build jobs = CPU count
   - Profiling duration cap = 5 minutes
   - Screenshot size limit = 10MB

## Success Metrics

1. **Developer Productivity**:
   - Reduce build debugging time by 50%
   - Enable AI agents to fix 80% of simple compilation errors
   - Automate 60% of test failure investigations

2. **Code Quality**:
   - Increase test coverage by 20% through AI-generated tests
   - Identify 90% of memory leaks before release
   - Reduce performance regressions by 40%

3. **Agent Capabilities**:
   - Agents can complete full edit-compile-test cycles
   - Agents can diagnose and suggest fixes for crashes
   - Agents can navigate and understand codebase structure

## Appendix A: Error Pattern Database

Common compilation error patterns for parsing:

```regex
# GCC/Clang error
^([^:]+):(\d+):(\d+):\s+(error|warning):\s+(.+)$

# MSVC error
^([^(]+)\((\d+)\):\s+(error|warning)\s+(\w+):\s+(.+)$

# Linker error
^(ld|link):\s+(error|warning):\s+(.+)$
```

## Appendix B: Platform-Specific Considerations

### macOS
- Use `xcrun` for toolchain discovery
- Parse `.crash` files from `~/Library/Logs/DiagnosticReports/`
- Support both x86_64 and arm64 architectures

### Linux
- Check for gcc/g++ and clang/clang++
- Parse core dumps with gdb
- Use `perf` for profiling when available

### Windows
- Handle both MSVC and MinGW toolchains
- Parse `.dmp` files for crash analysis
- Use Windows Performance Toolkit for profiling

## Appendix C: AI Agent Use Cases

### Use Case 1: Compilation Error Resolution
```
Agent: I see you have a compilation error in overworld_editor.cc
Agent: Let me analyze the error...
[Executes: build_compile --verbose]
[Executes: find_symbol --symbol LoadGraphics]
Agent: The function LoadGraphics was renamed to LoadAreaGraphics in commit abc123
Agent: I'll update the call site...
[Modifies file]
[Executes: build_compile --target yaze]
Agent: Build successful! The error is resolved.
```

### Use Case 2: Test Failure Investigation
```
Agent: The SnesColorTest is failing. Let me investigate...
[Executes: build_test --filter "*SnesColor*" --show-output]
[Executes: get_class_members --class SnesColor]
Agent: The test expects 0x7FFF but gets 0x7FFE
Agent: This is a rounding error in the conversion function
[Analyzes implementation]
Agent: The issue is at line 45 - we need to round instead of truncate
```

### Use Case 3: Performance Optimization
```
Agent: Let me profile the slow rendering...
[Executes: analyze_performance --scenario "load_overworld"]
Agent: I found that ApplyPalette takes 23% of CPU time
[Executes: get_call_hierarchy --function ApplyPalette]
Agent: It's called 1M times per frame - that's excessive
Agent: I suggest caching the palette conversions...
```

## Document History

- 2025-11-22: Initial specification (v1.0) - CLAUDE_AIINF