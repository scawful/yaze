#ifndef YAZE_APP_GFX_COMPRESSION_H
#define YAZE_APP_GFX_COMPRESSION_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/macro.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze {
namespace gfx {

std::vector<uint8_t> HyruleMagicCompress(uint8_t const* const src,
                                         int const oldsize, int* const size,
                                         int const flag);

std::vector<uint8_t> HyruleMagicDecompress(uint8_t const* src, int* const size,
                                           int const p_big_endian);

/**
 * @namespace yaze::gfx::lc_lz2
 * @brief Contains the LC_LZ2 compression algorithm.
 */
namespace lc_lz2 {

const int D_NINTENDO_C_MODE1 = 0;
const int D_NINTENDO_C_MODE2 = 1;

const int D_CMD_COPY = 0;
const int D_CMD_BYTE_REPEAT = 1;
const int D_CMD_WORD_REPEAT = 2;
const int D_CMD_BYTE_INC = 3;
const int D_CMD_COPY_EXISTING = 4;

const int D_MAX_NORMAL_LENGTH = 32;
const int D_MAX_LENGTH = 1024;

const int INITIAL_ALLOC_SIZE = 1024;

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

// Represents a command in the compression algorithm.
struct CompressionCommand {
  // The command arguments for each possible command.
  std::array<std::array<char, 2>, 5> arguments;

  // The size of each possible command.
  std::array<uint, 5> cmd_size;

  // The size of the data processed by each possible command.
  std::array<uint, 5> data_size;
};

using CommandArgumentArray = std::array<std::array<char, 2>, 5>;
using CommandSizeArray = std::array<uint, 5>;
using DataSizeArray = std::array<uint, 5>;

// Represents a piece of compressed data.
struct CompressionPiece {
  char command;
  int length;
  int argument_length;
  std::string argument;
  std::shared_ptr<CompressionPiece> next = nullptr;
  CompressionPiece() = default;
  CompressionPiece(int cmd, int len, std::string& args, int arg_len)
      : command(cmd), length(len), argument_length(arg_len), argument(args) {}
};
using CompressionPiece = struct CompressionPiece;
using CompressionPiecePointer = std::shared_ptr<CompressionPiece>;

void PrintCompressionPiece(const CompressionPiecePointer& piece);

void PrintCompressionChain(const CompressionPiecePointer& chain_head);

// Compression V1

void CheckByteRepeat(const uint8_t* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const unsigned int last_pos);

void CheckWordRepeat(const uint8_t* rom_data, DataSizeArray& data_size_taken,
                     CommandArgumentArray& cmd_args, uint& src_data_pos,
                     const unsigned int last_pos);

void CheckIncByte(const uint8_t* rom_data, DataSizeArray& data_size_taken,
                  CommandArgumentArray& cmd_args, uint& src_data_pos,
                  const unsigned int last_pos);

void CheckIntraCopy(const uint8_t* rom_data, DataSizeArray& data_size_taken,
                    CommandArgumentArray& cmd_args, uint& src_data_pos,
                    const unsigned int last_pos, unsigned int start);

void ValidateForByteGain(const DataSizeArray& data_size_taken,
                         const CommandSizeArray& cmd_size, uint& max_win,
                         uint& cmd_with_max);

void CompressionCommandAlternative(const uint8_t* rom_data,
                                   CompressionPiecePointer& compressed_chain,
                                   const CommandSizeArray& cmd_size,
                                   const CommandArgumentArray& cmd_args,
                                   uint& src_data_pos, uint& comp_accumulator,
                                   uint& cmd_with_max, uint& max_win);

// Compression V2

void CheckByteRepeatV2(const uint8_t* data, uint& src_pos, const unsigned int last_pos,
                       CompressionCommand& cmd);

void CheckWordRepeatV2(const uint8_t* data, uint& src_pos, const unsigned int last_pos,
                       CompressionCommand& cmd);

void CheckIncByteV2(const uint8_t* data, uint& src_pos, const unsigned int last_pos,
                    CompressionCommand& cmd);

void CheckIntraCopyV2(const uint8_t* data, uint& src_pos, const unsigned int last_pos,
                      unsigned int start, CompressionCommand& cmd);

void ValidateForByteGainV2(const CompressionCommand& cmd, uint& max_win,
                           uint& cmd_with_max);

void CompressionCommandAlternativeV2(const uint8_t* data,
                                     const CompressionCommand& cmd,
                                     CompressionPiecePointer& compressed_chain,
                                     uint& src_pos, uint& comp_accumulator,
                                     uint& cmd_with_max, uint& max_win);

/**
 * @brief Compresses a buffer of data using the LC_LZ2 algorithm.
 * \deprecated Use HyruleMagicDecompress instead.
 */
absl::StatusOr<std::vector<uint8_t>> CompressV2(const uint8_t* data,
                                                const int start,
                                                const int length, int mode = 1,
                                                bool check = false);

absl::StatusOr<std::vector<uint8_t>> CompressGraphics(const uint8_t* data,
                                                      const int pos,
                                                      const int length);
absl::StatusOr<std::vector<uint8_t>> CompressOverworld(const uint8_t* data,
                                                       const int pos,
                                                       const int length);
absl::StatusOr<std::vector<uint8_t>> CompressOverworld(
    const std::vector<uint8_t> data, const int pos, const int length);

absl::StatusOr<CompressionPiecePointer> SplitCompressionPiece(
    CompressionPiecePointer& piece, int mode);

std::vector<uint8_t> CreateCompressionString(CompressionPiecePointer& start,
                                             int mode);

absl::Status ValidateCompressionResult(CompressionPiecePointer& chain_head,
                                       int mode, int start, int src_data_pos);

CompressionPiecePointer MergeCopy(CompressionPiecePointer& start);

struct CompressionContext {
  std::vector<uint8_t> data;
  std::vector<uint8_t> compressed_data;
  std::vector<CompressionPiece> compression_pieces;
  std::vector<uint8_t> compression_string;
  unsigned int src_pos;
  unsigned int last_pos;
  unsigned int start;
  unsigned int comp_accumulator = 0;
  unsigned int cmd_with_max = kCommandDirectCopy;
  unsigned int max_win = 0;
  CompressionCommand current_cmd = {};
  int mode;

