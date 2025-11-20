#ifndef YAZE_CLI_SERVICE_AGENT_AGENT_PRETRAINING_H_
#define YAZE_CLI_SERVICE_AGENT_AGENT_PRETRAINING_H_

#include <string>
#include <vector>
#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

/**
 * @brief Pre-training system for AI agents
 * 
 * Provides structured knowledge injection before interactive use.
 * Teaches agent about ROM structure, common patterns, and tool usage.
 */
class AgentPretraining {
 public:
  struct KnowledgeModule {
    std::string name;
    std::string content;
    bool required;
  };

  /**
   * @brief Load all pre-training modules
   */
  static std::vector<KnowledgeModule> GetModules();

  /**
   * @brief Get ROM structure explanation
   */
  static std::string GetRomStructureKnowledge(Rom* rom);

  /**
   * @brief Get hex data analysis patterns
   */
  static std::string GetHexAnalysisKnowledge();

  /**
   * @brief Get map editing workflow
   */
  static std::string GetMapEditingKnowledge();

  /**
   * @brief Get tool usage examples
   */
  static std::string GetToolUsageExamples();

  /**
   * @brief Generate pre-training prompt for agent
   */
  static std::string GeneratePretrainingPrompt(Rom* rom);
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_AGENT_PRETRAINING_H_
