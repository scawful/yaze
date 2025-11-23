#include "cli/service/agent/dev_assist_agent.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/common.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Common file extensions for C++ source and test files
const std::vector<std::string> kSourceExtensions = {".cc", ".cpp", ".cxx", ".c"};
const std::vector<std::string> kHeaderExtensions = {".h", ".hpp", ".hxx"};
const std::vector<std::string> kTestSuffixes = {"_test", "_unittest", "_tests"};

// Extract filename from path
std::string GetFileName(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Extract directory from path
std::string GetDirectory(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

// Check if string contains any of the patterns
bool ContainsAny(const std::string& text, const std::vector<std::string>& patterns) {
  for (const auto& pattern : patterns) {
    if (text.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

DevAssistAgent::DevAssistAgent() {
  InitializeErrorPatterns();
}

DevAssistAgent::~DevAssistAgent() = default;

absl::Status DevAssistAgent::Initialize(
    std::shared_ptr<ToolDispatcher> tool_dispatcher,
    std::shared_ptr<yaze::cli::AIService> ai_service) {
  if (!tool_dispatcher) {
    return absl::InvalidArgumentError("Tool dispatcher is required");
  }

  tool_dispatcher_ = tool_dispatcher;
  ai_service_ = ai_service;
  initialized_ = true;

  return absl::OkStatus();
}

void DevAssistAgent::InitializeErrorPatterns() {
  // GCC/Clang compilation errors
  error_patterns_.push_back({
      std::regex(R"(([^:]+):(\d+):(\d+):\s*(error|warning):\s*(.+))"),
      AnalysisResult::ErrorType::kCompilationError,
      "Compilation",
      [](const std::smatch& match, AnalysisResult& result) {
        result.file_path = match[1];
        result.line_number = std::stoi(match[2]);
        result.description = match[5];
        result.raw_error = match[0];
      }
  });

  // MSVC compilation errors
  error_patterns_.push_back({
      std::regex(R"(([^(]+)\((\d+)\):\s*(error|warning)\s*([A-Z0-9]+):\s*(.+))"),
      AnalysisResult::ErrorType::kCompilationError,
      "Compilation",
      [](const std::smatch& match, AnalysisResult& result) {
        result.file_path = match[1];
        result.line_number = std::stoi(match[2]);
        result.description = match[5];
        result.raw_error = match[0];
      }
  });

  // Undefined reference (link error)
  error_patterns_.push_back({
      std::regex(R"(undefined reference to\s*[`']([^']+)[`'])"),
      AnalysisResult::ErrorType::kLinkError,
      "Linking",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = absl::StrCat("Undefined reference to: ", match[1].str());
        result.raw_error = match[0];
      }
  });

  // Undefined symbols (macOS linker)
  error_patterns_.push_back({
      std::regex(R"(Undefined symbols for architecture .+:\s*\"([^\"]+)\")"),
      AnalysisResult::ErrorType::kLinkError,
      "Linking",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = absl::StrCat("Undefined symbol: ", match[1].str());
        result.raw_error = match[0];
      }
  });

  // Missing header file
  error_patterns_.push_back({
      std::regex(R"(fatal error:\s*([^:]+):\s*No such file or directory)"),
      AnalysisResult::ErrorType::kMissingHeader,
      "Compilation",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = absl::StrCat("Missing header file: ", match[1].str());
        result.raw_error = match[0];
      }
  });

  // CMake errors
  error_patterns_.push_back({
      std::regex(R"(CMake Error at ([^:]+):(\d+)\s*\(([^)]+)\):\s*(.+))"),
      AnalysisResult::ErrorType::kCMakeError,
      "CMake",
      [](const std::smatch& match, AnalysisResult& result) {
        result.file_path = match[1];
        result.line_number = std::stoi(match[2]);
        result.description = match[4];
        result.raw_error = match[0];
      }
  });

  // Segmentation fault
  error_patterns_.push_back({
      std::regex(R"(Segmentation fault|SIGSEGV|segfault)"),
      AnalysisResult::ErrorType::kSegmentationFault,
      "Runtime",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = "Segmentation fault detected";
        result.raw_error = match[0];
      }
  });

  // Assertion failure
  error_patterns_.push_back({
      std::regex(R"(Assertion\s*[`']([^']+)[`']\s*failed)"),
      AnalysisResult::ErrorType::kAssertionFailure,
      "Runtime",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = absl::StrCat("Assertion failed: ", match[1].str());
        result.raw_error = match[0];
      }
  });

  // Stack overflow
  error_patterns_.push_back({
      std::regex(R"(Stack overflow|stack smashing detected)"),
      AnalysisResult::ErrorType::kStackOverflow,
      "Runtime",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = "Stack overflow detected";
        result.raw_error = match[0];
      }
  });

  // Test failure (Google Test)
  error_patterns_.push_back({
      std::regex(R"(\[\s*FAILED\s*\]\s*([^.]+)\.([^\s]+))"),
      AnalysisResult::ErrorType::kTestFailure,
      "Test",
      [](const std::smatch& match, AnalysisResult& result) {
        result.description = absl::StrCat("Test failed: ", match[1].str(), ".", match[2].str());
        result.raw_error = match[0];
      }
  });
}

std::vector<DevAssistAgent::AnalysisResult> DevAssistAgent::AnalyzeBuildOutput(
    const std::string& output) {
  std::vector<AnalysisResult> results;
  std::istringstream stream(output);
  std::string line;

  while (std::getline(stream, line)) {
    for (const auto& pattern : error_patterns_) {
      std::smatch match;
      if (std::regex_search(line, match, pattern.pattern)) {
        AnalysisResult result;
        result.error_type = pattern.type;
        result.error_category = pattern.category;
        pattern.extractor(match, result);

        // Generate fix suggestions
        GenerateFixSuggestions(result);

        // Try to get AI suggestions if available
        if (use_ai_ && ai_service_) {
          auto ai_suggestion = GetAISuggestion(result);
          if (ai_suggestion.ok()) {
            result.suggested_fixes.push_back(*ai_suggestion);
            result.ai_assisted = true;
          }
        }

        results.push_back(result);
        break;  // Only match first pattern per line
      }
    }
  }

  return results;
}

DevAssistAgent::AnalysisResult DevAssistAgent::AnalyzeCrash(
    const std::string& stack_trace) {
  AnalysisResult result;
  result.error_category = "Runtime";

  // Check for common crash types
  if (stack_trace.find("SIGSEGV") != std::string::npos ||
      stack_trace.find("Segmentation fault") != std::string::npos) {
    result.error_type = AnalysisResult::ErrorType::kSegmentationFault;
    result.description = "Segmentation fault (memory access violation)";
    result.root_cause = "Likely causes: null pointer dereference, buffer overflow, use-after-free";
  } else if (stack_trace.find("Stack overflow") != std::string::npos) {
    result.error_type = AnalysisResult::ErrorType::kStackOverflow;
    result.description = "Stack overflow detected";
    result.root_cause = "Likely causes: infinite recursion, large stack allocations";
  } else if (stack_trace.find("Assertion") != std::string::npos) {
    result.error_type = AnalysisResult::ErrorType::kAssertionFailure;
    result.description = "Assertion failure";
    result.root_cause = "A debug assertion or CHECK failed";
  }

  // Extract file and line from stack trace (if present)
  std::regex frame_regex(R"(#\d+\s+.+\s+at\s+([^:]+):(\d+))");
  std::smatch match;
  if (std::regex_search(stack_trace, match, frame_regex)) {
    result.file_path = match[1];
    result.line_number = std::stoi(match[2]);
  }

  // Extract function names from stack
  std::regex func_regex(R"(#\d+\s+.+\s+in\s+([^\s(]+))");
  std::string functions;
  auto begin = std::sregex_iterator(stack_trace.begin(), stack_trace.end(), func_regex);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    if (!functions.empty()) functions += " -> ";
    functions += (*it)[1].str();
  }
  if (!functions.empty()) {
    result.description += "\nCall stack: " + functions;
  }

  GenerateFixSuggestions(result);

  // Get AI analysis if available
  if (use_ai_ && ai_service_) {
    auto ai_suggestion = GetAISuggestion(result);
    if (ai_suggestion.ok()) {
      result.suggested_fixes.push_back(*ai_suggestion);
      result.ai_assisted = true;
    }
  }

  result.confidence = 0.8;  // High confidence for crash analysis
  return result;
}

