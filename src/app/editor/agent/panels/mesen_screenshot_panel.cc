#include "app/editor/agent/panels/mesen_screenshot_panel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

extern "C" {
#include <png.h>
}

namespace yaze {
namespace editor {

namespace {

// Minimal in-memory PNG read context for libpng callbacks.
struct PngMemoryReader {
  const uint8_t* data;
  size_t offset;
  size_t size;
};

void PngReadFromMemory(png_structp png_ptr, png_bytep out, png_size_t count) {
  auto* reader = static_cast<PngMemoryReader*>(png_get_io_ptr(png_ptr));
  if (reader->offset + count > reader->size) {
    png_error(png_ptr, "Read past end of PNG data");
    return;
  }
  std::memcpy(out, reader->data + reader->offset, count);
  reader->offset += count;
}

}  // namespace

// ---------------------------------------------------------------------------
// Base64 decoder (RFC 4648, no external dependency)
// ---------------------------------------------------------------------------

std::vector<uint8_t> MesenScreenshotPanel::DecodeBase64(
    const std::string& encoded) {
  // clang-format off
  static constexpr int kTable[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  };
  // clang-format on

  std::vector<uint8_t> out;
  out.reserve((encoded.size() / 4) * 3);

  uint32_t accum = 0;
  int bits = 0;

  for (unsigned char c : encoded) {
    int val = kTable[c];
    if (val < 0) continue;  // Skip whitespace, padding, invalid chars
    accum = (accum << 6) | static_cast<uint32_t>(val);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
    }
  }

  return out;
}

// ---------------------------------------------------------------------------
// PNG-to-RGBA decoder using libpng (already in the source tree)
// ---------------------------------------------------------------------------

bool MesenScreenshotPanel::DecodePngToRgba(const std::vector<uint8_t>& png_data,
                                           std::vector<uint8_t>& rgba_out,
                                           int& width_out, int& height_out) {
  if (png_data.size() < 8) return false;

  // Verify PNG signature
  if (png_sig_cmp(reinterpret_cast<png_bytep>(
                      const_cast<uint8_t*>(png_data.data())),
                  0, 8) != 0) {
    return false;
  }

  png_structp png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) return false;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return false;
  }

  PngMemoryReader reader{png_data.data(), 0, png_data.size()};
  png_set_read_fn(png_ptr, &reader, PngReadFromMemory);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 w = png_get_image_width(png_ptr, info_ptr);
  png_uint_32 h = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  // Normalize all formats to 8-bit RGBA
  if (bit_depth == 16) png_set_strip_16(png_ptr);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  width_out = static_cast<int>(w);
  height_out = static_cast<int>(h);
  rgba_out.resize(w * h * 4);

  std::vector<png_bytep> row_pointers(h);
  for (png_uint_32 y = 0; y < h; ++y) {
    row_pointers[y] = rgba_out.data() + y * w * 4;
  }

  png_read_image(png_ptr, row_pointers.data());
  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

  return true;
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

MesenScreenshotPanel::MesenScreenshotPanel() {
  client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  RefreshSocketList();
  if (!socket_paths_.empty()) {
    selected_socket_index_ = 0;
    std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                  socket_paths_[0].c_str());
  }
}

MesenScreenshotPanel::~MesenScreenshotPanel() { DestroyTexture(); }

// ---------------------------------------------------------------------------
// Connection management (mirrors MesenDebugPanel)
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::SetClient(
    std::shared_ptr<emu::mesen::MesenSocketClient> client) {
  client_ = std::move(client);
  emu::mesen::MesenClientRegistry::SetClient(client_);
}

bool MesenScreenshotPanel::IsConnected() const {
  return client_ && client_->IsConnected();
}

void MesenScreenshotPanel::Connect() {
  if (!client_) {
    client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  }
  auto status = client_->Connect();
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    emu::mesen::MesenClientRegistry::SetClient(client_);
  }
}

void MesenScreenshotPanel::ConnectToPath(const std::string& socket_path) {
  if (!client_) {
    client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  }
  auto status = client_->Connect(socket_path);
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    emu::mesen::MesenClientRegistry::SetClient(client_);
  }
}

void MesenScreenshotPanel::Disconnect() {
  if (client_) {
    client_->Disconnect();
  }
  streaming_ = false;
}

void MesenScreenshotPanel::RefreshSocketList() {
  socket_paths_ = emu::mesen::MesenSocketClient::ListAvailableSockets();
  if (!socket_paths_.empty()) {
    if (selected_socket_index_ < 0 ||
        selected_socket_index_ >= static_cast<int>(socket_paths_.size())) {
      selected_socket_index_ = 0;
    }
    if (socket_path_buffer_[0] == '\0') {
      std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                    socket_paths_[selected_socket_index_].c_str());
    }
  } else {
    selected_socket_index_ = -1;
  }
}

