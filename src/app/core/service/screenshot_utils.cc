#include "app/core/service/screenshot_utils.h"

#ifdef YAZE_WITH_GRPC

#include <SDL.h>

#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace yaze {
namespace test {

namespace {

struct ImGui_ImplSDLRenderer2_Data {
  SDL_Renderer* Renderer;
};

std::filesystem::path DefaultScreenshotPath() {
  std::filesystem::path base_dir =
      std::filesystem::temp_directory_path() / "yaze" / "test-results";
  std::error_code ec;
  std::filesystem::create_directories(base_dir, ec);

  const int64_t timestamp_ms = absl::ToUnixMillis(absl::Now());
  return base_dir /
         std::filesystem::path(
             absl::StrFormat("harness_%lld.bmp", static_cast<long long>(timestamp_ms)));
}

}  // namespace

absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshot(
    const std::string& preferred_path) {
  ImGuiIO& io = ImGui::GetIO();
  auto* backend_data =
      static_cast<ImGui_ImplSDLRenderer2_Data*>(io.BackendRendererUserData);

  if (!backend_data || !backend_data->Renderer) {
    return absl::FailedPreconditionError("SDL renderer not available");
  }

  SDL_Renderer* renderer = backend_data->Renderer;
  int width = 0;
  int height = 0;
  if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to get renderer size: %s", SDL_GetError()));
  }

  std::filesystem::path output_path = preferred_path.empty()
                                         ? DefaultScreenshotPath()
                                         : std::filesystem::path(preferred_path);
  if (output_path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
  }

  SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000,
                                              0x0000FF00, 0x000000FF,
                                              0xFF000000);
  if (!surface) {
    return absl::InternalError(
        absl::StrFormat("Failed to create SDL surface: %s", SDL_GetError()));
  }

  if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                           surface->pixels, surface->pitch) != 0) {
    SDL_FreeSurface(surface);
    return absl::InternalError(
        absl::StrFormat("Failed to read renderer pixels: %s", SDL_GetError()));
  }

  if (SDL_SaveBMP(surface, output_path.string().c_str()) != 0) {
    SDL_FreeSurface(surface);
    return absl::InternalError(
        absl::StrFormat("Failed to save BMP: %s", SDL_GetError()));
  }

  SDL_FreeSurface(surface);

  std::error_code ec;
  const int64_t file_size =
      std::filesystem::file_size(output_path, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to stat screenshot %s: %s",
                        output_path.string(), ec.message()));
  }

  ScreenshotArtifact artifact;
  artifact.file_path = output_path.string();
  artifact.width = width;
  artifact.height = height;
  artifact.file_size_bytes = file_size;
  return artifact;
}

absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshotRegion(
    const std::optional<CaptureRegion>& region,
    const std::string& preferred_path) {
  ImGuiIO& io = ImGui::GetIO();
  auto* backend_data =
      static_cast<ImGui_ImplSDLRenderer2_Data*>(io.BackendRendererUserData);

  if (!backend_data || !backend_data->Renderer) {
    return absl::FailedPreconditionError("SDL renderer not available");
  }

  SDL_Renderer* renderer = backend_data->Renderer;
  
  // Get full renderer size
  int full_width = 0;
  int full_height = 0;
  if (SDL_GetRendererOutputSize(renderer, &full_width, &full_height) != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to get renderer size: %s", SDL_GetError()));
  }

  // Determine capture region
  int capture_x = 0;
  int capture_y = 0;
  int capture_width = full_width;
  int capture_height = full_height;
  
  if (region.has_value()) {
    capture_x = region->x;
    capture_y = region->y;
    capture_width = region->width;
    capture_height = region->height;
    
    // Clamp to renderer bounds
    if (capture_x < 0) capture_x = 0;
    if (capture_y < 0) capture_y = 0;
    if (capture_x + capture_width > full_width) {
      capture_width = full_width - capture_x;
    }
    if (capture_y + capture_height > full_height) {
      capture_height = full_height - capture_y;
    }
    
    if (capture_width <= 0 || capture_height <= 0) {
      return absl::InvalidArgumentError("Invalid capture region");
    }
  }

  std::filesystem::path output_path = preferred_path.empty()
                                         ? DefaultScreenshotPath()
                                         : std::filesystem::path(preferred_path);
  if (output_path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
  }

  // Create surface for the capture region
  SDL_Surface* surface = SDL_CreateRGBSurface(0, capture_width, capture_height, 
                                              32, 0x00FF0000, 0x0000FF00, 
                                              0x000000FF, 0xFF000000);
  if (!surface) {
    return absl::InternalError(
        absl::StrFormat("Failed to create SDL surface: %s", SDL_GetError()));
  }

  // Read pixels from the specified region
  SDL_Rect region_rect = {capture_x, capture_y, capture_width, capture_height};
  if (SDL_RenderReadPixels(renderer, &region_rect, SDL_PIXELFORMAT_ARGB8888,
                           surface->pixels, surface->pitch) != 0) {
    SDL_FreeSurface(surface);
    return absl::InternalError(
        absl::StrFormat("Failed to read renderer pixels: %s", SDL_GetError()));
  }

  if (SDL_SaveBMP(surface, output_path.string().c_str()) != 0) {
    SDL_FreeSurface(surface);
    return absl::InternalError(
        absl::StrFormat("Failed to save BMP: %s", SDL_GetError()));
  }

  SDL_FreeSurface(surface);

  std::error_code ec;
  const int64_t file_size = std::filesystem::file_size(output_path, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to stat screenshot %s: %s",
                        output_path.string(), ec.message()));
  }

  ScreenshotArtifact artifact;
  artifact.file_path = output_path.string();
  artifact.width = capture_width;
  artifact.height = capture_height;
  artifact.file_size_bytes = file_size;
  return artifact;
}

absl::StatusOr<ScreenshotArtifact> CaptureActiveWindow(
    const std::string& preferred_path) {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx || !ctx->NavWindow) {
    return absl::FailedPreconditionError("No active ImGui window");
  }

  ImGuiWindow* window = ctx->NavWindow;
  CaptureRegion region;
  region.x = static_cast<int>(window->Pos.x);
  region.y = static_cast<int>(window->Pos.y);
  region.width = static_cast<int>(window->Size.x);
  region.height = static_cast<int>(window->Size.y);

  return CaptureHarnessScreenshotRegion(region, preferred_path);
}

absl::StatusOr<ScreenshotArtifact> CaptureWindowByName(
    const std::string& window_name,
    const std::string& preferred_path) {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx) {
    return absl::FailedPreconditionError("No ImGui context");
  }

  ImGuiWindow* window = ImGui::FindWindowByName(window_name.c_str());
  if (!window) {
    return absl::NotFoundError(
        absl::StrFormat("Window '%s' not found", window_name));
  }

  CaptureRegion region;
  region.x = static_cast<int>(window->Pos.x);
  region.y = static_cast<int>(window->Pos.y);
  region.width = static_cast<int>(window->Size.x);
  region.height = static_cast<int>(window->Size.y);

  return CaptureHarnessScreenshotRegion(region, preferred_path);
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
