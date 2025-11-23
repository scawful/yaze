#ifndef YAZE_SRC_CLI_SERVICE_AGENT_DEV_ASSIST_AGENT_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_DEV_ASSIST_AGENT_H_

#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/common.h"

namespace yaze {
namespace cli {
namespace agent {

// Forward declarations
class ToolDispatcher;

/**
 * @brief Development Assistant Agent for AI-assisted yaze development
 *
 * This agent helps developers with:
 * - Build error resolution
 * - Crash analysis
 * - Test automation and suggestions
 * - Code analysis and improvements
 */
class DevAssistAgent {
 public:
  /**
   * @brief Analysis result for build errors or crashes
   */
  struct AnalysisResult {
    // Error classification
    enum class ErrorType {
      kUnknown,
      kCompilationError,
      kLinkError,
      kCMakeError,
      kMissingHeader,
      kUndefinedSymbol,
      kTypeMismatch,
      kCircularDependency,
      kSegmentationFault,
      kAssertionFailure,
      kStackOverflow,
      kTestFailure,
    };

    ErrorType error_type = ErrorType::kUnknown;
    std::string error_category;  // High-level category (e.g., "Build", "Runtime")
    std::string file_path;       // File where error occurred
    int line_number = 0;          // Line number if applicable
    std::string description;      // Human-readable description
    std::string raw_error;        // Original error message

    // Analysis and suggestions
    std::vector<std::string> suggested_fixes;  // Ordered fix suggestions
    std::vector<std::string> related_files;    // Files that may be involved
    std::string root_cause;                    // Root cause analysis

    // Confidence and metadata
    double confidence = 0.0;  // 0.0 to 1.0 confidence in analysis
    bool ai_assisted = false;  // Whether AI was used for suggestions
  };

  /**
   * @brief Build monitoring configuration
   */
  struct BuildConfig {
    std::string build_dir = "build";
    std::string preset = "";  // CMake preset to use
    bool verbose = false;
    bool parallel = true;
    int parallel_jobs = 0;  // 0 = auto-detect
    bool stop_on_error = false;
  };

  /**
   * @brief Test suggestion for changed code
   */
  struct TestSuggestion {
    std::string test_file;       // Suggested test file path
    std::string test_name;        // Test case name
    std::string reason;           // Why this test is relevant
    std::string test_code;        // Suggested test code (if generated)
    bool is_existing = true;      // false if this is a new test suggestion
  };

  DevAssistAgent();
  ~DevAssistAgent();

  /**
   * @brief Initialize the agent with optional AI service
   *
   * @param tool_dispatcher Tool dispatcher for accessing build/filesystem tools
   * @param ai_service Optional AI service for enhanced suggestions
   * @return Status of initialization
   */
  absl::Status Initialize(std::shared_ptr<ToolDispatcher> tool_dispatcher,
                         std::shared_ptr<yaze::cli::AIService> ai_service = nullptr);

  /**
   * @brief Analyze build output for errors and warnings
   *
   * @param output Build output to analyze
   * @return Vector of analysis results, one per error/warning
   */
  std::vector<AnalysisResult> AnalyzeBuildOutput(const std::string& output);

  /**
   * @brief Analyze a crash or stack trace
   *
   * @param stack_trace Stack trace or crash dump
   * @return Analysis result with suggested fixes
   */
  AnalysisResult AnalyzeCrash(const std::string& stack_trace);

  /**
   * @brief Get suggested tests for changed files
   *
   * @param changed_files List of modified source files
   * @return Vector of test suggestions
   */
  std::vector<TestSuggestion> GetAffectedTests(
      const std::vector<std::string>& changed_files);

  /**
   * @brief Generate test code for a function or class
   *
   * @param source_file Path to source file
   * @param function_name Function or class to test (optional, tests whole file if empty)
   * @return Generated test code or error status
   */
  absl::StatusOr<std::string> GenerateTestCode(
      const std::string& source_file,
      const std::string& function_name = "");

  /**
   * @brief Monitor build process interactively
   *
   * @param config Build configuration
   * @param on_error Callback for each error found
   * @return Status of monitoring (OK if completed, error if monitoring failed)
   */
  absl::Status MonitorBuild(
      const BuildConfig& config,
      std::function<void(const AnalysisResult&)> on_error);

  /**
   * @brief Run a build command and analyze output
   *
   * @param command Build command to execute (e.g., "cmake --build build")
   * @return Analysis results for any errors found
   */
  absl::StatusOr<std::vector<AnalysisResult>> RunBuildWithAnalysis(
      const std::string& command);

  /**
   * @brief Run tests and analyze failures
   *
   * @param test_pattern Test pattern to run (empty = all tests)
   * @return Analysis results for any test failures
   */
  absl::StatusOr<std::vector<AnalysisResult>> RunTestsWithAnalysis(
      const std::string& test_pattern = "");

  /**
   * @brief Check code for common issues
   *
   * @param file_path Source file to analyze
   * @return Analysis results for any issues found
   */
  std::vector<AnalysisResult> AnalyzeCodeFile(const std::string& file_path);

  /**
   * @brief Get build system status and health
   *
   * @return Human-readable status report
   */
  std::string GetBuildStatus() const;

  /**
   * @brief Enable/disable AI-powered suggestions
   */
  void SetAIEnabled(bool enabled) { use_ai_ = enabled; }

  /**
   * @brief Check if AI service is available
   */
  bool IsAIAvailable() const { return ai_service_ != nullptr; }

 private:
  // Error pattern matchers
  struct ErrorPattern {
    std::regex pattern;
    AnalysisResult::ErrorType type;
    std::string category;
    std::function<void(const std::smatch&, AnalysisResult&)> extractor;
  };

  // Initialize error patterns for various compilers
  void InitializeErrorPatterns();

  // Parse specific error types
  AnalysisResult ParseCompilationError(const std::string& line);
  AnalysisResult ParseLinkError(const std::string& line);
  AnalysisResult ParseCMakeError(const std::string& line);
  AnalysisResult ParseTestFailure(const std::string& output);
  AnalysisResult ParseSegfault(const std::string& stack_trace);

  // Extract information from error messages
  void ExtractFileAndLine(const std::string& text, AnalysisResult& result);
  void ExtractUndefinedSymbol(const std::string& text, AnalysisResult& result);

  // Generate fix suggestions
  void GenerateFixSuggestions(AnalysisResult& result);
  void SuggestMissingHeaderFix(AnalysisResult& result);
  void SuggestLinkOrderFix(AnalysisResult& result);
  void SuggestTypeMismatchFix(AnalysisResult& result);

  // AI-enhanced analysis
  absl::StatusOr<std::string> GetAISuggestion(const AnalysisResult& result);
  absl::StatusOr<std::string> GenerateTestWithAI(
      const std::string& source_code,
      const std::string& function_name);

  // Test discovery
  std::vector<std::string> FindTestsForFile(const std::string& source_file);
  bool IsTestFile(const std::string& file_path) const;
  std::string GetTestFileForSource(const std::string& source_file) const;

  // Build system interaction
  absl::StatusOr<std::string> ExecuteCommand(const std::string& command);
  absl::Status RunCMakeConfigure(const BuildConfig& config);
  absl::Status RunCMakeBuild(const BuildConfig& config);

  // Member variables
  std::shared_ptr<ToolDispatcher> tool_dispatcher_;
  std::shared_ptr<yaze::cli::AIService> ai_service_;
  std::vector<ErrorPattern> error_patterns_;
  bool use_ai_ = true;
  bool initialized_ = false;

  // Cache for test mappings
  std::map<std::string, std::vector<std::string>> test_file_cache_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_DEV_ASSIST_AGENT_H_