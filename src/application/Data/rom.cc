#include "rom.h"

#include <filesystem>

#include "Core/constants.h"

namespace yaze {
namespace application {
namespace Data {

ROM::~ROM() {
  if (loaded) {
    delete[] current_rom_;
  }
}

// TODO: check if the rom has a header on load
void ROM::LoadFromFile(const std::string &path) {
  size_ = std::filesystem::file_size(path.c_str());
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file.is_open()) {
    std::cout << "Error: Could not open ROM file " << path << std::endl;
    return;
  }
  current_rom_ = new unsigned char[size_];
  for (unsigned int i = 0; i < size_; i++) {
    char byte_read_ = ' ';
    file.read(&byte_read_, sizeof(char));
    current_rom_[i] = byte_read_;
  }
  file.close();
  memcpy(title, current_rom_ + 32704, 21);
  version_ = current_rom_[27];
  loaded = true;
}

std::vector<tile8> ROM::ExtractTiles(Graphics::TilePreset &preset) {
  std::cout << "Extracting tiles..." << std::endl;
  uint filePos = 0;
  uint size_out = 0;
  uint size = preset.length_;
  int tilePos = preset.pc_tiles_location_;
  std::vector<tile8> rawTiles;
  filePos = GetRomPosition(tilePos, preset.SNESTilesLocation);
  std::cout << "ROM Position: " << filePos << " from "
            << preset.SNESTilesLocation << std::endl;

  // decompress the graphics
  auto data = (char *)malloc(sizeof(char) * size);
  memcpy(data, (current_rom_ + filePos), size);
  data = alttp_decompress_gfx(data, 0, size, &size_out, &compressed_size_);
  std::cout << "size: " << size << std::endl;
  std::cout << "lastCompressedSize: " << compressed_size_ << std::endl;
  if (data == nullptr) {
    std::cout << alttp_decompression_error << std::endl;
    return rawTiles;
  }

  // unpack the tiles based on their depth
  unsigned tileCpt = 0;
  std::cout << "Unpacking tiles..." << std::endl;
  for (unsigned int tilePos = 0; tilePos < size;
       tilePos += preset.bits_per_pixel_ * 8) {
    tile8 newTile = unpack_bpp_tile(data, tilePos, preset.bits_per_pixel_);
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
  uint filePos =
      GetRomPosition(preset.pc_palette_location_, preset.SNESPaletteLocation);
  std::cout << "Palette pos : " << filePos << std::endl;  // TODO: make this hex
  uint palette_size = pow(2, preset.bits_per_pixel_);     // - 1;

  auto palette_data = std::make_unique<unsigned char[]>(palette_size * 2);
  memcpy(palette_data.get(), current_rom_ + filePos, palette_size * 2);

  // char *ab = (char *)malloc(sizeof(char) * (palette_size * 2));
  // memcpy(ab, current_rom_ + filePos, palette_size * 2);

  for (int i = 0; i < palette_size; i++) {
    std::cout << palette_data[i];
  }
  std::cout << std::endl;

  const unsigned char *data = palette_data.get();
  Graphics::SNESPalette pal(data);
  if (preset.no_zero_color_) {
    Graphics::SNESColor col;

    col.setRgb(ImVec4(153, 153, 153, 255));
    pal.colors.push_back(col);
    pal.colors.erase(pal.colors.begin(),
                     pal.colors.begin() + pal.colors.size() - 1);
  }
  return pal;
}

uint32_t ROM::GetRomPosition(int direct_addr, uint snes_addr) const {
  unsigned int filePos = -1;
  std::cout << "directAddr:" << direct_addr << std::endl;
  if (direct_addr == -1) {
    filePos = rommapping_snes_to_pc(snes_addr, type_, has_header_);
  } else {
    filePos = direct_addr;
    if (has_header_) filePos += 0x200;
  }
  std::cout << "filePos:" << filePos << std::endl;
  return filePos;
}

uchar *ROM::SNES3bppTo8bppSheet(uchar *buffer_in, int sheet_id)  // 128x32
{
  // 8bpp sheet out
  const uchar bitmask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  uchar *sheet_buffer_out = (unsigned char *)malloc(0x1000);
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;

  if (sheet_id != 0) {
    yy = sheet_id;
  }

  for (int i = 0; i < 64; i++)  // for each tiles //16 per lines
  {
    for (int y = 0; y < 8; y++)  // for each lines
    {
      //[0] + [1] + [16]
      for (int x = 0; x < 8; x++) {
        unsigned char b1 = (unsigned char)((
            buffer_in[(y * 2) + (24 * pos)] & (bitmask[x])));
        unsigned char b2 =
            (unsigned char)(buffer_in[((y * 2) + (24 * pos)) + 1] &
                            (bitmask[x]));
        unsigned char b3 =
            (unsigned char)(buffer_in[(16 + y) + (24 * pos)] &
                            (bitmask[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        };
        if (b2 != 0) {
          b |= 2;
        };
        if (b3 != 0) {
          b |= 4;
        };
        sheet_buffer_out[x + (xx) + (y * 128) + (yy * 1024)] = b;
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

char *ROM::Decompress(int pos, bool reversed) {
  char *buffer = new char[0x800];
  for (int i = 0; i < 0x800; i++) {
    buffer[i] = 0;
  }
  unsigned int bufferPos = 0;
  unsigned char cmd = 0;
  unsigned int length = 0;

  uchar databyte = current_rom_[pos];
  while (true) {
    databyte = (unsigned char)current_rom_[pos];
    if (databyte == 0xFF)  // End of decompression
    {
      break;
    }

    if ((databyte & 0xE0) == 0xE0)  // Expanded Command
    {
      cmd = (unsigned char)((databyte >> 2) & 0x07);
      length =
          (unsigned short)(((current_rom_[pos] << 8) | current_rom_[pos + 1]) &
                           0x3FF);
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
          buffer[bufferPos++] = (unsigned char)current_rom_[pos++];
        }
        // Do not advance in the ROM
        break;
      case 01:  // Byte Fill
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = (unsigned char)current_rom_[pos];
        }
        pos += 1;  // Advance 1 byte in the ROM
        break;
      case 02:  // Word Fill
        for (int i = 0; i < length; i += 2) {
          buffer[bufferPos++] = (unsigned char)current_rom_[pos];
          buffer[bufferPos++] = (unsigned char)current_rom_[pos + 1];
        }
        pos += 2;  // Advance 2 byte in the ROM
        break;
      case 03:  // Increasing Fill
      {
        unsigned char incByte = (unsigned char)current_rom_[pos];
        for (int i = 0; i < (unsigned int)length; i++) {
          buffer[bufferPos++] = (unsigned char)incByte++;
        }
        pos += 1;  // Advance 1 byte in the ROM
      } break;
      case 04:  // Repeat (Reversed byte order for maps)
      {
        unsigned short s1 = ((current_rom_[pos + 1] & 0xFF) << 8);
        unsigned short s2 = ((current_rom_[pos] & 0xFF));
        unsigned short Addr = (unsigned short)(s1 | s2);
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

SDL_Surface* ROM::GetGraphicsSheet(int num_sheets) {
  int height = 32 * num_sheets;
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 128, height, 8, SDL_PIXELFORMAT_INDEX8);
  std::cout << "Drawing surface" << std::endl;
  uchar *sheet_buffer = nullptr;
  // int sheet_buffer_pos = 0;
  for (int i = 0; i < 8; i++) {
    surface->format->palette->colors[i].r = (unsigned char)(i * 31);
    surface->format->palette->colors[i].g = (unsigned char)(i * 31);
    surface->format->palette->colors[i].b = (unsigned char)(i * 31);
  }

  unsigned int snesAddr = 0;
  unsigned int pcAddr = 0;
  for (int i = 0; i < num_sheets; i++) {
    snesAddr =
        (unsigned int)((((unsigned char)(current_rom_[0x4F80 + i]) <<
        16) |
                        ((unsigned char)(current_rom_[0x505F + i]) << 8)
                        |
                        ((unsigned char)(current_rom_[0x513E]))));
    pcAddr = SnesToPc(snesAddr);
    std::cout << "Decompressing..." << std::endl;
    char *decomp = Decompress(pcAddr);
    std::cout << "Converting to 8bpp sheet..." << std::endl;
    sheet_buffer = SNES3bppTo8bppSheet((uchar *)decomp, i);
    std::cout << "Assigning pixel data..." << std::endl;
  }

  surface->pixels = sheet_buffer;
  return surface;
}

int AddressFromBytes(uchar addr1, uchar addr2, uchar addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

}  // namespace Data
}  // namespace application
}  // namespace yaze