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

std::vector<tile8> ROM::ExtractTiles(TilePreset &preset) {
  std::cout << "Extracting tiles..." << std::endl;
  uint filePos = 0;
  uint size_out = 0;
  uint size = preset.length;
  int tilePos = preset.pcTilesLocation;
  std::vector<tile8> rawTiles;
  filePos = getRomPosition(preset, tilePos, preset.SNESTilesLocation);
  std::cout << "ROM Position: " << filePos << " from "
            << preset.SNESTilesLocation << std::endl;

  // decompress the graphics
  char *data = (char *)malloc(sizeof(char) * size);
  memcpy(data, (data_ + filePos), size);
  // data = alttp_decompress_gfx(data, 0, size, &size_out, &compressed_size_);
  //  std::cout << "size: " << size << std::endl;
  //  std::cout << "lastCompressedSize: " << compressed_size_ << std::endl;
  data = Decompress(filePos);
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

SNESPalette ROM::ExtractPalette(TilePreset &preset) {
  unsigned int filePos = getRomPosition(preset, preset.pcPaletteLocation,
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

char *ROM::Decompress(int pos, bool reversed) {
  char *buffer = new char[0x600];
  for (int i = 0; i < 0x600; i++) {
    buffer[i] = 0;
  }
  unsigned int bufferPos = 0;
  unsigned char cmd = 0;
  unsigned int length = 0;
  unsigned char databyte = (unsigned char)data_[pos];
  while (true) {
    databyte = (unsigned char)data_[pos];
    if (databyte == 0xFF)  // End of decompression
    {
      break;
    }

    if ((databyte & 0xE0) == 0xE0)  // Expanded Command
    {
      cmd = (unsigned char)((databyte >> 2) & 0x07);
      length = (unsigned short)(((data_[pos] << 8) | data_[pos + 1]) & 0x3FF);
      pos += 2;  // Advance 2 bytes in ROM
    } else       // Normal Command
    {
      cmd = (unsigned char)((databyte >> 5) & 0x07);
      length = (unsigned char)(databyte & 0x1F);
      pos += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // Every commands are at least 1 size even if 00
    switch (cmd) {
      case 00:  // Direct Copy (Could be replaced with a MEMCPY)
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = (unsigned char)data_[pos++];
        }
        // Do not advance in the ROM
        break;
      case 01:  // Byte Fill
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = (unsigned char)data_[pos];
        }
        pos += 1;  // Advance 1 byte in the ROM
        break;
      case 02:  // Word Fill
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = (unsigned char)data_[pos];
          buffer[bufferPos++] = (unsigned char)data_[pos + 1];
        }
        pos += 2;  // Advance 2 byte in the ROM
        break;
      case 03:  // Increasing Fill
      {
        unsigned char incByte = (unsigned char)data_[pos];
        for (int i = 0; i < (unsigned int)length; i++) {
          buffer[bufferPos++] = (unsigned char)incByte++;
        }
        pos += 1;  // Advance 1 byte in the ROM
      } break;
      case 04:  // Repeat (Reversed byte order for maps)
      {
        unsigned short s1 = ((data_[pos + 1] & 0xFF) << 8);
        unsigned short s2 = ((data_[pos] & 0xFF));
        unsigned short Addr = (unsigned short)(s1 | s2);
        printf("Repeat Address :  %4X", Addr);
        for (int i = 0; i < length; i++) {
          buffer[bufferPos] = (unsigned char)buffer[Addr + i];
          bufferPos++;
        }
        pos += 2;  // Advance 2 bytes in the ROM
      } break;
    }
  }
  return buffer;
}

}  // namespace Data
}  // namespace Application
}  // namespace yaze