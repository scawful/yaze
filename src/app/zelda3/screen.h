#ifndef YAZE_APP_ZELDA3_SCREEN_H
#define YAZE_APP_ZELDA3_SCREEN_H

#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {
namespace zelda3 {

class Screen {
 public:
  Screen() = default;

 private:
  gfx::Bitmap screen;
  uchar *data = nullptr;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze