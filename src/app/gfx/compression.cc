#include "compression.h"

#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gfx {

namespace lc_lz2 {

void PrintCompressionPiece(const CompressionPiecePointer& piece) {
  std::cout << "Command: " << std::to_string(piece->command) << "\n";
  std::cout << "Command Length: " << piece->length << "\n";
  std::cout << "Argument: ";
  auto arg_size = piece->argument.size();
  for (int i = 0; i < arg_size; ++i) {
    printf("%02X ", piece->argument.at(i));
  }
  std::cout << "\nArgument Length: " << piece->argument_length << "\n";
}

void PrintCompressionChain(const CompressionPiecePointer& chain_head) {
  auto compressed_chain = chain_head->next;
  while (compressed_chain != nullptr) {
    std::cout << "- Compression Piece -\n";
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

void CheckByteRepeatV2(const uchar* data, uint& src_pos, const uint last_pos,
                       CompressionCommand& cmd) {
  uint i = 0;
  while (src_pos + i < last_pos && data[src_pos] == data[src_pos + i]) {
    ++i;
  }
  cmd.data_size[kCommandByteFill] = i;
  cmd.arguments[kCommandByteFill][0] = data[src_pos];
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

void CheckWordRepeatV2(const uchar* data, uint& src_pos, const uint last_pos,
                       CompressionCommand& cmd) {
  if (src_pos + 2 <= last_pos && data[src_pos] != data[src_pos + 1]) {
    uint pos = src_pos;
    char byte1 = data[pos];
    char byte2 = data[pos + 1];
    pos += 2;
    cmd.data_size[kCommandWordFill] = 2;
    while (pos + 1 <= last_pos) {
      if (data[pos] == byte1 && data[pos + 1] == byte2)
        cmd.data_size[kCommandWordFill] += 2;
      else
        break;
      pos += 2;
    }
    cmd.arguments[kCommandWordFill][0] = byte1;
    cmd.arguments[kCommandWordFill][1] = byte2;
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

void CheckIncByteV2(const uchar* rom_data, uint& src_data_pos,
                    const uint last_pos, CompressionCommand& cmd) {
  uint pos = src_data_pos;
  char byte = rom_data[pos];
  pos++;
  cmd.data_size[kCommandIncreasingFill] = 1;
  byte++;
  while (pos <= last_pos && byte == rom_data[pos]) {
    cmd.data_size[kCommandIncreasingFill]++;
    byte++;
    pos++;
  }
  cmd.arguments[kCommandIncreasingFill][0] = rom_data[src_data_pos];
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

void CheckIntraCopyV2(const uchar* rom_data, uint& src_data_pos,
                      const uint last_pos, uint start,
                      CompressionCommand& cmd) {
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
      if (copied_size > cmd.data_size[kCommandRepeatingBytes]) {
        search_start -= start;
        printf("- Found repeat of %d at %d\n", copied_size, search_start);
        cmd.data_size[kCommandRepeatingBytes] = copied_size;
        cmd.arguments[kCommandRepeatingBytes][0] = search_start & kSnesByteMax;
        cmd.arguments[kCommandRepeatingBytes][1] = search_start >> 8;
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

// Table indicating command sizes, in bytes
const std::array<int, 5> kCommandSizes = {1, 2, 2, 2, 3};

// TODO(@scawful): TEST ME
void ValidateForByteGainV2(const CompressionCommand& cmd, uint& max_win,
                           uint& cmd_with_max) {
  for (uint cmd_i = 1; cmd_i < 5; cmd_i++) {
    uint cmd_size_taken = cmd.data_size[cmd_i];
    // Check if the command size exceeds the maximum win and the size in the
    // command sizes table, except for the repeating bytes command when the size
    // taken is 3
    if (cmd_size_taken > max_win && cmd_size_taken > kCommandSizes[cmd_i] &&
        !(cmd_i == kCommandRepeatingBytes && cmd_size_taken == 3)) {
      printf("==> C:%d / S:%d\n", cmd_i, cmd_size_taken);
      cmd_with_max = cmd_i;
      max_win = cmd_size_taken;
    }
  }
}

void CompressionCommandAlternative(const uchar* rom_data,
                                   CompressionPiecePointer& compressed_chain,
                                   const CommandSizeArray& cmd_size,
                                   const CommandArgumentArray& cmd_args,
                                   uint& src_data_pos, uint& comp_accumulator,
                                   uint& cmd_with_max, uint& max_win) {
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

void CompressionCommandAlternativeV2(const uchar* rom_data,
                                     const CompressionCommand& cmd,
                                     CompressionPiecePointer& compressed_chain,
                                     uint& src_data_pos, uint& comp_accumulator,
                                     uint& cmd_with_max, uint& max_win) {
  printf("- Ok we get a gain from %d\n", cmd_with_max);
  std::string buffer;
  buffer.push_back(cmd.arguments[cmd_with_max][0]);
  if (cmd.cmd_size[cmd_with_max] == 2) {
    buffer.push_back(cmd.arguments[cmd_with_max][1]);
  }

  auto new_comp_piece = std::make_shared<CompressionPiece>(
      cmd_with_max, max_win, buffer, cmd.cmd_size[cmd_with_max]);
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

void AddAlternativeCompressionCommand(
    const uchar* rom_data, CompressionPiecePointer& compressed_chain,
    const CompressionCommand& command, uint& source_data_position,
    uint& uncompressed_data_size, uint& best_command, uint& best_command_gain) {
  std::cout << "- Identified a gain from command: " << best_command
            << std::endl;

  // Create a buffer to store the arguments for the best command.
  std::string argument_buffer;
  argument_buffer.push_back(command.arguments[best_command][0]);
  if (command.cmd_size[best_command] == 2) {
    argument_buffer.push_back(command.arguments[best_command][1]);
  }

  // Create a new compression piece for the best command.
  auto new_compression_piece = std::make_shared<CompressionPiece>(
      best_command, best_command_gain, argument_buffer,
      command.cmd_size[best_command]);
  PrintCompressionPiece(new_compression_piece);

  // If there is uncompressed data, create a direct copy compression piece for
  // it.
  if (uncompressed_data_size != 0) {
    std::string copy_buffer(uncompressed_data_size, 0);
    for (int i = 0; i < uncompressed_data_size; ++i) {
      copy_buffer[i] =
          rom_data[i + source_data_position - uncompressed_data_size];
    }
    auto direct_copy_piece = std::make_shared<CompressionPiece>(
        kCommandDirectCopy, uncompressed_data_size, copy_buffer,
        uncompressed_data_size);

    // Append the direct copy piece to the chain.
    compressed_chain->next = direct_copy_piece;
    compressed_chain = direct_copy_piece;
  }

  // Append the new compression piece to the chain.
  compressed_chain->next = new_compression_piece;
  compressed_chain = new_compression_piece;

  // Update the position in the source data and reset the uncompressed data
  // size.
  source_data_position += best_command_gain;
  uncompressed_data_size = 0;
}

absl::StatusOr<CompressionPiecePointer> SplitCompressionPiece(
    CompressionPiecePointer& piece, int mode) {
  CompressionPiecePointer new_piece;
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

Bytes CreateCompressionString(CompressionPiecePointer& start, int mode) {
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

absl::Status ValidateCompressionResult(CompressionPiecePointer& chain_head,
                                       int mode, int start, int src_data_pos) {
  if (chain_head->next != nullptr) {
    ROM temp_rom;
    RETURN_IF_ERROR(
        temp_rom.LoadFromBytes(CreateCompressionString(chain_head->next, mode)))
    ASSIGN_OR_RETURN(auto decomp_data,
                     DecompressV2(temp_rom.data(), 0, temp_rom.size()))
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

// TODO TEST compressed data border for each cmd
absl::StatusOr<Bytes> CompressV2(const uchar* data, const int start,
                                 const int length, int mode, bool check) {
  // Surely there's no need to compress zero...
  if (length == 0) {
    return Bytes();
  }

  // Worst case should be a copy of the string with extended header
  auto compressed_chain = std::make_shared<CompressionPiece>(1, 1, "aaa", 2);
  auto compressed_chain_start = compressed_chain;

  CompressionCommand current_cmd = {/*argument*/ {{}},
                                    /*cmd_size*/ {0, 1, 2, 1, 2},
                                    /*data_size*/ {0, 0, 0, 0, 0}};

  uint src_pos = start;
  uint last_pos = start + length - 1;
  uint comp_accumulator = 0;  // Used when skipping using copy

  while (true) {
    current_cmd.data_size.fill({});
    current_cmd.arguments.fill({{}});

    CheckByteRepeatV2(data, src_pos, last_pos, current_cmd);
    CheckWordRepeatV2(data, src_pos, last_pos, current_cmd);
    CheckIncByteV2(data, src_pos, last_pos, current_cmd);
    CheckIntraCopyV2(data, src_pos, last_pos, start, current_cmd);

    uint max_win = 2;
    uint cmd_with_max = kCommandDirectCopy;
    ValidateForByteGain(current_cmd.data_size, current_cmd.cmd_size, max_win,
                        cmd_with_max);
    // ValidateForByteGainV2(current_cmd, max_win, cmd_with_max);

    if (cmd_with_max == kCommandDirectCopy) {
      // This is the worst case scenario
      // Progress through the next byte, in case there's a different
      // compression command we can implement before we hit 32 bytes.
      src_pos++;
      comp_accumulator++;

      // Arbitrary choice to do a 32 bytes grouping for copy.
      if (comp_accumulator == 32 || src_pos > last_pos) {
        std::string buffer = SetBuffer(data, src_pos, comp_accumulator);
        auto new_comp_piece = std::make_shared<CompressionPiece>(
            kCommandDirectCopy, comp_accumulator, buffer, comp_accumulator);
        compressed_chain->next = new_comp_piece;
        comp_accumulator = 0;
      }
    } else {
      AddAlternativeCompressionCommand(data, compressed_chain, current_cmd,
                                       src_pos, comp_accumulator, cmd_with_max,
                                       max_win);
    }

    if (src_pos > last_pos) {
      printf("Breaking compression loop\n");
      break;
    }

    if (check) {
      RETURN_IF_ERROR(ValidateCompressionResult(compressed_chain_start, mode,
                                                start, src_pos))
    }
  }

  // Skipping compression chain header
  MergeCopy(compressed_chain_start->next);
  PrintCompressionChain(compressed_chain_start);
  return CreateCompressionString(compressed_chain_start->next, mode);
}

absl::StatusOr<Bytes> CompressGraphics(const uchar* data, const int pos,
                                       const int length) {
  return CompressV2(data, pos, length, kNintendoMode2);
}

absl::StatusOr<Bytes> CompressOverworld(const uchar* data, const int pos,
                                        const int length) {
  return CompressV2(data, pos, length, kNintendoMode1);
}

std::string SetBuffer(const uchar* data, int src_pos, int comp_accumulator) {
  std::string buffer;
  for (int i = 0; i < comp_accumulator; ++i) {
    buffer.push_back(data[i + src_pos - comp_accumulator]);
  }
  return buffer;
}

void memfill(const uchar* data, Bytes& buffer, int buffer_pos, int offset,
             int length) {
  auto a = data[offset];
  auto b = data[offset + 1];
  for (int i = 0; i < length; i = i + 2) {
    buffer[buffer_pos + i] = a;
    if ((i + 1) < length) buffer[buffer_pos + i + 1] = b;
  }
}

absl::StatusOr<Bytes> DecompressV2(const uchar* data, int offset, int size,
                                   int mode) {
  if (size == 0) {
    return Bytes();
  }

  Bytes buffer(size, 0);
  uint length = 0;
  uint buffer_pos = 0;
  uchar command = 0;
  uchar header = data[offset];

  while (header != kSnesByteMax) {
    if ((header & kExpandedMod) == kExpandedMod) {
      // Expanded Command
      command = ((header >> 2) & kCommandMod);
      length = (((header << 8) | data[offset + 1]) & kExpandedLengthMod);
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
        memcpy(buffer.data() + buffer_pos, data + offset, length);
        buffer_pos += length;
        offset += length;
        break;
      case kCommandByteFill:
        memset(buffer.data() + buffer_pos, (int)(data[offset]), length);
        buffer_pos += length;
        offset += 1;  // Advances 1 byte in the ROM
        break;
      case kCommandWordFill:
        memfill(data, buffer, buffer_pos, offset, length);
        buffer_pos += length;
        offset += 2;  // Advance 2 byte in the ROM
        break;
      case kCommandIncreasingFill: {
        auto inc_byte = data[offset];
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos] = inc_byte++;
          buffer_pos++;
        }
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case kCommandRepeatingBytes: {
        ushort s1 = ((data[offset + 1] & kSnesByteMax) << 8);
        ushort s2 = (data[offset] & kSnesByteMax);
        int addr = (s1 | s2);
        if (mode == kNintendoMode1) {  // Reversed byte order for
                                       // overworld maps
          addr = (data[offset + 1] & kSnesByteMax) |
                 ((data[offset] & kSnesByteMax) << 8);
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
    header = data[offset];
  }

  return buffer;
}

absl::StatusOr<Bytes> DecompressGraphics(const uchar* data, int pos, int size) {
  return DecompressV2(data, pos, size, kNintendoMode2);
}

absl::StatusOr<Bytes> DecompressOverworld(const uchar* data, int pos,
                                          int size) {
  return DecompressV2(data, pos, size, kNintendoMode1);
}

}  // namespace lc_lz2
}  // namespace gfx
}  // namespace app
}  // namespace yaze