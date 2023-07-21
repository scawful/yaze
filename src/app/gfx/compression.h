#ifndef YAZE_APP_GFX_COMPRESSION_H
#define YAZE_APP_GFX_COMPRESSION_H

#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/constants.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze {
namespace app {
namespace gfx {

namespace lc_lz2 {

constexpr int kCommandDirectCopy = 0;
constexpr int kCommandByteFill = 1;
constexpr int kCommandWordFill = 2;
constexpr int kCommandIncreasingFill = 3;
constexpr int kCommandRepeatingBytes = 4;
constexpr int kCommandLongLength = 7;
constexpr int kMaxLengthNormalHeader = 32;
constexpr int kMaxLengthCompression = 1024;
constexpr int kNintendoMode1 = 0;
constexpr int kNintendoMode2 = 1;
constexpr int kSnesByteMax = 0xFF;
constexpr int kCommandMod = 0x07;
constexpr int kExpandedMod = 0xE0;
constexpr int kExpandedLengthMod = 0x3FF;
constexpr int kNormalLengthMod = 0x1F;
constexpr int kCompressionStringMod = 7 << 5;

using CommandArgumentArray = std::array<std::array<char, 2>, 5>;
using CommandSizeArray = std::array<uint, 5>;
using DataSizeArray = std::array<uint, 5>;
struct CompressionPiece {
  char command;
  int length;
  int argument_length;
  std::string argument;
  std::shared_ptr<CompressionPiece> next = nullptr;
  CompressionPiece() = default;
  CompressionPiece(int cmd, int len, std::string args, int arg_len)
      : command(cmd), length(len), argument_length(arg_len), argument(args) {}
};
using CompressionPiece = struct CompressionPiece;
using CompressionPiecePointer = std::shared_ptr<CompressionPiece>;

void PrintCompressionPiece(const CompressionPiecePointer& piece);

void PrintCompressionChain(const CompressionPiecePointer& chain_head);

void CheckByteRepeat(const uchar* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const uint last_pos);

void CheckWordRepeat(const uchar* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const uint last_pos);

void CheckIncByte(const uchar* rom_data, DataSizeArray& data_size_taken,
                  CommandArgumentArray& cmd_args, uint& src_data_pos,
                  const uint last_pos);

void CheckIntraCopy(const uchar* rom_data, DataSizeArray& data_size_taken,
                    CommandArgumentArray& cmd_args, uint& src_data_pos,
                    const uint last_pos, uint start);

void ValidateForByteGain(const DataSizeArray& data_size_taken,
                         const CommandSizeArray& cmd_size, uint& max_win,
                         uint& cmd_with_max);

void CompressionCommandAlternative(const uchar* rom_data,
                                   CompressionPiecePointer& compressed_chain,
                                   const CommandSizeArray& cmd_size,
                                   const CommandArgumentArray& cmd_args,
                                   uint& src_data_pos, uint& comp_accumulator,
                                   uint& cmd_with_max, uint& max_win);

absl::StatusOr<CompressionPiecePointer> SplitCompressionPiece(
    CompressionPiecePointer& piece, int mode);

Bytes CreateCompressionString(CompressionPiecePointer& start, int mode);

absl::Status ValidateCompressionResult(CompressionPiecePointer& chain_head,
                                       int mode, int start, int src_data_pos);

CompressionPiecePointer MergeCopy(CompressionPiecePointer& start);

}  // namespace lc_lz2

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_COMPRESSION_H