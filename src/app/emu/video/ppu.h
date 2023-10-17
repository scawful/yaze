#ifndef YAZE_APP_EMU_PPU_H
#define YAZE_APP_EMU_PPU_H

#include <array>
#include <cstdint>
#include <vector>

#include "app/emu/clock.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu_registers.h"

namespace yaze {
namespace app {
namespace emu {

class IPPU {
 public:
  virtual ~IPPU() = default;

  virtual void writeRegister(uint16_t address, uint8_t data) = 0;
  virtual uint8_t readRegister(uint16_t address) const = 0;
  virtual void setOAMData(const std::vector<uint8_t>& data) = 0;
  virtual std::vector<uint8_t> getOAMData() const = 0;
  virtual void setVRAMData(const std::vector<uint8_t>& data) = 0;
  virtual std::vector<uint8_t> getVRAMData() const = 0;
  virtual void setCGRAMData(const std::vector<uint8_t>& data) = 0;
  virtual std::vector<uint8_t> getCGRAMData() const = 0;
  virtual void renderFrame() = 0;
  virtual std::vector<uint32_t> getFrameBuffer() const = 0;
};

// Enum representing different background modes
enum class BackgroundMode {
  Mode0,  // 4 layers, each 2bpp (4 colors)
  Mode1,  // 2 layers, 4bpp (16 colors), 1 layer, 2bpp (4 colors)
  Mode2,  // 2 layers, 4bpp (16 colors), 1 layer for offset-per-tile
  Mode3,  // 1 layer, 8bpp (256 colors), 1 layer, 4bpp (16 colors)
  Mode4,  // 1 layer, 8bpp (256 colors), 1 layer, 2bpp (4 colors)
          // 1 layer for offset-per-tile
  Mode5,  // 1 layer, 4bpp (16 colors), 1 layer, 2bpp (4 colors) hi-res
  Mode6,  // 1 layer, 4bpp (16 colors), 1 layer for offset-per-tile, hi-res
  Mode7,  // 1 layer, 8bpp (256 colors), rotation/scaling
};

// Enum representing sprite sizes
enum class SpriteSize { Size8x8, Size16x16, Size32x32, Size64x64 };

// Struct representing a sprite's attributes
struct SpriteAttributes {
  uint8_t x;         // X position of the sprite
  uint8_t y;         // Y position of the sprite
  uint16_t tile;     // Tile number for the sprite
  uint8_t palette;   // Palette number for the sprite
  uint8_t priority;  // Priority for the sprite
  bool hFlip;        // Horizontal flip flag
  bool vFlip;        // Vertical flip flag
};

// Struct representing a tilemap entry
struct TilemapEntry {
  uint16_t tileNumber;  // Tile number for the tile
  uint8_t palette;      // Palette number for the tile
  uint8_t priority;     // Priority for the tile
  bool hFlip;           // Horizontal flip flag
  bool vFlip;           // Vertical flip flag
};

// Struct representing a tilemap
struct Tilemap {
  std::vector<TilemapEntry> entries;  // Entries for the tilemap
};

// Struct representing a color
struct Color {
  uint8_t r;  // Red component
  uint8_t g;  // Green component
  uint8_t b;  // Blue component
};

// Registers
struct OAMSize {
  uint8_t base_selection : 3;
  uint8_t name_selection : 2;
  uint8_t object_size : 3;
};

struct OAMAddress {
  uint8_t oam_address_low : 8;
  uint8_t oam_address_msb : 1;
  uint8_t oam_priority_rotation : 1;
  uint8_t unused : 6;
};

struct TileMapLocation {
  uint8_t SC_size : 2;
  uint8_t tile_map_address : 5;
  uint8_t unused : 1;
};

struct CharacterLocation {
  uint8_t BG1_address : 4;
  uint8_t BG2_address : 4;
  uint8_t BG3_address : 4;
  uint8_t BG4_address : 4;
};

struct VideoPortControl {
  uint8_t increment_rate : 2;
  uint8_t full_graphic : 2;
  uint8_t increment_mode : 1;
  uint8_t unused : 3;
};

struct ScreenDisplay {
  uint8_t brightness : 4;
  uint8_t disable_screen : 1;
  uint8_t unused : 3;
};

struct ScreenMode {
  uint8_t general_screen_mode : 3;
  uint8_t priority : 1;
  uint8_t BG1_tile_size : 1;
  uint8_t BG2_tile_size : 1;
  uint8_t BG3_tile_size : 1;
  uint8_t BG4_tile_size : 1;
};

struct ScrollRegister {
  uint8_t offset : 8;
  uint8_t mode7_bits : 3;
  uint8_t unused : 5;
};

struct MainSubScreenDesignation {
  uint8_t BG1_enable : 1;
  uint8_t BG2_enable : 1;
  uint8_t BG3_enable : 1;
  uint8_t BG4_enable : 1;
  uint8_t sprites_enable : 1;
  uint8_t unused : 3;
};

struct WindowMaskSettings {
  uint8_t BG1_clip_in_out : 1;
  uint8_t BG1_enable : 1;
  uint8_t BG2_clip_in_out : 1;
  uint8_t BG2_enable : 1;
  uint8_t BG3_clip_in_out : 1;
  uint8_t BG3_enable : 1;
  uint8_t BG4_clip_in_out : 1;
  uint8_t BG4_enable : 1;
};

struct WindowMaskSettings2 {
  uint8_t sprites_clip_in_out : 1;
  uint8_t sprites_enable : 1;
  uint8_t color_windows_clip_in_out : 1;
  uint8_t color_windows_enable : 1;
  uint8_t unused : 4;
};

struct WindowPosition {
  uint8_t position : 8;
};

struct MaskLogicSettings {
  uint8_t BG1_mask_logic : 2;
  uint8_t BG2_mask_logic : 2;
  uint8_t BG3_mask_logic : 2;
  uint8_t BG4_mask_logic : 2;
};

// Counter/IRQ/NMI Registers
struct CounterIrqNmiRegisters {
  uint8_t softwareLatchHvCounter;   // Register $2137
  uint16_t horizontalScanLocation;  // Register $213C
  uint16_t verticalScanLocation;    // Register $213D
  uint8_t counterEnable;            // Register $4200
  uint16_t horizontalIrqTrigger;    // Register $4207/$4208
  uint16_t verticalIrqTrigger;      // Register $4209/$420A
  uint8_t nmiRegister;              // Register $4210
  uint8_t irqRegister;              // Register $4211
  uint8_t statusRegisterIrq;        // Register $4212
};

// Joypad Registers
struct JoypadRegisters {
  uint16_t joypadData[4];              // Register $4218 to $421F
  uint8_t oldStyleJoypadRegisters[2];  // Registers $4016/$4217
};

// DMA Registers
struct DmaRegisters {
  uint8_t startDmaTransfer;              // Register $420B
  uint8_t enableHDmaTransfer;            // Register $420C
  uint8_t dmacontrol_register_ister[8];  // Register $43?0
  uint8_t dmaDestinationAddress[8];      // Register $43?1
  uint32_t dmaSourceAddress[8];          // Register $43?2/$43?3/$43?4
  uint16_t bytesToTransfer[8];           // Register $43?5/$43?6/$43?7
  uint16_t hdmaCountPointer[8];          // Register $43?8/$43?9
  uint8_t scanlinesLeft[8];              // Register $43?A
};

// WRAM access Registers
struct WramAccessRegisters {
  uint8_t dataByte;  // Register $2180
  uint32_t address;  // Register $2181/$2182/$2183
};

struct Tile {
  uint16_t index;    // Index of the tile in VRAM
  uint8_t palette;   // Palette number used for this tile
  bool flip_x;       // Horizontal flip flag
  bool flip_y;       // Vertical flip flag
  uint8_t priority;  // Priority of this tile
};

struct BackgroundLayer {
  enum class Size { SIZE_32x32, SIZE_64x32, SIZE_32x64, SIZE_64x64 };

