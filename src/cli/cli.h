#ifndef YAZE_CLI_CLI_H
#define YAZE_CLI_CLI_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/macro.h"

namespace yaze {
namespace cli {

class ModernCLI {
 public:
  ModernCLI();
  absl::Status Run(int argc, char* argv[]);
  void PrintTopLevelHelp() const;
  void PrintCategoryHelp(const std::string& category) const;
  void PrintCommandSummary() const;

 private:
  void ShowHelp();
  void ShowCategoryHelp(const std::string& category) const;
  void ShowCommandSummary() const;

  // Commands are now managed by CommandRegistry singleton (no member needed)
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_CLI_H
