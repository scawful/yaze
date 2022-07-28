#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL2/SDL.h>

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
constexpr int kMaxLengthNormalHeader = 32;
constexpr int kMaxLengthCompression = 1024;
constexpr int kNintendoMode1 = 0;
constexpr int kNintendoMode2 = 1;
constexpr int kTile32Num = 4432;
constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

using OWBlockset = std::vector<std::vector<ushort>>;
struct OWMapTiles {
  OWBlockset light_world;    // 64 maps
  OWBlockset dark_world;     // 64 maps
  OWBlockset special_world;  // 32 maps
} typedef OWMapTiles;

typedef struct sCompressionPiece CompressionPiece;
struct sCompressionPiece {
  char command;
  int length;
  char* argument;
  int argument_length;
  CompressionPiece* next;
};

class ROM {
 public:
  absl::Status LoadFromFile(const absl::string_view& filename);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadAllGraphicsData();

  absl::StatusOr<Bytes> CompressGraphics(const int pos, const int length);
  absl::StatusOr<Bytes> CompressOverworld(const int pos, const int length);
  absl::StatusOr<Bytes> Compress(const int start, const int length,
                                 int mode = 0);

  absl::StatusOr<Bytes> DecompressGraphics(int pos, int size);
  absl::StatusOr<Bytes> DecompressOverworld(int pos, int size);
  absl::StatusOr<Bytes> Decompress(int offset, int size = 0x800,
                                   bool reversed = false);

  absl::StatusOr<Bytes> Convert3bppTo8bppSheet(Bytes sheet, int size = 0x1000);

  uint GetGraphicsAddress(uint8_t id) const;
  auto GetSize() const { return size_; }
  auto GetTitle() const { return title; }
  auto GetBytes() const { return rom_data_; }
  auto GetGraphicsBin() const { return graphics_bin_; }

  auto isLoaded() const { return is_loaded_; }

  auto Renderer() { return renderer_; }
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
    renderer_ = renderer;
  }
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

  Bytes rom_data_;
  std::shared_ptr<SDL_Renderer> renderer_;
  absl::flat_hash_map<int, gfx::Bitmap> graphics_bin_;
};

}  // namespace app
}  // namespace yaze

#endif