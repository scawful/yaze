#ifndef YAZE_CLI_CLI_H
#define YAZE_CLI_CLI_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/macro.h"

#include "cli/service/resources/command_handler.h"
#include <map>
#include <memory>

// Forward declarations
namespace ftxui {
class ScreenInteractive;
}

namespace yaze {
namespace cli {

// Forward declaration
class TuiComponent;

class ModernCLI {
 public:
  ModernCLI();
  absl::Status Run(int argc, char* argv[]);
  void PrintTopLevelHelp() const;
  void PrintCategoryHelp(const std::string& category) const;
  void PrintCommandSummary() const;

 private:
  void SetupCommands();
  void ShowHelp();
  void ShowCategoryHelp(const std::string& category) const;
  void ShowCommandSummary() const;

  std::map<std::string, std::unique_ptr<resources::CommandHandler>> commands_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_CLI_H
