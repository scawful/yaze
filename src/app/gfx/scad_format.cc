#include "scad_format.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

CgxHeader ExtractCgxHeader(std::vector<uint8_t>& cgx_header) {
  CgxHeader header;
  memcpy(&header, cgx_header.data(), sizeof(CgxHeader));
  return header;
}

absl::Status DecodeCgxFile(std::string_view filename,
                           std::vector<uint8_t>& cgx_data,
                           std::vector<uint8_t>& extra_cgx_data,
                           std::vector<uint8_t>& decoded_cgx) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("CGX file not found.");
  }

  std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  cgx_data =
      std::vector<uint8_t>(file_content.begin(), file_content.end() - 0x500);
  file.seekg(cgx_data.size() + 0x100);
  extra_cgx_data = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
  file.close();

  decoded_cgx.clear();
  const uint16_t num_tiles = cgx_data.size() >> 5;
  for (size_t i = 0; i < num_tiles; i++) {
    for (int j = 0; j < 8; j++) {
      for (int h = 0; h < 8; h++) {
        uint8_t pixel = 0;
        for (int l = 0; l < 2; l++) {
          for (int k = 0; k < 2; k++) {
            if (cgx_data[(i * 0x20) + (l * 0x10) + (j * 2) + k] &
                (1 << (7 - h))) {
              pixel = pixel | (1 << (l * 2 + k));
            }
          }
        }
        decoded_cgx.push_back(pixel);
      }
    }
  }

  if (decoded_cgx.size() < 0x10000) {
    std::cout << "Resetting VRAM Offset to not be out of bounds." << std::endl;
    //// default_offset  "0"
  }
  return absl::OkStatus();
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

  std::ifstream file(filename, std::ios::binary);
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

std::vector<SDL_Color> DecodeColFile(const std::string& filename) {
  std::vector<SDL_Color> decoded_col;
  std::ifstream file(filename, std::ios::binary | std::ios::ate);

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

SDL_Surface* CreateCgxPreviewImage(int default_cgram,
                                   const std::vector<uint8_t>& cgx_data,
                                   const std::vector<uint8_t>& extra_cgx_data,
                                   std::vector<SDL_Color> decoded_col) {
  std::vector<std::vector<std::vector<int>>> tiles;

  const int target_width = 16;
  const int target_height = 64;
  int num_tiles = cgx_data.size() >> 5;

  int set_height = std::floor(num_tiles / target_width);

  for (int tile = 0; tile < num_tiles; tile++) {
    std::vector<std::vector<int>> single_tile;
    for (int row = 0; row < 8; row++) {
      std::vector<int> single_row;
      for (int col = 0; col < 8; col++) {
        int palette_num = 0;
        for (int pair = 0; pair < 2; pair++) {
          for (int bitplane = 0; bitplane < 2; bitplane++) {
            if (cgx_data[(tile * 0x20) + (pair * 0x10) + (row * 2) + bitplane] &
                (1 << (7 - col))) {
              palette_num |= (1 << (pair * 2 + bitplane));
            }
          }
        }
        single_row.push_back(palette_num);
      }
      single_tile.push_back(single_row);
    }
    tiles.push_back(single_tile);
  }

  std::vector<int> pixmap;
  int row_i = 0;
  for (int line = 0; line < set_height; line++) {
    for (int row = 0; row < 8; row++) {
      for (int i = 0; i < target_width; i++) {
        for (int color : tiles[row_i + i][row]) {
          pixmap.push_back(color);
        }
      }
    }
    row_i += target_width;
  }

  int cols = target_width * 8;
  int rows = target_height * 8;

  SDL_Surface* cgx_image = SDL_CreateRGBSurface(0, cols, rows, 32, 0, 0, 0, 0);

  int use_palette = default_cgram;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      int extra_data_index = (col >> 3) | (row & 0xFF8) << 1;
      int palette_row = extra_cgx_data[extra_data_index];
      int index = (row * cols) + col;

      if (index >= 0 && index < pixmap.size() &&
          (pixmap[index] + use_palette + palette_row * 16) <
              decoded_col.size()) {
        SDL_Color color =
            decoded_col[pixmap[index] + use_palette + palette_row * 16];
        uint32_t pixel =
            SDL_MapRGBA(cgx_image->format, color.r, color.g, color.b, color.a);
        ((uint32_t*)cgx_image->pixels)[(row * cols) + col] = pixel;
      }
    }
  }

  return cgx_image;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze