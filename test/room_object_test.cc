#include "app/zelda3/dungeon/room_object.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/memory/mock_memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace test {

TEST(DungeonObjectTest, RenderObjectsAsBitmaps) {
  app::ROM rom;
  //     rom.LoadFromFile("/Users/scawful/Code/yaze/build/bin/zelda3.sfc"));
  // EXPECT_EQ(rom_status, absl::Status::ok());

  app::zelda3::dungeon::DungeonObjectRenderer renderer;
}

}  // namespace test
}  // namespace yaze