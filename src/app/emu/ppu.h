#ifndef YAZE_APP_EMU_PPU_H
#define YAZE_APP_EMU_PPU_H

#include <cstdint>
#include <vector>

#include "app/emu/mem.h"

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

namespace PPURegisters {

// OAM Size Register ($2101): Controls the size of the object/sprite, the base
// address, and the name selection for the OAM (Object Attribute Memory).

// OAM Address Register ($2102-$2103): Sets the address for accessing OAM data.

// OAM Data Register ($2104): Holds the data to be written to the OAM at a
// specified address.

// OAM Data Read Register ($2138): Allows reading data from the OAM.

// Screen Display Register ($2100): Controls screen on/off and brightness.

// Screen Mode Register ($2105): Defines the screen mode and character size for
// each background layer.

// Screen Pixelation Register ($2106): Sets the pixel size and screen
// designation for the mosaic display.

// BGx VRAM Location Registers ($2107-$210A): Define the location in VRAM where
// the background screen data is stored.

// BGx & BGy VRAM Location Registers ($210B-$210C): Set the base address for BG
// character data in VRAM.

// BGx Scroll Registers ($210D-$2114): Control the horizontal and vertical
// scroll values for each background layer.

// Video Port Control Register ($2115): Designates the VRAM address increment
// value.

// Video Port Address Register ($2116-$2117): Sets the initial address for
// reading from or writing to VRAM.
constexpr uint16_t INIDISP = 0x2100;
constexpr uint16_t OBJSEL = 0x2101;
constexpr uint16_t OAMADDL = 0x2102;
constexpr uint16_t OAMADDH = 0x2103;
constexpr uint16_t OAMDATA = 0x2104;
constexpr uint16_t BGMODE = 0x2105;
constexpr uint16_t MOSAIC = 0x2106;
constexpr uint16_t BG1SC = 0x2107;
constexpr uint16_t BG2SC = 0x2108;
constexpr uint16_t BG3SC = 0x2109;
constexpr uint16_t BG4SC = 0x210A;
constexpr uint16_t BG12NBA = 0x210B;
constexpr uint16_t BG34NBA = 0x210C;
constexpr uint16_t BG1HOFS = 0x210D;
constexpr uint16_t BG1VOFS = 0x210E;
constexpr uint16_t BG2HOFS = 0x210F;
constexpr uint16_t BG2VOFS = 0x2110;
constexpr uint16_t BG3HOFS = 0x2111;
constexpr uint16_t BG3VOFS = 0x2112;
constexpr uint16_t BG4HOFS = 0x2113;
constexpr uint16_t BG4VOFS = 0x2114;
constexpr uint16_t VMAIN = 0x2115;
constexpr uint16_t VMADDL = 0x2116;
constexpr uint16_t VMADDH = 0x2117;
constexpr uint16_t VMDATAL = 0x2118;
constexpr uint16_t VMDATAH = 0x2119;
constexpr uint16_t M7SEL = 0x211A;
constexpr uint16_t M7A = 0x211B;
constexpr uint16_t M7B = 0x211C;
constexpr uint16_t M7C = 0x211D;
constexpr uint16_t M7D = 0x211E;
constexpr uint16_t M7X = 0x211F;
constexpr uint16_t M7Y = 0x2120;
constexpr uint16_t CGADD = 0x2121;
constexpr uint16_t CGDATA = 0x2122;
constexpr uint16_t W12SEL = 0x2123;
constexpr uint16_t W34SEL = 0x2124;
constexpr uint16_t WOBJSEL = 0x2125;
constexpr uint16_t WH0 = 0x2126;
constexpr uint16_t WH1 = 0x2127;
constexpr uint16_t WH2 = 0x2128;
constexpr uint16_t WH3 = 0x2129;
constexpr uint16_t WBGLOG = 0x212A;
constexpr uint16_t WOBJLOG = 0x212B;
constexpr uint16_t TM = 0x212C;
constexpr uint16_t TS = 0x212D;
constexpr uint16_t TMW = 0x212E;
constexpr uint16_t TSW = 0x212F;
constexpr uint16_t CGWSEL = 0x2130;
constexpr uint16_t CGADSUB = 0x2131;
constexpr uint16_t COLDATA = 0x2132;
constexpr uint16_t SETINI = 0x2133;
constexpr uint16_t MPYL = 0x2134;
constexpr uint16_t MPYM = 0x2135;
constexpr uint16_t MPYH = 0x2136;
constexpr uint16_t SLHV = 0x2137;
constexpr uint16_t OAMDATAREAD = 0x2138;
constexpr uint16_t VMDATALREAD = 0x2139;
constexpr uint16_t VMDATAHREAD = 0x213A;
constexpr uint16_t CGDATAREAD = 0x213B;
constexpr uint16_t OPHCT = 0x213C;
constexpr uint16_t OPVCT = 0x213D;
constexpr uint16_t STAT77 = 0x213E;
constexpr uint16_t STAT78 = 0x213F;

struct INIDISP {
  uint8_t brightness : 4;
  uint8_t forced_blanking : 1;
  uint8_t unused : 3;
};

struct OBJSEL {
  uint8_t name_base_address : 2;
  uint8_t name_secondary_select : 1;
  uint8_t sprite_size : 2;
  uint8_t unused : 3;
};

struct OAMADDL {
  uint8_t address : 8;
};

struct OAMADDH {
  uint8_t high_bit : 1;
  uint8_t priority_rotation : 1;
  uint8_t unused : 6;
};

struct OAMDATA {
  uint8_t data : 8;
};

struct BGMODE {
  uint8_t bg_mode : 3;
  uint8_t bg3_priority : 1;
  uint8_t tile_size : 4;
};

struct MOSAIC {
  uint8_t bg_enable : 4;
  uint8_t mosaic_size : 4;
};

struct BGSC {
  uint8_t horizontal_tilemap_count : 1;
  uint8_t vertical_tilemap_count : 1;
  uint8_t vram_address : 6;
};

struct BGNBA {
  uint8_t chr_base_address_2 : 4;
  uint8_t chr_base_address_1 : 4;
};

struct BGHOFS {
  uint16_t horizontal_scroll : 10;
  uint8_t unused : 6;
};

struct BGVOFS {
  uint16_t vertical_scroll : 10;
  uint8_t unused : 6;
};

struct VMAIN {
  uint8_t increment_size : 2;
  uint8_t remapping : 2;
  uint8_t address_increment_mode : 1;
  uint8_t unused : 3;
};

struct VMADDL {
  uint8_t address_low : 8;
};

struct VMADDH {
  uint8_t address_high : 8;
};

struct VMDATA {
  uint8_t data : 8;
};

struct M7SEL {
  uint8_t flip_horizontal : 1;
  uint8_t flip_vertical : 1;
  uint8_t fill : 1;
  uint8_t tilemap_repeat : 1;
  uint8_t unused : 4;
};

struct M7A {
  int16_t matrix_a : 16;
};

struct M7B {
  int16_t matrix_b : 16;
};

struct M7C {
  int16_t matrix_c : 16;
};

struct M7D {
  int16_t matrix_d : 16;
};

struct M7X {
  uint16_t center_x : 13;
  uint8_t unused : 3;
};

struct M7Y {
  uint16_t center_y : 13;
  uint8_t unused : 3;
};

struct CGADD {
  uint8_t address : 8;
};

struct CGDATA {
  uint16_t data : 15;
  uint8_t unused : 1;
};

struct W12SEL {
  uint8_t enable_bg1_a : 1;
  uint8_t invert_bg1_a : 1;
  uint8_t enable_bg1_b : 1;
  uint8_t invert_bg1_b : 1;
  uint8_t enable_bg2_c : 1;
  uint8_t invert_bg2_c : 1;
  uint8_t enable_bg2_d : 1;
  uint8_t invert_bg2_d : 1;
};

struct W34SEL {
  uint8_t enable_bg3_e : 1;
  uint8_t invert_bg3_e : 1;
  uint8_t enable_bg3_f : 1;
  uint8_t invert_bg3_f : 1;
  uint8_t enable_bg4_g : 1;
  uint8_t invert_bg4_g : 1;
  uint8_t enable_bg4_h : 1;
  uint8_t invert_bg4_h : 1;
};

struct WOBJSEL {
  uint8_t enable_obj_i : 1;
  uint8_t invert_obj_i : 1;
  uint8_t enable_obj_j : 1;
  uint8_t invert_obj_j : 1;
  uint8_t enable_color_k : 1;
  uint8_t invert_color_k : 1;
  uint8_t enable_color_l : 1;
  uint8_t invert_color_l : 1;
};

struct WH0 {
  uint8_t left_position : 8;
};

struct WH1 {
  uint8_t right_position : 8;
};

struct WH2 {
  uint8_t left_position : 8;
};

struct WH3 {
  uint8_t right_position : 8;
};

struct WBGLOG {
  uint8_t mask_logic_bg4 : 2;
  uint8_t mask_logic_bg3 : 2;
  uint8_t mask_logic_bg2 : 2;
  uint8_t mask_logic_bg1 : 2;
};

struct WOBJLOG {
  uint8_t unused : 4;
  uint8_t mask_logic_color : 2;
  uint8_t mask_logic_obj : 2;
};

struct TM {
  uint8_t enable_layer : 5;
  uint8_t unused : 3;
};

struct TS {
  uint8_t enable_layer : 5;
  uint8_t unused : 3;
};

struct TMW {
  uint8_t enable_window : 5;
  uint8_t unused : 3;
};

struct TSW {
  uint8_t enable_window : 5;
  uint8_t unused : 3;
};

struct CGWSEL {
  uint8_t direct_color : 1;
  uint8_t fixed_subscreen : 1;
  uint8_t sub_color_window : 2;
  uint8_t main_color_window : 2;
  uint8_t unused : 2;
};

struct CGADSUB {
  uint8_t enable_layer : 5;
  uint8_t backdrop : 1;
  uint8_t half : 1;
  uint8_t add_subtract : 1;
};

struct COLDATA {
  uint8_t value : 4;
  uint8_t channel_select : 3;
  uint8_t unused : 1;
};

struct SETINI {
  uint8_t screen_interlace : 1;
  uint8_t obj_interlace : 1;
  uint8_t overscan : 1;
  uint8_t hi_res : 1;
  uint8_t extbg : 1;
  uint8_t external_sync : 1;
  uint8_t unused : 2;
};

struct MPYL {
  uint8_t multiplication_result_low : 8;
};

struct MPYM {
  uint8_t multiplication_result_mid : 8;
};

struct MPYH {
  uint8_t multiplication_result_high : 8;
};

struct SLHV {
  uint8_t software_latch : 8;
};

struct OAMDATAREAD {
  uint8_t oam_data_read : 8;
};

struct VMDATALREAD {
  uint8_t vram_data_read_low : 8;
};

struct VMDATAHREAD {
  uint8_t vram_data_read_high : 8;
};

struct CGDATAREAD {
  uint8_t cgram_data_read : 8;
};

struct OPHCT {
  uint16_t horizontal_counter_output : 9;
  uint8_t unused : 7;
};

struct OPVCT {
  uint16_t vertical_counter_output : 9;
  uint8_t unused : 7;
};

struct STAT77 {
  uint8_t ppu1_version : 4;
  uint8_t master_slave : 1;
  uint8_t sprite_tile_overflow : 1;
  uint8_t sprite_overflow : 1;
  uint8_t unused : 1;
};

struct STAT78 {
  uint8_t ppu2_version : 4;
  uint8_t ntsc_pal : 1;
  uint8_t counter_latch_value : 1;
  uint8_t interlace_field : 1;
  uint8_t unused : 1;
};

}  // namespace PPURegisters

// Enum representing different background modes
enum class BackgroundMode {
  Mode0,  // 4 layers, each 2bpp (4 colors)
  Mode1,  // 2 layers, 4bpp (16 colors), 1 layer, 2bpp (4 colors)
  Mode2,  // 2 layers, 4bpp (16 colors), 1 layer for offset-per-tile
  Mode3,  // 1 layer, 8bpp (256 colors), 1 layer, 4bpp (16 colors)
  Mode4,  // 1 layer, 8bpp (256 colors), 1 layer, 2bpp (4 colors), 1 layer for
          // offset-per-tile
  Mode5,  // 1 layer, 4bpp (16 colors), 1 layer, 2bpp (4 colors), high
          // resolution
  Mode6,  // 1 layer, 4bpp (16 colors), 1 layer for offset-per-tile, high
          // resolution
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
  uint8_t startDmaTransfer;          // Register $420B
  uint8_t enableHDmaTransfer;        // Register $420C
  uint8_t dmaControlRegister[8];     // Register $43?0
  uint8_t dmaDestinationAddress[8];  // Register $43?1
  uint32_t dmaSourceAddress[8];      // Register $43?2/$43?3/$43?4
  uint16_t bytesToTransfer[8];       // Register $43?5/$43?6/$43?7
  uint16_t hdmaCountPointer[8];      // Register $43?8/$43?9
  uint8_t scanlinesLeft[8];          // Register $43?A
};

