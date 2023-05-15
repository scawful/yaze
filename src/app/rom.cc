#include "rom.h"

#include <SDL.h>
#include <asar/src/asar/interface-lib.h>

#include <cstddef>
#include <cstdio>
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

namespace lc_lz2 {

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
        cmd_args[kCommandRepeatingBytes][0] = search_start & kSnesByteMax;
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
            (offset + kMaxLengthCompression) & kSnesByteMax;
        new_piece->argument[1] = (offset + kMaxLengthCompression) >> 8;
      }
      if (mode == kNintendoMode1) {
        new_piece->argument[1] =
            (offset + kMaxLengthCompression) & kSnesByteMax;
        new_piece->argument[0] = (offset + kMaxLengthCompression) >> 8;
      }
    } break;
    default: {
      return absl::InvalidArgumentError(
          "SplitCompressionCommand: Invalid Command");
    }
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
        output.push_back(kCompressionStringMod | ((uchar)piece->command << 2) |
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
  output.push_back(kSnesByteMax);
  return output;
}

absl::Status ValidateCompressionResult(
    CompressionPiecePointer& compressed_chain_start, int mode, int start,
    int src_data_pos) {
  if (compressed_chain_start->next != nullptr) {
    ROM temp_rom;
    RETURN_IF_ERROR(temp_rom.LoadFromBytes(
        CreateCompressionString(compressed_chain_start->next, mode)))
    ASSIGN_OR_RETURN(auto decomp_data, temp_rom.Decompress(0, temp_rom.size()))
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
CompressionPiecePointer MergeCopy(CompressionPiecePointer& start) {
  CompressionPiecePointer piece = start;

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

}  // namespace lc_lz2

namespace {

int GetGraphicsAddress(const uchar* data, uint8_t offset) {
  auto part_one = data[kOverworldGraphicsPos1 + offset] << 16;
  auto part_two = data[kOverworldGraphicsPos2 + offset] << 8;
  auto part_three = data[kOverworldGraphicsPos3 + offset];
  auto snes_addr = (part_one | part_two | part_three);
  return core::SnesToPc(snes_addr);
}

Bytes SnesTo8bppSheet(Bytes sheet, int bpp) {
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;
  int num_tiles = 64;
  int buffer_size = 0x1000;
  if (bpp == 2) {
    bpp = 16;
    num_tiles = 128;
    buffer_size = 0x2000;
  } else if (bpp == 3) {
    bpp = 24;
  }
  Bytes sheet_buffer_out(buffer_size);

  for (int i = 0; i < num_tiles; i++) {  // for each tiles, 16 per line
    for (int y = 0; y < 8; y++) {        // for each line
      for (int x = 0; x < 8; x++) {      //[0] + [1] + [16]
        auto b1 = (sheet[(y * 2) + (bpp * pos)] & (kGraphicsBitmap[x]));
        auto b2 = (sheet[((y * 2) + (bpp * pos)) + 1] & (kGraphicsBitmap[x]));
        auto b3 = (sheet[(16 + y) + (bpp * pos)] & (kGraphicsBitmap[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0 && bpp != 16) {
          b |= 4;
        }
        sheet_buffer_out[x + xx + (y * 128) + (yy * 1024)] = b;
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

    lc_lz2::CheckByteRepeat(rom_data_.data(), data_size_taken, cmd_args,
                            src_data_pos, last_pos);
    lc_lz2::CheckWordRepeat(rom_data_.data(), data_size_taken, cmd_args,
                            src_data_pos, last_pos);
    lc_lz2::CheckIncByte(rom_data_.data(), data_size_taken, cmd_args,
                         src_data_pos, last_pos);
    lc_lz2::CheckIntraCopy(rom_data_.data(), data_size_taken, cmd_args,
                           src_data_pos, last_pos, start);

    uint max_win = 2;
    uint cmd_with_max = kCommandDirectCopy;
    lc_lz2::ValidateForByteGain(data_size_taken, cmd_size, max_win,
                                cmd_with_max);

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
      lc_lz2::CompressionCommandAlternative(
          rom_data_.data(), compressed_chain, cmd_size, cmd_args, src_data_pos,
          comp_accumulator, cmd_with_max, max_win);
    }

    if (src_data_pos > last_pos) {
      printf("Breaking compression loop\n");
      break;
    }

    if (check) {
      RETURN_IF_ERROR(lc_lz2::ValidateCompressionResult(
          compressed_chain_start, mode, start, src_data_pos))
    }
  }

  // Skipping compression chain header
  lc_lz2::MergeCopy(compressed_chain_start->next);
  lc_lz2::PrintCompressionChain(compressed_chain_start);
  return lc_lz2::CreateCompressionString(compressed_chain_start->next, mode);
}

absl::StatusOr<Bytes> ROM::CompressGraphics(const int pos, const int length) {
  return Compress(pos, length, kNintendoMode2);
}

absl::StatusOr<Bytes> ROM::CompressOverworld(const int pos, const int length) {
  return Compress(pos, length, kNintendoMode1);
}

// ============================================================================

absl::StatusOr<Bytes> ROM::Decompress(int offset, int size, int mode) {
  Bytes buffer(size, 0);
  uint length = 0;
  uint buffer_pos = 0;
  uchar command = 0;
  uchar header = rom_data_[offset];

  while (header != kSnesByteMax) {
    if ((header & kExpandedMod) == kExpandedMod) {
      // Expanded Command
      command = ((header >> 2) & kCommandMod);
      length = (((header << 8) | rom_data_[offset + 1]) & kExpandedLengthMod);
      offset += 2;  // Advance 2 bytes in ROM
    } else {
      // Normal Command
      command = ((header >> 5) & kCommandMod);
      length = (header & kNormalLengthMod);
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
        memset(buffer.data() + buffer_pos, (int)(rom_data_[offset]), length);
        buffer_pos += length;
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
        auto inc_byte = rom_data_[offset];
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos] = inc_byte++;
          buffer_pos++;
        }
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((rom_data_[offset + 1] & kSnesByteMax) << 8);
        ushort s2 = ((rom_data_[offset] & kSnesByteMax));
        int addr = (s1 | s2);
        if (mode == kNintendoMode1) {  // Reversed byte order for overworld maps
          addr = (rom_data_[offset + 1] & kSnesByteMax) |
                 ((rom_data_[offset] & kSnesByteMax) << 8);
        }
        if (addr > offset) {
          return absl::InternalError(absl::StrFormat(
              "Decompress: Offset for command copy exceeds current position "
              "(Offset : %#04x | Pos : %#06x)\n",
              addr, offset));
        }
        if (buffer_pos + length >= size) {
          size *= 2;
          buffer.resize(size);
        }
        memcpy(buffer.data() + buffer_pos, buffer.data() + addr, length);
        buffer_pos += length;
        offset += 2;
      } break;
      default: {
        std::cout << absl::StrFormat(
            "Decompress: Invalid header (Offset : %#06x, Command: %#04x)\n",
            offset, command);
      } break;
    }
    // check next byte
    header = rom_data_[offset];
  }

  return buffer;
}

absl::StatusOr<Bytes> ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, kNintendoMode2);
}

absl::StatusOr<Bytes> ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, kNintendoMode1);
}

