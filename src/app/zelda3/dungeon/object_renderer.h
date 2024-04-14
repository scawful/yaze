#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

struct PseudoVram {
  std::array<uint8_t, 16> sheets;
  std::vector<gfx::SnesPalette> palettes;
};

struct SubtypeInfo {
  uint32_t subtype_ptr;
  uint32_t routine_ptr;
};

class DungeonObjectRenderer : public SharedROM {
 public:
  DungeonObjectRenderer() = default;

  void LoadObject(uint16_t objectId, std::array<uint8_t, 16>& sheet_ids);
  gfx::Bitmap* bitmap() { return &bitmap_; }
  auto memory() { return memory_; }
  auto mutable_memory() { return &memory_; }

 private:
  SubtypeInfo FetchSubtypeInfo(uint16_t object_id);
  void ConfigureObject(const SubtypeInfo& info);
  void RenderObject(const SubtypeInfo& info);
  void UpdateObjectBitmap();

  std::vector<uint8_t> tilemap_;
  uint16_t pc_with_rts_;
  std::vector<uint8_t> rom_data_;
  emu::MemoryImpl memory_;
  emu::ClockImpl clock_;
  emu::Cpu cpu{memory_, clock_};
  emu::Ppu ppu{memory_, clock_};
  gfx::Bitmap bitmap_;
  PseudoVram vram_;
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze