#ifndef YAZE_CLI_SERVICE_AGENT_ADVANCED_ROUTING_H_
#define YAZE_CLI_SERVICE_AGENT_ADVANCED_ROUTING_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

/**
 * @brief Advanced routing system for agent tool responses
 *
 * Optimizes information flow back to agent for:
 * - Map editing with GUI automation
 * - Hex data analysis and pattern recognition
 * - Multi-step operations with context preservation
 */
class AdvancedRouter {
 public:
  struct RouteContext {
    Rom* rom;
    std::string user_intent;
    std::vector<std::string> tool_calls_made;
    std::string accumulated_knowledge;
  };

  struct RoutedResponse {
    std::string summary;                   // High-level answer
    std::string detailed_data;             // Raw data for agent processing
    std::string next_steps;                // Suggested follow-up actions
    std::vector<std::string> gui_actions;  // For test harness
    bool needs_approval;                   // For proposals
  };

  /**
   * @brief Route hex data analysis response
   */
  static RoutedResponse RouteHexAnalysis(const std::vector<uint8_t>& data,
                                         uint32_t address,
                                         const RouteContext& ctx);

  /**
   * @brief Route map editing response
   */
  static RoutedResponse RouteMapEdit(const std::string& edit_intent,
                                     const RouteContext& ctx);

  /**
   * @brief Route palette analysis response
   */
  static RoutedResponse RoutePaletteAnalysis(
      const std::vector<uint16_t>& colors, const RouteContext& ctx);

  /**
   * @brief Synthesize multi-tool response
   */
  static RoutedResponse SynthesizeMultiToolResponse(
      const std::vector<std::string>& tool_results, const RouteContext& ctx);

  /**
   * @brief Generate GUI automation script
   */
  static std::string GenerateGUIScript(const std::vector<std::string>& actions);

 private:
  static std::string InferDataType(const std::vector<uint8_t>& data);
  static std::vector<std::string> ExtractPatterns(
      const std::vector<uint8_t>& data);
  static std::string FormatForAgent(const std::string& raw_data);
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_ADVANCED_ROUTING_H_
