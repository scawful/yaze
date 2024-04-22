#ifndef YAZE_APP_EMU_PPU_H
#define YAZE_APP_EMU_PPU_H

#include <array>
#include <cstdint>
#include <vector>

#include "app/emu/cpu/clock.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu_registers.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {
namespace video {

using namespace PpuRegisters;
using namespace memory;

class PpuInterface {
 public:
  virtual ~PpuInterface() = default;

  // Memory Interactions
  virtual void Write(uint16_t address, uint8_t data) = 0;
  virtual uint8_t Read(uint16_t address) const = 0;

  // Rendering Controls
  virtual void RenderFrame() = 0;
  virtual void RenderScanline() = 0;
  virtual void RenderBackground(int layer) = 0;
  virtual void RenderSprites() = 0;

  // State Management
  virtual void Init() = 0;
  virtual void Reset() = 0;
  virtual void Update(double deltaTime) = 0;
  virtual void UpdateClock(double deltaTime) = 0;
  virtual void UpdateInternalState(int cycles) = 0;

  // Data Access
  virtual const std::vector<uint8_t>& GetFrameBuffer() const = 0;
  virtual std::shared_ptr<gfx::Bitmap> GetScreen() const = 0;

  // Mode and Setting Updates
  virtual void UpdateModeSettings() = 0;
  virtual void UpdateTileData() = 0;
  virtual void UpdateTileMapData() = 0;
  virtual void UpdatePaletteData() = 0;

  // Layer Composition
  virtual void ApplyEffects() = 0;
  virtual void ComposeLayers() = 0;

  // Display Output
  virtual void DisplayFrameBuffer() = 0;

  // Notification (Observer pattern)
  virtual void Notify(uint32_t address, uint8_t data) = 0;
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

typedef struct Layer {
  bool mainScreenEnabled;
  bool subScreenEnabled;
  bool mainScreenWindowed;
  bool subScreenWindowed;
} Layer;

typedef struct BgLayer {
  uint16_t hScroll;
  uint16_t vScroll;
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  uint16_t tileAdr;
  bool bigTiles;
  bool mosaicEnabled;
} BgLayer;

typedef struct WindowLayer {
  bool window1enabled;
  bool window2enabled;
  bool window1inversed;
  bool window2inversed;
  uint8_t maskLogic;
} WindowLayer;

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

class Ppu : public SharedRom {
 public:
  // Initializes the PPU with the necessary resources and dependencies
  Ppu(memory::Memory& memory, Clock& clock) : memory_(memory), clock_(clock) {}

  // Initialize the frame buffer
  void Init() {
    clock_.SetFrequency(kPpuClockSpeed);
    frame_buffer_.resize(256 * 240, 0);
    pixelOutputFormat = 1;
  }

  // Resets the PPU to its initial state
  void Reset();

  // Runs the PPU for one frame.
  void Update();
  void UpdateClock(double delta_time) { clock_.UpdateClock(delta_time); }

  void HandleFrameStart();
  void RunLine(int line);
  void HandlePixel(int x, int y);

  int GetPixel(int x, int y, bool sub, int* r, int* g, int* b);

  void EvaluateSprites(int line);

  void CalculateMode7Starts(int y);

  bool GetWindowState(int layer, int x);

  bool frame_overscan_ = false;
  bool overscan_ = false;

  // settings
  bool forcedBlank;
  uint8_t brightness;
  uint8_t mode;
  bool bg3priority;
  bool even_frame;
  bool pseudoHires;
  bool overscan;
  bool
      frameOverscan;  // if we are overscanning this frame (determined at 0,225)
  bool interlace;
  bool frame_interlace;  // if we are interlacing this frame (determined at
                         // start vblank)
  bool directColor;

  bool CheckOverscan() {
    frame_overscan_ = overscan_;
    return frame_overscan_;
  }

  void HandleVblank();
  void HandleOPT(int layer, int* lx, int* ly);
  uint16_t GetOffsetValue(int col, int row);
  int GetPixelForBgLayer(int x, int y, int layer, bool priority);

  uint8_t Read(uint8_t adr);
  void Write(uint8_t adr, uint8_t val);

  uint16_t GetVramRemap();

