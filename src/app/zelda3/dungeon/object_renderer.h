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

/**
 * @struct PseudoVram
 * @brief Simulates the SNES VRAM for object rendering
 * 
 * This structure holds the sheet IDs and palettes needed for rendering
 * dungeon objects in Link to the Past.
 */
struct PseudoVram {
	std::array<uint8_t, 16> sheets = { 0 };
  std::vector<gfx::SnesPalette> palettes;
};

/**
 * @class DungeonObjectRenderer
 * @brief Renders dungeon objects from Link to the Past
 * 
 * This class uses the emulator subsystem to simulate the SNES CPU
 * drawing routines for dungeon objects. It captures the tile data
 * written to memory and renders it to a bitmap.
 */
class DungeonObjectRenderer : public SharedRom {
 public:
  DungeonObjectRenderer() = default;

  /**
   * @brief Loads and renders a dungeon object
   * 
   * @param routine_ptr Pointer to the drawing routine in ROM
   * @param sheet_ids Array of graphics sheet IDs used by the object
   */
  void LoadObject(uint32_t routine_ptr, std::array<uint8_t, 16>& sheet_ids);
  
  /**
   * @brief Configures the CPU state for object rendering
   */
  void ConfigureObject();
  
  /**
   * @brief Executes the object drawing routine
   * 
   * @param routine_ptr Pointer to the drawing routine in ROM
   */
  void RenderObject(uint32_t routine_ptr);
  
  /**
   * @brief Updates the bitmap with the rendered object
   */
  void UpdateObjectBitmap();
  
  /**
   * @brief Sets the palette for the rendered object
   * 
   * @param palette The palette to use for the object
   * @param transparent_index Index of the transparent color (default: 0)
   * @return absl::Status Success or error status
   */
  absl::Status SetPalette(const gfx::SnesPalette& palette, size_t transparent_index = 0);

  /**
   * @brief Gets the rendered bitmap
   * 
   * @return gfx::Bitmap* Pointer to the bitmap
   */
  gfx::Bitmap* bitmap() { return &bitmap_; }
  
  /**
   * @brief Gets the memory implementation
   * 
   * @return Memory implementation
   */
  auto memory() { return memory_; }
  
  /**
   * @brief Gets a mutable pointer to the memory implementation
   * 
   * @return Mutable pointer to the memory implementation
   */
  auto mutable_memory() { return &memory_; }

 private:
  std::vector<uint8_t> tilemap_;
  std::vector<uint8_t> rom_data_;

  PseudoVram vram_;

  emu::MemoryImpl memory_;
  emu::CpuCallbacks cpu_callbacks_;
  emu::Ppu ppu{memory_};
  emu::Cpu cpu{memory_, cpu_callbacks_};

  gfx::Bitmap bitmap_;
};

}  // namespace zelda3
}  // namespace yaze
