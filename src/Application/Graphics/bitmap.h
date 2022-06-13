#ifndef YAZE_APPLICATION_UTILS_BITMAP_H
#define YAZE_APPLICATION_UTILS_BITMAP_H

#include <SDL2/SDL.h>

#include "Core/constants.h"

namespace yaze {
namespace Application {
namespace Graphics {

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

static bool isbpp3[Core::Constants::NumberOfSheets];

int GetPCGfxAddress(char *romData, char id);
char *CreateAllGfxDataRaw(char *romData);
void CreateAllGfxData(char *romData, char *allgfx16Ptr);

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif