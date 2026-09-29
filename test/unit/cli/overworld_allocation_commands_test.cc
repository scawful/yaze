#include "cli/handlers/tools/overworld_allocation_commands.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "cli/service/command_registry.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unit/zelda3/overworld_allocation_test_rom.h"

namespace yaze::cli {
namespace {

Rom LoadRom(const std::vector<uint8_t>& bytes) {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

TEST(OverworldAllocationCommandTest, JsonReportWithTailCheck) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  OverworldAllocationCommandHandler handler;
  std::string out;
  const absl::Status status =
      handler.Run({"--format=json", "--tail-check"}, &rom, &out);
  ASSERT_TRUE(status.ok()) << status;
  const auto begin = out.find('{');
  ASSERT_NE(begin, std::string::npos) << out;
  const auto json = nlohmann::json::parse(out.substr(begin), nullptr, false);
  ASSERT_FALSE(json.is_discarded()) << out;

  EXPECT_EQ(json["catalogue"], "zscustom_v3");
  ASSERT_EQ(json["summary"].size(), 4u);
  EXPECT_EQ(json["summary"][3]["world"], "special_tail");
  EXPECT_EQ(json["summary"][3]["free"], 32);
  EXPECT_EQ(json["areas"].size(), 0xC0u);
  EXPECT_EQ(json["areas"][0xA0]["map"]["source"], "none");
  EXPECT_EQ(json["tail_check"]["verdict"], "unsafe");
  EXPECT_EQ(json["tail_check"]["rom_compatible"], true);
}

TEST(OverworldAllocationCommandTest, TextReportOmitsTailCheckByDefault) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/false));
  OverworldAllocationCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=text"}, &rom, &out).ok()) << out;
  EXPECT_NE(out.find("Summary"), std::string::npos) << out;
  EXPECT_NE(out.find("special_tail"), std::string::npos);
  EXPECT_EQ(out.find("TailMapExpansion check"), std::string::npos);
}

TEST(OverworldAllocationCommandTest, RegisteredInTheOverworldCategory) {
  auto& registry = CommandRegistry::Instance();
  const auto commands = registry.GetCommandsInCategory("overworld");
  EXPECT_NE(std::find(commands.begin(), commands.end(), "overworld-allocation"),
            commands.end());
  const std::string help = registry.GenerateHelp("overworld-allocation");
  EXPECT_NE(help.find("--tail-check"), std::string::npos) << help;
}

}  // namespace
}  // namespace yaze::cli
