#ifndef YAZE_APP_SERVICE_HEADLESS_OVERLAY_RENDERER_H_
#define YAZE_APP_SERVICE_HEADLESS_OVERLAY_RENDERER_H_

#include <cstdint>
#include <vector>

namespace yaze {
namespace app {
namespace service {

// CPU pixel-buffer rasterizer — draws filled rects, outlines, and lines
// directly into an RGBA byte buffer. Replaces ImDrawList* for headless
// (off-screen) rendering. Knows nothing about ROM format, HTTP, or ImGui.
//
// Coordinate space: pixel coordinates relative to top-left of the buffer,
// before scale is applied. All draw calls apply the scale factor so callers
// can work in "room tile" space (8px/tile at scale 1.0) and pass scale > 1
// for upsampled output.
class HeadlessOverlayRenderer {
 public:
  // rgba: RGBA byte buffer (4 bytes per pixel, row-major).
  // width/height: dimensions of the buffer in pixels.
  // scale: pixel scale factor — multiply all coordinates/sizes by this.
  HeadlessOverlayRenderer(std::vector<uint8_t>& rgba, int width, int height,
                          float scale = 1.0f);

  // Filled rectangle with alpha blending.
  void DrawFilledRect(float x, float y, float w, float h, uint8_t r, uint8_t g,
                      uint8_t b, uint8_t a);

  // Outlined rectangle (border only), 1px thick.
  void DrawRect(float x, float y, float w, float h, uint8_t r, uint8_t g,
                uint8_t b, uint8_t a);

  // Bresenham line.
  void DrawLine(float x0, float y0, float x1, float y1, uint8_t r, uint8_t g,
                uint8_t b, uint8_t a);

 private:
  // Alpha-blend src onto dst at pixel (px, py). Out-of-bounds writes are
  // silently ignored.
  void BlendPixel(int px, int py, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

  std::vector<uint8_t>& rgba_;
  int width_;
  int height_;
  float scale_;
};

}  // namespace service
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_SERVICE_HEADLESS_OVERLAY_RENDERER_H_
