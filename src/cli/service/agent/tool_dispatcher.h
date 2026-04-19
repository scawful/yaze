#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/agent/tool_registry.h"  // For ToolDefinition
#include "cli/service/ai/common.h"
#include "core/asar_wrapper.h"  // For AsarWrapper + AsarSymbol
#include "core/project.h"       // For YazeProject

namespace yaze {

class Rom;

namespace cli {
namespace agent {

// ToolCallType enum (moved to ToolRegistry for better cohesion)
// Removed here.

class ToolDispatcher {
 public:
  // ToolPreferences (remains here)
  struct ToolPreferences {
    bool resources = true;
    bool dungeon = true;
    bool overworld = true;
    bool messages = true;
    bool dialogue = true;  // Added missing dialogue
    bool gui = true;
    bool music = true;
    bool sprite = true;
#ifdef YAZE_WITH_GRPC
    bool emulator = true;
#else
    bool emulator = false;
#endif
    bool filesystem = true;
    bool build = true;  // Placeholder for future build tools
    bool memory_inspector = true;
    bool test_helpers = true;
    bool meta_tools = true;  // tools-list, tools-describe, tools-search
    bool visual_analysis = true;
    bool code_gen = true;
    bool project = true;  // Project management tools
    bool allow_mutating_tools = false;
  };

  /**
   * @brief Tool information for discoverability
   */
  struct ToolInfo {
    std::string name;
    std::string category;
    std::string description;
    std::string usage;
    std::vector<std::string> examples;
    bool requires_rom;
    bool requires_project;  // New: distinguish project vs ROM dependency
    bool requires_authorization;
    std::vector<std::string> required_args;
    std::vector<std::string> flag_args;
  };

  /**
   * @brief Get list of all available tools
   */
  std::vector<ToolInfo> GetAvailableTools() const;

  /**
   * @brief Get detailed information about a specific tool
   */
  std::optional<ToolInfo> GetToolInfo(const std::string& tool_name) const;

  /**
   * @brief Search tools by keyword
   */
  std::vector<ToolInfo> SearchTools(const std::string& query) const;

  /**
   * @brief Batch tool call request
   */
  struct BatchToolCall {
    std::vector<ToolCall> calls;
    bool parallel = false;  // If true, execute independent calls in parallel
  };

  /**
   * @brief Result of a batch tool call
   */
  struct BatchResult {
    std::vector<std::string> results;
    std::vector<absl::Status> statuses;
    double total_execution_time_ms = 0.0;
    size_t successful_count = 0;
    size_t failed_count = 0;
  };

  /**
   * @brief Execute multiple tool calls in a batch
   *
   * When parallel=false, calls are executed sequentially.
   * When parallel=true, calls are executed concurrently (if no dependencies).
   */
  BatchResult DispatchBatch(const BatchToolCall& batch);

  ToolDispatcher() = default;

  // Execute a tool call and return the result as a string.
  absl::StatusOr<std::string> Dispatch(const ::yaze::cli::ToolCall& tool_call);

  // Provide ROM, Project, and Asar contexts for tool calls.
  void SetRomContext(Rom* rom) { rom_context_ = rom; }
  void SetProjectContext(project::YazeProject* project) {
    project_context_ = project;
  }
  void SetAsarWrapper(core::AsarWrapper* asar_wrapper) {
    asar_wrapper_ = asar_wrapper;
  }
  // Backend-agnostic assembly symbol table. Preferred path for tools that
  // just need to read labels — works for both Asar and z3dk backends.
  void SetAssemblySymbolTable(
      const std::map<std::string, core::AsarSymbol>* table) {
    assembly_symbol_table_ = table;
  }

  void SetToolPreferences(const ToolPreferences& prefs) {
    preferences_ = prefs;
  }
  const ToolPreferences& preferences() const { return preferences_; }

 private:
  // Check if a tool is enabled based on preferences and its category
  bool IsToolEnabled(const ToolDefinition& def) const;
  absl::Status ValidateCall(const ToolDefinition& def,
                            const ToolCall& call) const;
  absl::StatusOr<std::string> DispatchMetaTool(const ToolCall& call) const;

  Rom* rom_context_ = nullptr;
  project::YazeProject* project_context_ = nullptr;  // New: Project context
  core::AsarWrapper* asar_wrapper_ = nullptr;  // New: Asar wrapper context
  const std::map<std::string, core::AsarSymbol>* assembly_symbol_table_ =
      nullptr;  // Backend-agnostic symbol source (preferred)
  // Lazy snapshot of asar_wrapper_->GetSymbolTable() used when no explicit
  // symbol-table pointer was supplied. See Dispatch().
  std::map<std::string, core::AsarSymbol> asar_symbols_cache_;
  ToolPreferences preferences_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
