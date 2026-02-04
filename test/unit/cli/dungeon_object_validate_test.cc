#include "cli/handlers/tools/dungeon_object_validate_commands.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

TEST(DungeonObjectValidateTest, TraceDumpWritesFile) {
  DungeonObjectValidateCommandHandler handler;
  const auto trace_path =
      std::filesystem::temp_directory_path() / "yaze_dungeon_trace_dump_test.json";

  if (std::filesystem::exists(trace_path)) {
    std::filesystem::remove(trace_path);
  }

  std::string output;
  auto status = handler.Run(
      {"--mock-rom", "--object=0x000", "--size=0", "--trace-out",
       trace_path.string(), "--format=json"},
      nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_THAT(output, ::testing::HasSubstr("trace_dump"));
  ASSERT_TRUE(std::filesystem::exists(trace_path));

  std::ifstream infile(trace_path);
  std::string contents((std::istreambuf_iterator<char>(infile)),
                       std::istreambuf_iterator<char>());
  EXPECT_THAT(contents, ::testing::HasSubstr("\"case_count\""));
  EXPECT_THAT(contents, ::testing::HasSubstr("\"cases\""));

  std::filesystem::remove(trace_path);
}

}  // namespace
}  // namespace yaze::cli
