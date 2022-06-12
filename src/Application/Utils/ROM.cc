#include "ROM.h"

#define ROMMAPPING_LOCATION_SNES_RESERVED -1
#define ROMMAPPING_LOCATION_SRAM -2
#define ROMMAPPING_LOCATION_WRAM -3

namespace yaze {
namespace Application {
namespace Utils {

using namespace Graphics;

void ROM::LoadFromFile(const std::string &path) {
  FILE *file = fopen(path.c_str(), "r+");
  if (file == NULL) return;
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fclose(file);

  // Reading data to array of unsigned chars
  file = fopen(path.c_str(), "r+");
  current_rom_ = (unsigned char *)malloc(size);
  rom_data_ = (char *)malloc(size);
  fread(rom_data_, sizeof(char), size, file);
  int bytes_read = fread(current_rom_, sizeof(unsigned char), size, file);
  fclose(file);

  memcpy(title, rom_data_ + 32704, 21);
  type = LoROM;
  version = current_rom_[27];
  loaded = true;
}

std::vector<tile8> ROM::ExtractTiles(TilePreset &preset) {
  std::cout << "Begin ROM::ExtractTiles" << std::endl;

  uint filePos = 0;
  uint size_out = 0;
  uint size = preset.length;
  int tilePos = preset.pcTilesLocation;
  std::vector<tile8> rawTiles;

  filePos = getRomPosition(preset, tilePos, preset.SNESTilesLocation);

  // decompress the graphics
  char *data = (char *)malloc(sizeof(char) * size);
  memcpy(data, (rom_data_ + filePos), size);
  data = alttp_decompress_gfx(data, 0, size, &size_out, &lastCompressedSize);
  std::cout << "size: " << size << std::endl;
  std::cout << "lastCompressedSize: " << lastCompressedSize << std::endl;
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
  std::cout << "End ROM::ExtractTiles" << std::endl;
  free(data);
  return rawTiles;
}

SNESPalette ROM::ExtractPalette(TilePreset &preset) {
  unsigned int filePos = getRomPosition(preset, preset.pcPaletteLocation,
                                        preset.SNESPaletteLocation);
  std::cout << "Palette pos : " << filePos << std::endl;  // TODO: make this hex
  unsigned int palette_size = pow(2, preset.bpp);         // - 1;
  char *ab = (char *)malloc(sizeof(char) * (palette_size * 2));
  memcpy(ab, rom_data_ + filePos, palette_size * 2);

  for (int i = 0; i < palette_size; i++) {
    std::cout << ab[i];
  }
  std::cout << std::endl;

  const char *data = ab;
  SNESPalette pal(ab);
  if (preset.paletteNoZeroColor) {
    SNESColor col;

    col.setRgb(ImVec4(153, 153, 153, 255));
    pal.colors.push_back(col);
    pal.colors.erase(pal.colors.begin(),
                     pal.colors.begin() + pal.colors.size() - 1);
  }
  return pal;
}

unsigned int ROM::getRomPosition(const TilePreset &preset, int directAddr,
                                 unsigned int snesAddr) {
  bool romHasHeader = false;  // romInfo.hasHeader
  if (overrideHeaderInfo) romHasHeader = overridenHeaderInfo;
  unsigned int filePos = -1;
  enum rom_type rType = LoROM;
  std::cout << "ROM::getRomPosition: directAddr:" << directAddr << std::endl;
  if (directAddr == -1) {
    filePos = rommapping_snes_to_pc(snesAddr, rType, romHasHeader);
  } else {
    filePos = directAddr;
    if (romHasHeader) filePos += 0x200;
  }
  std::cout << "ROM::getRomPosition: filePos:" << filePos << std::endl;
  return filePos;
}

int AddressFromBytes(byte addr1, byte addr2, byte addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

short ROM::AddressFromBytes(byte addr1, byte addr2) {
  return (short)((addr1 << 8) | (addr2));
}

}  // namespace Utils
}  // namespace Application
}  // namespace yaze