std::vector<DevAssistAgent::TestSuggestion> DevAssistAgent::GetAffectedTests(
    const std::vector<std::string>& changed_files) {
  std::vector<TestSuggestion> suggestions;

  for (const auto& file : changed_files) {
    // Skip non-source files
    bool is_source = false;
    for (const auto& ext : kSourceExtensions) {
      if (absl::EndsWith(file, ext)) {
        is_source = true;
        break;
      }
    }
    if (!is_source) {
      for (const auto& ext : kHeaderExtensions) {
        if (absl::EndsWith(file, ext)) {
          is_source = true;
          break;
        }
      }
    }
    if (!is_source) continue;

    // Find corresponding test file
    std::string test_file = GetTestFileForSource(file);

    // Check if test file exists
    if (std::filesystem::exists(test_file)) {
      TestSuggestion suggestion;
      suggestion.test_file = test_file;
      suggestion.test_name = GetFileName(test_file);
      suggestion.reason = absl::StrCat("Tests for modified file: ", file);
      suggestion.is_existing = true;
      suggestions.push_back(suggestion);
    } else {
      // Suggest creating a new test file
      TestSuggestion suggestion;
      suggestion.test_file = test_file;
      suggestion.test_name = GetFileName(test_file);
      suggestion.reason = absl::StrCat("No tests found for: ", file, ". Consider adding tests.");
      suggestion.is_existing = false;
      suggestions.push_back(suggestion);
    }

    // Also find any other tests that might reference this file
    auto related_tests = FindTestsForFile(file);
    for (const auto& related : related_tests) {
      if (related != test_file) {  // Avoid duplicates
        TestSuggestion suggestion;
        suggestion.test_file = related;
        suggestion.test_name = GetFileName(related);
        suggestion.reason = absl::StrCat("May test functionality from: ", file);
        suggestion.is_existing = true;
        suggestions.push_back(suggestion);
      }
    }
  }

  return suggestions;
}

void DevAssistAgent::GenerateFixSuggestions(AnalysisResult& result) {
  switch (result.error_type) {
    case AnalysisResult::ErrorType::kMissingHeader:
      SuggestMissingHeaderFix(result);
      break;

    case AnalysisResult::ErrorType::kLinkError:
    case AnalysisResult::ErrorType::kUndefinedSymbol:
      SuggestLinkOrderFix(result);
      break;

    case AnalysisResult::ErrorType::kTypeMismatch:
      SuggestTypeMismatchFix(result);
      break;

    case AnalysisResult::ErrorType::kSegmentationFault:
      result.suggested_fixes.push_back("Check for null pointer dereferences");
      result.suggested_fixes.push_back("Verify array bounds and buffer sizes");
      result.suggested_fixes.push_back("Look for use-after-free or dangling pointers");
      result.suggested_fixes.push_back("Run with AddressSanitizer: -fsanitize=address");
      break;

    case AnalysisResult::ErrorType::kStackOverflow:
      result.suggested_fixes.push_back("Check for infinite recursion");
      result.suggested_fixes.push_back("Reduce large stack allocations (use heap instead)");
      result.suggested_fixes.push_back("Increase stack size if necessary");
      break;

    case AnalysisResult::ErrorType::kCircularDependency:
      result.suggested_fixes.push_back("Review include dependencies");
      result.suggested_fixes.push_back("Use forward declarations where possible");
      result.suggested_fixes.push_back("Consider extracting common interfaces");
      break;

    case AnalysisResult::ErrorType::kCMakeError:
      result.suggested_fixes.push_back("Check CMakeLists.txt syntax");
      result.suggested_fixes.push_back("Verify target and dependency names");
      result.suggested_fixes.push_back("Run 'cmake --debug-output' for more details");
      break;

    case AnalysisResult::ErrorType::kTestFailure:
      result.suggested_fixes.push_back("Review test expectations vs actual behavior");
      result.suggested_fixes.push_back("Check for recent changes to tested code");
      result.suggested_fixes.push_back("Run test in isolation to rule out interference");
      break;

    default:
      result.suggested_fixes.push_back("Review the error message and surrounding code");
      result.suggested_fixes.push_back("Check recent changes that might have introduced the issue");
      break;
  }
}

void DevAssistAgent::SuggestMissingHeaderFix(AnalysisResult& result) {
  // Extract header name from error
  std::regex header_regex(R"(["<]([^">]+)[">])");
  std::smatch match;
  if (std::regex_search(result.description, match, header_regex)) {
    std::string header = match[1];

    result.suggested_fixes.push_back(
        absl::StrCat("Add '#include \"", header, "\"' or '#include <", header, ">'"));
    result.suggested_fixes.push_back(
        absl::StrCat("Check if '", header, "' exists in include paths"));
    result.suggested_fixes.push_back(
        "Verify CMakeLists.txt includes the correct directories");
    result.suggested_fixes.push_back(
        absl::StrCat("Search for the header: find . -name '", header, "'"));

    // Common header mappings
    if (header.find("absl/") == 0) {
      result.suggested_fixes.push_back("Ensure Abseil is properly linked in CMakeLists.txt");
    } else if (header.find("gtest") != std::string::npos) {
      result.suggested_fixes.push_back("Ensure Google Test is included in the build");
    }
  }
}

void DevAssistAgent::SuggestLinkOrderFix(AnalysisResult& result) {
  // Extract symbol name if present
  std::regex symbol_regex(R"([`"']([^`"']+)[`"'])");
  std::smatch match;
  if (std::regex_search(result.description, match, symbol_regex)) {
    std::string symbol = match[1];

    result.suggested_fixes.push_back(
        absl::StrCat("Symbol '", symbol, "' is not defined or not linked"));
    result.suggested_fixes.push_back(
        "Check if the source file containing this symbol is compiled");
    result.suggested_fixes.push_back(
        "Verify library link order in CMakeLists.txt");
    result.suggested_fixes.push_back(
        absl::StrCat("Search for definition: grep -r '", symbol, "' src/"));

    // Check for common patterns
    if (symbol.find("::") != std::string::npos) {
      result.suggested_fixes.push_back(
          "C++ namespace issue - ensure implementation matches declaration");
    }
    if (symbol.find("vtable") != std::string::npos) {
      result.suggested_fixes.push_back(
          "Virtual function not implemented - check pure virtual functions");
    }
  }
}

void DevAssistAgent::SuggestTypeMismatchFix(AnalysisResult& result) {
  result.suggested_fixes.push_back("Check function signatures match between declaration and definition");
  result.suggested_fixes.push_back("Verify template instantiations are correct");
  result.suggested_fixes.push_back("Look for const-correctness issues");
  result.suggested_fixes.push_back("Check for implicit conversions that might fail");
}

absl::StatusOr<std::string> DevAssistAgent::GetAISuggestion(const AnalysisResult& result) {
  if (!ai_service_) {
    return absl::UnavailableError("AI service not available");
  }

  // Build prompt for AI
  std::string prompt = absl::StrCat(
      "Analyze this build error and suggest a fix:\n",
      "Error Type: ", result.error_category, "\n",
      "Description: ", result.description, "\n"
  );

  if (!result.file_path.empty()) {
    prompt += absl::StrCat("File: ", result.file_path, "\n");
    if (result.line_number > 0) {
      prompt += absl::StrCat("Line: ", result.line_number, "\n");
    }
  }

  if (!result.raw_error.empty()) {
    prompt += absl::StrCat("Raw Error: ", result.raw_error, "\n");
  }

  prompt += "\nProvide a concise, actionable fix suggestion.";

  // Call AI service
  auto response = ai_service_->GenerateResponse(prompt);
  if (!response.ok()) {
    return response.status();
  }

  return response->text_response;
}

std::vector<std::string> DevAssistAgent::FindTestsForFile(const std::string& source_file) {
  std::vector<std::string> test_files;

  // Check cache first
  auto it = test_file_cache_.find(source_file);
  if (it != test_file_cache_.end()) {
    return it->second;
  }

  // Extract base name without extension
  std::string base_name = source_file;
  for (const auto& ext : kSourceExtensions) {
    size_t pos = base_name.rfind(ext);
    if (pos != std::string::npos) {
      base_name = base_name.substr(0, pos);
      break;
    }
  }
  for (const auto& ext : kHeaderExtensions) {
    size_t pos = base_name.rfind(ext);
    if (pos != std::string::npos) {
      base_name = base_name.substr(0, pos);
      break;
    }
  }

  // Look for test files with common patterns
  for (const auto& suffix : kTestSuffixes) {
    for (const auto& ext : kSourceExtensions) {
      std::string test_file = base_name + suffix + ext;
      if (std::filesystem::exists(test_file)) {
        test_files.push_back(test_file);
      }

      // Also check in test/ directory
      std::string test_dir_file = "test/" + GetFileName(base_name) + suffix + ext;
      if (std::filesystem::exists(test_dir_file)) {
        test_files.push_back(test_dir_file);
      }

      // Check in test/unit/ and test/integration/
      test_dir_file = "test/unit/" + GetFileName(base_name) + suffix + ext;
      if (std::filesystem::exists(test_dir_file)) {
        test_files.push_back(test_dir_file);
      }

      test_dir_file = "test/integration/" + GetFileName(base_name) + suffix + ext;
      if (std::filesystem::exists(test_dir_file)) {
        test_files.push_back(test_dir_file);
      }
    }
  }

  // Cache the result
  test_file_cache_[source_file] = test_files;

  return test_files;
}

bool DevAssistAgent::IsTestFile(const std::string& file_path) const {
  for (const auto& suffix : kTestSuffixes) {
    if (file_path.find(suffix) != std::string::npos) {
      return true;
    }
  }
  return file_path.find("/test/") != std::string::npos ||
         file_path.find("\\test\\") != std::string::npos;
}

std::string DevAssistAgent::GetTestFileForSource(const std::string& source_file) const {
  // Remove extension
  std::string base = source_file;
  for (const auto& ext : kSourceExtensions) {
    size_t pos = base.rfind(ext);
    if (pos != std::string::npos) {
      base = base.substr(0, pos);
      break;
    }
  }
  for (const auto& ext : kHeaderExtensions) {
    size_t pos = base.rfind(ext);
    if (pos != std::string::npos) {
      base = base.substr(0, pos);
      break;
    }
  }

  // Convert src/ to test/
  if (base.find("src/") == 0) {
    base = "test/unit/" + base.substr(4);
  }

  // Add test suffix
  return base + "_test.cc";
}

absl::StatusOr<std::string> DevAssistAgent::ExecuteCommand(const std::string& command) {
  // Use tool dispatcher to execute build commands
  if (!tool_dispatcher_) {
    return absl::FailedPreconditionError("Tool dispatcher not initialized");
  }

  // Create a build tool call
  ::yaze::cli::ToolCall tool_call;
  tool_call.tool_name = "build-compile";
  tool_call.args["command"] = command;

  return tool_dispatcher_->Dispatch(tool_call);
}

absl::StatusOr<std::vector<DevAssistAgent::AnalysisResult>>
DevAssistAgent::RunBuildWithAnalysis(const std::string& command) {
  auto output = ExecuteCommand(command);
  if (!output.ok()) {
    // Even if build fails, we want to analyze the output
    std::string error_output = std::string(output.status().message().data(), output.status().message().size());
    return AnalyzeBuildOutput(error_output);
  }

  // Analyze successful build output for warnings
  return AnalyzeBuildOutput(*output);
}

absl::StatusOr<std::vector<DevAssistAgent::AnalysisResult>>
DevAssistAgent::RunTestsWithAnalysis(const std::string& test_pattern) {
  std::string command = "ctest --output-on-failure";
  if (!test_pattern.empty()) {
    command += " -R " + test_pattern;
  }

  auto output = ExecuteCommand(command);
  if (!output.ok()) {
    // Analyze test failures
    std::string error_output = std::string(output.status().message().data(), output.status().message().size());
    return AnalyzeBuildOutput(error_output);
  }

  // Check for test failures in successful run
  return AnalyzeBuildOutput(*output);
}

std::string DevAssistAgent::GetBuildStatus() const {
  std::stringstream status;

  status << "DevAssistAgent Build Status\n";
  status << "===========================\n";
  status << "Initialized: " << (initialized_ ? "Yes" : "No") << "\n";
  status << "AI Service: " << (ai_service_ ? "Available" : "Not available") << "\n";
  status << "AI Enabled: " << (use_ai_ ? "Yes" : "No") << "\n";
  status << "Error Patterns Loaded: " << error_patterns_.size() << "\n";
  status << "Test File Cache Size: " << test_file_cache_.size() << "\n";

  return status.str();
}

absl::Status DevAssistAgent::MonitorBuild(
    const BuildConfig& config,
    std::function<void(const AnalysisResult&)> on_error) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Agent not initialized");
  }

  // Build command based on config
  std::string command = "cmake --build " + config.build_dir;
  if (!config.preset.empty()) {
    command = "cmake --preset " + config.preset + " && " + command;
  }
  if (config.verbose) {
    command += " --verbose";
  }
  if (config.parallel && config.parallel_jobs > 0) {
    command += " -j" + std::to_string(config.parallel_jobs);
  }

  // Run build and analyze output
  auto results = RunBuildWithAnalysis(command);
  if (!results.ok()) {
    return results.status();
  }

  // Report errors via callback
  for (const auto& result : *results) {
    if (on_error) {
      on_error(result);
    }

    if (config.stop_on_error &&
        result.error_type != AnalysisResult::ErrorType::kUnknown) {
      return absl::AbortedError("Build stopped on first error");
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> DevAssistAgent::GenerateTestCode(
    const std::string& source_file,
    const std::string& function_name) {
  if (!std::filesystem::exists(source_file)) {
    return absl::NotFoundError(absl::StrCat("Source file not found: ", source_file));
  }

  // Read source file
  std::ifstream file(source_file);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source_code = buffer.str();

  if (use_ai_ && ai_service_) {
    return GenerateTestWithAI(source_code, function_name);
  }

  // Basic test template without AI
  std::string test_code = absl::StrCat(
      "#include \"gtest/gtest.h\"\n",
      "#include \"", source_file, "\"\n\n",
      "namespace yaze {\n",
      "namespace test {\n\n"
  );

  if (!function_name.empty()) {
    test_code += absl::StrCat(
        "TEST(", GetFileName(source_file), "Test, ", function_name, ") {\n",
        "  // TODO: Implement test for ", function_name, "\n",
        "  EXPECT_TRUE(true);\n",
        "}\n\n"
    );
  } else {
    test_code += absl::StrCat(
        "TEST(", GetFileName(source_file), "Test, BasicTest) {\n",
        "  // TODO: Implement tests\n",
        "  EXPECT_TRUE(true);\n",
        "}\n\n"
    );
  }

  test_code += "}  // namespace test\n}  // namespace yaze\n";

  return test_code;
}

absl::StatusOr<std::string> DevAssistAgent::GenerateTestWithAI(
    const std::string& source_code,
    const std::string& function_name) {
  if (!ai_service_) {
    return absl::UnavailableError("AI service not available");
  }

  std::string prompt = "Generate comprehensive Google Test unit tests for the following C++ code:\n\n";
  prompt += source_code;
  prompt += "\n\n";

  if (!function_name.empty()) {
    prompt += absl::StrCat("Focus on testing the function: ", function_name, "\n");
  }

  prompt += "Include edge cases, error conditions, and normal operation tests.";
  prompt += "Follow the yaze project testing conventions.";

  auto response = ai_service_->GenerateResponse(prompt);
  if (!response.ok()) {
    return response.status();
  }

  return response->text_response;
}

std::vector<DevAssistAgent::AnalysisResult> DevAssistAgent::AnalyzeCodeFile(
    const std::string& file_path) {
  std::vector<AnalysisResult> results;

  if (!std::filesystem::exists(file_path)) {
    AnalysisResult result;
    result.error_type = AnalysisResult::ErrorType::kUnknown;
    result.error_category = "File";
    result.description = "File not found";
    result.file_path = file_path;
    results.push_back(result);
    return results;
  }

  // Read file
  std::ifstream file(file_path);
  std::string line;
  int line_number = 0;

  while (std::getline(file, line)) {
    line_number++;

    // Check for common code issues

    // TODO comments
    if (line.find("TODO") != std::string::npos ||
        line.find("FIXME") != std::string::npos ||
        line.find("XXX") != std::string::npos) {
      AnalysisResult result;
      result.error_type = AnalysisResult::ErrorType::kUnknown;
      result.error_category = "Code Quality";
      result.description = "TODO/FIXME comment found";
      result.file_path = file_path;
      result.line_number = line_number;
      result.suggested_fixes.push_back("Address the TODO/FIXME comment");
      results.push_back(result);
    }

    // Very long lines
    if (line.length() > 100) {
      AnalysisResult result;
      result.error_type = AnalysisResult::ErrorType::kUnknown;
      result.error_category = "Style";
      result.description = absl::StrCat("Line too long (", line.length(), " characters)");
      result.file_path = file_path;
      result.line_number = line_number;
      result.suggested_fixes.push_back("Break long line for better readability");
      results.push_back(result);
    }

    // Potential null pointer issues
    if (line.find("->") != std::string::npos &&
        line.find("if") == std::string::npos &&
        line.find("?") == std::string::npos) {
      // Simple heuristic: pointer dereference without obvious null check
      AnalysisResult result;
      result.error_type = AnalysisResult::ErrorType::kUnknown;
      result.error_category = "Potential Issue";
      result.description = "Pointer dereference without visible null check";
      result.file_path = file_path;
      result.line_number = line_number;
      result.confidence = 0.3;  // Low confidence heuristic
      result.suggested_fixes.push_back("Ensure pointer is checked for null before dereferencing");
      results.push_back(result);
    }
  }

  return results;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze