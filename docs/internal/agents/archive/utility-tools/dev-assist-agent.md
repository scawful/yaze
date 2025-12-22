# DevAssistAgent - AI Development Assistant

## Overview

The DevAssistAgent is an AI-powered development assistant that helps developers while coding yaze itself. It provides intelligent analysis and suggestions for build errors, crashes, and test failures, making the development process more efficient.

## Key Features

### 1. Build Monitoring & Error Resolution
- **Real-time compilation error analysis**: Parses compiler output and provides targeted fixes
- **Link failure diagnosis**: Identifies missing symbols and suggests library ordering fixes
- **CMake configuration issues**: Helps resolve CMake errors and missing dependencies
- **Cross-platform support**: Handles GCC, Clang, and MSVC error formats

### 2. Crash Analysis
- **Stack trace analysis**: Parses segfaults, assertions, and stack overflows
- **Root cause identification**: Suggests likely causes based on crash patterns
- **Fix recommendations**: Provides actionable steps to resolve crashes
- **Debug tool suggestions**: Recommends AddressSanitizer, Valgrind, etc.

### 3. Test Automation
- **Affected test discovery**: Identifies tests related to changed files
- **Test generation**: Creates unit tests for new or modified code
- **Test failure analysis**: Parses test output and suggests fixes
- **Coverage recommendations**: Suggests missing test cases

### 4. Code Quality Analysis
- **Static analysis**: Checks for common C++ issues
- **TODO/FIXME tracking**: Identifies technical debt markers
- **Style violations**: Detects long lines and formatting issues
- **Potential bugs**: Simple heuristics for null pointer risks

## Architecture

### Core Components

```cpp
class DevAssistAgent {
  // Main analysis interface
  std::vector<AnalysisResult> AnalyzeBuildOutput(const std::string& output);
  AnalysisResult AnalyzeCrash(const std::string& stack_trace);
  std::vector<TestSuggestion> GetAffectedTests(const std::vector<std::string>& changed_files);

  // Build monitoring
  absl::Status MonitorBuild(const BuildConfig& config,
                            std::function<void(const AnalysisResult&)> on_error);

  // AI-enhanced features (optional)
  absl::StatusOr<std::string> GenerateTestCode(const std::string& source_file);
};
```

### Analysis Result Structure

```cpp
struct AnalysisResult {
  ErrorType error_type;           // Compilation, Link, Runtime, etc.
  std::string file_path;          // Affected file
  int line_number;                // Line where error occurred
  std::string description;        // Human-readable description
  std::vector<std::string> suggested_fixes;  // Ordered fix suggestions
  std::vector<std::string> related_files;    // Files that may be involved
  double confidence;              // 0.0-1.0 confidence in analysis
  bool ai_assisted;              // Whether AI was used
};
```

### Error Pattern Recognition

The agent uses regex patterns to identify different error types:

1. **Compilation Errors**
   - Pattern: `([^:]+):(\d+):(\d+):\s*(error|warning):\s*(.+)`
   - Extracts: file, line, column, severity, message

2. **Link Errors**
   - Pattern: `undefined reference to\s*[']([^']+)[']`
   - Extracts: missing symbol name

3. **CMake Errors**
   - Pattern: `CMake Error at ([^:]+):(\d+)`
   - Extracts: CMakeLists.txt file and line

4. **Runtime Crashes**
   - Patterns for SIGSEGV, stack overflow, assertions
   - Stack frame extraction for debugging

## Usage Examples

### Basic Build Error Analysis

```cpp
// Initialize the agent
auto tool_dispatcher = std::make_shared<ToolDispatcher>();
auto ai_service = ai::ServiceFactory::Create("ollama");  // Optional

DevAssistAgent agent;
agent.Initialize(tool_dispatcher, ai_service);

// Analyze build output
std::string build_output = R"(
src/app/editor/overworld.cc:45:10: error: 'Rom' was not declared in this scope
src/app/editor/overworld.cc:50:20: error: undefined reference to 'LoadOverworld'
)";

auto results = agent.AnalyzeBuildOutput(build_output);
for (const auto& result : results) {
  std::cout << "Error: " << result.description << "\n";
  std::cout << "File: " << result.file_path << ":" << result.line_number << "\n";
  for (const auto& fix : result.suggested_fixes) {
    std::cout << "  - " << fix << "\n";
  }
}
```

### Interactive Build Monitoring