// ---------------------------------------------------------------------------
// Texture management
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::EnsureTexture(int width, int height) {
  if (texture_ && texture_width_ == width && texture_height_ == height) {
    return;  // Reuse existing texture
  }
  DestroyTexture();

  // Create an SDL streaming texture in RGBA byte order.
  // In the ImGui+SDL2 backend, SDL_Texture* is used directly as ImTextureID.
  SDL_Renderer* renderer = nullptr;

  // Get the renderer from the current SDL window (ImGui backend owns it)
  SDL_Window* window = SDL_GetMouseFocus();
  if (!window) {
    window = SDL_GetKeyboardFocus();
  }
  if (window) {
    renderer = SDL_GetRenderer(window);
  }

  if (!renderer) {
    return;  // No renderer available yet
  }

  // Use RGBA32 so the input byte order is RGBA on both little and big endian.
  texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                               SDL_TEXTUREACCESS_STREAMING, width, height);
  if (texture_) {
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    texture_width_ = width;
    texture_height_ = height;
  }
}

void MesenScreenshotPanel::UpdateTexture(const std::vector<uint8_t>& rgba,
                                         int width, int height) {
  EnsureTexture(width, height);
  if (!texture_) return;

  // Our decoder produces RGBA bytes; the texture uses SDL_PIXELFORMAT_RGBA32 so
  // the byte order matches across endianness.
  SDL_UpdateTexture(texture_, nullptr, rgba.data(), width * 4);
}

void MesenScreenshotPanel::DestroyTexture() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
    texture_width_ = 0;
    texture_height_ = 0;
  }
}

// ---------------------------------------------------------------------------
// Screenshot capture pipeline
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::CaptureScreenshot() {
  if (!IsConnected()) return;

  auto t0 = std::chrono::steady_clock::now();

  auto result = client_->Screenshot();
  if (!result.ok()) {
    frame_stale_ = true;
    status_message_ = std::string(result.status().message());
    return;
  }

  // Decode base64 -> PNG bytes -> RGBA pixels
  std::vector<uint8_t> png_bytes = DecodeBase64(*result);
  if (png_bytes.empty()) {
    frame_stale_ = true;
    status_message_ = "Base64 decode failed";
    return;
  }

  std::vector<uint8_t> rgba;
  int w = 0, h = 0;
  if (!DecodePngToRgba(png_bytes, rgba, w, h)) {
    frame_stale_ = true;
    status_message_ = "PNG decode failed";
    return;
  }

  // Upload to GPU texture
  UpdateTexture(rgba, w, h);

  auto t1 = std::chrono::steady_clock::now();
  last_capture_latency_ms_ = std::chrono::duration<float, std::milli>(t1 - t0).count();

  frame_width_ = w;
  frame_height_ = h;
  frame_counter_++;
  frame_stale_ = false;
  status_message_.clear();
}

// ---------------------------------------------------------------------------
// Main Draw
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::Draw() {
  ImGui::PushID("MesenScreenshotPanel");

  // Timer-driven streaming capture
  if (IsConnected() && streaming_) {
    time_accumulator_ += ImGui::GetIO().DeltaTime;
    float interval = 1.0f / target_fps_;
    if (time_accumulator_ >= interval) {
      CaptureScreenshot();
      time_accumulator_ = 0.0f;
    }
  }

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("MesenScreenshot_Panel", ImVec2(0, 0), true)) {
    if (ImGui::IsWindowAppearing()) {
      RefreshSocketList();
    }
    DrawConnectionHeader();

    if (IsConnected()) {
      ImGui::Spacing();
      DrawControlsToolbar();
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      DrawPreviewArea();
      ImGui::Spacing();
      DrawStatusBar();
    }
  }
  ImGui::EndChild();
  AgentUI::PopPanelStyle();

  ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Connection header (same pattern as MesenDebugPanel)
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::DrawConnectionHeader() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::TextColored(theme.accent_color, "%s Mesen2 Screenshot Preview",
                     ICON_MD_CAMERA_ALT);

  // Connection status indicator
  ImGui::SameLine(ImGui::GetWindowWidth() - 100);
  if (IsConnected()) {
    float pulse = 0.7f + 0.3f * std::sin(ImGui::GetTime() * 2.0f);
    ImVec4 color = ImVec4(0.1f, pulse, 0.3f, 1.0f);
    ImGui::TextColored(color, "%s Connected", ICON_MD_CHECK_CIRCLE);
  } else {
    ImGui::TextColored(theme.status_error, "%s Disconnected", ICON_MD_ERROR);
  }

  ImGui::Separator();

  if (!IsConnected()) {
    ImGui::TextDisabled("Socket");
    const char* preview =
        (selected_socket_index_ >= 0 &&
         selected_socket_index_ < static_cast<int>(socket_paths_.size()))
            ? socket_paths_[selected_socket_index_].c_str()
            : "No sockets found";
    ImGui::SetNextItemWidth(-40);
    if (ImGui::BeginCombo("##ss_socket_combo", preview)) {
      for (int i = 0; i < static_cast<int>(socket_paths_.size()); ++i) {
        bool selected = (i == selected_socket_index_);
        if (ImGui::Selectable(socket_paths_[i].c_str(), selected)) {
          selected_socket_index_ = i;
          std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                        socket_paths_[i].c_str());
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_REFRESH "##ss_refresh")) {
      RefreshSocketList();
    }

    ImGui::TextDisabled("Path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##ss_socket_path", "/tmp/mesen2-12345.sock",
                             socket_path_buffer_, sizeof(socket_path_buffer_));

    if (ImGui::Button(ICON_MD_LINK " Connect")) {
      std::string path = socket_path_buffer_;
      if (path.empty() && selected_socket_index_ >= 0 &&
          selected_socket_index_ < static_cast<int>(socket_paths_.size())) {
        path = socket_paths_[selected_socket_index_];
      }
      if (path.empty()) {
        Connect();
      } else {
        ConnectToPath(path);
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_AUTO_MODE " Auto")) {
      Connect();
    }
    if (!connection_error_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(theme.status_error, "%s", connection_error_.c_str());
    }
  } else {
    if (ImGui::Button(ICON_MD_LINK_OFF " Disconnect")) {
      Disconnect();
    }
  }
}

