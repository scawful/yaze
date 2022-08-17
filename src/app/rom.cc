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
#define COMPRESSION_STRING_MOD 7 << 5

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

Bytes SNES3bppTo8bppSheet(Bytes sheet) {
  Bytes sheet_buffer_out(0x1000);
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

void PrintCompressionPiece(const std::shared_ptr<CompressionPiece>& piece) {
  printf("Command: %d\n", piece->command);
  printf("Command kength: %d\n", piece->length);
  printf("Argument:");
  auto arg_size = piece->argument.size();
  for (int i = 0; i < arg_size; ++i) {
    printf("%02X ", piece->argument.at(i));
  }
  printf("\nArgument length: %d\n", piece->argument_length);
}

void PrintCompressionChain(
    const std::shared_ptr<CompressionPiece>& compressed_chain_start) {
  auto compressed_chain = compressed_chain_start->next;
  while (compressed_chain != nullptr) {
    printf("- Compression Piece -\n");
    PrintCompressionPiece(compressed_chain);
    compressed_chain = compressed_chain->next;
  }
}

void CheckByteRepeat(const uchar* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const uint last_pos) {
  uint pos = src_data_pos;
  char byte_to_repeat = rom_data[pos];
  while (pos <= last_pos && rom_data[pos] == byte_to_repeat) {
    data_size_taken[kCommandByteFill]++;
    pos++;
  }
  cmd_args[kCommandByteFill][0] = byte_to_repeat;
}

void CheckWordRepeat(const uchar* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const uint last_pos) {
  if (src_data_pos + 2 <= last_pos &&
      rom_data[src_data_pos] != rom_data[src_data_pos + 1]) {
    uint pos = src_data_pos;
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

void CheckIncByte(const uchar* rom_data, DataSizeArray& data_size_taken,
                  CommandArgumentArray& cmd_args, uint& src_data_pos,
                  const uint last_pos) {
  uint pos = src_data_pos;
  char byte = rom_data[pos];
  pos++;
  data_size_taken[kCommandIncreasingFill] = 1;
  byte++;
  while (pos <= last_pos && byte == rom_data[pos]) {
    data_size_taken[kCommandIncreasingFill]++;
    byte++;
    pos++;
  }
  cmd_args[kCommandIncreasingFill][0] = rom_data[src_data_pos];
}

void CheckIntraCopy(const uchar* rom_data, DataSizeArray& data_size_taken,
                    CommandArgumentArray& cmd_args, uint& src_data_pos,
                    const uint last_pos, uint start) {
  if (src_data_pos != start) {
    uint searching_pos = start;
    uint current_pos_u = src_data_pos;
    uint copied_size = 0;
    uint search_start = start;

    while (searching_pos < src_data_pos && current_pos_u <= last_pos) {
      while (rom_data[current_pos_u] != rom_data[searching_pos] &&
             searching_pos < src_data_pos)
        searching_pos++;
      search_start = searching_pos;
      while (current_pos_u <= last_pos &&
             rom_data[current_pos_u] == rom_data[searching_pos] &&
             searching_pos < src_data_pos) {
        copied_size++;
        current_pos_u++;
        searching_pos++;
      }
      if (copied_size > data_size_taken[kCommandRepeatingBytes]) {
        search_start -= start;
        printf("- Found repeat of %d at %d\n", copied_size, search_start);
        data_size_taken[kCommandRepeatingBytes] = copied_size;
        cmd_args[kCommandRepeatingBytes][0] = search_start & SNES_BYTE_MAX;
        cmd_args[kCommandRepeatingBytes][1] = search_start >> 8;
      }
      current_pos_u = src_data_pos;
      copied_size = 0;
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
    // TODO(@scawful): Replace conditional with table of command sizes
    // "Table that is even with copy but all other cmd are 2"
    auto table_check =
        !(cmd_i == kCommandRepeatingBytes && cmd_size_taken == 3);
    if (cmd_size_taken > max_win && cmd_size_taken > cmd_size[cmd_i] &&
        table_check) {
      printf("==> C:%d / S:%d\n", cmd_i, cmd_size_taken);
      cmd_with_max = cmd_i;
      max_win = cmd_size_taken;
    }
  }
}

void CompressionCommandAlternative(
    const uchar* rom_data, std::shared_ptr<CompressionPiece>& compressed_chain,
    const CommandSizeArray& cmd_size, const CommandArgumentArray& cmd_args,
    uint& src_data_pos, uint& comp_accumulator, uint& cmd_with_max,
    uint& max_win) {
  printf("- Ok we get a gain from %d\n", cmd_with_max);
  std::string buffer;
  buffer.push_back(cmd_args[cmd_with_max][0]);
  if (cmd_size[cmd_with_max] == 2) {
    buffer.push_back(cmd_args[cmd_with_max][1]);
  }

  auto new_comp_piece = std::make_shared<CompressionPiece>(
      cmd_with_max, max_win, buffer, cmd_size[cmd_with_max]);
  PrintCompressionPiece(new_comp_piece);
  // If we let non compressed stuff, we need to add a copy chunk before
  if (comp_accumulator != 0) {
    std::string copy_buff;
    copy_buff.resize(comp_accumulator);
    for (int i = 0; i < comp_accumulator; ++i) {
      copy_buff[i] = rom_data[i + src_data_pos - comp_accumulator];
    }
    auto copy_chunk = std::make_shared<CompressionPiece>(
        kCommandDirectCopy, comp_accumulator, copy_buff, comp_accumulator);
    compressed_chain->next = copy_chunk;
    compressed_chain = copy_chunk;
  } else {
    compressed_chain->next = new_comp_piece;
    compressed_chain = new_comp_piece;
  }
  src_data_pos += max_win;
  comp_accumulator = 0;
}

absl::StatusOr<std::shared_ptr<CompressionPiece>> SplitCompressionPiece(
    std::shared_ptr<CompressionPiece>& piece, int mode) {
  std::shared_ptr<CompressionPiece> new_piece;
  uint length_left = piece->length - kMaxLengthCompression;
  piece->length = kMaxLengthCompression;

  switch (piece->command) {
    case kCommandByteFill:
    case kCommandWordFill:
      new_piece = std::make_shared<CompressionPiece>(
          piece->command, length_left, piece->argument, piece->argument_length);
      break;
    case kCommandIncreasingFill:
      new_piece = std::make_shared<CompressionPiece>(
          piece->command, length_left, piece->argument, piece->argument_length);
      new_piece->argument[0] =
          (char)(piece->argument[0] + kMaxLengthCompression);
      break;
    case kCommandDirectCopy:
      piece->argument_length = kMaxLengthCompression;
      new_piece = std::make_shared<CompressionPiece>(
          piece->command, length_left, nullptr, length_left);
      // MEMCPY
      for (int i = 0; i < length_left; ++i) {
        new_piece->argument[i] = piece->argument[i + kMaxLengthCompression];
      }
      break;
    case kCommandRepeatingBytes: {
      piece->argument_length = kMaxLengthCompression;
      uint offset = piece->argument[0] + (piece->argument[1] << 8);
      new_piece = std::make_shared<CompressionPiece>(
          piece->command, length_left, piece->argument, piece->argument_length);
      if (mode == kNintendoMode2) {
        new_piece->argument[0] =
            (offset + kMaxLengthCompression) & SNES_BYTE_MAX;
        new_piece->argument[1] = (offset + kMaxLengthCompression) >> 8;
      }
      if (mode == kNintendoMode1) {
        new_piece->argument[1] =
            (offset + kMaxLengthCompression) & SNES_BYTE_MAX;
        new_piece->argument[0] = (offset + kMaxLengthCompression) >> 8;
      }
    } break;
    default: {
      return absl::InvalidArgumentError(
          "SplitCompressionCommand: Invalid Command");
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
    if (piece->length <= kMaxLengthNormalHeader) {  // Normal header
      output.push_back(BUILD_HEADER(piece->command, piece->length));
      pos++;
    } else {
      if (piece->length <= kMaxLengthCompression) {
        output.push_back((COMPRESSION_STRING_MOD) |
                         ((uchar)piece->command << 2) |
                         (((piece->length - 1) & 0xFF00) >> 8));
        pos++;
        printf("Building extended header : cmd: %d, length: %d -  %02X\n",
               piece->command, piece->length, output[pos - 1]);
        output.push_back(((piece->length - 1) & 0x00FF));  // (char)
        pos++;
      } else {
        // We need to split the command
        auto new_piece = SplitCompressionPiece(piece, mode);
        if (!new_piece.ok()) {
          std::cout << new_piece.status().ToString() << std::endl;
        }
        printf("New added piece\n");
        auto piece_data = new_piece.value();
        PrintCompressionPiece(piece_data);
        piece_data->next = piece->next;
        piece->next = piece_data;
        continue;
      }
    }

    if (piece->command == kCommandRepeatingBytes) {
      char tmp[2];
      tmp[0] = piece->argument[0];
      tmp[1] = piece->argument[1];
      if (mode == kNintendoMode1) {
        tmp[0] = piece->argument[1];
        tmp[1] = piece->argument[0];
      }
      for (const auto& each : tmp) {
        output.push_back(each);
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

absl::Status ValidateCompressionResult(
    std::shared_ptr<CompressionPiece>& compressed_chain_start, int mode,
    int start, int src_data_pos) {
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
          (uint)(src_data_pos - start)));
    }
  }
  return absl::OkStatus();
}

// Merge consecutive copy if possible
std::shared_ptr<CompressionPiece> MergeCopy(
    std::shared_ptr<CompressionPiece>& start) {
  std::shared_ptr<CompressionPiece> piece = start;

  while (piece != nullptr) {
    if (piece->command == kCommandDirectCopy && piece->next != nullptr &&
        piece->next->command == kCommandDirectCopy &&
        piece->length + piece->next->length <= kMaxLengthCompression) {
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
    piece = piece->next;
  }
  return start;
}

}  // namespace

// TODO TEST compressed data border for each cmd
absl::StatusOr<Bytes> ROM::Compress(const int start, const int length, int mode,
                                    bool check) {
  // Worse case should be a copy of the string with extended header
  auto compressed_chain = std::make_shared<CompressionPiece>(1, 1, "aaa", 2);
  auto compressed_chain_start = compressed_chain;

  CommandArgumentArray cmd_args = {{}};
  DataSizeArray data_size_taken = {0, 0, 0, 0, 0};
  CommandSizeArray cmd_size = {0, 1, 2, 1, 2};

  uint src_data_pos = start;
  uint last_pos = start + length - 1;
  uint comp_accumulator = 0;  // Used when skipping using copy

  while (true) {
    data_size_taken.fill({});
    cmd_args.fill({{}});

    CheckByteRepeat(rom_data_.data(), data_size_taken, cmd_args, src_data_pos,
                    last_pos);
    CheckWordRepeat(rom_data_.data(), data_size_taken, cmd_args, src_data_pos,
                    last_pos);
    CheckIncByte(rom_data_.data(), data_size_taken, cmd_args, src_data_pos,
                 last_pos);
    CheckIntraCopy(rom_data_.data(), data_size_taken, cmd_args, src_data_pos,
                   last_pos, start);

    uint max_win = 2;
    uint cmd_with_max = kCommandDirectCopy;
    ValidateForByteGain(data_size_taken, cmd_size, max_win, cmd_with_max);

    if (cmd_with_max == kCommandDirectCopy) {
      // This is the worst case scenario
      // Progress through the next byte, in case there's a different
      // compression command we can implement before we hit 32 bytes.
      src_data_pos++;
      comp_accumulator++;

      // Arbitrary choice to do a 32 bytes grouping for copy.
      if (comp_accumulator == 32 || src_data_pos > last_pos) {
        std::string buffer;
        for (int i = 0; i < comp_accumulator; ++i) {
          buffer.push_back(rom_data_[i + src_data_pos - comp_accumulator]);
        }
        auto new_comp_piece = std::make_shared<CompressionPiece>(
            kCommandDirectCopy, comp_accumulator, buffer, comp_accumulator);
        compressed_chain->next = new_comp_piece;
        compressed_chain = new_comp_piece;
        comp_accumulator = 0;
      }
    } else {
      // Anything is better than directly copying bytes...
      CompressionCommandAlternative(rom_data_.data(), compressed_chain,
                                    cmd_size, cmd_args, src_data_pos,
                                    comp_accumulator, cmd_with_max, max_win);
    }

    if (src_data_pos > last_pos) {
      printf("Breaking compression loop\n");
      break;
    }

    if (check) {
      auto response = ValidateCompressionResult(compressed_chain_start, mode,
                                                start, src_data_pos);
      if (!response.ok()) {
        return response;
      }
    }
  }

  MergeCopy(compressed_chain_start->next);  // Skipping compression chain header
  PrintCompressionChain(compressed_chain_start);
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
  uchar command = 0;
  uchar header = rom_data_[offset];

  while (header != SNES_BYTE_MAX) {
    if ((header & CMD_EXPANDED_MOD) == CMD_EXPANDED_MOD) {
      // Expanded Command
      command = ((header >> 2) & CMD_MOD);
      length =
          (((header << 8) | rom_data_[offset + 1]) & CMD_EXPANDED_LENGTH_MOD);
      offset += 2;  // Advance 2 bytes in ROM
    } else {
      // Normal Command
      command = ((header >> 5) & CMD_MOD);
      length = (header & CMD_NORMAL_LENGTH_MOD);
      offset += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // each commands is at least of size 1 even if index 00

    switch (command) {
      case kCommandDirectCopy:  // Does not advance in the ROM
        memcpy(buffer.data() + buffer_pos, rom_data_.data() + offset, length);
        buffer_pos += length;
        offset += length;
        break;
      case kCommandByteFill:
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos] = rom_data_[offset];
          buffer_pos++;
        }
        offset += 1;  // Advances 1 byte in the ROM
        break;
      case kCommandWordFill: {
        auto a = rom_data_[offset];
        auto b = rom_data_[offset + 1];
        for (int i = 0; i < length; i = i + 2) {
          buffer[buffer_pos + i] = a;
          if ((i + 1) < length) buffer[buffer_pos + i + 1] = b;
        }
        buffer_pos += length;
        offset += 2;  // Advance 2 byte in the ROM
      } break;
      case kCommandIncreasingFill: {
        uchar inc_byte = rom_data_[offset];
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos] = inc_byte++;
          buffer_pos++;
        }
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((rom_data_[offset + 1] & SNES_BYTE_MAX) << 8);
        ushort s2 = ((rom_data_[offset] & SNES_BYTE_MAX));
        if (reversed) {  // Reversed byte order for overworld maps
          auto addr = (rom_data_[offset + 2]) | ((rom_data_[offset + 1]) << 8);
          if (addr > offset) {
            return absl::InternalError(absl::StrFormat(
                "DecompressOverworld: Offset for command copy exceeds "
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
      default: {
        std::cout << absl::StrFormat(
            "DecompressOverworld: Invalid command in header for "
            "decompression (Offset : %#06x, Command: %#04x)\n",
            offset, command);
      } break;
    }
    // check next byte
    header = rom_data_[offset];
  }

  return buffer;
}

absl::StatusOr<Bytes> ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, false);
}

absl::StatusOr<Bytes> ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, true);
}

// 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
// 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
// 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
// 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
// 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
absl::Status ROM::LoadAllGraphicsData() {
  Bytes sheet;
  bool convert = false;

  for (int i = 0; i < core::NumberOfSheets; i++) {
    if (i >= 115 && i <= 126) {  // uncompressed sheets
      sheet.resize(core::Uncompressed3BPPSize);
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        sheet[j] = rom_data_[j + offset];
      }
      convert = true;
    } else if (i == 113 || i == 114 || i >= 218) {
      convert = false;
    } else {
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      absl::StatusOr<Bytes> new_sheet = Decompress(offset);
      if (!new_sheet.ok()) {
        return new_sheet.status();
      } else {
        sheet = std::move(*new_sheet);
      }
      convert = true;
    }

    if (convert) {
      auto converted_sheet = SNES3bppTo8bppSheet(sheet);
      graphics_bin_[i] =
          gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                      core::kTilesheetDepth, converted_sheet.data());
      graphics_bin_.at(i).CreateTexture(renderer_);
    }
  }
  return absl::OkStatus();
}

absl::Status ROM::LoadFromFile(const absl::string_view& filename) {
  filename_ = filename;
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

  // copy ROM title
  memcpy(title, rom_data_.data() + kTitleStringOffset, kTitleStringLength);

  file.close();
  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status ROM::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

absl::Status ROM::LoadFromBytes(const Bytes& data) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  return absl::OkStatus();
}

absl::Status ROM::SaveToFile() {
  std::fstream file(filename_.data(), std::ios::binary | std::ios::out);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }
  for (auto i = 0; i < size_; ++i) {
    file << rom_data_[i];
  }
  return absl::OkStatus();
}

}  // namespace app
}  // namespace yaze