```cpp
DevAssistAgent::BuildConfig config;
config.build_dir = "build";
config.preset = "mac-dbg";
config.verbose = true;
config.stop_on_error = false;

agent.MonitorBuild(config, [](const DevAssistAgent::AnalysisResult& error) {
  // Handle each error as it's detected
  std::cout << "Build error detected: " << error.description << "\n";

  if (error.ai_assisted && !error.suggested_fixes.empty()) {
    std::cout << "AI suggestion: " << error.suggested_fixes[0] << "\n";
  }
});
```

### Crash Analysis

```cpp
std::string stack_trace = R"(
Thread 1 "yaze" received signal SIGSEGV, Segmentation fault.
0x00005555555a1234 in OverworldEditor::Update() at src/app/editor/overworld.cc:123
#0  0x00005555555a1234 in OverworldEditor::Update() at src/app/editor/overworld.cc:123
#1  0x00005555555b5678 in EditorManager::UpdateEditors() at src/app/editor/manager.cc:456
)";

auto crash_result = agent.AnalyzeCrash(stack_trace);
std::cout << "Crash type: " << crash_result.description << "\n";
std::cout << "Location: " << crash_result.file_path << ":" << crash_result.line_number << "\n";
std::cout << "Root cause: " << crash_result.root_cause << "\n";
```

### Test Discovery and Generation

```cpp
// Find tests affected by changes
std::vector<std::string> changed_files = {
  "src/app/gfx/bitmap.cc",
  "src/app/editor/overworld.h"
};

auto test_suggestions = agent.GetAffectedTests(changed_files);
for (const auto& suggestion : test_suggestions) {
  std::cout << "Test: " << suggestion.test_file << "\n";
  std::cout << "Reason: " << suggestion.reason << "\n";

  if (!suggestion.is_existing) {
    // Generate new test if it doesn't exist
    auto test_code = agent.GenerateTestCode(changed_files[0], "ApplyPalette");
    if (test_code.ok()) {
      std::cout << "Generated test:\n" << *test_code << "\n";
    }
  }
}
```

## Integration with z3ed CLI

The DevAssistAgent can be used through the z3ed CLI tool:

```bash
# Monitor build with error analysis
z3ed agent dev-assist --monitor-build --preset mac-dbg

# Analyze a crash dump
z3ed agent dev-assist --analyze-crash crash.log

# Generate tests for changed files
z3ed agent dev-assist --generate-tests --files "src/app/gfx/*.cc"

# Get build status
z3ed agent dev-assist --build-status
```

## Common Error Patterns and Fixes

### Missing Headers
**Pattern**: `fatal error: 'absl/status/status.h': No such file or directory`
**Fixes**:
1. Add `#include "absl/status/status.h"`
2. Check CMakeLists.txt includes Abseil
3. Verify include paths are correct

### Undefined References
**Pattern**: `undefined reference to 'yaze::Rom::LoadFromFile'`
**Fixes**:
1. Ensure source file is compiled
2. Check library link order
3. Verify function is implemented (not just declared)

### Segmentation Faults
**Pattern**: `Segmentation fault (core dumped)`
**Fixes**:
1. Check for null pointer dereferences
2. Verify array bounds
3. Look for use-after-free
4. Run with AddressSanitizer

### CMake Configuration
**Pattern**: `CMake Error: Could not find package Abseil`
**Fixes**:
1. Install missing dependency
2. Set CMAKE_PREFIX_PATH
3. Use vcpkg or system package manager

## AI Enhancement

When AI service is available (Ollama or Gemini), the agent provides:
- Context-aware fix suggestions based on codebase patterns
- Test generation with comprehensive edge cases
- Natural language explanations of complex errors
- Code quality recommendations

To enable AI features:
```cpp
auto ai_service = ai::ServiceFactory::Create("ollama");
agent.Initialize(tool_dispatcher, ai_service);
agent.SetAIEnabled(true);
```

## Performance Considerations

- Error pattern matching is fast (regex-based)
- File system operations are cached for test discovery
- AI suggestions are optional and async when possible
- Build monitoring uses streaming output parsing

## Future Enhancements

1. **Incremental Build Analysis**: Track which changes trigger which errors
2. **Historical Error Database**: Learn from past fixes in the codebase
3. **Automated Fix Application**: Apply simple fixes automatically
4. **CI Integration**: Analyze CI build failures and suggest fixes
5. **Performance Profiling**: Identify build bottlenecks and optimization opportunities

## Related Documentation

- [Build Tool Documentation](filesystem-tool.md)
- [AI Infrastructure Initiative (archived)](archive/legacy-2025-11/ai-infrastructure-initiative-archived-2025-11-25.md)
- [Test Suite Configuration](../../test-suite-configuration.md)
