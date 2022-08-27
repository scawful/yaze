#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze {
namespace app {

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
constexpr int kTile32Num = 4432;
constexpr int kTitleStringOffset = 0x7FC0;
constexpr int kTitleStringLength = 20;
constexpr int kOverworldGraphicsPos1 = 0x4F80;
constexpr int kOverworldGraphicsPos2 = 0x505F;
constexpr int kOverworldGraphicsPos3 = 0x513E;
constexpr int kSnesByteMax = 0xFF;
constexpr int kCommandMod = 0x07;
constexpr int kExpandedMod = 0xE0;
constexpr int kExpandedLengthMod = 0x3FF;
constexpr int kNormalLengthMod = 0x1F;

constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

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

class ROM {
 public:
  absl::StatusOr<Bytes> Compress(const int start, const int length,
                                 int mode = 1, bool check = false);
  absl::StatusOr<Bytes> CompressGraphics(const int pos, const int length);
  absl::StatusOr<Bytes> CompressOverworld(const int pos, const int length);

  absl::StatusOr<Bytes> Decompress(int offset, int size = 0x800, int mode = 1);
  absl::StatusOr<Bytes> DecompressGraphics(int pos, int size);
  absl::StatusOr<Bytes> DecompressOverworld(int pos, int size);

  absl::Status LoadAllGraphicsData();
  absl::Status LoadFromFile(const absl::string_view& filename);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadFromBytes(const Bytes& data);

  absl::Status SaveToFile();

  auto GetSize() const { return size_; }
  auto GetTitle() const { return title; }
  auto GetGraphicsBin() const { return graphics_bin_; }
  auto GetRenderer() const { return renderer_; }
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
    renderer_ = renderer;
  }
  auto isLoaded() const { return is_loaded_; }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  auto data() { return rom_data_.data(); }

  uchar& operator[](int i) {
    if (i > size_) {
      std::cout << "ROM: Index out of bounds" << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  uchar& operator+(int i) {
    if (i > size_) {
      std::cout << "ROM: Index out of bounds" << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  const uchar* operator&() { return rom_data_.data(); }

 private:
  long size_ = 0;
  uchar title[21] = "ROM Not Loaded";
  bool is_loaded_ = false;
  std::string filename_;

  Bytes rom_data_;
  std::shared_ptr<SDL_Renderer> renderer_;
  absl::flat_hash_map<int, gfx::Bitmap> graphics_bin_;
};

}  // namespace app
}  // namespace yaze

#endif