#ifndef YAZE_APPLICATION_UTILS_BITMAP_H
#define YAZE_APPLICATION_UTILS_BITMAP_H

#include "GL/glew.h"
#include <SDL2/SDL_opengl.h>

#include "Utils/ROM.h"

namespace yaze {
namespace Application {
namespace Graphics {
class Bitmap {
 public:
  Bitmap();

  bool LoadBitmapFromROM(unsigned char* texture_data, GLuint* out_texture,
                         int* out_width, int* out_height);
};
}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif