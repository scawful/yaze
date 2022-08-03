#include "rom.h"

#include <SDL.h>

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

#define OVERWORLD_GRAPHICS_POS_1 0x4F80
#define OVERWORLD_GRAPHICS_POS_2 0x505F
#define OVERWORLD_GRAPHICS_POS_3 0x513E
#define COMPRESSION_STRING_MOD   7 << 5

#define SNES_BYTE_MAX 0xFF

#define CMD_MOD 0x07
#define CMD_EXPANDED_MOD 0xE0
#define CMD_EXPANDED_LENGTH_MOD 0x3FF
#define CMD_NORMAL_LENGTH_MOD 0x1F

namespace yaze {
namespace app {

namespace {

int GetGraphicsAddress(const uchar* data, uint8_t offset) {
  auto part_one = data[OVERWORLD_GRAPHICS_POS_1 + offset] << 16;
  auto part_two = data[OVERWORLD_GRAPHICS_POS_2 + offset] << 8;
  auto part_three = data[OVERWORLD_GRAPHICS_POS_3 + offset];
  auto snes_addr = (part_one | part_two | part_three);
  return core::SnesToPc(snes_addr);
}

void PrintCompressionPiece(const std::shared_ptr<CompressionPiece>& piece) {
  printf("Command : %d\n", piece->command);
  printf("length  : %d\n", piece->length);
  printf("Argument :");
  auto arg_size = piece->argument.size();
  for (int i = 0; i < arg_size; ++i) {
    printf("%02X ", piece->argument.at(i));
  }
  printf("\nArgument length : %d\n", piece->argument_length);
}

std::shared_ptr<CompressionPiece> NewCompressionPiece(
    const char command, const int length, const std::string args,
    const int argument_length) {
  auto new_piece = std::make_shared<CompressionPiece>(command, length, args,
                                                      argument_length);
  return std::move(new_piece);
}

// Merge consecutive copy if possible
std::shared_ptr<CompressionPiece> MergeCopy(
    std::shared_ptr<CompressionPiece>& start) {
  std::shared_ptr<CompressionPiece> piece = start;

  while (piece != nullptr) {
    if (piece->command == kCommandDirectCopy && piece->next != nullptr &&
        piece->next->command == kCommandDirectCopy) {
      if (piece->length + piece->next->length <= kMaxLengthCompression) {
        uint previous_length = piece->length;
        piece->length = piece->length + piece->next->length;

        for (int i = 0; i < piece->next->argument_length; ++i) {
          piece->argument[i + previous_length] = piece->next->argument[i];
        }
        piece->argument_length = piece->length;

        PrintCompressionPiece(piece);

        auto p_next_next = piece->next->next;
        piece->next = p_next_next;
        continue;  // Next could be another copy
      }
    }
    piece = piece->next;
  }
  return start;
}

std::shared_ptr<CompressionPiece> SplitCompressionPiece(
    std::shared_ptr<CompressionPiece>& piece, int mode) {
  std::shared_ptr<CompressionPiece> new_piece;
  uint length_left = piece->length - kMaxLengthCompression;
  piece->length = kMaxLengthCompression;
  switch (piece->command) {
    case kCommandByteFill:
    case kCommandWordFill:
      new_piece = NewCompressionPiece(piece->command, length_left,
                                      piece->argument, piece->argument_length);
      break;
    case kCommandIncreasingFill:
      new_piece = NewCompressionPiece(piece->command, length_left,
                                      piece->argument, piece->argument_length);
      new_piece->argument[0] =
          (char)(piece->argument[0] + kMaxLengthCompression);
      break;
    case kCommandDirectCopy:
      piece->argument_length = kMaxLengthCompression;
      new_piece = NewCompressionPiece(piece->command, length_left, nullptr,
                                      length_left);
      // MEMCPY
      for (int i = 0; i < length_left; ++i) {
        new_piece->argument[i] = piece->argument[i + kMaxLengthCompression];
      }
      break;
    case kCommandRepeatingBytes: {
      piece->argument_length = kMaxLengthCompression;
      uint offset = piece->argument[0] + (piece->argument[1] << 8);
      new_piece = NewCompressionPiece(piece->command, length_left,
                                      piece->argument, piece->argument_length);
      if (mode == kNintendoMode2) {
        new_piece->argument[0] = (offset + kMaxLengthCompression) & SNES_BYTE_MAX;
        new_piece->argument[1] = (offset + kMaxLengthCompression) >> 8;
      }
      if (mode == kNintendoMode1) {
        new_piece->argument[1] = (offset + kMaxLengthCompression) & SNES_BYTE_MAX;
        new_piece->argument[0] = (offset + kMaxLengthCompression) >> 8;
      }
    } break;
  }
  return new_piece;
}

Bytes CreateCompressionString(std::shared_ptr<CompressionPiece>& start,
                                int mode) {
  uint pos = 0;
  auto piece = start;
  Bytes output;

  while (piece != nullptr) {
    if (piece->length <= kMaxLengthNormalHeader) { // Normal header
      output.push_back(BUILD_HEADER(piece->command, piece->length));
      pos++;
    } else {
      if (piece->length <= kMaxLengthCompression) {
        output.push_back((COMPRESSION_STRING_MOD) | ((uchar)piece->command << 2) |
                         (((piece->length - 1) & 0xFF00) >> 8));
        pos++;
        printf("Building extended header : cmd: %d, length: %d -  %02X\n",
               piece->command, piece->length, output[pos - 1]);
        output.push_back(((piece->length - 1) & 0x00FF));  // (char)
        pos++;
      } else {
        // We need to split the command
        auto new_piece = SplitCompressionPiece(piece, mode);
        printf("New added piece\n");
        PrintCompressionPiece(new_piece);
        new_piece->next = piece->next;
        piece->next = new_piece;
        continue;
      }
    }

    if (piece->command == kCommandRepeatingBytes) {
      char tmp[2];
      if (mode == kNintendoMode2) {
        tmp[0] = piece->argument[0];
        tmp[1] = piece->argument[1];
      }
      if (mode == kNintendoMode1) {
        tmp[0] = piece->argument[1];
        tmp[1] = piece->argument[0];
      }
      for (int i = 0; i < 2; ++i) {
        output.push_back(tmp[i]);
        pos++;
      }
    } else {
      for (int i = 0; i < piece->argument_length; ++i) {
        output.push_back(piece->argument[i]);
        pos++;
      }
    }
    pos += piece->argument_length;
    piece = piece->next;
  }
  output.push_back(SNES_BYTE_MAX);
  return output;
}

// Test every command to see the gain with current position
void TestAllCommands(const uchar* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& u_data_pos,
                     const uint last_pos, uint start) {
  {  // BYTE REPEAT
    uint pos = u_data_pos;
    char byte_to_repeat = rom_data[pos];
    while (pos <= last_pos && rom_data[pos] == byte_to_repeat) {
      data_size_taken[kCommandByteFill]++;
      pos++;
    }
    cmd_args[kCommandByteFill][0] = byte_to_repeat;
  }
  {  // WORD REPEAT
    if (u_data_pos + 2 <= last_pos &&
        rom_data[u_data_pos] != rom_data[u_data_pos + 1]) {
      uint pos = u_data_pos;
      char byte1 = rom_data[pos];
      char byte2 = rom_data[pos + 1];
      pos += 2;
      data_size_taken[kCommandWordFill] = 2;
      while (pos + 1 <= last_pos) {
        if (rom_data[pos] == byte1 && rom_data[pos + 1] == byte2)
          data_size_taken[kCommandWordFill] += 2;
        else
          break;
        pos += 2;
      }
      cmd_args[kCommandWordFill][0] = byte1;
      cmd_args[kCommandWordFill][1] = byte2;
    }
  }
  {  // INC BYTE
    uint pos = u_data_pos;
    char byte = rom_data[pos];
    pos++;
    data_size_taken[kCommandIncreasingFill] = 1;
    while (pos <= last_pos && ++byte == rom_data[pos]) {
      data_size_taken[kCommandIncreasingFill]++;
      pos++;
    }
    cmd_args[kCommandIncreasingFill][0] = rom_data[u_data_pos];
  }
  {  // INTRA CPY
    if (u_data_pos != start) {
      uint searching_pos = start;
      uint current_pos_u = u_data_pos;
      uint copied_size = 0;
      uint search_start = start;

      while (searching_pos < u_data_pos && current_pos_u <= last_pos) {
        while (rom_data[current_pos_u] != rom_data[searching_pos] &&
               searching_pos < u_data_pos)
          searching_pos++;
        search_start = searching_pos;
        while (current_pos_u <= last_pos &&
               rom_data[current_pos_u] == rom_data[searching_pos] &&
               searching_pos < u_data_pos) {
          copied_size++;
          current_pos_u++;
          searching_pos++;
        }
        if (copied_size > data_size_taken[kCommandRepeatingBytes]) {
          search_start -= start;
          printf("-Found repeat of %d at %d\n", copied_size, search_start);
          data_size_taken[kCommandRepeatingBytes] = copied_size;
          cmd_args[kCommandRepeatingBytes][0] = search_start & SNES_BYTE_MAX;
          cmd_args[kCommandRepeatingBytes][1] = search_start >> 8;
        }
        current_pos_u = u_data_pos;
        copied_size = 0;
      }
    }
  }
}

// Check if a command managed to pick up `max_win` or more bytes
// Avoids being even with copy command, since it's possible to merge copy
void ValidateForByteGain(const DataSizeArray& data_size_taken,
                         const CommandSizeArray& cmd_size, uint& max_win,
                         uint& cmd_with_max) {
  for (uint cmd_i = 1; cmd_i < 5; cmd_i++) {
    uint cmd_size_taken = data_size_taken[cmd_i];
    // FIXME: Should probably be a table that say what is even with copy
    // but all other cmd are 2
    auto table_check =
        !(cmd_i == kCommandRepeatingBytes && cmd_size_taken == 3);
    if (cmd_size_taken > max_win && cmd_size_taken > cmd_size[cmd_i] &&
        table_check) {
      printf("--C:%d / S:%d\n", cmd_i, cmd_size_taken);
      cmd_with_max = cmd_i;
      max_win = cmd_size_taken;
    }
  }
}

void CompressionDirectCopy(const uchar* rom_data,
                           std::shared_ptr<CompressionPiece>& compressed_chain,
                           uint& u_data_pos, uint& bytes_since_last_compression,
                           uint& last_pos) {
  // We just move through the next byte and don't 'compress' yet, maybe
  // something is better after.
  u_data_pos++;
  bytes_since_last_compression++;

  // Arbitrary choice to do a 32 bytes grouping
  if (bytes_since_last_compression == 32 || u_data_pos > last_pos) {
    std::string buffer;
    for (int i = 0; i < bytes_since_last_compression; ++i) {
      buffer.push_back(rom_data[i + u_data_pos - bytes_since_last_compression]);
    }
    auto new_comp_piece =
        NewCompressionPiece(kCommandDirectCopy, bytes_since_last_compression,
                            buffer, bytes_since_last_compression);
    compressed_chain->next = new_comp_piece;
    compressed_chain = new_comp_piece;
    bytes_since_last_compression = 0;
  }
}

void CompressionCommandAlternative(
    const uchar* rom_data, std::shared_ptr<CompressionPiece>& compressed_chain,
    const CommandSizeArray& cmd_size, const CommandArgumentArray& cmd_args,
    uint& u_data_pos, uint& bytes_since_last_compression, uint& cmd_with_max,
    uint& max_win) {
  printf("- Ok we get a gain from %d\n", cmd_with_max);
  std::string buffer;
  buffer.push_back(cmd_args[cmd_with_max][0]);
  if (cmd_size[cmd_with_max] == 2) {
    buffer.push_back(cmd_args[cmd_with_max][1]);
  }

  auto new_comp_piece = NewCompressionPiece(cmd_with_max, max_win, buffer,
                                            cmd_size[cmd_with_max]);
  PrintCompressionPiece(new_comp_piece);
  // If we let non compressed stuff, we need to add a copy chunk before
  if (bytes_since_last_compression != 0) {
    std::string copy_buff;
    copy_buff.resize(bytes_since_last_compression);
    for (int i = 0; i < bytes_since_last_compression; ++i) {
      copy_buff[i] = rom_data[i + u_data_pos - bytes_since_last_compression];
    }
    auto copy_chunk =
        NewCompressionPiece(kCommandDirectCopy, bytes_since_last_compression,
                            copy_buff, bytes_since_last_compression);
    compressed_chain->next = copy_chunk;
    compressed_chain = copy_chunk;
  } else {
    compressed_chain->next = new_comp_piece;
    compressed_chain = new_comp_piece;
  }
  u_data_pos += max_win;
  bytes_since_last_compression = 0;
}

}  // namespace

// TODO TEST compressed data border for each cmd
absl::StatusOr<Bytes> ROM::Compress(const int start, const int length,
                                    int mode) {
  // Worse case should be a copy of the string with extended header
  auto compressed_chain = NewCompressionPiece(1, 1, "aaa", 2);
  auto compressed_chain_start = compressed_chain;

  CommandArgumentArray cmd_args = {{}};
  DataSizeArray data_size_taken = {0, 0, 0, 0, 0};
  CommandSizeArray cmd_size = {0, 1, 2, 1, 2};

  uint u_data_pos = start;
  uint last_pos = start + length - 1;
  uint bytes_since_last_compression = 0;  // Used when skipping using copy

  while (1) {
    data_size_taken.fill({});
    cmd_args.fill({{}});

    TestAllCommands(rom_data_.data(), data_size_taken, cmd_args, u_data_pos,
                    last_pos, start);

    uint max_win = 2;
    uint cmd_with_max = kCommandDirectCopy;
    ValidateForByteGain(data_size_taken, cmd_size, max_win, cmd_with_max);

    if (cmd_with_max == kCommandDirectCopy) {
      printf("- Best command is copy\n");
      // This is the worse case
      CompressionDirectCopy(rom_data_.data(), compressed_chain, u_data_pos,
                            bytes_since_last_compression, last_pos);
    } else {
      // Yay we get something better
      CompressionCommandAlternative(
          rom_data_.data(), compressed_chain, cmd_size, cmd_args, u_data_pos,
          bytes_since_last_compression, cmd_with_max, max_win);
    }

    if (u_data_pos > last_pos) {
      printf("Breaking compression loop\n");
      break;
    }

    // Validate compression result
    if (compressed_chain_start->next != nullptr) {
      ROM temp_rom;
      auto rom_response = temp_rom.LoadFromBytes(
          CreateCompressionString(compressed_chain_start->next, mode));
      if (!rom_response.ok()) {
        return rom_response;
      }
      auto decomp_response = temp_rom.Decompress(0, temp_rom.GetSize());
      if (!decomp_response.ok()) {
        return decomp_response.status();
      }

      auto decomp_data = std::move(*decomp_response);
      if (!std::equal(decomp_data.begin() + start, decomp_data.end(),
                      temp_rom.begin())) {
        return absl::InternalError(absl::StrFormat(
            "Compressed data does not match uncompressed data at %d\n",
            (uint)(u_data_pos - start)));
      }
    }
  }

  MergeCopy(compressed_chain_start->next);  // Skipping compression chain header

  compressed_chain = compressed_chain_start->next;
  while (compressed_chain != NULL) {
    printf("--Piece--\n");
    PrintCompressionPiece(compressed_chain);
    compressed_chain = compressed_chain->next;
  }

  return CreateCompressionString(compressed_chain_start->next, mode);
}

absl::StatusOr<Bytes> ROM::CompressGraphics(const int pos, const int length) {
  return Compress(pos, length, kNintendoMode2);
}
absl::StatusOr<Bytes> ROM::CompressOverworld(const int pos, const int length) {
  return Compress(pos, length, kNintendoMode1);
}

absl::StatusOr<Bytes> ROM::Decompress(int offset, int size, bool reversed) {
  Bytes buffer(size, 0);
  uint length = 0;
  uint buffer_pos = 0;
  uchar cmd = 0;
  uchar databyte = rom_data_[offset];
  while (databyte != SNES_BYTE_MAX) {  // End of decompression
    databyte = rom_data_[offset];

    if ((databyte & CMD_EXPANDED_MOD) == CMD_EXPANDED_MOD) {  // Expanded Command
      cmd = ((databyte >> 2) & CMD_MOD);
      length = (((databyte << 8) | rom_data_[offset + 1]) & CMD_EXPANDED_LENGTH_MOD);
      offset += 2;  // Advance 2 bytes in ROM
    } else {        // Normal Command
      cmd = ((databyte >> 5) & CMD_MOD);
      length = (databyte & CMD_NORMAL_LENGTH_MOD);
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
        ushort s1 = ((rom_data_[offset + 1] & SNES_BYTE_MAX) << 8);
        ushort s2 = ((rom_data_[offset] & SNES_BYTE_MAX));
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

absl::StatusOr<Bytes> ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, false);
}

absl::StatusOr<Bytes> ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, true);
}

absl::StatusOr<Bytes> ROM::SNES3bppTo8bppSheet(Bytes sheet, int size) {
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

absl::Status ROM::LoadFromFile(const absl::string_view& filename) {
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

absl::Status ROM::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

absl::Status ROM::LoadFromBytes(Bytes data) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
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
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        sheet[j] = rom_data_[j + offset];
      }
    } else {
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      absl::StatusOr<Bytes> new_sheet =
          Decompress(offset, core::UncompressedSheetSize);
      if (!new_sheet.ok()) {
        return new_sheet.status();
      } else {
        sheet = std::move(*new_sheet);
      }
    }

    absl::StatusOr<Bytes> converted_sheet = SNES3bppTo8bppSheet(sheet);
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

}  // namespace app
}  // namespace yaze