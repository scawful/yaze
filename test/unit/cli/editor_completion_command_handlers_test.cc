#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/overworld_commands.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

TEST(EditorCompletionCommandHandlersTest,
     OverworldGetTileRejectsOutOfRangeMap) {
  handlers::OverworldGetTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--map=0xA0", "--x=0", "--y=0", "--format=json"}, nullptr,
      &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr("Map ID out of range"));
}

TEST(EditorCompletionCommandHandlersTest,
     OverworldSetTileRejectsOutOfRangeTileId) {
  handlers::OverworldSetTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--mock-rom", "--map=0x00", "--x=1", "--y=1",
                                   "--tile=0x10000", "--format=json"},
                                  nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr("Tile ID out of range"));
}

TEST(EditorCompletionCommandHandlersTest,
     DungeonGetRoomTilesReturnsCollisionRows) {
  handlers::DungeonGetRoomTilesCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x00", "--format=json"}, nullptr, &output);

  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("status"));
  EXPECT_THAT(output, HasSubstr("success"));
  EXPECT_THAT(output, HasSubstr("collision_rows"));
}

TEST(EditorCompletionCommandHandlersTest,
     DungeonSetRoomPropertyRejectsUnsupportedProperty) {
  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x00", "--property=unknown",
                   "--value=0x01", "--format=json"},
                  nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr("Unsupported property"));
}

TEST(EditorCompletionCommandHandlersTest, DungeonSetRoomPropertyWritesPalette) {
  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x00", "--property=palette",
                   "--value=0x02", "--format=json"},
                  nullptr, &output);

  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("save_status"));
}

}  // namespace
}  // namespace yaze::cli
