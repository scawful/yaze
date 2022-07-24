#include "rom.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/pseudo_vram.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

absl::Status ROM::OpenFromFile(const absl::string_view &filename) {
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file ", filename));
  }
  size_ = std::filesystem::file_size(filename);
  rom_data_.resize(size_);
  for (auto i = 0; i < size_; ++i) {
    char byte_to_read = ' ';
    file.read(&byte_to_read, sizeof(char));
    rom_data_[i] = byte_to_read;
  }
  file.close();
  is_loaded_ = true;
  return absl::OkStatus();
}

void ROM::Close() {
  if (is_loaded_) {
    delete[] current_rom_;
    for (auto i = 0; i < num_sheets_; i++) {
      delete[] decompressed_graphic_sheets_[i];
      delete[] converted_graphic_sheets_[i];
    }
  }
}

void ROM::SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
  renderer_ = renderer;
}

// TODO: check if the rom has a header on load
void ROM::LoadFromFile(const std::string &path) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file.is_open()) {
    std::cout << "Error: Could not open ROM file " << path << std::endl;
    return;
  }
  size_ = std::filesystem::file_size(path.c_str());
  current_rom_ = new uchar[size_];
  for (uint i = 0; i < size_; i++) {
    char byte_read_ = ' ';
    file.read(&byte_read_, sizeof(char));
    current_rom_[i] = byte_read_;
  }
  file.close();
  SDL_memcpy(title, current_rom_ + 32704, 20);
  is_loaded_ = true;
}

void ROM::LoadFromPointer(uchar *data) { current_rom_ = data; }

// 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
// 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
// 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
// 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
// 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
void ROM::LoadAllGraphicsData() {
  auto buffer = new uchar[346624];
  auto data = new uchar[2048];
  int buffer_pos = 0;

  for (int i = 0; i < core::NumberOfSheets; i++) {
    // uncompressed sheets
    if (i >= 115 && i <= 126) {
      data = new uchar[core::Uncompressed3BPPSize];
      int startAddress = GetGraphicsAddress(i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        data[j] = current_rom_[j + startAddress];
      }
    } else {
      auto gfx_addr = GetGraphicsAddress(i);
      data = Decompress(gfx_addr, core::UncompressedSheetSize);
    }

    gfx::Bitmap tilesheet_bmp(core::kTilesheetWidth, core::kTilesheetHeight,
                              core::kTilesheetDepth, SNES3bppTo8bppSheet(data));
    tilesheet_bmp.CreateTexture(renderer_);
    graphics_bin_[i] = tilesheet_bmp;

    for (int j = 0; j < sizeof(data); j++) {
      buffer[j + buffer_pos] = data[j];
    }

    buffer_pos += sizeof(data);
  }

  master_gfx_bin_ = buffer;
}

absl::Status ROM::LoadAllGraphicsDataV2() {
  Bytes sheet;
  int buffer_pos = 0;

  for (int i = 0; i < core::NumberOfSheets; i++) {
    if (i >= 115 && i <= 126) {  // uncompressed sheets
      sheet.resize(core::Uncompressed3BPPSize);
      auto offset = GetGraphicsAddress(i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        sheet[j] = rom_data_[j + offset];
      }
    } else {
      auto offset = GetGraphicsAddress(i);
      absl::StatusOr<Bytes> new_sheet =
          DecompressV2(offset, core::UncompressedSheetSize);
      if (!new_sheet.ok()) {
        return new_sheet.status();
      } else {
        sheet = std::move(*new_sheet);
      }
    }

    absl::StatusOr<Bytes> converted_sheet = Convert3bppTo8bppSheet(sheet);
    if (!converted_sheet.ok()) {
      return converted_sheet.status();
    } else {
      Bytes result = std::move(*converted_sheet);
      gfx::Bitmap tilesheet_bmp(core::kTilesheetWidth, core::kTilesheetHeight,
                                core::kTilesheetDepth, result.data());
      tilesheet_bmp.CreateTexture(renderer_);
      graphics_bin_v2_[i] = tilesheet_bmp;
    }
  }
  return absl::OkStatus();
}

uint ROM::GetGraphicsAddress(uint8_t offset) const {
  uint snes_address = 0;
  uint pc_address = 0;
  snes_address = (uint)((((current_rom_[0x4F80 + offset]) << 16) |
                         ((current_rom_[0x505F + offset]) << 8) |
                         ((current_rom_[0x513E + offset]))));
  pc_address = core::SnesToPc(snes_address);
  return pc_address;
}

uchar *ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, false);
}

uchar *ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, true);
}

uchar *ROM::Decompress(int pos, int size, bool reversed) {
  auto buffer = new uchar[size];
  uint length = 0;
  uint buffer_pos = 0;
  uchar cmd = 0;

  uchar databyte = current_rom_[pos];
  while (databyte != 0xFF) {  // End of decompression
    databyte = current_rom_[pos];

    // Expanded Command
    if ((databyte & 0xE0) == 0xE0) {
      cmd = (uchar)((databyte >> 2) & 0x07);
      length =
          (ushort)(((current_rom_[pos] << 8) | current_rom_[pos + 1]) & 0x3FF);
      pos += 2;  // Advance 2 bytes in ROM
    } else {     // Normal Command
      cmd = (uchar)((databyte >> 5) & 0x07);
      length = (uchar)(databyte & 0x1F);
      pos += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // each commands is at least of size 1 even if index 00

    switch (cmd) {
      case kCommandDirectCopy:
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos++] = current_rom_[pos++];
        }
        // Do not advance in the ROM
        break;
      case kCommandByteFill:
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos++] = current_rom_[pos];
        }
        pos += 1;  // Advance 1 byte in the ROM
        break;
      case kCommandWordFill:
        for (int i = 0; i < length; i += 2) {
          buffer[buffer_pos++] = current_rom_[pos];
          buffer[buffer_pos++] = current_rom_[pos + 1];
        }
        pos += 2;  // Advance 2 byte in the ROM
        break;
      case kCommandIncreasingFill: {
        uchar inc_byte = current_rom_[pos];
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos++] = inc_byte++;
        }
        pos += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((current_rom_[pos + 1] & 0xFF) << 8);
        ushort s2 = ((current_rom_[pos] & 0xFF));
        // Reversed byte order for overworld maps
        if (reversed) {
          auto addr = (current_rom_[pos + 2]) | ((current_rom_[pos + 1]) << 8);
          if (buffer_pos + length >= size) {
            size *= 2;
            buffer = new uchar[size];
            std::cout << "Reallocate buffer" << std::endl;
          }
          memcpy(buffer + buffer_pos, current_rom_ + pos, length);
          pos += 2;
        } else {
          auto addr = (ushort)(s1 | s2);
          for (int i = 0; i < length; i++) {
            buffer[buffer_pos] = buffer[addr + i];
            buffer_pos++;
          }
          pos += 2;  // Advance 2 bytes in the ROM
        }
      } break;
    }
  }
  num_sheets_++;
  decompressed_graphic_sheets_.push_back(buffer);
  return buffer;
}

absl::StatusOr<Bytes> ROM::DecompressV2(int offset, int size, bool reversed) {
  Bytes buffer(size);
  uint length = 0;
  uint buffer_pos = 0;
  uchar cmd = 0;

  uchar databyte = rom_data_[offset];
  while (databyte != 0xFF) {  // End of decompression
    databyte = rom_data_[offset];

    // Expanded Command
    if ((databyte & 0xE0) == 0xE0) {
      cmd = ((databyte >> 2) & 0x07);
      length = (((rom_data_[offset] << 8) | rom_data_[offset + 1]) & 0x3FF);
      offset += 2;  // Advance 2 bytes in ROM
    } else {        // Normal Command
      cmd = ((databyte >> 5) & 0x07);
      length = (databyte & 0x1F);
      offset += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // each commands is at least of size 1 even if index 00

    switch (cmd) {
      case kCommandDirectCopy:
        for (int i = 0; i < length; i++)
          buffer[buffer_pos++] = rom_data_[offset++];
        // Do not advance in the ROM
        break;
      case kCommandByteFill:
        for (int i = 0; i < length; i++)
          buffer[buffer_pos++] = rom_data_[offset];
        offset += 1;  // Advance 1 byte in the ROM
        break;
      case kCommandWordFill:
        for (int i = 0; i < length; i += 2) {
          buffer[buffer_pos++] = rom_data_[offset];
          buffer[buffer_pos++] = rom_data_[offset + 1];
        }
        offset += 2;  // Advance 2 byte in the ROM
        break;
      case kCommandIncreasingFill: {
        uchar inc_byte = rom_data_[offset];
        for (int i = 0; i < length; i++) buffer[buffer_pos++] = inc_byte++;
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((rom_data_[offset + 1] & 0xFF) << 8);
        ushort s2 = ((rom_data_[offset] & 0xFF));
        // Reversed byte order for overworld maps
        if (reversed) {
          auto addr = (rom_data_[offset + 2]) | ((rom_data_[offset + 1]) << 8);
          if (addr > buffer_pos) {
            return absl::InternalError(
                absl::StrFormat("DecompressOverworldV2: Offset for command "
                                "copy existing is larger than the current "
                                "position (Offset : %#04x | Pos %#06x\n",
                                addr, buffer_pos));
          }

          if (buffer_pos + length >= size) {
            size *= 2;
            buffer.resize(size);
            std::cout << "DecompressOverworldV2: Reallocating buffer";
          }
          memcpy(buffer.data() + buffer_pos, rom_data_.data() + offset, length);
          offset += 2;
        } else {
          auto addr = (ushort)(s1 | s2);
          for (int i = 0; i < length; i++) {
            buffer[buffer_pos] = buffer[addr + i];
            buffer_pos++;
          }
          offset += 2;  // Advance 2 bytes in the ROM
        }
      } break;
    }
  }

  return buffer;
}

absl::StatusOr<Bytes> ROM::Convert3bppTo8bppSheet(Bytes sheet, int size) {
  Bytes sheet_buffer_out(size);
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;

  // for each tiles, 16 per line
  for (int i = 0; i < 64; i++) {
    // for each line
    for (int y = 0; y < 8; y++) {
      //[0] + [1] + [16]
      for (int x = 0; x < 8; x++) {
        auto b1 = ((sheet[(y * 2) + (24 * pos)] & (kGraphicsBitmap[x])));
        auto b2 = (sheet[((y * 2) + (24 * pos)) + 1] & (kGraphicsBitmap[x]));
        auto b3 = (sheet[(16 + y) + (24 * pos)] & (kGraphicsBitmap[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0) {
          b |= 4;
        }
        sheet_buffer_out[x + (xx) + (y * 128) + (yy * 1024)] = b;
      }
    }
    pos++;
    ypos++;
    xx += 8;
    if (ypos >= 16) {
      yy++;
      xx = 0;
      ypos = 0;
    }
  }
  return sheet_buffer_out;
}

// 128x32
uchar *ROM::SNES3bppTo8bppSheet(uchar *buffer_in, int sheet_id, int size) {
  // 8bpp sheet out
  auto sheet_buffer_out = new uchar[size];
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;

  if (sheet_id != 0) {
    yy = sheet_id;
  }

  // for each tiles
  // 16 per line
  for (int i = 0; i < 64; i++) {
    // for each line
    for (int y = 0; y < 8; y++) {
      //[0] + [1] + [16]
      for (int x = 0; x < 8; x++) {
        auto b1 = ((buffer_in[(y * 2) + (24 * pos)] & (kGraphicsBitmap[x])));
        auto b2 =
            (buffer_in[((y * 2) + (24 * pos)) + 1] & (kGraphicsBitmap[x]));
        auto b3 = (buffer_in[(16 + y) + (24 * pos)] & (kGraphicsBitmap[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0) {
          b |= 4;
        }
        sheet_buffer_out[x + (xx) + (y * 128) + (yy * 1024)] = b;
      }
    }
    pos++;
    ypos++;
    xx += 8;
    if (ypos >= 16) {
      yy++;
      xx = 0;
      ypos = 0;
    }
  }
  converted_graphic_sheets_.push_back(sheet_buffer_out);
  return sheet_buffer_out;
}

SDL_Texture *ROM::DrawGraphicsSheet(int offset) {
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 128, 32, 8, SDL_PIXELFORMAT_INDEX8);
  std::cout << "Drawing surface #" << offset << std::endl;
  uchar *sheet_buffer = nullptr;
  for (int i = 0; i < 8; i++) {
    surface->format->palette->colors[i].r = i * 31;
    surface->format->palette->colors[i].g = i * 31;
    surface->format->palette->colors[i].b = i * 31;
  }

  uint graphics_address = GetGraphicsAddress(offset);
  std::cout << "Decompressing..." << std::endl;
  auto decomp = Decompress(graphics_address);
  std::cout << "Converting to 8bpp sheet..." << std::endl;
  sheet_buffer = SNES3bppTo8bppSheet(decomp);
  std::cout << "Assigning pixel data..." << std::endl;
  surface->pixels = sheet_buffer;
  std::cout << "Creating texture from surface..." << std::endl;
  SDL_Texture *sheet_texture = nullptr;
  sheet_texture = SDL_CreateTextureFromSurface(renderer_.get(), surface);
  if (sheet_texture == nullptr) {
    std::cout << "Error: " << SDL_GetError() << std::endl;
  }
  return sheet_texture;
}

}  // namespace app
}  // namespace yaze