// ============================================================================

absl::StatusOr<Bytes> ROM::Load2bppGraphics() {
  Bytes sheet;
  const uint8_t sheets[] = {113, 114, 218, 219, 220, 221};

  for (const auto& sheet_id : sheets) {
    auto offset = GetGraphicsAddress(rom_data_.data(), sheet_id);
    ASSIGN_OR_RETURN(auto decomp_sheet, Decompress(offset))
    auto converted_sheet = SnesTo8bppSheet(decomp_sheet, 2);
    for (const auto& each_pixel : converted_sheet) {
      sheet.push_back(each_pixel);
    }
  }
  return sheet;
}

// ============================================================================

// 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
// 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
// 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
// 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
// 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
absl::Status ROM::LoadAllGraphicsData() {
  Bytes sheet;
  bool bpp3 = false;

  for (int i = 0; i < core::NumberOfSheets; i++) {
    if (i >= 115 && i <= 126) {  // uncompressed sheets
      sheet.resize(core::Uncompressed3BPPSize);
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        sheet[j] = rom_data_[j + offset];
      }
      bpp3 = true;
    } else if (i == 113 || i == 114 || i >= 218) {
      bpp3 = false;
    } else {
      auto offset = GetGraphicsAddress(rom_data_.data(), i);
      ASSIGN_OR_RETURN(sheet, Decompress(offset))
      bpp3 = true;
    }

    if (bpp3) {
      auto converted_sheet = SnesTo8bppSheet(sheet, 3);
      graphics_bin_[i] =
          gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                      core::kTilesheetDepth, converted_sheet.data(), 0x1000);
      graphics_bin_.at(i).CreateTexture(renderer_);

      for (int j = 0; j < graphics_bin_.at(i).GetSize(); ++j) {
        graphics_buffer_.push_back(graphics_bin_.at(i).GetByte(j));
      }
    } else {
      for (int j = 0; j < graphics_bin_.at(0).GetSize(); ++j) {
        graphics_buffer_.push_back(0xFF);
      }
    }
  }
  return absl::OkStatus();
}