// WRAM access Registers
struct WramAccessRegisters {
  uint8_t dataByte;  // Register $2180
  uint32_t address;  // Register $2181/$2182/$2183
};

class PPU {
 public:
  // Initializes the PPU with the necessary resources and dependencies
  PPU(Memory& memory);

  void Init() {
    // Initialize the frame buffer with a size that corresponds to the
    // screen resolution
    frame_buffer_.resize(256 * 240, 0);
  }

  // Resets the PPU to its initial state
  void Reset() { std::fill(frame_buffer_.begin(), frame_buffer_.end(), 0); }

  // Runs the PPU for a specified number of clock cycles
  void Run(int cycles);

  // Reads a byte from the specified PPU register
  uint8_t ReadRegister(uint16_t address);

  // Writes a byte to the specified PPU register
  void WriteRegister(uint16_t address, uint8_t value);

  // Renders a scanline of the screen
  void RenderScanline();

  // Returns the pixel data for the current frame
  const std::vector<uint8_t>& GetFrameBuffer() const { return frame_buffer_; }

 private:
  // Internal methods to handle PPU rendering and operations
  void UpdateTileData();

  // Updates internal state based on PPU register settings
  void UpdateModeSettings();

  // Renders a background layer
  void RenderBackground(int layer);

  // Renders sprites (also known as objects)
  void RenderSprites();

  void UpdateTileMapData() {
    // Fetches the tile map data from memory and stores it in an internal
    // buffer
  }

  void UpdatePaletteData() {
    // Fetches the palette data from CGRAM and stores it in an internal
    // buffer
  }

  void ApplyEffects() {
    // Applies effects to the layers based on the current mode and register
    // settings
  }

  void ComposeLayers() {
    // Combines the layers into a single image and stores it in the frame
    // buffer
  }

  void DisplayFrameBuffer() {
    // Sends the frame buffer to the display hardware (e.g., SDL2)
  }

  // Retrieves a pixel color from the palette
  uint32_t GetPaletteColor(uint8_t colorIndex);

  // Handles VRAM (Video RAM) reads and writes
  uint8_t ReadVRAM(uint16_t address);
  void WriteVRAM(uint16_t address, uint8_t value);

  // Handles OAM (Object Attribute Memory) reads and writes
  uint8_t ReadOAM(uint16_t address);
  void WriteOAM(uint16_t address, uint8_t value);

  // Handle CGRAM (Color Palette RAM) reads and writes
  uint8_t ReadCGRAM(uint16_t address);
  void WriteCGRAM(uint16_t address, uint8_t value);

  // ===========================================================
  // Member variables to store internal PPU state and resources
  Memory& memory_;
  std::vector<uint8_t> frame_buffer_;

  // The VRAM memory area holds tiles and tile maps.
  std::array<uint8_t, 64 * 1024> vram_;

  // The OAM memory area holds sprite properties.
  std::array<uint8_t, 544> oam_;

  // The CGRAM memory area holds the color palette data.
  std::array<uint8_t, 512> cgram_;

  uint16_t oam_address_;

  BackgroundMode bg_mode_;
  std::vector<SpriteAttributes> sprites_;
  std::vector<uint8_t> tileData_;
  Tilemap tilemap_;
  uint16_t tileDataSize_;
  uint16_t oamBaseAddress_;
  uint16_t vramBaseAddress_;
  uint16_t tilemapBaseAddress_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_PPU_H