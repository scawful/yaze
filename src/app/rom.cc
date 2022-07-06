#include "rom.h"

#include <compressions/alttpcompression.h>
#include <rommapping.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/tile.h"

namespace yaze {
namespace app {
namespace rom {

void ROM::Close() {
  if (loaded) {
    delete[] current_rom_;
    for (auto &each : decompressed_graphic_sheets_) {
      free(each);
    }
    for (auto &each : converted_graphic_sheets_) {
      free(each);
    }
  }
}

void ROM::SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
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

char *ROM::Decompress(int pos, int size, bool reversed) {
  auto *buffer = new char[size];
  decompressed_graphic_sheets_.push_back(buffer);
  for (int i = 0; i < size; i++) {
    buffer[i] = 0;
  }
  unsigned int bufferPos = 0;
  unsigned char cmd = 0;
  unsigned int length = 0;

  uchar databyte = current_rom_[pos];
  while (true) {
    databyte = current_rom_[pos];
    // End of decompression
    if (databyte == 0xFF) {
      break;
    }

    // Expanded Command
    if ((databyte & 0xE0) == 0xE0) {
      cmd = (uchar)((databyte >> 2) & 0x07);
      length =
          (ushort)(((current_rom_[pos] << 8) | current_rom_[pos + 1]) & 0x3FF);
      pos += 2;  // Advance 2 bytes in ROM
    } else       // Normal Command
    {
      cmd = (uchar)((databyte >> 5) & 0x07);
      length = (uchar)(databyte & 0x1F);
      pos += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // Every commands are at least 1 size even if 00
    switch (cmd) {
      case 00:  // Direct Copy (Could be replaced with a MEMCPY)
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = current_rom_[pos++];
        }
        // Do not advance in the ROM
        break;
      case 01:  // Byte Fill
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = current_rom_[pos];
        }
        pos += 1;  // Advance 1 byte in the ROM
        break;
      case 02:  // Word Fill
        for (int i = 0; i < length; i += 2) {
          buffer[bufferPos++] = current_rom_[pos];
          buffer[bufferPos++] = current_rom_[pos + 1];
        }
        pos += 2;  // Advance 2 byte in the ROM
        break;
      case 03:  // Increasing Fill
      {
        uchar incByte = current_rom_[pos];
        for (int i = 0; i < length; i++) {
          buffer[bufferPos++] = incByte++;
        }
        pos += 1;  // Advance 1 byte in the ROM
      } break;
      case 04:  // Repeat (Reversed byte order for maps)
      {
        ushort s1 = ((current_rom_[pos + 1] & 0xFF) << 8);
        ushort s2 = ((current_rom_[pos] & 0xFF));
        auto Addr = (ushort)(s1 | s2);
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

std::vector<tile8> ROM::ExtractTiles(gfx::TilePreset &preset) {
  uint size_out = 0;
  uint size = preset.length_;
  int tile_pos = preset.pc_tiles_location_;
  std::vector<tile8> rawTiles;

  // decompress the gfx
  auto data = (char *)malloc(sizeof(char) * size);
  memcpy(data, (current_rom_ + tile_pos), size);
  data = alttp_decompress_gfx(data, 0, size, &size_out, &compressed_size_);
  if (data == nullptr) {
    std::cout << alttp_decompression_error << std::endl;
    return rawTiles;
  }

  // unpack the tiles based on their depth
  unsigned tileCpt = 0;
  for (tile_pos = 0; tile_pos < size; tile_pos += preset.bits_per_pixel_ * 8) {
    tile8 newTile = unpack_bpp_tile(data, tile_pos, preset.bits_per_pixel_);
    newTile.id = tileCpt;
    rawTiles.push_back(newTile);
    tileCpt++;
  }
  free(data);
  return rawTiles;
}

gfx::SNESPalette ROM::ExtractPalette(uint addr, int bpp) {
  uint filePos = addr;
  uint palette_size = pow(2, bpp);
  auto *palette_data = (char *)malloc(sizeof(char) * (palette_size * 2));
  memcpy(palette_data, current_rom_ + filePos, palette_size * 2);
  for (int i = 0; i < palette_size; i++) std::cout << palette_data[i];
  std::cout << std::endl;
  gfx::SNESPalette pal(palette_data);
  return pal;
}

// 128x32
uchar *ROM::SNES3bppTo8bppSheet(uchar *buffer_in, int sheet_id, int size) {
  // 8bpp sheet out
  const uchar bitmask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  auto *sheet_buffer_out = (uchar *)malloc(size);
  converted_graphic_sheets_.push_back(sheet_buffer_out);
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;

  if (sheet_id != 0) {
    yy = sheet_id;
  }

  // for each tiles
  // 16 per line
  for (int i = 0; i < 64; i++) {
    // for each line
    for (int y = 0; y < 8; y++) {
      //[0] + [1] + [16]
      for (int x = 0; x < 8; x++) {
        auto b1 = (uchar)((buffer_in[(y * 2) + (24 * pos)] & (bitmask[x])));
        auto b2 = (uchar)(buffer_in[((y * 2) + (24 * pos)) + 1] & (bitmask[x]));
        auto b3 = (uchar)(buffer_in[(16 + y) + (24 * pos)] & (bitmask[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0) {
          b |= 4;
        }
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

SDL_Texture *ROM::DrawGraphicsSheet(int offset) {
  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, 128, 32, 8, SDL_PIXELFORMAT_INDEX8);
  std::cout << "Drawing surface #" << offset << std::endl;
  uchar *sheet_buffer = nullptr;
  for (int i = 0; i < 8; i++) {
    surface->format->palette->colors[i].r = i * 31;
    surface->format->palette->colors[i].g = i * 31;
    surface->format->palette->colors[i].b = i * 31;
  }

  uint snesAddr = 0;
  uint pcAddr = 0;
  snesAddr = (uint)((((current_rom_[0x4F80 + offset]) << 16) |
                     ((current_rom_[0x505F + offset]) << 8) |
                     ((current_rom_[0x513E + offset]))));
  pcAddr = SnesToPc(snesAddr);
  std::cout << "Decompressing..." << std::endl;
  char *decomp = Decompress(pcAddr);
  std::cout << "Converting to 8bpp sheet..." << std::endl;
  sheet_buffer = SNES3bppTo8bppSheet((uchar *)decomp);
  std::cout << "Assigning pixel data..." << std::endl;
  surface->pixels = sheet_buffer;
  std::cout << "Creating texture from surface..." << std::endl;
  SDL_Texture *sheet_texture = nullptr;
  sheet_texture = SDL_CreateTextureFromSurface(sdl_renderer_.get(), surface);
  if (sheet_texture == nullptr) {
    std::cout << "Error: " << SDL_GetError() << std::endl;
  }
  return sheet_texture;
}

int ROM::AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3) const {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

int ROM::GetPCGfxAddress(uint8_t id) {
  int gfxPtr1 =
      SnesToPc((current_rom_[core::constants::gfx_1_pointer + 1] << 8) +
               (current_rom_[core::constants::gfx_1_pointer]));
  int gfxPtr2 =
      SnesToPc((current_rom_[core::constants::gfx_2_pointer + 1] << 8) +
               (current_rom_[core::constants::gfx_2_pointer]));
  int gfxPtr3 =
      SnesToPc((current_rom_[core::constants::gfx_3_pointer + 1] << 8) +
               (current_rom_[core::constants::gfx_3_pointer]));

  uint8_t gfxGamePointer1 = current_rom_[gfxPtr1 + id];
  uint8_t gfxGamePointer2 = current_rom_[gfxPtr2 + id];
  uint8_t gfxGamePointer3 = current_rom_[gfxPtr3 + id];

  return SnesToPc(
      AddressFromBytes(gfxGamePointer1, gfxGamePointer2, gfxGamePointer3));
}

char *ROM::CreateAllGfxDataRaw() {
  // 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
  // 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
  // 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
  // 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
  // 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars

  auto *buffer = new char[346624];
  auto *data = new char[2048];
  int bufferPos = 0;
  unsigned int uncompressedSize = 0;
  unsigned int compressedSize = 0;

  for (int i = 0; i < core::constants::NumberOfSheets; i++) {
    isbpp3[i] = ((i >= 0 && i <= 112) ||    // Compressed 3bpp bg
                 (i >= 115 && i <= 126) ||  // Uncompressed 3bpp sprites
                 (i >= 127 && i <= 217)     // Compressed 3bpp sprites
    );

    // uncompressed sheets
    if (i >= 115 && i <= 126) {
      data = new char[core::constants::Uncompressed3BPPSize];
      int startAddress = GetPCGfxAddress(i);
      for (int j = 0; j < core::constants::Uncompressed3BPPSize; j++) {
        data[j] = current_rom_[j + startAddress];
      }
    } else {
      data =
          alttp_decompress_gfx((char *)current_rom_, GetPCGfxAddress((char)i),
                               core::constants::UncompressedSheetSize,
                               &uncompressedSize, &compressedSize);
    }

    for (int j = 0; j < sizeof(data); j++) {
      buffer[j + bufferPos] = data[j];
    }

    bufferPos += sizeof(data);
  }

  return buffer;
}

void ROM::CreateAllGraphicsData(uchar *allGfx16Ptr) {
  int sheetPosition = 0;
  char const *data = CreateAllGfxDataRaw();
  auto *newData = new char[0x6F800];
  uchar const *mask =
      new uchar[]{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

  // 8x8 tile
  // Per Sheet
  for (int s = 0; s < core::constants::NumberOfSheets; s++) {
    for (int j = 0; j < 4; j++) {      // Per Tile Line Y
      for (int i = 0; i < 16; i++) {   // Per Tile Line X
        for (int y = 0; y < 8; y++) {  // Per Pixel Line
          if (isbpp3[s]) {
            uchar lineBits0 =
                data[(y * 2) + (i * 24) + (j * 384) + sheetPosition];
            uchar lineBits1 =
                data[(y * 2) + (i * 24) + (j * 384) + 1 + sheetPosition];
            uchar lineBits2 =
                data[(y) + (i * 24) + (j * 384) + 16 + sheetPosition];

            // Per Pixel X
            for (int x = 0; x < 4; x++) {
              uchar pixdata = 0;
              uchar pixdata2 = 0;

              if ((lineBits0 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 1;
              }
              if ((lineBits1 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 2;
              }
              if ((lineBits2 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 4;
              }

              if ((lineBits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 1;
              }
              if ((lineBits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 2;
              }
              if ((lineBits2 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 4;
              }

              newData[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                  (char)((pixdata << 4) | pixdata2);
            }
          } else {
            uchar lineBits0 =
                data[(y * 2) + (i * 16) + (j * 256) + sheetPosition];
            uchar lineBits1 =
                data[(y * 2) + (i * 16) + (j * 256) + 1 + sheetPosition];

            // Per Pixel X
            for (int x = 0; x < 4; x++) {
              uchar pixdata = 0;
              uchar pixdata2 = 0;

              if ((lineBits0 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 1;
              }
              if ((lineBits1 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 2;
              }

              if ((lineBits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 1;
              }
              if ((lineBits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 2;
              }

              newData[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                  (uchar)((pixdata << 4) | pixdata2);
            }
          }
        }
      }
    }

    if (isbpp3[s]) {
      sheetPosition += core::constants::Uncompressed3BPPSize;
    } else {
      sheetPosition += core::constants::UncompressedSheetSize;
    }
  }

  auto *allgfx16Data = allGfx16Ptr;
  for (int i = 0; i < 0x6F800; i++) {
    allgfx16Data[i] = newData[i];
  }
  allgfx16Data = SNES3bppTo8bppSheet(allgfx16Data);
}

void ROM::LoadBlocksetGraphics(int graphics_id) {}

}  // namespace rom
}  // namespace app
}  // namespace yaze