  void PutPixels(uint8_t* pixel_data);

  // Returns the pixel data for the current frame
  const std::vector<uint8_t>& GetFrameBuffer() const { return frame_buffer_; }

 private:
  bool enable_forced_blanking_ = false;

  int cycle_count_ = 0;
  int current_scanline_ = 0;
  const int cyclesPerScanline = 341;  // SNES PPU has 341 cycles per scanline
  const int totalScanlines = 262;     // SNES PPU has 262 scanlines per frame
  const int visibleScanlines = 224;   // SNES PPU renders 224 visible scanlines

  int GetPixelForMode7(int x, int layer, bool priority);

  // vram access
  uint16_t vram[0x8000];
  uint16_t vramPointer;
  bool vramIncrementOnHigh;
  uint16_t vramIncrement;
  uint8_t vramRemapMode;
  uint16_t vramReadBuffer;

  // cgram access
  uint16_t cgram[0x100];
  uint8_t cgramPointer;
  bool cgramSecondWrite;
  uint8_t cgramBuffer;

  // oam access
  uint16_t oam[0x100];
  uint8_t highOam[0x20];
  uint8_t oamAdr;
  uint8_t oamAdrWritten;
  bool oamInHigh;
  bool oamInHighWritten;
  bool oamSecondWrite;
  uint8_t oamBuffer;

  // Objects / Sprites
  bool objPriority;
  uint16_t objTileAdr1;
  uint16_t objTileAdr2;
  uint8_t objSize;
  std::array<uint8_t, 256> obj_pixel_buffer_;
  uint8_t objPriorityBuffer[256];
  bool time_over_ = false;
  bool range_over_ = false;
  bool objInterlace;

  // Color Math
  uint8_t clip_mode_ = 0;
  uint8_t prevent_math_mode_ = 0;
  bool math_enabled_array_[6] = {false, false, false, false, false, false};
  bool add_subscreen_ = false;
  bool subtractColor;
  bool halfColor;
  uint8_t fixedColorR;
  uint8_t fixedColorG;
  uint8_t fixedColorB;

  // layers
  Layer layer_[5];

  // mode 7
  int16_t m7matrix[8];  // a, b, c, d, x, y, h, v
  uint8_t m7prev;
  bool m7largeField;
  bool m7charFill;
  bool m7xFlip;
  bool m7yFlip;
  bool m7extBg;

  // mode 7 internal
  int32_t m7startX;
  int32_t m7startY;

  // windows
  WindowLayer windowLayer[6];
  uint8_t window1left;
  uint8_t window1right;
  uint8_t window2left;
  uint8_t window2right;

  // Background Layers
  std::array<BackgroundLayer, 4> bg_layers_;
  uint8_t mosaic_startline_ = 1;

  BgLayer bgLayer[4];
  uint8_t scrollPrev;
  uint8_t scrollPrev2;
  uint8_t mosaicSize;
  uint8_t mosaicStartLine;

  // pixel buffer (xbgr)
  // times 2 for even and odd frame
  uint8_t pixelBuffer[512 * 4 * 239 * 2];
  uint8_t pixelOutputFormat = 0;

  // latching
  uint16_t hCount;
  uint16_t vCount;
  bool hCountSecond;
  bool vCountSecond;
  bool countersLatched;
  uint8_t ppu1openBus;
  uint8_t ppu2openBus;

  uint16_t tile_data_size_;
  uint16_t vram_base_address_;
  uint16_t tilemap_base_address_;
  uint16_t screen_brightness_ = 0x00;

  Memory& memory_;
  Clock& clock_;

  Tilemap tilemap_;
  BackgroundMode bg_mode_;
  std::vector<SpriteAttributes> sprites_;
  std::vector<uint8_t> tile_data_;
  std::vector<uint8_t> frame_buffer_;

  // PPU registers
  OAMSize oam_size_;
  OAMAddress oam_address_;
  Mosaic mosaic_;
  std::array<BGSC, 4> bgsc_;
  std::array<BGNBA, 4> bgnba_;
  std::array<BGHOFS, 4> bghofs_;
  std::array<BGVOFS, 4> bgvofs_;
};

}  // namespace video
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_PPU_H