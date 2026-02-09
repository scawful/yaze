#ifndef YAZE_CLI_SERVICE_AGENT_TOOLS_PROJECT_GRAPH_TOOL_H_
#define YAZE_CLI_SERVICE_AGENT_TOOLS_PROJECT_GRAPH_TOOL_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/resources/command_context.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @class ProjectGraphTool
 * @brief Provides the AI agent with structured information about the project.
 *
 * This tool allows the agent to query:
 * - Basic project information (name, paths)
 * - File structure of the code folder
 * - Loaded symbol table from Asar
 */
class ProjectGraphTool : public resources::CommandHandler {
 public:
  ProjectGraphTool() = default;
  ~ProjectGraphTool() override = default;

  std::string GetName() const override { return "project-graph"; }
  std::string GetUsage() const override {
    return "project-graph --query=<info|files|symbols> [--path=<folder>]";
  }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  absl::Status GetProjectInfo(resources::OutputFormatter& formatter) const;
  absl::Status GetFileStructure(const std::string& path, resources::OutputFormatter& formatter) const;
  absl::Status GetSymbolTable(resources::OutputFormatter& formatter) const;
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_TOOLS_PROJECT_GRAPH_TOOL_H_
