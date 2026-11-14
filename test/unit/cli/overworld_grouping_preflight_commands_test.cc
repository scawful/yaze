#include "cli/handlers/tools/overworld_grouping_preflight_commands.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "cli/service/command_registry.h"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

TEST(OverworldGroupingPreflightCommandTest, RegisteredInTheOverworldCategory) {
  auto& registry = CommandRegistry::Instance();
  const auto commands = registry.GetCommandsInCategory("overworld");
  EXPECT_NE(std::find(commands.begin(), commands.end(),
                      "overworld-grouping-preflight"),
            commands.end());
  const std::string help =
      registry.GenerateHelp("overworld-grouping-preflight");
  EXPECT_NE(help.find("parent"), std::string::npos) << help;
  EXPECT_NE(help.find("experimental-page-tile16"), std::string::npos) << help;
}

TEST(OverworldGroupingPreflightCommandTest, RequiresGroupingArgs) {
  OverworldGroupingPreflightCommandHandler handler;
  std::string out;
  Rom empty;
  const absl::Status status = handler.Run({"--format=json"}, &empty, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_NE(std::string(status.message()).find("parent"), std::string::npos)
      << status;
}

}  // namespace
}  // namespace yaze::cli
