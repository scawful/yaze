#ifndef YAZE_APP_UTILS_BITMAP_H
#define YAZE_APP_UTILS_BITMAP_H

#include <SDL2/SDL.h>

#include "Core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, char *data)
      : width_(width), height_(height), pixel_data_(data) {}

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

 private:
  int width_;
  int height_;
  char *pixel_data_;
};

static bool isbpp3[core::constants::NumberOfSheets];

int GetPCGfxAddress(char *romData, char id);
char *CreateAllGfxDataRaw(char *romData);
void CreateAllGfxData(char *romData, char *allgfx16Ptr);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif