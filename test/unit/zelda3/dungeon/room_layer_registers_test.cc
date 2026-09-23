#include "zelda3/dungeon/room_layer_registers.h"

#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {
namespace {

RoomLayerRegisters Derive(uint8_t bgact, bool dark = false, int effect = 0,
                          int tag2 = 0, std::vector<RoomObject> objects = {},
                          uint16_t flags = 0) {
  return DeriveRoomLayerRegisters(bgact, dark, effect, tag2, objects, flags);
}

// Underworld_SubscreenEnable and Module06_UnderworldLoad, per BGACT.
TEST(RoomLayerRegistersTest, BgactSelectsSubscreenAndColorMath) {
  struct Case {
    uint8_t bgact, tm, ts, cgadsub;
  };
  for (const Case& c : {Case{0, 0x16, 0x00, 0x20}, Case{1, 0x16, 0x01, 0x20},
                        Case{2, 0x16, 0x03, 0x20}, Case{3, 0x17, 0x00, 0x20},
                        Case{4, 0x16, 0x01, 0x62}, Case{6, 0x16, 0x01, 0x20},
                        Case{7, 0x16, 0x01, 0x32}}) {
    SCOPED_TRACE(c.bgact);
    const auto r = Derive(c.bgact);
    EXPECT_EQ(r.tm, c.tm);
    EXPECT_EQ(r.ts, c.ts);
    EXPECT_EQ(r.cgadsub, c.cgadsub);
  }
  EXPECT_FALSE(Derive(0).LowerTilemapShown());
  EXPECT_TRUE(Derive(3).TilemapsShareMainScreen());
  EXPECT_TRUE(Derive(4).UpperTilemapBlended());
  EXPECT_FALSE(Derive(1).UpperTilemapBlended());
  EXPECT_TRUE(Derive(4).BlendHalves());
}

TEST(RoomLayerRegistersTest, DarkRoomsSubtractAndEffectsOverride) {
  EXPECT_EQ(Derive(0, /*dark=*/true).cgadsub, 0xB3);
  EXPECT_TRUE(Derive(0, true).BlendSubtracts());
  EXPECT_EQ(Derive(1, false, /*effect=*/6).ts, 0x02);  // invisible floor
  EXPECT_EQ(Derive(2, false, /*effect=*/5).ts, 0x02);  // Agahnim 2
  const auto ganon = Derive(2, true, /*effect=*/7);
  EXPECT_EQ(ganon.ts, 0x00);
  EXPECT_EQ(ganon.cgadsub, 0x70);
}

// Draw routines that STZ $0414 depend on the room's persistent flags.
TEST(RoomLayerRegistersTest, ObjectsClearBgactInTheRightState) {
  const std::vector<RoomObject> overlay_b = {RoomObject(0xDA, 0, 0, 0, 1)};
  const std::vector<RoomObject> overlay_a = {RoomObject(0xD8, 0, 0, 0, 1)};
  const std::vector<RoomObject> stairs = {RoomObject(0x133, 0, 0, 0, 0)};
  EXPECT_EQ(Derive(4, false, 0, 0, overlay_b).ts, 0x00);
  EXPECT_EQ(Derive(4, false, 0, 0, overlay_b, 0x0080).ts, 0x01);
  EXPECT_EQ(Derive(4, false, 0, 0, overlay_a).ts, 0x01);
  EXPECT_EQ(Derive(4, false, 0, 0, overlay_a, 0x0080).ts, 0x00);
  EXPECT_EQ(Derive(4, false, 0, /*tag2=*/0x1B, stairs).ts, 0x00);
  EXPECT_EQ(Derive(4, false, 0, 0x1B, stairs, 0x0100).ts, 0x01);
  EXPECT_EQ(Derive(4, false, 0, /*tag2=*/0x00, stairs).ts, 0x01);
}

}  // namespace
}  // namespace yaze::zelda3
