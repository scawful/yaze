#include "app/service/headless_overlay_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace yaze {
namespace app {
namespace service {

HeadlessOverlayRenderer::HeadlessOverlayRenderer(std::vector<uint8_t>& rgba,
                                                 int width, int height,
                                                 float scale)
    : rgba_(rgba), width_(width), height_(height), scale_(scale) {}

void HeadlessOverlayRenderer::BlendPixel(int px, int py, uint8_t r, uint8_t g,
                                         uint8_t b, uint8_t a) {
  if (px < 0 || px >= width_ || py < 0 || py >= height_)
    return;
  const size_t base = static_cast<size_t>((py * width_ + px) * 4);
  if (base + 3 >= rgba_.size())
    return;

  if (a == 255) {
    rgba_[base + 0] = r;
    rgba_[base + 1] = g;
    rgba_[base + 2] = b;
    rgba_[base + 3] = 255;
    return;
  }

  // Porter-Duff src-over alpha blend.
  const float fa = a / 255.0f;
  const float fb = 1.0f - fa;
  rgba_[base + 0] = static_cast<uint8_t>(r * fa + rgba_[base + 0] * fb);
  rgba_[base + 1] = static_cast<uint8_t>(g * fa + rgba_[base + 1] * fb);
  rgba_[base + 2] = static_cast<uint8_t>(b * fa + rgba_[base + 2] * fb);
  rgba_[base + 3] = static_cast<uint8_t>(255 * fa + rgba_[base + 3] * fb);
}

void HeadlessOverlayRenderer::DrawFilledRect(float x, float y, float w, float h,
                                             uint8_t r, uint8_t g, uint8_t b,
                                             uint8_t a) {
  const int x0 = static_cast<int>(std::floor(x * scale_));
  const int y0 = static_cast<int>(std::floor(y * scale_));
  const int x1 = static_cast<int>(std::ceil((x + w) * scale_));
  const int y1 = static_cast<int>(std::ceil((y + h) * scale_));
  for (int py = y0; py < y1; ++py) {
    for (int px = x0; px < x1; ++px) {
      BlendPixel(px, py, r, g, b, a);
    }
  }
}

void HeadlessOverlayRenderer::DrawRect(float x, float y, float w, float h,
                                       uint8_t r, uint8_t g, uint8_t b,
                                       uint8_t a) {
  const int x0 = static_cast<int>(std::floor(x * scale_));
  const int y0 = static_cast<int>(std::floor(y * scale_));
  const int x1 = static_cast<int>(std::ceil((x + w) * scale_)) - 1;
  const int y1 = static_cast<int>(std::ceil((y + h) * scale_)) - 1;

  // Top and bottom edges.
  for (int px = x0; px <= x1; ++px) {
    BlendPixel(px, y0, r, g, b, a);
    BlendPixel(px, y1, r, g, b, a);
  }
  // Left and right edges.
  for (int py = y0 + 1; py < y1; ++py) {
    BlendPixel(x0, py, r, g, b, a);
    BlendPixel(x1, py, r, g, b, a);
  }
}

void HeadlessOverlayRenderer::DrawLine(float x0, float y0, float x1, float y1,
                                       uint8_t r, uint8_t g, uint8_t b,
                                       uint8_t a) {
  int px0 = static_cast<int>(x0 * scale_);
  int py0 = static_cast<int>(y0 * scale_);
  int px1 = static_cast<int>(x1 * scale_);
  int py1 = static_cast<int>(y1 * scale_);

  // Bresenham's line algorithm.
  const int dx = std::abs(px1 - px0);
  const int dy = std::abs(py1 - py0);
  const int sx = (px0 < px1) ? 1 : -1;
  const int sy = (py0 < py1) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    BlendPixel(px0, py0, r, g, b, a);
    if (px0 == px1 && py0 == py1)
      break;
    const int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      px0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      py0 += sy;
    }
  }
}

}  // namespace service
}  // namespace app
}  // namespace yaze
