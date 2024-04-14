#include "app/zelda3/dungeon/room_object.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/memory/mock_memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze_test {
namespace zelda3_test {

using yaze::app::ROM;
using yaze::app::zelda3::dungeon::DungeonObjectRenderer;

TEST(DungeonObjectTest, RenderObjectsAsBitmaps) {
  ROM rom;
  //     rom.LoadFromFile("/Users/scawful/Code/yaze/build/bin/zelda3.sfc"));
  // EXPECT_EQ(rom_status, absl::Status::ok());

  DungeonObjectRenderer renderer;
}

}  // namespace zelda3_test
}  // namespace yaze_test