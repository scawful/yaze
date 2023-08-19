#include "scad_format.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace gfx {

void FindMetastamp() {
  int matching_position = -1;
  bool matched = false;
  Bytes cgx_rom;
  Bytes raw_data_;
  for (int i = 0;
       i < cgx_rom.size() - sizeof(kMatchedBytes) - kOffsetFromMatchedBytesEnd;
       i++) {
    raw_data_.push_back(cgx_rom[i]);
    bool is_match = std::equal(std::begin(kMatchedBytes),
                               std::end(kMatchedBytes), &cgx_rom[i]);
    if (is_match) {
      matching_position = i;
      matched = true;
      break;
    }
  }
  if (matched) {
    int bpp_marker_position =
        matching_position + sizeof(kMatchedBytes) + kOffsetFromMatchedBytesEnd;
    int bpp_marker = cgx_rom[bpp_marker_position];
    std::string bpp_type = (bpp_marker == 0x31) ? "8bpp" : "4bpp";
  }
}

absl::Status LoadCgx(uint8_t bpp, std::string_view filename,
                     std::vector<uint8_t>& cgx_data,
                     std::vector<uint8_t>& cgx_loaded,
                     std::vector<uint8_t>& cgx_header) {
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("CGX file not found.");
  }
  std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  cgx_data =
      std::vector<uint8_t>(file_content.begin(), file_content.end() - 0x500);
  file.seekg(cgx_data.size() + 0x100);
  cgx_header = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  file.close();

  if (bpp > 8) {
    cgx_loaded = gfx::BPP8SNESToIndexed(cgx_data, 40);
    return absl::OkStatus();
  }
  cgx_loaded = gfx::BPP8SNESToIndexed(cgx_data, bpp);
  return absl::OkStatus();
}

absl::Status LoadScr(std::string_view filename, uint8_t input_value,
                     std::vector<uint8_t>& map_data) {
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("SCR/PNL/MAP file not found.");
  }

  // Check if file extension is PNL
  bool pnl = false;
  if (filename.find("PNL") != std::string::npos) {
    std::vector<uint8_t> scr_data;
    map_data.resize(0x8000);
    scr_data.resize(0x8000);

    // Read from file for 0x8000 bytes
    std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
    scr_data = std::vector<uint8_t>(file_content.begin(), file_content.end());

    int md = 0x100;

    for (int i = input_value * 0x400; i < 0x1000 + input_value * 0x400;
         i += 2) {
      auto b1_pos = (i - (input_value * 0x400));
      map_data[b1_pos] = gfx::TileInfoToShort(
          gfx::GetTilesInfo((ushort)scr_data[md + (i * 2)]));

      auto b2_pos = (i - (input_value * 0x400) + 1);
      map_data[b2_pos] = gfx::TileInfoToShort(
          gfx::GetTilesInfo((ushort)scr_data[md + (i * 2) + 2]));
    }
    // 0x900

  } else {
    int offset = 0;
    std::vector<uint8_t> scr_data;
    map_data.resize(0x2000);
    scr_data.resize(0x2000);

    // read from file for 0x2000 bytes
    std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
    scr_data = std::vector<uint8_t>(file_content.begin(), file_content.end());

    for (int i = 0; i < 0x1000 - offset; i++) {
      map_data[i] = gfx::TileInfoToShort(
          gfx::GetTilesInfo((ushort)scr_data[((i + offset) * 2)]));
    }
  }
  return absl::OkStatus();
}

