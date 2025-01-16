#include <cstdint>
#include <vector>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

struct PseudoVram {
	std::array<uint8_t, 16> sheets = { 0 };
  std::vector<gfx::SnesPalette> palettes;
};

class DungeonObjectRenderer : public SharedRom {
 public:
  DungeonObjectRenderer() = default;

  void LoadObject(uint32_t routine_ptr, std::array<uint8_t, 16>& sheet_ids);
  void ConfigureObject();
  void RenderObject(uint32_t routine_ptr);
  void UpdateObjectBitmap();

  gfx::Bitmap* bitmap() { return &bitmap_; }
  auto memory() { return memory_; }
  auto mutable_memory() { return &memory_; }

 private:
  std::vector<uint8_t> tilemap_;
  std::vector<uint8_t> rom_data_;

  PseudoVram vram_;

  emu::ClockImpl clock_;
  emu::MemoryImpl memory_;
  emu::CpuCallbacks cpu_callbacks_;
  emu::Ppu ppu{memory_, clock_};
  emu::Cpu cpu{memory_, clock_, cpu_callbacks_};

  gfx::Bitmap bitmap_;
};

}  // namespace zelda3
}  // namespace yaze
