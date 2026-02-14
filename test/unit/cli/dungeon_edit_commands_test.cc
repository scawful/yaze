#include "cli/handlers/game/dungeon_edit_commands.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

void ExpectInvalidArgument(const absl::Status& status,
                           const std::string& message_fragment) {
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr(message_fragment));
}

TEST(DungeonEditCommandsTest, PlaceSpriteRejectsInvalidX) {
  handlers::DungeonPlaceSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x77", "--id=0xA3", "--x=nope", "--y=21",
       "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--x'");
}

TEST(DungeonEditCommandsTest, PlaceSpriteRejectsInvalidSubtype) {
  handlers::DungeonPlaceSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x77", "--id=0xA3", "--x=16", "--y=21",
       "--subtype=nope", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--subtype'");
}

TEST(DungeonEditCommandsTest, RemoveSpriteRejectsInvalidIndex) {
  handlers::DungeonRemoveSpriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x77", "--index=nope", "--format=json"},
                  nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--index'");
}

TEST(DungeonEditCommandsTest, PlaceObjectRejectsInvalidSize) {
  handlers::DungeonPlaceObjectCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x98", "--id=0x0031", "--x=1", "--y=2",
       "--size=nope", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--size'");
}

TEST(DungeonEditCommandsTest, PlaceObjectRejectsRoomIdOutOfRange) {
  handlers::DungeonPlaceObjectCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x200", "--id=0x0031", "--x=1", "--y=2",
       "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Room ID out of range");
}

TEST(DungeonEditCommandsTest, SetCollisionTileRejectsRoomIdOutOfRange) {
  handlers::DungeonSetCollisionTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x200", "--tiles=10,5,0xB7", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Room ID out of range");
}

TEST(DungeonEditCommandsTest, SetCollisionTileRejectsMalformedTileTuple) {
  handlers::DungeonSetCollisionTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0xB8", "--tiles=10,5", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Invalid tile spec");
}

}  // namespace
}  // namespace yaze::cli