  enum class ColorDepth { BPP_2, BPP_4, BPP_8 };

  Size size;                        // Size of the background layer
  ColorDepth color_depth;           // Color depth of the background layer
  std::vector<Tile> tilemap;        // Tilemap data
  std::vector<uint8_t> tile_data;   // Tile data in VRAM
  uint16_t tilemap_base_address;    // Base address of the tilemap in VRAM
  uint16_t tile_data_base_address;  // Base address of the tile data in VRAM
  uint8_t scroll_x;                 // Horizontal scroll offset
  uint8_t scroll_y;                 // Vertical scroll offset
  bool enabled;                     // Whether the background layer is enabled
};

const int kPpuClockSpeed = 5369318;  // 5.369318 MHz

class PPU : public Observer {
 public:
  // Initializes the PPU with the necessary resources and dependencies
  PPU(Memory& memory, Clock& clock) : memory_(memory), clock_(clock) {}

  // Initialize the frame buffer
  void Init() {
    clock_.SetFrequency(kPpuClockSpeed);
    frame_buffer_.resize(256 * 240, 0);
  }

  // Resets the PPU to its initial state
  void Reset() { std::fill(frame_buffer_.begin(), frame_buffer_.end(), 0); }

  // Runs the PPU for one frame.
  void Update();
  void UpdateClock(double delta_time) { clock_.UpdateClock(delta_time); }
  void UpdateInternalState(int cycles);

  // Renders a scanline of the screen
  void RenderScanline();

  void Notify(uint32_t address, uint8_t data) override;

  // Returns the pixel data for the current frame
  const std::vector<uint8_t>& GetFrameBuffer() const { return frame_buffer_; }

 private:
  // Updates internal state based on PPU register settings
  void UpdateModeSettings();

  // Internal methods to handle PPU rendering and operations
  void UpdateTileData();

  // Fetches the tile map data from memory and stores it in an internal buffer
  void UpdateTileMapData();

  // Renders a background layer
  void RenderBackground(int layer);

  // Renders sprites (also known as objects)
  void RenderSprites();

  // Fetches the palette data from CGRAM and stores it in an internal buffer
  void UpdatePaletteData();

  // Applies effects to the layers based on the current mode and register
  void ApplyEffects();

  // Combines the layers into a single image and stores it in the frame buffer
  void ComposeLayers();

  // Sends the frame buffer to the display hardware (e.g., SDL2)
  void DisplayFrameBuffer();

  // ===========================================================
  // Member variables to store internal PPU state and resources
  Memory& memory_;
  Clock& clock_;

  Tilemap tilemap_;
  BackgroundMode bg_mode_;
  std::array<BackgroundLayer, 4> bg_layers_;
  std::vector<SpriteAttributes> sprites_;
  std::vector<uint8_t> tile_data_;
  std::vector<uint8_t> frame_buffer_;

  uint16_t oam_address_;
  uint16_t tile_data_size_;
  uint16_t vram_base_address_;
  uint16_t tilemap_base_address_;

  uint16_t screen_brightness_ = 0x00;

  bool enable_forced_blanking_ = false;

  int cycle_count_ = 0;
  int current_scanline_ = 0;
  const int cyclesPerScanline = 341;  // SNES PPU has 341 cycles per scanline
  const int totalScanlines = 262;     // SNES PPU has 262 scanlines per frame
  const int visibleScanlines = 224;   // SNES PPU renders 224 visible scanlines
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_PPU_H