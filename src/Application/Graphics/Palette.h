#ifndef YAZE_APPLICATION_GRAPHICS_PALETTE_H
#define YAZE_APPLICATION_GRAPHICS_PALETTE_H

namespace yaze {
namespace Application {
namespace Graphics {

extern "C" {

typedef unsigned char uchar;

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} m_color;

typedef struct {
  unsigned int id;
  unsigned int size;
  m_color* colors;
} r_palette;

r_palette* palette_create(const unsigned int size, const unsigned int id);
void palette_free(r_palette* tofree);

r_palette* palette_extract(const char* data, const unsigned int offset,
                           const unsigned int palette_size);

char* palette_convert(const r_palette pal);

m_color convertcolor_snes_to_rgb(const unsigned short snes_color);
unsigned short convertcolor_rgb_to_snes(const m_color color);
unsigned short convertcolor_rgb_to_snes2(const uchar red, const uchar green,
                                         const uchar blue);
}

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_GRAPHICS_PALETTE_H