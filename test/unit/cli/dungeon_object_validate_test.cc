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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

TEST(DungeonObjectValidateTest, TraceDumpWritesFile) {
  DungeonObjectValidateCommandHandler handler;
  const auto trace_path = std::filesystem::temp_directory_path() /
                          "yaze_dungeon_trace_dump_test.json";

  if (std::filesystem::exists(trace_path)) {
    std::filesystem::remove(trace_path);
  }

  std::string output;
  auto status =
      handler.Run({"--mock-rom", "--object=0x000", "--size=0", "--trace-out",
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

  std::string output;
  auto status =
      handler.Run({"--mock-rom", "--object=0x009", "--size=0", "--format=json"},
                  nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_THAT(output, ::testing::HasSubstr("\"mismatch_count\": 0"));
}

}  // namespace
}  // namespace yaze::cli