absl::Status DrawScrWithCgx(uint8_t bpp, std::vector<uint8_t>& map_data,
                            std::vector<uint8_t>& map_bitmap_data,
                            std::vector<uint8_t>& cgx_loaded) {
  const std::vector<uint16_t> dimensions = {0x000, 0x400, 0x800, 0xC00};
  uint8_t p = 0;
  for (const auto each_dimension : dimensions) {
    p = each_dimension;
    // for each tile on the tile buffer
    for (int i = 0; i < 0x400; i++) {
      if (map_data[i + p] != 0xFFFF) {
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
                  (uchar)((pixel & 0xFF) + t.palette_ * 16);
            } else {
              map_bitmap_data[index] = (uchar)(pixel & 0xFF);
            }
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

std::vector<SDL_Color> DecodeColFile(const std::string_view filename) {
  std::vector<SDL_Color> decoded_col;
  std::ifstream file(filename.data(), std::ios::binary | std::ios::ate);

  if (!file.is_open()) {
    return decoded_col;  // Return an empty vector if the file couldn't be
                         // opened.
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (file.read(buffer.data(), size)) {
    buffer.resize(size - 0x200);

    int k = 0;
    for (size_t i = 0; i < buffer.size() / 2; i++) {
      uint16_t current_color = static_cast<unsigned char>(buffer[k]) |
                               (static_cast<unsigned char>(buffer[k + 1]) << 8);

      SDL_Color color;
      color.r = (current_color & 31) << 3;
      color.g = ((current_color >> 5) & 31) << 3;
      color.b = ((current_color >> 10) & 31) << 3;
      color.a = (i & 0xF) == 0 ? 0 : 255;

      decoded_col.push_back(color);
      k += 2;
    }
  }

  return decoded_col;
}

absl::Status DecodeObjFile(
    std::string_view filename, std::vector<uint8_t>& obj_data,
    std::vector<uint8_t> actual_obj_data,
    std::unordered_map<std::string, std::vector<uint8_t>> decoded_obj,
    std::vector<uint8_t>& decoded_extra_obj, int& obj_loaded) {
  std::vector<uint8_t> header_obj;
  int obj_range;
  int expected_cut;
  if (obj_loaded == 0) {
    obj_range = 0x180;
    expected_cut = 0x500;
  } else {
    obj_range = 0x300;
    expected_cut = 0x900;
  }

  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("OBJ file not found.");
  }

  std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  obj_data = file_content;
  file.close();

  int cut = obj_data.size() & 0x0FFF;
  actual_obj_data =
      std::vector<uint8_t>(obj_data.begin(), obj_data.end() - cut);
  decoded_extra_obj =
      std::vector<uint8_t>(obj_data.begin() + actual_obj_data.size(),
                           obj_data.begin() + actual_obj_data.size() + 0x100);
  header_obj = std::vector<uint8_t>(
      actual_obj_data.begin() + actual_obj_data.size(), actual_obj_data.end());

  if (cut > expected_cut) {
    std::vector<uint8_t> scad_data;
    int j = 0;
    int k = (obj_loaded == 0) ? 63 : 127;

    for (size_t i = 0; i < (actual_obj_data.size() / 6); i++) {
      std::vector<uint8_t> data = {
          actual_obj_data[k * 6 + 0 + j],  // display
          actual_obj_data[k * 6 + 1 + j],  // unknown
          actual_obj_data[k * 6 + 2 + j],  // y-disp
          actual_obj_data[k * 6 + 3 + j],  // x-disp
          actual_obj_data[k * 6 + 5 + j],  // props
          actual_obj_data[k * 6 + 4 + j]   // tile
      };
      scad_data.insert(scad_data.end(), data.begin(), data.end());

      k = k - 1;
      if (k == -1) {
        k = (obj_loaded == 0) ? 63 : 127;
        j = j + ((k + 1) * 6);
      }
    }

    int extra_data_range = 0x400 * (obj_loaded + 1) + 0x100;
    for (int i = 0; i < extra_data_range; i++) {
      scad_data.push_back(header_obj[i]);
    }

    obj_data = scad_data;
    actual_obj_data =
        std::vector<uint8_t>(obj_data.begin(), obj_data.end() - cut);
  }

  decoded_obj.clear();
  for (int k = 0; k < 128; k++) {
    decoded_obj["frame " + std::to_string(k)] = std::vector<uint8_t>(obj_range);
    for (int i = 0; i < obj_range; i++) {
      try {
        decoded_obj["frame " + std::to_string(k)][i] =
            obj_data[i + (obj_range * k)];
      } catch (...) {
        decoded_obj["frame " + std::to_string(k)][i] = 0;
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze