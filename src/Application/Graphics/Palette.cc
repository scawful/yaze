#include "Palette.h"

#include <cstdlib>
#include <cstring>

namespace yaze {
namespace Application {
namespace Graphics {

SNESColor::SNESColor() {
  rgb = ImVec4(0.f, 0.f, 0.f, 0.f);
  snes = 0;
}

void SNESColor::setRgb(ImVec4 val) {
  rgb = val;
  m_color col;
  col.red = val.x;
  col.blue = val.y;
  col.green = val.z;
  snes = convertcolor_rgb_to_snes(col);
}

void SNESColor::setSNES(uint16_t val) {
  snes = val;
  m_color col = convertcolor_snes_to_rgb(val);
  rgb = ImVec4(col.red, col.green, col.blue, 1.f);
}

SNESPalette::SNESPalette() { size = 0; }

SNESPalette::SNESPalette(uint8_t mSize) {
  size = mSize;
  for (unsigned int i = 0; i < mSize; i++) {
    SNESColor col;
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(char* data) {
  //assert((data.size() % 4 == 0) && data.size() <= 32);
  //size = data.size() / 2;
  size = sizeof(data) / 2;
  for (unsigned i = 0; i < sizeof(data); i += 2) {
    SNESColor col;
    col.snes = static_cast<uchar>(data[i + 1]) << 8;
    col.snes = col.snes | static_cast<uchar>(data[i]);
    m_color mColor = convertcolor_snes_to_rgb(col.snes);
    col.rgb = ImVec4(mColor.red, mColor.green, mColor.blue, 1.f);
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(std::vector<ImVec4> cols) {
  // foreach (ImVec4 col, cols) {
  //   SNESColor scol;
  //   scol.setRgb(col);
  //   colors.push_back(scol);
  // }
  // size = cols.size();
}

char* SNESPalette::encode() {
  //char* data(size * 2, 0);
  char* data = new char[size * 2];
  for (unsigned int i = 0; i < size; i++) {
    //std::cout << QString::number(colors[i].snes, 16);
    data[i * 2] = (char)(colors[i].snes & 0xFF);
    data[i * 2 + 1] = (char)(colors[i].snes >> 8);
  }
  return data;
}

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze