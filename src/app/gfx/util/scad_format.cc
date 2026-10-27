#include "scad_format.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace gfx {

namespace {

// A CGX file ends with a 0x500-byte trailer whose last 0x400 bytes LoadCgx
// returns as the header; a COL file ends with a 0x200-byte trailer.
constexpr size_t kCgxTrailerSize = 0x500;
constexpr size_t kColTrailerSize = 0x200;

std::vector<uint8_t> ReadAll(std::ifstream& file) {
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

}  // namespace

absl::Status LoadCgx(uint8_t bpp, std::string_view filename,
                     std::vector<uint8_t>& cgx_data,
                     std::vector<uint8_t>& cgx_loaded,
                     std::vector<uint8_t>& cgx_header) {
  std::ifstream file(std::string(filename), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("CGX file not found.");
  }
  std::vector<uint8_t> file_content = ReadAll(file);
  if (file_content.size() < kCgxTrailerSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("CGX file is %d bytes; expected at least %d.",
                        file_content.size(), kCgxTrailerSize));
  }
  const size_t data_size = file_content.size() - kCgxTrailerSize;
  cgx_data.assign(file_content.begin(), file_content.begin() + data_size);
  cgx_header.assign(file_content.begin() + data_size + 0x100,
                    file_content.end());

  cgx_loaded = gfx::Bpp8SnesToIndexed(cgx_data, bpp > 8 ? 40 : bpp);
  return absl::OkStatus();
}

absl::Status LoadScr(std::string_view filename, uint8_t input_value,
                     std::vector<uint8_t>& map_data) {
  std::ifstream file(std::string(filename), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("SCR/PNL/MAP file not found.");
  }
  const std::vector<uint8_t> scr_data = ReadAll(file);

  if (filename.find("PNL") != std::string_view::npos) {
    constexpr int kPanelOffset = 0x100;
    const int first = input_value * 0x400;
    const int last = first + 0x1000 - 2;
    const size_t required = kPanelOffset + (last * 2) + 2 + 1;
    if (scr_data.size() < required) {
      return absl::InvalidArgumentError(
          absl::StrFormat("PNL file is %d bytes; panel %d needs %d.",
                          scr_data.size(), input_value, required));
    }
    map_data.resize(0x8000);
    for (int i = first; i <= last; i += 2) {
      map_data[i - first] = gfx::TileInfoToShort(gfx::GetTilesInfo(
          static_cast<uint16_t>(scr_data[kPanelOffset + (i * 2)])));
      map_data[i - first + 1] = gfx::TileInfoToShort(gfx::GetTilesInfo(
          static_cast<uint16_t>(scr_data[kPanelOffset + (i * 2) + 2])));
    }
  } else {
    constexpr int kTileCount = 0x1000;
    if (scr_data.size() < (kTileCount - 1) * 2 + 1) {
      return absl::InvalidArgumentError(
          absl::StrFormat("SCR file is %d bytes; expected at least %d.",
                          scr_data.size(), (kTileCount - 1) * 2 + 1));
    }
    map_data.resize(0x2000);
    for (int i = 0; i < kTileCount; i++) {
      map_data[i] = gfx::TileInfoToShort(
          gfx::GetTilesInfo(static_cast<uint16_t>(scr_data[i * 2])));
    }
  }
  return absl::OkStatus();
}

absl::Status DrawScrWithCgx(uint8_t bpp, std::vector<uint8_t>& map_bitmap_data,
                            std::vector<uint8_t>& map_data,
                            std::vector<uint8_t>& cgx_loaded) {
  const std::vector<uint16_t> dimensions = {0x000, 0x400, 0x800, 0xC00};
  uint8_t p = 0;
  for (const auto each_dimension : dimensions) {
    p = each_dimension;
    // for each tile on the tile buffer
    for (int i = 0; i < 0x400; i++) {
      if (map_data[i + p] != 0xFF) {
        auto t = gfx::GetTilesInfo(map_data[i + p]);

        for (auto yl = 0; yl < 8; yl++) {
          for (auto xl = 0; xl < 8; xl++) {
            int mx = xl * (1 - t.horizontal_mirror_) +
                     (7 - xl) * (t.horizontal_mirror_);
            int my =
                yl * (1 - t.vertical_mirror_) + (7 - yl) * (t.vertical_mirror_);

            int ty = (t.id_ / 16) * 1024;
            int tx = (t.id_ % 16) * 8;
            auto pixel = cgx_loaded[(tx + ty) + (yl * 128) + xl];

            int index = (((i % 32) * 8) + ((i / 32) * 2048) + mx + (my * 256));

            if (bpp != 8) {
              map_bitmap_data[index] =
                  static_cast<uint8_t>((pixel & 0xFF) + t.palette_ * 16);
            } else {
              map_bitmap_data[index] = static_cast<uint8_t>(pixel & 0xFF);
            }
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status SaveCgx(uint8_t bpp, std::string_view filename,
                     const std::vector<uint8_t>& cgx_data,
                     const std::vector<uint8_t>& cgx_header) {
  std::ofstream file(std::string(filename), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file for writing.");
  }

  CgxHeader header{};
  strncpy(header.file_type, "SCH", 4);
  std::string bpp_str = std::to_string(bpp) + "BIT";
  strncpy(header.bit_mode, bpp_str.c_str(), 5);
  strncpy(header.version_number, "Ver-0.01\n", 9);
  header.header_size = sizeof(CgxHeader);
  strncpy(header.hardware_name, "SFC", 4);
  header.bg_obj_flag = 0;
  header.color_palette_number = 0;

  file.write(reinterpret_cast<const char*>(&header), sizeof(CgxHeader));
  file.write(reinterpret_cast<const char*>(cgx_data.data()), cgx_data.size());
  file.write(reinterpret_cast<const char*>(cgx_header.data()),
             cgx_header.size());

  file.close();
  return absl::OkStatus();
}

std::vector<SDL_Color> DecodeColFile(std::string_view filename) {
  std::vector<SDL_Color> decoded_col;
  std::ifstream file(std::string(filename), std::ios::binary);
  if (!file.is_open()) {
    return decoded_col;
  }
  std::vector<uint8_t> buffer = ReadAll(file);
  if (buffer.size() < kColTrailerSize) {
    return decoded_col;
  }
  buffer.resize(buffer.size() - kColTrailerSize);

  for (size_t i = 0; i < buffer.size() / 2; i++) {
    const auto current_color =
        static_cast<uint16_t>(buffer[i * 2] | (buffer[i * 2 + 1] << 8));

    SDL_Color color;
    color.r = (current_color & 31) << 3;
    color.g = ((current_color >> 5) & 31) << 3;
    color.b = ((current_color >> 10) & 31) << 3;
    color.a = (i & 0xF) == 0 ? 0 : 255;
    decoded_col.push_back(color);
  }
  return decoded_col;
}

absl::Status SaveCol(std::string_view filename,
                     const std::vector<SDL_Color>& palette) {
  std::ofstream file(std::string(filename), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file for writing.");
  }

  for (const auto& color : palette) {
    uint16_t snes_color =
        ((color.b >> 3) << 10) | ((color.g >> 3) << 5) | (color.r >> 3);
    file.write(reinterpret_cast<const char*>(&snes_color), sizeof(snes_color));
  }

  file.close();
  return absl::OkStatus();
}

}  // namespace gfx
}  // namespace yaze
