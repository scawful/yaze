#ifndef YAZE_SRC_CLI_MODERN_CLI_H_
#define YAZE_SRC_CLI_MODERN_CLI_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/z3ed.h"

namespace yaze {
namespace cli {

struct CommandInfo {
  std::string name;
  std::string description;
  std::string usage;
  std::function<absl::Status(const std::vector<std::string>&)> handler;
};

class ModernCLI {
 public:
  ModernCLI();
  absl::Status Run(int argc, char* argv[]);
  CommandHandler* GetCommandHandler(const std::string& name);
  void PrintTopLevelHelp() const;

  std::map<std::string, CommandInfo> commands_;

 private:
  void SetupCommands();
  void ShowHelp();

  // Command Handlers
  absl::Status HandleAsarPatchCommand(const std::vector<std::string>& args);
  absl::Status HandleBpsPatchCommand(const std::vector<std::string>& args);
  absl::Status HandleExtractSymbolsCommand(const std::vector<std::string>& args);
  absl::Status HandleAgentCommand(const std::vector<std::string>& args);
  absl::Status HandleProjectBuildCommand(const std::vector<std::string>& args);
  absl::Status HandleProjectInitCommand(const std::vector<std::string>& args);
  absl::Status HandleRomInfoCommand(const std::vector<std::string>& args);
  absl::Status HandleRomGenerateGoldenCommand(const std::vector<std::string>& args);
  absl::Status HandleRomDiffCommand(const std::vector<std::string>& args);
  absl::Status HandleDungeonExportCommand(const std::vector<std::string>& args);
  absl::Status HandleDungeonListObjectsCommand(const std::vector<std::string>& args);
  absl::Status HandleGfxExportCommand(const std::vector<std::string>& args);
  absl::Status HandleGfxImportCommand(const std::vector<std::string>& args);
  absl::Status HandleCommandPaletteCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteExportCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteImportCommand(const std::vector<std::string>& args);
  absl::Status HandlePaletteCommand(const std::vector<std::string>& args);
  absl::Status HandleRomValidateCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldGetTileCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldFindTileCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldDescribeMapCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldListWarpsCommand(const std::vector<std::string>& args);
  absl::Status HandleOverworldSetTileCommand(const std::vector<std::string>& args);
  absl::Status HandleSpriteCreateCommand(const std::vector<std::string>& args);
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_MODERN_CLI_H_