  // Constructor to initialize the context
  CompressionContext(const std::vector<uint8_t>& data_, const int start,
                     const int length)
      : data(data_), src_pos(start), last_pos(start + length - 1), mode(0) {}

  // Constructor to initialize the context
  CompressionContext(const std::vector<uint8_t>& data_, const int start,
                     const int length, int mode_)
      : data(data_),
        src_pos(start),
        last_pos(start + length - 1),
        mode(mode_) {}
};

void CheckByteRepeatV3(CompressionContext& context);
void CheckWordRepeatV3(CompressionContext& context);
void CheckIncByteV3(CompressionContext& context);
void CheckIntraCopyV3(CompressionContext& context);

void InitializeCompression(CompressionContext& context);
void CheckAvailableCompressionCommands(CompressionContext& context);
void DetermineBestCompression(CompressionContext& context);
void HandleDirectCopy(CompressionContext& context);
void AddCompressionToChain(CompressionContext& context);
absl::Status ValidateCompressionResultV3(const CompressionContext& context);

absl::StatusOr<CompressionPiece> SplitCompressionPieceV3(
    CompressionPiece& piece, int mode);
void FinalizeCompression(CompressionContext& context);

/**
 * @brief Compresses a buffer of data using the LC_LZ2 algorithm.
 * \deprecated Use HyruleMagicCompress
 */
absl::StatusOr<std::vector<uint8_t>> CompressV3(
    const std::vector<uint8_t>& data, const int start, const int length,
    int mode = 1, bool check = false);

std::string SetBuffer(const std::vector<uint8_t>& data, int src_pos,
                      int comp_accumulator);
std::string SetBuffer(const uint8_t* data, int src_pos, int comp_accumulator);
void memfill(const uint8_t* data, std::vector<uint8_t>& buffer, int buffer_pos,
             int offset, int length);

/**
 * @brief Decompresses a buffer of data using the LC_LZ2 algorithm.
 * @note Works well for graphics but not overworld data. Prefer Hyrule Magic
 * routines for overworld data.
 */
absl::StatusOr<std::vector<uint8_t>> DecompressV2(const uint8_t* data,
                                                  int offset, int size = 0x800,
                                                  int mode = 1);
absl::StatusOr<std::vector<uint8_t>> DecompressGraphics(const uint8_t* data,
                                                        int pos, int size);
absl::StatusOr<std::vector<uint8_t>> DecompressOverworld(const uint8_t* data,
                                                         int pos, int size);
absl::StatusOr<std::vector<uint8_t>> DecompressOverworld(
    const std::vector<uint8_t> data, int pos, int size);

}  // namespace lc_lz2
}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_COMPRESSION_H
