#include "Bitmap.h"
#include "Utils/ROM.h"
#include "Utils/Compression.h"

namespace yaze {
namespace Application {
namespace Graphics {

int GetPCGfxAddress(byte *romData, byte id) {
  char** info1, **info2,** info3, **info4;
  int gfxPointer1 =
      Utils::lorom_snes_to_pc((romData[Constants::gfx_1_pointer + 1] << 8) +
                      (romData[Constants::gfx_1_pointer]), info1);
  int gfxPointer2 =
      Utils::lorom_snes_to_pc((romData[Constants::gfx_2_pointer + 1] << 8) +
                      (romData[Constants::gfx_2_pointer]), info2);
  int gfxPointer3 =
      Utils::lorom_snes_to_pc((romData[Constants::gfx_3_pointer + 1] << 8) +
                      (romData[Constants::gfx_3_pointer]), info3);

  byte gfxGamePointer1 = romData[gfxPointer1 + id];
  byte gfxGamePointer2 = romData[gfxPointer2 + id];
  byte gfxGamePointer3 = romData[gfxPointer3 + id];

  return Utils::lorom_snes_to_pc(Utils::AddressFromBytes(gfxGamePointer1, gfxGamePointer2,
                                               gfxGamePointer3), info4);
}

byte *CreateAllGfxDataRaw(byte *romData) {
  // 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 bytes
  // 113-114 -> compressed 2bpp -> (decompressed each) 0x800 bytes
  // 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 bytes
  // 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 bytes
  // 218-222 -> compressed 2bpp -> (decompressed each) 0x800 bytes

  Utils::ALTTPCompression alttp_compressor_;

  byte *buffer = new byte[346624];
  int bufferPos = 0;
  byte *data = new byte[2048];
  unsigned int uncompressedSize = 0;
  unsigned int compressedSize = 0;

  for (int i = 0; i < Constants::NumberOfSheets; i++) {
    isbpp3[i] = ((i >= 0 && i <= 112) ||   // Compressed 3bpp bg
                 (i >= 115 && i <= 126) || // Uncompressed 3bpp sprites
                 (i >= 127 && i <= 217)    // Compressed 3bpp sprites
    );

    // uncompressed sheets
    if (i >= 115 && i <= 126) {
      data = new byte[Constants::Uncompressed3BPPSize];
      int startAddress = GetPCGfxAddress(romData, (byte)i);
      for (int j = 0; j < Constants::Uncompressed3BPPSize; j++) {
        data[j] = romData[j + startAddress];
      }
    } else {
      data = alttp_compressor_.DecompressGfx(
         romData, GetPCGfxAddress(romData, (byte)i),
          Constants::UncompressedSheetSize, &uncompressedSize, &compressedSize);
    }

    for (int j = 0; j < sizeof(data); j++) {
      buffer[j + bufferPos] = data[j];
    }

    bufferPos += sizeof(data);
  }

  return buffer;
}

void CreateAllGfxData(byte *romData, byte* allgfx16Ptr) {
  byte* data = CreateAllGfxDataRaw(romData);
  byte* newData =
      new byte[0x6F800]; // NEED TO GET THE APPROPRIATE SIZE FOR THAT
  byte* mask = new byte[]{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  int sheetPosition = 0;

  // 8x8 tile
  for (int s = 0; s < Constants::NumberOfSheets; s++) // Per Sheet
  {
    for (int j = 0; j < 4; j++) // Per Tile Line Y
    {
      for (int i = 0; i < 16; i++) // Per Tile Line X
      {
        for (int y = 0; y < 8; y++) // Per Pixel Line
        {
          if (isbpp3[s]) {
            byte lineBits0 =
                data[(y * 2) + (i * 24) + (j * 384) + sheetPosition];
            byte lineBits1 =
                data[(y * 2) + (i * 24) + (j * 384) + 1 + sheetPosition];
            byte lineBits2 =
                data[(y) + (i * 24) + (j * 384) + 16 + sheetPosition];

            for (int x = 0; x < 4; x++) // Per Pixel X
            {
              byte pixdata = 0;
              byte pixdata2 = 0;

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
                  (byte)((pixdata << 4) | pixdata2);
            }
          } else {
            byte lineBits0 =
                data[(y * 2) + (i * 16) + (j * 256) + sheetPosition];
            byte lineBits1 =
                data[(y * 2) + (i * 16) + (j * 256) + 1 + sheetPosition];

            for (int x = 0; x < 4; x++) // Per Pixel X
            {
              byte pixdata = 0;
              byte pixdata2 = 0;

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
                  (byte)((pixdata << 4) | pixdata2);
            }
          }
        }
      }
    }

    if (isbpp3[s]) {
      sheetPosition += Constants::Uncompressed3BPPSize;
    } else {
      sheetPosition += Constants::UncompressedSheetSize;
    }
  }

  byte *allgfx16Data = (byte *) allgfx16Ptr;

  for (int i = 0; i < 0x6F800; i++) {
    allgfx16Data[i] = newData[i];
  }
}

Bitmap::Bitmap(int width, int height, byte *data)
    : width_(width), height_(height), pixel_data_(data) {}

void Bitmap::Create(GLuint *out_texture) {
  // // Read the pixel data from the ROM
  // SDL_RWops * src = SDL_RWFromMem(pixel_data_, 0);
  // // Create the surface from that RW stream
  // SDL_Surface* surface = SDL_LoadBMP_RW(src, SDL_FALSE);
  // GLenum mode = 0;
  // Uint8 bpp = surface->format->BytesPerPixel;
  // Uint32 rm = surface->format->Rmask;
  // if (bpp == 3 && rm == 0x000000ff) mode = GL_RGB;
  // if (bpp == 3 && rm == 0x00ff0000) mode = GL_BGR;
  // if (bpp == 4 && rm == 0x000000ff) mode = GL_RGBA;
  // if (bpp == 4 && rm == 0xff000000) mode = GL_BGRA;

  // GLsizei width = surface->w;
  // GLsizei height = surface->h;
  // GLenum format = mode;
  // GLvoid* pixels = surface->pixels;

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixel_data_);

  *out_texture = image_texture;
}

int Bitmap::GetWidth() { return width_; }
int Bitmap::GetHeight() { return height_; }

// Simple helper function to load an image into a OpenGL texture with common
// settings
bool Bitmap::LoadBitmapFromROM(unsigned char *texture_data, GLuint *out_texture,
                               int *out_width, int *out_height) {
  // Load from file
  int image_width = 0;
  int image_height = 0;
  if (texture_data == NULL)
    return false;

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_CLAMP_TO_EDGE); // This is required on WebGL for non
                                     // power-of-two textures
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

  // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, texture_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

} // namespace Graphics
} // namespace Application
} // namespace yaze