// ---------------------------------------------------------------------------
// Controls toolbar
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::DrawControlsToolbar() {
  // Play / Pause toggle
  if (streaming_) {
    if (ImGui::Button(ICON_MD_PAUSE " Pause")) {
      streaming_ = false;
    }
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Stream")) {
      streaming_ = true;
      time_accumulator_ = 999.0f;  // Trigger immediate first capture
    }
  }

  ImGui::SameLine();

  // FPS slider
  ImGui::SetNextItemWidth(120);
  ImGui::SliderFloat("##ss_fps", &target_fps_, 1.0f, 30.0f, "%.0f FPS");

  ImGui::SameLine();

  // One-shot capture button
  if (ImGui::Button(ICON_MD_PHOTO_CAMERA " Capture")) {
    CaptureScreenshot();
  }
}

// ---------------------------------------------------------------------------
// Preview area (aspect-ratio-preserved image)
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::DrawPreviewArea() {
  ImVec2 avail = ImGui::GetContentRegionAvail();

  if (!texture_ || frame_width_ <= 0 || frame_height_ <= 0) {
    // Placeholder when no frame is available
    float placeholder_h = std::max(avail.y - 40.0f, 100.0f);
    ImVec2 center(avail.x * 0.5f, placeholder_h * 0.5f);
    ImGui::BeginChild("##ss_placeholder", ImVec2(0, placeholder_h), true);
    ImGui::SetCursorPos(ImVec2(center.x - 80, center.y - 10));
    ImGui::TextDisabled("No screenshot captured");
    ImGui::EndChild();
    return;
  }

  // Compute display size preserving SNES aspect ratio
  float src_aspect =
      static_cast<float>(frame_width_) / static_cast<float>(frame_height_);
  float max_h = std::max(avail.y - 60.0f, 100.0f);  // Reserve space for info
  float display_w = avail.x;
  float display_h = display_w / src_aspect;

  if (display_h > max_h) {
    display_h = max_h;
    display_w = display_h * src_aspect;
  }

  // Center horizontally
  float offset_x = (avail.x - display_w) * 0.5f;
  if (offset_x > 0) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
  }

  ImGui::Image(reinterpret_cast<ImTextureID>(texture_),
               ImVec2(display_w, display_h), ImVec2(0, 0), ImVec2(1, 1));

  // Stale indicator overlay
  if (frame_stale_) {
    ImVec2 img_min = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddRectFilled(
        img_min, ImVec2(img_min.x + 50, img_min.y + 20),
        IM_COL32(200, 60, 60, 200));
    ImGui::GetWindowDrawList()->AddText(ImVec2(img_min.x + 4, img_min.y + 2),
                                        IM_COL32(255, 255, 255, 255), "Stale");
  }

  // Info line below the image
  const auto& theme = AgentUI::GetTheme();
  ImGui::TextColored(theme.text_secondary_color,
                     "Frame #%llu  |  %dx%d  |  Latency: %.1f ms",
                     frame_counter_, frame_width_, frame_height_,
                     last_capture_latency_ms_);
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void MesenScreenshotPanel::DrawStatusBar() {
  const auto& theme = AgentUI::GetTheme();
  ImGui::Separator();

  if (IsConnected() && !socket_path_buffer_[0]) {
    ImGui::TextColored(theme.text_secondary_color, "Connected (auto-detect)");
  } else if (IsConnected()) {
    ImGui::TextColored(theme.text_secondary_color, "Connected to %s",
                       socket_path_buffer_);
  }

  if (streaming_) {
    ImGui::SameLine();
    float pulse = 0.7f + 0.3f * std::sin(ImGui::GetTime() * 3.0f);
    ImGui::TextColored(ImVec4(0.2f, pulse, 0.2f, 1.0f),
                       ICON_MD_FIBER_MANUAL_RECORD " Streaming at %.0f FPS",
                       target_fps_);
  } else if (IsConnected()) {
    ImGui::SameLine();
    ImGui::TextDisabled("Paused");
  }

  if (!status_message_.empty()) {
    ImGui::TextColored(theme.status_error, "%s", status_message_.c_str());
  }
}

}  // namespace editor
}  // namespace yaze
