#ifndef YAZE_APP_EDITOR_AGENT_PANELS_MESEN_SCREENSHOT_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_MESEN_SCREENSHOT_PANEL_H_

#include <memory>
#include <string>
#include <vector>

#include "app/emu/mesen/mesen_socket_client.h"
#include "app/platform/sdl_compat.h"

namespace yaze {
namespace editor {

/**
 * @brief ImGui panel that streams live screenshots from a running Mesen2
 * emulator via socket and displays them as a GPU texture preview.
 *
 * The panel polls the Mesen2 Screenshot() API at a configurable rate,
 * decodes the base64-encoded PNG response to RGBA pixels, uploads them
 * to an SDL texture, and renders the result with ImGui::Image().
 */
class MesenScreenshotPanel {
 public:
  MesenScreenshotPanel();
  ~MesenScreenshotPanel();

  // Non-copyable, non-movable (owns GPU texture)
  MesenScreenshotPanel(const MesenScreenshotPanel&) = delete;
  MesenScreenshotPanel& operator=(const MesenScreenshotPanel&) = delete;

  void Draw();

  void SetClient(std::shared_ptr<emu::mesen::MesenSocketClient> client);
  bool IsConnected() const;
  void Connect();
  void ConnectToPath(const std::string& socket_path);
  void Disconnect();

 private:
  void DrawConnectionHeader();
  void DrawControlsToolbar();
  void DrawPreviewArea();
  void DrawStatusBar();

  void RefreshSocketList();
  void CaptureScreenshot();
  void UpdateTexture(const std::vector<uint8_t>& rgba, int width, int height);
  void EnsureTexture(int width, int height);
  void DestroyTexture();

  // Base64 + PNG decode pipeline
  static std::vector<uint8_t> DecodeBase64(const std::string& encoded);
  static bool DecodePngToRgba(const std::vector<uint8_t>& png_data,
                              std::vector<uint8_t>& rgba_out, int& width_out,
                              int& height_out);

  std::shared_ptr<emu::mesen::MesenSocketClient> client_;

  // Texture state
  SDL_Texture* texture_ = nullptr;
  int texture_width_ = 0;
  int texture_height_ = 0;

  // Captured frame state
  int frame_width_ = 0;
  int frame_height_ = 0;
  uint64_t frame_counter_ = 0;
  float last_capture_latency_ms_ = 0.0f;
  bool frame_stale_ = false;

  // UI controls
  bool streaming_ = false;
  float target_fps_ = 10.0f;
  float time_accumulator_ = 0.0f;

  // Connection UI state
  std::string connection_error_;
  std::vector<std::string> socket_paths_;
  int selected_socket_index_ = -1;
  char socket_path_buffer_[256] = {};
  std::string status_message_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_MESEN_SCREENSHOT_PANEL_H_
