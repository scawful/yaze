#ifndef YAZE_APP_EMU_PPU_H
#define YAZE_APP_EMU_PPU_H

#include <array>
#include <cstdint>
#include <vector>

#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu_registers.h"
#include "app/rom.h"

namespace yaze {
namespace emu {

class PpuInterface {
 public:
  virtual ~PpuInterface() = default;

  // Memory Interactions
  virtual void Write(uint16_t address, uint8_t data) = 0;
  virtual uint8_t Read(uint16_t address) const = 0;
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

class Ppu {
 public:
  // Initializes the PPU with the necessary resources and dependencies
  Ppu(Memory& memory) : memory_(memory) {}

  // Initialize the frame buffer
  void Init() {
    frame_buffer_.resize(256 * 240, 0);
    // Set to BGRX format (0) for compatibility with SDL_PIXELFORMAT_ARGB8888
    // Format 0 = BGRX: [B][G][R][A] byte order in memory (little-endian)
    pixelOutputFormat = 0;
  }

  void Reset();
  void HandleFrameStart();
  void RunLine(int line);
  void HandlePixel(int x, int y);

  void LatchHV() {
    h_count_ = memory_.h_pos() / 4;
    v_count_ = memory_.v_pos();
    counters_latched_ = true;
  }

  int GetPixel(int x, int y, bool sub, int* r, int* g, int* b);

  void EvaluateSprites(int line);

  void CalculateMode7Starts(int y);

  bool GetWindowState(int layer, int x);

  // if we are overscanning this frame (determined at  0,225)
  bool frame_overscan_ = false;
  bool overscan_ = false;

  // settings
  bool forced_blank_;
  uint8_t brightness;
  uint8_t mode;
  bool bg3priority;
  bool even_frame;
  bool pseudo_hires_;
  bool interlace;
  bool frame_interlace;  // if we are interlacing this frame (determined at
                         // start vblank)
  bool direct_color_;

  bool CheckOverscan() {
    frame_overscan_ = overscan_;
    return frame_overscan_;
  }

  void HandleVblank();
  void HandleOPT(int layer, int* lx, int* ly);
  uint16_t GetOffsetValue(int col, int row);
  int GetPixelForBgLayer(int x, int y, int layer, bool priority);

  uint8_t Read(uint8_t adr, bool latch);
  void Write(uint8_t adr, uint8_t val);

  uint16_t GetVramRemap();

  void PutPixels(uint8_t* pixel_data);

  // Returns the pixel data for the current frame
  const std::vector<uint8_t>& GetFrameBuffer() const { return frame_buffer_; }
  
  // Set pixel output format (0 = BGRX, 1 = XBGR)
  void SetPixelFormat(uint8_t format) { pixelOutputFormat = format; }

 private:
  int GetPixelForMode7(int x, int layer, bool priority);

  const int cyclesPerScanline = 341;  // SNES PPU has 341 cycles per scanline
  const int totalScanlines = 262;     // SNES PPU has 262 scanlines per frame
  const int visibleScanlines = 224;   // SNES PPU renders 224 visible scanlines

  bool enable_forced_blanking_ = false;

  int cycle_count_ = 0;
  int current_scanline_ = 0;

  // vram access
  uint16_t vram[0x8000];
  uint16_t vram_pointer;
  bool vram_increment_on_high_;
  uint16_t vram_increment_;
  uint8_t vram_remap_mode_;
  uint16_t vram_read_buffer_;

  // cgram access
  uint16_t cgram[0x100];
  uint8_t cgram_pointer_;
  bool cgram_second_write_;
  uint8_t cgram_buffer_;

  // oam access
  uint16_t oam[0x100];
  uint8_t high_oam_[0x20];
  uint8_t oam_adr_;
  uint8_t oam_adr_written_;
  bool oam_in_high_;
  bool oam_in_high_written_;
  bool oam_second_write_;
  uint8_t oam_buffer_;

  // Objects / Sprites
  bool time_over_ = false;
  bool range_over_ = false;
  bool obj_interlace_;
  bool obj_priority_;
  uint16_t obj_tile_adr1_;
  uint16_t obj_tile_adr2_;
  uint8_t obj_size_;
  std::array<uint8_t, 256> obj_pixel_buffer_;
  std::array<uint8_t, 256> obj_priority_buffer_;

  // Color Math
  uint8_t clip_mode_ = 0;
  uint8_t prevent_math_mode_ = 0;
  bool math_enabled_array_[6] = {false, false, false, false, false, false};
  bool add_subscreen_ = false;
  bool subtract_color_;
  bool half_color_;
  uint8_t fixed_color_r_;
  uint8_t fixed_color_g_;
  uint8_t fixed_color_b_;

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

  BgLayer bg_layer_[4];
  uint8_t scroll_prev_;
  uint8_t scroll_prev2_;
  uint8_t mosaic_size_;

  // pixel buffer (xbgr)
  // times 2 for even and odd frame
  uint8_t pixelBuffer[512 * 4 * 239 * 2];
  uint8_t pixelOutputFormat = 0;

  // latching
  uint16_t h_count_;
  uint16_t v_count_;
  bool h_count_second_;
  bool v_count_second_;
  bool counters_latched_;
  uint8_t ppu1_open_bus_;
  uint8_t ppu2_open_bus_;

  uint16_t tile_data_size_;
  uint16_t vram_base_address_;
  uint16_t tilemap_base_address_;
  uint16_t screen_brightness_ = 0x00;

  Memory& memory_;

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

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_PPU_H
