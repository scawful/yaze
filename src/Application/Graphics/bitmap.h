#ifndef YAZE_APPLICATION_UTILS_BITMAP_H
#define YAZE_APPLICATION_UTILS_BITMAP_H

#include <SDL2/SDL.h>

#include <memory>

#include "Core/constants.h"

namespace yaze {
namespace Application {
namespace Graphics {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, char *data);

  int GetWidth();
  int GetHeight();

  bool LoadBitmapFromROM(unsigned char *texture_data, int *out_width,
                         int *out_height);

 private:
  int width_;
  int height_;
  char *pixel_data_;
};

static bool isbpp3[Core::Constants::NumberOfSheets];

int GetPCGfxAddress(char *romData, char id);
char *CreateAllGfxDataRaw(char *romData);
void CreateAllGfxData(char *romData, char *allgfx16Ptr);

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif