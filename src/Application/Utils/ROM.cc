#include "ROM.h"

namespace yaze {
namespace Application {
namespace Utils {

void ROM::LoadFromFile(const std::string& path) {
  std::cout << "filename: " << path << std::endl;
  std::ifstream stream(path, std::ios::in | std::ios::binary);

  if (!stream.good()) {
    std::cout << "failure reading file" << std::endl;
    return;
  }

  std::vector<char> contents((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());

  for (auto i : contents) {
    int value = i;
    std::cout << "data: " << value << std::endl;
  }

  std::cout << "file size: " << contents.size() << std::endl;

  unsigned int uncompressed_data_size = 0;
  unsigned int compressed_length = 0;
  auto gfx_decompressed_data = alttp_compressor_.DecompressGfx(
      contents.data(), 0, contents.size(), &uncompressed_data_size,
      &compressed_length);
  auto overworld_decompressed = alttp_compressor_.DecompressOverworld(
      contents.data(), 0, contents.size(), &uncompressed_data_size,
      &compressed_length);
}

}  // namespace Utils
}  // namespace Application
}  // namespace yaze