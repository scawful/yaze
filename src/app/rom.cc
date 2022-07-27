#include "rom.h"

#include <SDL2/SDL.h>

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
#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {

absl::Status ROM::LoadFromFile(const absl::string_view &filename) {
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename));
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
  memcpy(title, rom_data_.data() + 32704, 20);  // copy ROM title
  return absl::OkStatus();
}

absl::Status ROM::LoadFromPointer(uchar *data, size_t length) {
  if (data == nullptr)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

// 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
// 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
// 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
// 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
// 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
absl::Status ROM::LoadAllGraphicsData() {
  Bytes sheet;

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
          Decompress(offset, core::UncompressedSheetSize);
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
      graphics_bin_[i] =
          gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                      core::kTilesheetDepth, result.data());
      graphics_bin_.at(i).CreateTexture(renderer_);
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Bytes> ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, false);
}

absl::StatusOr<Bytes> ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, true);
}

absl::StatusOr<Bytes> ROM::Decompress(int offset, int size, bool reversed) {
  Bytes buffer(size);
  uint length = 0;
  uint buffer_pos = 0;
  uchar cmd = 0;
  uchar databyte = rom_data_[offset];
  while (databyte != 0xFF) {  // End of decompression
    databyte = rom_data_[offset];

    if ((databyte & 0xE0) == 0xE0) {  // Expanded Command
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
      case kCommandDirectCopy:  // Does not advance in the ROM
        for (int i = 0; i < length; i++)
          buffer[buffer_pos++] = rom_data_[offset++];
        break;
      case kCommandByteFill:  // Advances 1 byte in the ROM
        for (int i = 0; i < length; i++)
          buffer[buffer_pos++] = rom_data_[offset];
        offset += 1;
        break;
      case kCommandWordFill:  // Advance 2 byte in the ROM
        for (int i = 0; i < length; i += 2) {
          buffer[buffer_pos++] = rom_data_[offset];
          buffer[buffer_pos++] = rom_data_[offset + 1];
        }
        offset += 2;
        break;
      case kCommandIncreasingFill: {
        uchar inc_byte = rom_data_[offset];
        for (int i = 0; i < length; i++) buffer[buffer_pos++] = inc_byte++;
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((rom_data_[offset + 1] & 0xFF) << 8);
        ushort s2 = ((rom_data_[offset] & 0xFF));
        if (reversed) {  // Reversed byte order for overworld maps
          auto addr = (rom_data_[offset + 2]) | ((rom_data_[offset + 1]) << 8);
          if (addr > offset) {
            return absl::InternalError(absl::StrFormat(
                "DecompressOverworldV2: Offset for command copy exceeds "
                "current position (Offset : %#04x | Pos : %#06x)\n",
                addr, offset));
          }
          if (buffer_pos + length >= size) {
            size *= 2;
            buffer.resize(size);
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

uint ROM::GetGraphicsAddress(uint8_t offset) const {
  auto snes_address = (uint)((((rom_data_[0x4F80 + offset]) << 16) |
                              ((rom_data_[0x505F + offset]) << 8) |
                              ((rom_data_[0x513E + offset]))));
  return core::SnesToPc(snes_address);
}

}  // namespace app
}  // namespace yaze