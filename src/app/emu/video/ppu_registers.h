#ifndef YAZE_APP_EMU_VIDEO_PPU_REGISTERS_H
#define YAZE_APP_EMU_VIDEO_PPU_REGISTERS_H

#include <array>
#include <cstdint>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

namespace PPURegisters {

constexpr uint16_t INIDISP = 0x2100;

// OAM Size Register ($2101): Controls the size of the object/sprite, the base
// address, and the name selection for the OAM (Object Attribute Memory).
constexpr uint16_t OBJSEL = 0x2101;

// OAM Address Register ($2102-$2103): Sets the address for accessing OAM data.
constexpr uint16_t OAMADDL = 0x2102;
constexpr uint16_t OAMADDH = 0x2103;

// OAM Data Register ($2104): Holds the data to be written to the OAM at a
// specified address.
constexpr uint16_t OAMDATA = 0x2104;

// OAM Data Read Register ($2138): Allows reading data from the OAM.

// Screen Display Register ($2100): Controls screen on/off and brightness.

// Screen Mode Register ($2105): Defines the screen mode and character size for
// each background layer.
constexpr uint16_t BGMODE = 0x2105;

// Screen Pixelation Register ($2106): Sets the pixel size and screen
// designation for the mosaic display.
constexpr uint16_t MOSAIC = 0x2106;

// BGx VRAM Location Registers ($2107-$210A)
// Define the location in VRAM where the background screen data is stored.
constexpr uint16_t BG1SC = 0x2107;
constexpr uint16_t BG2SC = 0x2108;
constexpr uint16_t BG3SC = 0x2109;
constexpr uint16_t BG4SC = 0x210A;

// BGx & BGy VRAM Location Registers ($210B-$210C):
// Set the base address for BG character data in VRAM.
constexpr uint16_t BG12NBA = 0x210B;
constexpr uint16_t BG34NBA = 0x210C;

// BGx Scroll Registers ($210D-$2114): Control the horizontal and vertical
// scroll values for each background layer.
constexpr uint16_t BG1HOFS = 0x210D;
constexpr uint16_t BG1VOFS = 0x210E;
constexpr uint16_t BG2HOFS = 0x210F;
constexpr uint16_t BG2VOFS = 0x2110;
constexpr uint16_t BG3HOFS = 0x2111;
constexpr uint16_t BG3VOFS = 0x2112;
constexpr uint16_t BG4HOFS = 0x2113;
constexpr uint16_t BG4VOFS = 0x2114;

// Video Port Control Register ($2115): Designates the VRAM address increment
// value.
constexpr uint16_t VMAIN = 0x2115;

// Video Port Address Register ($2116-$2117): Sets the initial address for
// reading from or writing to VRAM.
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
  explicit BGSC(uint8_t value)
      : horizontal_tilemap_count(value & 0x01),
        vertical_tilemap_count((value >> 1) & 0x01),
        vram_address((value >> 2) & 0x3F) {}
  uint8_t horizontal_tilemap_count : 1;
  uint8_t vertical_tilemap_count : 1;
  uint8_t vram_address : 6;
};

struct BGNBA {
  explicit BGNBA(uint8_t value)
      : chr_base_address_2(value & 0x0F),
        chr_base_address_1((value >> 4) & 0x0F) {}
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

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_VIDEO_PPU_REGISTERS_H