#include "cli/handlers/tools/dungeon_object_validate_commands.h"

#include "cli/handlers/rom/mock_rom.h"
#include "testing.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room_object.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace yaze::cli {
namespace {

class ScopedValidationReport {
 public:
  explicit ScopedValidationReport(std::string name)
      : base_path_(std::filesystem::temp_directory_path() / std::move(name)) {
    Cleanup();
  }

  ~ScopedValidationReport() { Cleanup(); }

  std::string argument() const { return "--report=" + base_path_.string(); }

 private:
  void Cleanup() const {
    std::error_code error;
    std::filesystem::remove(base_path_.string() + ".json", error);
    error.clear();
    std::filesystem::remove(base_path_.string() + ".csv", error);
  }

  std::filesystem::path base_path_;
};

TEST(DungeonObjectValidateTest, TraceDumpWritesFile) {
  DungeonObjectValidateCommandHandler handler;
  ScopedValidationReport report("yaze_dungeon_trace_dump_report_test");
  const auto trace_path = std::filesystem::temp_directory_path() /
                          "yaze_dungeon_trace_dump_test.json";

  if (std::filesystem::exists(trace_path)) {
    std::filesystem::remove(trace_path);
  }

  std::string output;
  auto status =
      handler.Run({"--mock-rom", "--object=0x000", "--size=0", "--trace-out",
                   trace_path.string(), "--format=json", report.argument()},
                  nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_THAT(output, ::testing::HasSubstr("trace_dump"));
  ASSERT_TRUE(std::filesystem::exists(trace_path));

  std::ifstream infile(trace_path);
  std::string contents((std::istreambuf_iterator<char>(infile)),
                       std::istreambuf_iterator<char>());
  const auto trace_json = nlohmann::json::parse(contents);
  EXPECT_EQ(trace_json.at("case_count"), 1);
  EXPECT_EQ(trace_json.at("empty_case_count"), 0);
  EXPECT_EQ(trace_json.at("unexpected_empty_case_count"), 0);
  EXPECT_EQ(trace_json.at("expected_empty_case_count"), 0);
  ASSERT_EQ(trace_json.at("cases").size(), 1);
  EXPECT_TRUE(trace_json.at("cases").at(0).at("expected_has_tiles"));
  infile.close();

  std::filesystem::remove(trace_path);
}

TEST(DungeonObjectValidateTest, ClipSelectionBoundsToRoomClipsRepeatSpacing) {
  // Initialize ObjectDimensionTable so DimensionService's fallback path works.
  auto& table = zelda3::ObjectDimensionTable::Get();
  table.Reset();
  ::yaze::Rom rom;
  ASSERT_OK(InitializeMockRom(rom));
  ASSERT_OK(table.LoadFromRom(&rom));

  const int object_id = 0x3A;  // Decor 4x3 spaced 4 (defaults)
  const int size = 2;
  const int object_x = 56;
  const int object_y = 10;

  zelda3::RoomObject obj(object_id, object_x, object_y,
                         static_cast<uint8_t>(size), 0);
  auto bounds = zelda3::DimensionService::Get().GetDimensions(obj);
  auto clipped = detail::ClipSelectionBoundsToRoom(object_id, size, bounds,
                                                   object_x, object_y);

  EXPECT_EQ(clipped.offset_x_tiles, 0);
  EXPECT_EQ(clipped.offset_y_tiles, 0);
  EXPECT_EQ(clipped.width_tiles, 4);
  EXPECT_EQ(clipped.height_tiles, 3);

  table.Reset();
}

TEST(DungeonObjectValidateTest,
     ClipSelectionBoundsToRoomHandlesZeroBaseHeight) {
  // Initialize ObjectDimensionTable so DimensionService's fallback path works.
  auto& table = zelda3::ObjectDimensionTable::Get();
  table.Reset();
  ::yaze::Rom rom;
  ASSERT_OK(InitializeMockRom(rom));
  ASSERT_OK(table.LoadFromRom(&rom));

  const int object_id = 0x60;  // Vertical objects with base_h=0
  const int size = 1;
  const int object_x = 0;
  const int object_y = zelda3::DrawContext::kMaxTilesY - 1;

  zelda3::RoomObject obj(object_id, object_x, object_y,
                         static_cast<uint8_t>(size), 0);
  auto bounds = zelda3::DimensionService::Get().GetDimensions(obj);
  auto clipped = detail::ClipSelectionBoundsToRoom(object_id, size, bounds,
                                                   object_x, object_y);

  EXPECT_EQ(clipped.width_tiles, bounds.width_tiles);
  EXPECT_EQ(clipped.height_tiles, 1);

  table.Reset();
}

TEST(DungeonObjectValidateTest,
     NonRoomModeAvoidsFalseMismatchesFromNegativeOffsets) {
  DungeonObjectValidateCommandHandler handler;
  ScopedValidationReport report("yaze_dungeon_negative_offset_report_test");

  std::string output;
  auto status = handler.Run({"--mock-rom", "--object=0x009", "--size=0",
                             "--format=json", report.argument()},
                            nullptr, &output);

  ASSERT_TRUE(status.ok());
  const auto json = nlohmann::json::parse(output);
  EXPECT_EQ(json.at("mismatch_count"), 0);
}

TEST(DungeonObjectValidateTest, AllSizesValidatesEveryType1SizeNibble) {
  DungeonObjectValidateCommandHandler handler;
  ScopedValidationReport report("yaze_dungeon_all_sizes_report_test");

  std::string output;
  auto status = handler.Run({"--mock-rom", "--object=0x000", "--all-sizes",
                             "--format=json", report.argument()},
                            nullptr, &output);

  ASSERT_TRUE(status.ok()) << status;
  const auto json = nlohmann::json::parse(output);
  EXPECT_EQ(json.at("size_cases"), 16);
  EXPECT_EQ(json.at("state_cases"), 1);
  EXPECT_EQ(json.at("test_cases"), 16);
  EXPECT_EQ(json.at("mismatch_count"), 0);
}

TEST(DungeonObjectValidateTest, AllSizesUsesOnlyFixedSubtypeLegalSizes) {
  struct TestCase {
    const char* object_arg;
    const char* report_name;
    const char* trace_name;
    int expected_size;
  };

  for (const auto& test_case : {
           TestCase{"--object=0x100", "yaze_dungeon_type2_size_report_test",
                    "yaze_dungeon_type2_size_trace_test.json", 0},
           TestCase{"--object=0xF83", "yaze_dungeon_type3_size_report_test",
                    "yaze_dungeon_type3_size_trace_test.json", 12},
       }) {
    SCOPED_TRACE(test_case.object_arg);
    DungeonObjectValidateCommandHandler handler;
    ScopedValidationReport report(test_case.report_name);
    const auto trace_path =
        std::filesystem::temp_directory_path() / test_case.trace_name;
    std::filesystem::remove(trace_path);

    std::string output;
    auto status = handler.Run(
        {"--mock-rom", test_case.object_arg, "--all-sizes", "--trace-out",
         trace_path.string(), "--format=json", report.argument()},
        nullptr, &output);

    ASSERT_TRUE(status.ok()) << status;
    const auto json = nlohmann::json::parse(output);
    EXPECT_EQ(json.at("size_cases"), 1);
    EXPECT_EQ(json.at("test_cases"), 1);

    std::ifstream infile(trace_path);
    const auto trace_json = nlohmann::json::parse(infile);
    ASSERT_EQ(trace_json.at("cases").size(), 1);
    EXPECT_EQ(trace_json.at("cases").at(0).at("size"), test_case.expected_size);
    infile.close();
    std::filesystem::remove(trace_path);
  }
}

TEST(DungeonObjectValidateTest, AllStatesReportsDefaultAndActiveProfiles) {
  DungeonObjectValidateCommandHandler handler;
  ScopedValidationReport report("yaze_dungeon_all_states_report_test");
  const auto trace_path = std::filesystem::temp_directory_path() /
                          "yaze_dungeon_state_trace_test.json";
  std::filesystem::remove(trace_path);

  std::string output;
  auto status = handler.Run(
      {"--mock-rom", "--object=0xF80", "--all-states", "--trace-out",
       trace_path.string(), "--format=json", report.argument()},
      nullptr, &output);

  ASSERT_TRUE(status.ok()) << status;
  const auto json = nlohmann::json::parse(output);
  EXPECT_EQ(json.at("state_cases"), 2);
  EXPECT_EQ(json.at("test_cases"), 2);
  EXPECT_EQ(json.at("mismatch_count"), 0);
  EXPECT_EQ(json.at("expected_empty_traces"), 0);
  ASSERT_TRUE(std::filesystem::exists(trace_path));

  std::ifstream infile(trace_path);
  std::string contents((std::istreambuf_iterator<char>(infile)),
                       std::istreambuf_iterator<char>());
  const auto trace_json = nlohmann::json::parse(contents);
  ASSERT_EQ(trace_json.at("cases").size(), 2);
  EXPECT_EQ(trace_json.at("cases").at(0).at("state_profile"), "default");
  EXPECT_EQ(trace_json.at("cases").at(1).at("state_profile"), "active");
  EXPECT_TRUE(trace_json.at("cases").at(0).at("expected_has_tiles"));
  EXPECT_TRUE(trace_json.at("cases").at(1).at("expected_has_tiles"));
  infile.close();
  std::filesystem::remove(trace_path);
}

TEST(DungeonObjectValidateTest, AllStatesTracksExpectedEmptyBranches) {
  struct ExpectedEmptyCase {
    const char* object_arg;
    const char* label;
    bool needs_size;
  };

  for (const auto& test_case : {
           ExpectedEmptyCase{"--object=0x0CD", "west", true},
           ExpectedEmptyCase{"--object=0x0CE", "east", true},
           ExpectedEmptyCase{"--object=0xF92", "rupee_floor", false},
           ExpectedEmptyCase{"--object=0xF98", "big_key_lock", false},
       }) {
    SCOPED_TRACE(test_case.object_arg);
    DungeonObjectValidateCommandHandler handler;
    ScopedValidationReport report(std::string("yaze_dungeon_expected_empty_") +
                                  test_case.label + "_report_test");
    const auto trace_path = std::filesystem::temp_directory_path() /
                            (std::string("yaze_dungeon_expected_empty_") +
                             test_case.label + "_trace_test.json");
    std::filesystem::remove(trace_path);

    std::vector<std::string> args{"--mock-rom", test_case.object_arg};
    if (test_case.needs_size) {
      args.emplace_back("--size=0");
    }
    args.insert(args.end(), {"--all-states", "--trace-out", trace_path.string(),
                             "--format=json", report.argument()});

    std::string output;
    auto status = handler.Run(args, nullptr, &output);

    ASSERT_TRUE(status.ok()) << status;
    const auto json = nlohmann::json::parse(output);
    EXPECT_EQ(json.at("state_cases"), 2);
    EXPECT_EQ(json.at("test_cases"), 2);
    EXPECT_EQ(json.at("mismatch_count"), 0);
    EXPECT_EQ(json.at("empty_traces"), 0);
    EXPECT_EQ(json.at("expected_empty_traces"), 1);

    std::ifstream infile(trace_path);
    const auto trace_json = nlohmann::json::parse(infile);
    EXPECT_EQ(trace_json.at("empty_case_count"), 1);
    EXPECT_EQ(trace_json.at("unexpected_empty_case_count"), 0);
    EXPECT_EQ(trace_json.at("expected_empty_case_count"), 1);
    ASSERT_EQ(trace_json.at("cases").size(), 2);
    EXPECT_TRUE(trace_json.at("cases").at(0).at("expected_has_tiles"));
    EXPECT_FALSE(trace_json.at("cases").at(1).at("expected_has_tiles"));
    EXPECT_FALSE(trace_json.at("cases").at(0).at("tiles").empty());
    EXPECT_TRUE(trace_json.at("cases").at(1).at("tiles").empty());
    infile.close();
    std::filesystem::remove(trace_path);
  }
}

TEST(DungeonObjectValidateTest, RejectsSizeAndAllSizesTogether) {
  DungeonObjectValidateCommandHandler handler;

  std::string output;
  auto status =
      handler.Run({"--mock-rom", "--object=0x000", "--size=0", "--all-sizes"},
                  nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              ::testing::HasSubstr("--all-sizes cannot be combined"));
}

TEST(DungeonObjectValidateTest, RejectsRoomWithSizeFlags) {
  for (const char* size_flag : {"--size=0", "--all-sizes"}) {
    DungeonObjectValidateCommandHandler handler;
    std::vector<std::string> args{"--mock-rom", "--room=0", size_flag};

    std::string output;
    auto status = handler.Run(args, nullptr, &output);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(status.message(),
                ::testing::HasSubstr("--room cannot be combined"));
  }
}

TEST(DungeonObjectValidateTest, RejectsSizeWithoutType1Object) {
  for (const auto& args : {
           std::vector<std::string>{"--mock-rom", "--size=0"},
           std::vector<std::string>{"--mock-rom", "--object=0x100", "--size=0"},
           std::vector<std::string>{"--mock-rom", "--object=0xF83",
                                    "--size=12"},
       }) {
    DungeonObjectValidateCommandHandler handler;

    std::string output;
    auto status = handler.Run(args, nullptr, &output);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(status.message(),
                ::testing::HasSubstr("--size requires a Type 1 --object"));
  }
}

TEST(DungeonObjectValidateTest,
     CanonicalDoubledAndBarPayloadsAvoidSingleTileFallbackMismatches) {
  for (const char* object_arg : {"--object=0x03C", "--object=0x04C"}) {
    SCOPED_TRACE(object_arg);
    DungeonObjectValidateCommandHandler handler;
    const auto report_base =
        std::filesystem::temp_directory_path() /
        (std::string("yaze_dungeon_payload_count_") + object_arg + "_test");
    const auto json_report = report_base.string() + ".json";
    const auto csv_report = report_base.string() + ".csv";
    std::filesystem::remove(json_report);
    std::filesystem::remove(csv_report);

    std::string output;
    auto status =
        handler.Run({"--mock-rom", object_arg, "--size=0", "--format=json",
                     "--report=" + report_base.string()},
                    nullptr, &output);
    std::filesystem::remove(json_report);
    std::filesystem::remove(csv_report);

    ASSERT_TRUE(status.ok()) << status;
    EXPECT_THAT(output, ::testing::HasSubstr("\"test_cases\": 1"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"mismatch_count\": 0"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"empty_traces\": 0"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"mismatches\": []"));
  }
}

}  // namespace
}  // namespace yaze::cli
