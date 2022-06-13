#include "rom.h"

#include <filesystem>

namespace yaze {
namespace Application {
namespace Data {

ROM::~ROM() {
  if (loaded) {
    delete[] current_rom_;
    delete[] data_;
  }
}

// TODO: check if the rom has a header on load
void ROM::LoadFromFile(const std::string &path) {
  type_ = LoROM;
  size_ = std::filesystem::file_size(path.c_str());
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file.is_open()) {
    std::cout << "Error: Could not open ROM file " << path << std::endl;
    return;
  }

  current_rom_ = new unsigned char[size_];
  data_ = new char[size_];
  for (unsigned int i = 0; i < size_; i++) {
    char byte_read_ = ' ';
    file.read(&byte_read_, sizeof(char));
    current_rom_[i] = byte_read_;
    data_[i] = byte_read_;
  }
  file.close();

  memcpy(title, data_ + 32704, 21);
  version_ = current_rom_[27];
  loaded = true;
}

std::vector<tile8> ROM::ExtractTiles(Graphics::TilePreset &preset) {
  std::cout << "Extracting tiles..." << std::endl;
  uint filePos = 0;
  uint size_out = 0;
  uint size = preset.length;
  int tilePos = preset.pcTilesLocation;
  std::vector<tile8> rawTiles;
  filePos = GetRomPosition(preset, tilePos, preset.SNESTilesLocation);
  std::cout << "ROM Position: " << filePos << " from "
            << preset.SNESTilesLocation << std::endl;

  // decompress the graphics
  char *data = (char *)malloc(sizeof(char) * size);
  memcpy(data, (data_ + filePos), size);
  data = alttp_decompress_gfx(data, 0, size, &size_out, &compressed_size_);
  std::cout << "size: " << size << std::endl;
  std::cout << "lastCompressedSize: " << compressed_size_ << std::endl;
  if (data == NULL) {
    std::cout << alttp_decompression_error << std::endl;
    return rawTiles;
  }

  // unpack the tiles based on their depth
  unsigned tileCpt = 0;
  std::cout << "Unpacking tiles..." << std::endl;
  for (unsigned int tilePos = 0; tilePos < size; tilePos += preset.bpp * 8) {
    tile8 newTile = unpack_bpp_tile(data, tilePos, preset.bpp);
    newTile.id = tileCpt;
    rawTiles.push_back(newTile);
    tileCpt++;
  }
  std::cout << "Done unpacking tiles" << std::endl;
  free(data);
  std::cout << "Done extracting tiles." << std::endl;
  return rawTiles;
}

Graphics::SNESPalette ROM::ExtractPalette(Graphics::TilePreset &preset) {
  unsigned int filePos = GetRomPosition(preset, preset.pcPaletteLocation,
                                        preset.SNESPaletteLocation);
  std::cout << "Palette pos : " << filePos << std::endl;  // TODO: make this hex
  unsigned int palette_size = pow(2, preset.bpp);         // - 1;
  char *ab = (char *)malloc(sizeof(char) * (palette_size * 2));
  memcpy(ab, data_ + filePos, palette_size * 2);

  for (int i = 0; i < palette_size; i++) {
    std::cout << ab[i];
  }
  std::cout << std::endl;

  const char *data = ab;
  Graphics::SNESPalette pal(ab);
  if (preset.paletteNoZeroColor) {
    Graphics::SNESColor col;

    col.setRgb(ImVec4(153, 153, 153, 255));
    pal.colors.push_back(col);
    pal.colors.erase(pal.colors.begin(),
                     pal.colors.begin() + pal.colors.size() - 1);
  }
  return pal;
}

uint32_t ROM::GetRomPosition(const Graphics::TilePreset &preset, int directAddr,
                             unsigned int snesAddr) const {
  unsigned int filePos = -1;
  enum rom_type rType = LoROM;
  std::cout << "directAddr:" << directAddr << std::endl;
  if (directAddr == -1) {
    filePos = rommapping_snes_to_pc(snesAddr, rType, rom_has_header_);
  } else {
    filePos = directAddr;
    if (rom_has_header_) filePos += 0x200;
  }
  std::cout << "filePos:" << filePos << std::endl;
  return filePos;
}

int AddressFromBytes(uchar addr1, uchar addr2, uchar addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

}  // namespace Data
}  // namespace Application
}  // namespace yaze