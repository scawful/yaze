#ifndef YAZE_APP_GFX_scad_format_H
#define YAZE_APP_GFX_scad_format_H

#include "app/platform/sdl_compat.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace gfx {

/**
 * @brief Cgx file header
 *
 * @details
 * キャラクタ（．ＳＣＨ）ファイル
 * ヘッダー情報
 * アドレス 	       説明
 * 00000 - 00003 	ﾌｧｲﾙﾀｲﾌﾟ  "SCH"
 * 00004 - 00008 	ﾋﾞｯﾄﾓｰﾄﾞ  "?BIT"
 * 00009 - 00013 	ﾊﾞｰｼﾞｮﾝﾅﾝﾊﾞｰ "Ver-????\n"
 * 00014 - 00017 	ﾍｯﾀﾞｰｻｲｽﾞ
 * 00018 - 0001B 	ﾊｰﾄﾞ名  "SFC" or "CGB" or "GB"
 * 0001C - 0001C 	BG/OBJﾌﾗｸﾞ(AGBの時)
 * 0001D - 0001D 	Color Pallette Number
 * 0001D - 000FF 	予約
 * 00100 - 001FF 	Color Path
 */
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

constexpr uint16_t kMatchedBytes[] = {0x4E, 0x41, 0x4B, 0x31, 0x39, 0x38, 0x39};
constexpr uint16_t kOffsetFromMatchedBytesEnd = 0x1D;

/**
 * @brief Find metastamp in CGX file
 */
void FindMetastamp();

/**
 * @brief Load Scr file (screen data)
 */
absl::Status LoadScr(std::string_view filename, uint8_t input_value,
                     std::vector<uint8_t>& map_data);

/**
 * @brief Load Cgx file (graphical content)
 */
absl::Status LoadCgx(uint8_t bpp, std::string_view filename,
                     std::vector<uint8_t>& cgx_data,
                     std::vector<uint8_t>& cgx_loaded,
                     std::vector<uint8_t>& cgx_header);

/**
 * @brief Draw screen tilemap with graphical data
 */
absl::Status DrawScrWithCgx(uint8_t bpp, std::vector<uint8_t>& map_bitmap_data,
                            std::vector<uint8_t>& map_data,
                            std::vector<uint8_t>& cgx_loaded);

/**
 * @brief Save Cgx file (graphical content)
 */
absl::Status SaveCgx(uint8_t bpp, std::string_view filename,
                     const std::vector<uint8_t>& cgx_data,
                     const std::vector<uint8_t>& cgx_header);

/**
 * @brief Decode color file
 */
std::vector<SDL_Color> DecodeColFile(const std::string_view filename);

/**
 * @brief Decode obj file
 */
absl::Status DecodeObjFile(
    std::string_view filename, std::vector<uint8_t>& obj_data,
    std::vector<uint8_t> actual_obj_data,
    std::unordered_map<std::string, std::vector<uint8_t>> decoded_obj,
    std::vector<uint8_t>& decoded_extra_obj, int& obj_loaded);

/**
 * @brief Save Col file (palette data)
 */
absl::Status SaveCol(std::string_view filename,
                     const std::vector<SDL_Color>& palette);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_scad_format_H
