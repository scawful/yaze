#ifndef YAZE_APP_GFX_scad_format_H
#define YAZE_APP_GFX_scad_format_H

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

// Address       Description
// 00000 - 00003 File type "SCH"
// 00004 - 00008 Bit mode "?BIT"
// 00009 - 00013 Version number "Ver-????\n"
// 00014 - 00017 Header size
// 00018 - 0001B Hardware name "SFC" or "CGB" or "GB"
// 0001C - 0001C BG/OBJ flag (for AGB)
// 0001D - 0001D Color Pallette Number
// 0001D - 000FF Reserved
// 00100 - 001FF Color Path
struct CgxHeader {
  char file_type[4];
  char bit_mode[5];
  char version_number[9];
  uint32_t header_size;
  char hardware_name[4];
  uint8_t bg_obj_flag;
  uint8_t color_palette_number;
  uint8_t reserved[0xE3];
  uint8_t color_path[0x100];
};

CgxHeader ExtractCgxHeader(std::vector<uint8_t>& cgx_header);

absl::Status DecodeCgxFile(std::string_view filename,
                           std::vector<uint8_t>& cgx_data,
                           std::vector<uint8_t>& extra_cgx_data,
                           std::vector<uint8_t>& decoded_cgx);

std::vector<SDL_Color> DecodeColFile(const std::string& filename);

absl::Status DecodeObjFile(
    std::string_view filename, std::vector<uint8_t>& obj_data,
    std::vector<uint8_t> actual_obj_data,
    std::unordered_map<std::string, std::vector<uint8_t>> decoded_obj,
    std::vector<uint8_t>& decoded_extra_obj, int& obj_loaded);

SDL_Surface* CreateCgxPreviewImage(int default_cgram,
                                   const std::vector<uint8_t>& cgx_data,
                                   const std::vector<uint8_t>& extra_cgx_data,
                                   std::vector<SDL_Color> decoded_col);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_scad_format_H