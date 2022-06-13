#ifndef YAZE_APPLICATION_UTILS_BITMAP_H
#define YAZE_APPLICATION_UTILS_BITMAP_H

#include <SDL2/SDL.h>
#include <SDL_opengl.h>

#include <memory>

#include "Core/Constants.h"

namespace yaze {
namespace Application {
namespace Graphics {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, char *data);

  void Create(GLuint *out_texture);
  int GetWidth();
  int GetHeight();

  bool LoadBitmapFromROM(unsigned char *texture_data, GLuint *out_texture,
                         int *out_width, int *out_height);

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