// ============================================================================

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
  LoadAllPalettes();
  is_loaded_ = true;
  return absl::OkStatus();
}

// ============================================================================

absl::Status ROM::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

// ============================================================================

absl::Status ROM::LoadFromBytes(const Bytes& data) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  return absl::OkStatus();
}

// ============================================================================

void ROM::LoadAllPalettes() {
  // 35 colors each, 7x5 (0,2 on grid)
  for (int i = 0; i < 6; i++) {
    palette_groups_["ow_main"].AddPalette(
        ReadPalette(core::overworldPaletteMain + (i * (35 * 2)), 35));
  }
  // 21 colors each, 7x3 (8,2 and 8,5 on grid)
  for (int i = 0; i < 20; i++) {
    palette_groups_["ow_aux"].AddPalette(
        ReadPalette(core::overworldPaletteAuxialiary + (i * (21 * 2)), 21));
  }
  // 7 colors each 7x1 (0,7 on grid)
  for (int i = 0; i < 14; i++) {
    palette_groups_["ow_animated"].AddPalette(
        ReadPalette(core::overworldPaletteAnimated + (i * (7 * 2)), 7));
  }
  // 32 colors each 16x2 (0,0 on grid)
  for (int i = 0; i < 2; i++) {
    palette_groups_["hud"].AddPalette(
        ReadPalette(core::hudPalettes + (i * 64), 32));
  }

  palette_groups_["global_sprites"].AddPalette(
      ReadPalette(core::globalSpritePalettesLW, 60));
  palette_groups_["global_sprites"].AddPalette(
      ReadPalette(core::globalSpritePalettesDW, 60));

  for (int i = 0; i < 5; i++) {
    palette_groups_["armors"].AddPalette(
        ReadPalette(core::armorPalettes + (i * 30), 15));
  }
  for (int i = 0; i < 4; i++) {
    palette_groups_["swords"].AddPalette(
        ReadPalette(core::swordPalettes + (i * 6), 3));
  }
  for (int i = 0; i < 3; i++) {
    palette_groups_["shields"].AddPalette(
        ReadPalette(core::shieldPalettes + (i * 8), 4));
  }
  for (int i = 0; i < 12; i++) {
    palette_groups_["sprites_aux1"].AddPalette(
        ReadPalette(core::spritePalettesAux1 + (i * 14), 7));
  }
  for (int i = 0; i < 11; i++) {
    palette_groups_["sprites_aux2"].AddPalette(
        ReadPalette(core::spritePalettesAux2 + (i * 14), 7));
  }
  for (int i = 0; i < 24; i++) {
    palette_groups_["sprites_aux3"].AddPalette(
        ReadPalette(core::spritePalettesAux3 + (i * 14), 7));
  }
  for (int i = 0; i < 20; i++) {
    palette_groups_["dungeon_main"].AddPalette(
        ReadPalette(core::dungeonMainPalettes + (i * 180), 90));
  }

  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassLW));
  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassDW));
  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassSpecial));

  palette_groups_["3d_object"].AddPalette(
      ReadPalette(core::triforcePalette, 8));
  palette_groups_["3d_object"].AddPalette(ReadPalette(core::crystalPalette, 8));

  for (int i = 0; i < 2; i++) {
    palette_groups_["ow_mini_map"].AddPalette(
        ReadPalette(core::overworldMiniMapPalettes + (i * 256), 128));
  }
}

// ============================================================================

void ROM::SaveAllPalettes() {
  // Iterate through all palette_groups_
  for (auto& [groupName, palettes] : palette_groups_) {
    // Iterate through all palettes in the group
    for (size_t i = 0; i < palettes.size(); ++i) {
      auto palette = palettes[i];

      // Iterate through all colors in the palette
      for (size_t j = 0; j < palette.size(); ++j) {
        gfx::SNESColor color = palette[j];

        // If the color is modified, save the color to the ROM
        if (color.modified) {
          WriteColor(GetPaletteAddress(groupName, i, j), color);
          color.modified = false;  // Reset the modified flag after saving
        }
      }
    }
  }
}

// ============================================================================

absl::Status ROM::SaveToFile(bool backup) {
  // Check if backup is enabled
  if (backup) {
    // Create a backup file with timestamp in its name
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string backup_filename =
        absl::StrCat(filename_, "_backup_", std::ctime(&now_c));

    // Remove newline character from ctime()
    backup_filename.erase(
        std::remove(backup_filename.begin(), backup_filename.end(), '\n'),
        backup_filename.end());

    // Replace spaces with underscores
    std::replace(backup_filename.begin(), backup_filename.end(), ' ', '_');

    // Now, copy the original file to the backup file
    std::filesystem::copy(filename_, backup_filename,
                          std::filesystem::copy_options::overwrite_existing);
  }

  // Run the other save functions
  SaveAllPalettes();

  std::fstream file(filename_.data(), std::ios::binary | std::ios::out);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }

  // Save the data to the file
  for (auto i = 0; i < size_; ++i) {
    file << rom_data_[i];
  }

  // Check for write errors
  if (!file.good()) {
    return absl::InternalError(
        absl::StrCat("Error while writing to ROM file: ", filename_));
  }

  return absl::OkStatus();
}

// ============================================================================

gfx::SNESColor ROM::ReadColor(int offset) {
  short color = toint16(offset);
  gfx::snes_color new_color;
  new_color.red = (color & 0x1F) * 8;
  new_color.green = ((color >> 5) & 0x1F) * 8;
  new_color.blue = ((color >> 10) & 0x1F) * 8;
  gfx::SNESColor snes_color(new_color);
  return snes_color;
}

// ============================================================================

gfx::SNESPalette ROM::ReadPalette(int offset, int num_colors) {
  int color_offset = 0;
  std::vector<gfx::SNESColor> colors(num_colors);

  while (color_offset < num_colors) {
    short color = toint16(offset);
    gfx::snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].setSNES(new_color);
    color_offset++;
    offset += 2;
  }

  gfx::SNESPalette palette(colors);
  return palette;
}

// ============================================================================

void ROM::Write(int addr, int value) { rom_data_[addr] = value; }

void ROM::WriteShort(int addr, int value) {
  rom_data_[addr] = (uchar)(value & 0xFF);
  rom_data_[addr + 1] = (uchar)((value >> 8) & 0xFF);
}

// ============================================================================

void ROM::WriteColor(uint32_t address, const gfx::SNESColor& color) {
  uint16_t bgr = ((color.snes >> 10) & 0x1F) | ((color.snes & 0x1F) << 10) |
                 (color.snes & 0x7C00);

  // Write the 16-bit color value to the ROM at the specified address
  rom_data_[address] = static_cast<uint8_t>(bgr & 0xFF);
  rom_data_[address + 1] = static_cast<uint8_t>((bgr >> 8) & 0xFF);
}

// ============================================================================

uint32_t ROM::GetPaletteAddress(const std::string& groupName,
                                size_t paletteIndex, size_t colorIndex) const {
  // Retrieve the base address for the palette group
  uint32_t baseAddress = paletteGroupBaseAddresses.at(groupName);

  // Retrieve the number of colors for each palette in the group
  size_t colorsPerPalette = paletteGroupColorCounts.at(groupName);

  // Calculate the address for the specified color in the ROM
  uint32_t address =
      baseAddress + (paletteIndex * colorsPerPalette * 2) + (colorIndex * 2);

  return address;
}

// ============================================================================

absl::Status ROM::ApplyAssembly(const absl::string_view& filename,
                                size_t patch_size) {
  int count = 0;
  auto patch = filename.data();
  auto data = (char*)rom_data_.data();
  if (int size = size_; !asar_patch(patch, data, patch_size, &size)) {
    auto asar_error = asar_geterrors(&count);
    auto full_error = asar_error->fullerrdata;
    return absl::InternalError(absl::StrCat("ASAR Error: ", full_error));
  }
  return absl::OkStatus();
}

// ============================================================================

// TODO(scawful): Test me!
absl::Status ROM::PatchOverworldMosaic(
    char mosaic_tiles[core::kNumOverworldMaps], int routine_offset) {
  // Write the data for the mosaic tile array used by the assembly code.
  for (int i = 0; i < core::kNumOverworldMaps; i++) {
    if (mosaic_tiles[i]) {
      rom_data_[core::overworldCustomMosaicArray + i] = 0x01;
    } else {
      rom_data_[core::overworldCustomMosaicArray + i] = 0x00;
    }
  }

  std::string filename = "assets/asm/mosaic_change.asm";
  std::fstream file(filename, std::ios::out | std::ios::in);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        "Unable to open mosaic change assembly source");
  }

  std::stringstream assembly;
  assembly << file.rdbuf();
  file.close();
  auto assembly_string = assembly.str();

  if (!core::StringReplace(assembly_string, "<HOOK>", kMosaicChangeOffset)) {
    return absl::InternalError(
        "Mosaic template did not have proper `<HOOK>` to replace.");
  }

  if (!core::StringReplace(
          assembly_string, "<EXPANDED_SPACE>",
          absl::StrFormat("$%x", routine_offset + kSNESToPCOffset))) {
    return absl::InternalError(
        "Mosaic template did not have proper `<EXPANDED_SPACE>` to replace.");
  }

  return ApplyAssembly(filename, assembly_string.size());
}

}  // namespace app
}  // namespace yaze