#include "app/service/screenshot_utils.h"

#ifdef YAZE_WITH_GRPC

#include "app/platform/sdl_compat.h"

// SDL includes Windows headers whose macros conflict with protobuf/Abseil.
#ifdef _WIN32
#ifdef DWORD
#undef DWORD
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef OVERFLOW
#undef OVERFLOW
#endif
#ifdef IGNORE
#undef IGNORE
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#ifdef YAZE_SCREENSHOT_HAS_PNG
#include <png.h>
#endif

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "util/macro.h"
#include "util/platform_paths.h"

namespace yaze::test {
namespace {

using SurfacePtr =
    std::unique_ptr<SDL_Surface, decltype(&platform::DestroySurface)>;

const char* ExtensionForFormat(ScreenshotFormat format) {
  return format == ScreenshotFormat::kPng ? ".png" : ".bmp";
}

absl::StatusOr<SDL_Renderer*> GetScreenshotRenderer() {
  if (!ImGui::GetCurrentContext()) {
    return absl::FailedPreconditionError("No ImGui context");
  }
  const ImGuiIO& io = ImGui::GetIO();
#ifdef YAZE_USE_SDL3
  constexpr const char* kBackendName = "imgui_impl_sdlrenderer3";
#else
  constexpr const char* kBackendName = "imgui_impl_sdlrenderer2";
#endif
  if (!io.BackendRendererName ||
      std::strcmp(io.BackendRendererName, kBackendName) != 0) {
    return absl::UnimplementedError(
        "Screenshot capture requires the SDL renderer backend");
  }
  if (!io.BackendRendererUserData) {
    return absl::FailedPreconditionError("SDL renderer not available");
  }
  // Both supported ImGui SDL renderer backends store the main renderer as
  // their first field. Check the backend above before reading that field;
  // Metal/OpenGL backend data has a different layout. Copy avoids aliasing an
  // unrelated private backend struct from this translation unit.
  SDL_Renderer* renderer = nullptr;
  std::memcpy(&renderer, io.BackendRendererUserData, sizeof(renderer));
  if (!renderer) {
    return absl::FailedPreconditionError("SDL renderer not available");
  }
  if (SDL_GetRenderTarget(renderer) != nullptr) {
    return absl::FailedPreconditionError(
        "Screenshot capture requires the main framebuffer render target");
  }
  return renderer;
}

absl::Status GetOutputSize(SDL_Renderer* renderer, int* width, int* height) {
#ifdef YAZE_USE_SDL3
  const bool success = SDL_GetCurrentRenderOutputSize(renderer, width, height);
#else
  const bool success = SDL_GetRendererOutputSize(renderer, width, height) == 0;
#endif
  if (!success) {
    return absl::InternalError(
        absl::StrFormat("Failed to get renderer size: %s", SDL_GetError()));
  }
  if (*width <= 0 || *height <= 0) {
    return absl::FailedPreconditionError("Renderer has no visible framebuffer");
  }
  return absl::OkStatus();
}

absl::StatusOr<SDL_Rect> ClipRegion(const CaptureRegion& region, int width,
                                    int height) {
  if (region.width <= 0 || region.height <= 0) {
    return absl::InvalidArgumentError("Invalid capture region");
  }
  const int64_t left = std::max<int64_t>(0, region.x);
  const int64_t top = std::max<int64_t>(0, region.y);
  const int64_t right =
      std::min<int64_t>(width, static_cast<int64_t>(region.x) + region.width);
  const int64_t bottom =
      std::min<int64_t>(height, static_cast<int64_t>(region.y) + region.height);
  if (right <= left || bottom <= top) {
    return absl::InvalidArgumentError(
        "Capture region is outside the framebuffer");
  }
  return SDL_Rect{static_cast<int>(left), static_cast<int>(top),
                  static_cast<int>(right - left),
                  static_cast<int>(bottom - top)};
}

absl::StatusOr<SurfacePtr> ReadFramebufferRegion(SDL_Renderer* renderer,
                                                 const SDL_Rect& region) {
  // SDL2 intersects readback with the current renderer viewport, even when
  // passed an explicit framebuffer rectangle. ImGui restores the caller's
  // viewport after rendering, so read the full requested region temporarily.
  SDL_Rect previous_viewport;
#ifdef YAZE_USE_SDL3
  if (!SDL_GetRenderViewport(renderer, &previous_viewport)) {
    return absl::InternalError("Failed to get renderer viewport");
  }
  const bool viewport_was_set = SDL_RenderViewportSet(renderer);
  const bool reset = SDL_SetRenderViewport(renderer, nullptr);
#else
  SDL_RenderGetViewport(renderer, &previous_viewport);
  const bool reset = SDL_RenderSetViewport(renderer, nullptr) == 0;
#endif
  SurfacePtr surface(reset ? platform::ReadPixelsToSurface(renderer, region.w,
                                                           region.h, &region)
                           : nullptr,
                     platform::DestroySurface);
#ifdef YAZE_USE_SDL3
  const bool restored = SDL_SetRenderViewport(
      renderer, viewport_was_set ? &previous_viewport : nullptr);
#else
  const bool restored =
      SDL_RenderSetViewport(renderer, &previous_viewport) == 0;
#endif
  if (!restored) {
    return absl::InternalError(absl::StrFormat(
        "Failed to restore screenshot viewport: %s", SDL_GetError()));
  }
  if (!reset) {
    return absl::InternalError(absl::StrFormat(
        "Failed to reset screenshot viewport: %s", SDL_GetError()));
  }
  if (!surface) {
    return absl::InternalError(absl::StrFormat(
        "Failed to read pixels to surface: %s", SDL_GetError()));
  }
  return surface;
}

absl::StatusOr<CaptureRegion> WindowRegion(const ImGuiWindow& window) {
  if (!window.Active || window.Hidden || window.Collapsed) {
    return absl::FailedPreconditionError("Requested window is not visible");
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (window.Viewport != viewport) {
    return absl::UnimplementedError(
        "Detached window screenshots are not supported by the main renderer");
  }
  const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
  if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || scale.x <= 0 ||
      scale.y <= 0) {
    return absl::FailedPreconditionError("Invalid framebuffer scale");
  }
  const double left = std::floor(
      (static_cast<double>(window.Pos.x) - viewport->Pos.x) * scale.x);
  const double top = std::floor(
      (static_cast<double>(window.Pos.y) - viewport->Pos.y) * scale.y);
  const double right = std::ceil(
      (static_cast<double>(window.Pos.x) + window.Size.x - viewport->Pos.x) *
      scale.x);
  const double bottom = std::ceil(
      (static_cast<double>(window.Pos.y) + window.Size.y - viewport->Pos.y) *
      scale.y);
  constexpr double kMinInt = std::numeric_limits<int>::min();
  constexpr double kMaxInt = std::numeric_limits<int>::max();
  if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) ||
      !std::isfinite(bottom) || left < kMinInt || top < kMinInt ||
      right > kMaxInt || bottom > kMaxInt || right <= left || bottom <= top ||
      right - left > kMaxInt || bottom - top > kMaxInt) {
    return absl::InvalidArgumentError("Invalid window framebuffer bounds");
  }
  return CaptureRegion{static_cast<int>(left), static_cast<int>(top),
                       static_cast<int>(right - left),
                       static_cast<int>(bottom - top)};
}

absl::StatusOr<std::filesystem::path> ScreenshotPath(
    const std::string& preferred_path, ScreenshotFormat format) {
  std::filesystem::path path;
  if (!preferred_path.empty()) {
    path = preferred_path;
    if (!path.has_extension()) {
      path += ExtensionForFormat(format);
    }
  } else {
    ASSIGN_OR_RETURN(
        auto directory,
        util::PlatformPaths::GetAppDataSubdirectory("screenshots"));
    path =
        directory /
        absl::StrFormat("yaze_%lld%s",
                        static_cast<long long>(absl::ToUnixMillis(absl::Now())),
                        ExtensionForFormat(format));
  }
  std::error_code error;
  auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return absl::InternalError(absl::StrFormat(
        "Failed to resolve screenshot path: %s", error.message()));
  }
  return absolute;
}

absl::Status SaveSurface(SDL_Surface* surface,
                         const std::filesystem::path& path,
                         ScreenshotFormat format) {
  if (format == ScreenshotFormat::kBmp) {
#ifdef YAZE_USE_SDL3
    const bool success = SDL_SaveBMP(surface, path.string().c_str());
#else
    const bool success = SDL_SaveBMP(surface, path.string().c_str()) == 0;
#endif
    return success ? absl::OkStatus()
                   : absl::InternalError(absl::StrFormat(
                         "Failed to save BMP: %s", SDL_GetError()));
  }
#ifdef YAZE_SCREENSHOT_HAS_PNG
  SurfacePtr rgba(
      platform::ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0),
      platform::DestroySurface);
  if (!rgba) {
    return absl::InternalError(absl::StrFormat(
        "Failed to convert screenshot pixels: %s", SDL_GetError()));
  }
  // Construct C++ owners before setjmp; libpng's error jump must not bypass
  // their initialization/destruction. RGBA32 defines byte order on any endian.
  std::vector<png_bytep> rows(rgba->h);
  for (int y = 0; y < rgba->h; ++y) {
    rows[y] = static_cast<png_bytep>(rgba->pixels) +
              static_cast<size_t>(y) * rgba->pitch;
  }
  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file) {
    return absl::InternalError("Failed to open PNG output file");
  }
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png ? png_create_info_struct(png) : nullptr;
  if (!png || !info) {
    if (png) {
      png_destroy_write_struct(&png, nullptr);
    }
    std::fclose(file);
    return absl::InternalError("Failed to initialize PNG encoder");
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return absl::InternalError("Failed to encode PNG screenshot");
  }
  png_init_io(png, file);
  png_set_IHDR(png, info, rgba->w, rgba->h, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  png_write_image(png, rows.data());
  png_write_end(png, nullptr);
  png_destroy_write_struct(&png, &info);
  if (std::fclose(file) != 0) {
    return absl::InternalError("Failed to finish PNG screenshot file");
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "PNG screenshot encoding unavailable (libpng missing)");
#endif
}

void RevealScreenshot([[maybe_unused]] const std::filesystem::path& path) {
#ifdef __APPLE__
  // Use a file URL, not a shell command containing a caller-controlled path.
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return;
  }
  std::string url = "file://";
  for (const unsigned char byte : absolute.generic_string()) {
    if (absl::ascii_isalnum(byte) || byte == '/' || byte == '-' ||
        byte == '_' || byte == '.' || byte == '~') {
      url += byte;
    } else {
      url += absl::StrFormat("%%%02X", byte);
    }
  }
  (void)SDL_OpenURL(url.c_str());
#endif
}

}  // namespace

absl::StatusOr<ScreenshotFormat> ResolveScreenshotFormat(
    const std::string& path, ScreenshotFormat format) {
  if (format != ScreenshotFormat::kAuto && format != ScreenshotFormat::kPng &&
      format != ScreenshotFormat::kBmp) {
    return absl::InvalidArgumentError("Unknown screenshot format");
  }
  const std::filesystem::path output(path);
  if (!path.empty() &&
      (output.filename().empty() || output.filename() == "." ||
       output.filename() == ".." || path.find('\0') != std::string::npos)) {
    return absl::InvalidArgumentError("Screenshot path must name a file");
  }
  const std::string extension =
      absl::AsciiStrToLower(output.extension().string());
  ScreenshotFormat inferred = ScreenshotFormat::kAuto;
  if (extension == ".png") {
    inferred = ScreenshotFormat::kPng;
  } else if (extension == ".bmp") {
    inferred = ScreenshotFormat::kBmp;
  } else if (!extension.empty()) {
    return absl::InvalidArgumentError(
        "Screenshot extension must be .png or .bmp");
  }
  if (format == ScreenshotFormat::kAuto) {
    format =
        inferred == ScreenshotFormat::kAuto ? ScreenshotFormat::kBmp : inferred;
  } else if (inferred != ScreenshotFormat::kAuto && inferred != format) {
    return absl::InvalidArgumentError(
        "Screenshot extension does not match requested format");
  }
#ifndef YAZE_SCREENSHOT_HAS_PNG
  if (format == ScreenshotFormat::kPng) {
    return absl::UnimplementedError(
        "PNG screenshot encoding unavailable (libpng missing)");
  }
#endif
  return format;
}

absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshot(
    const std::string& preferred_path, bool reveal_to_user,
    ScreenshotFormat format) {
  return CaptureHarnessScreenshotRegion(std::nullopt, preferred_path,
                                        reveal_to_user, format);
}

absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshotRegion(
    const std::optional<CaptureRegion>& region,
    const std::string& preferred_path, bool reveal_to_user,
    ScreenshotFormat format) {
  ASSIGN_OR_RETURN(const auto resolved,
                   ResolveScreenshotFormat(preferred_path, format));
  ASSIGN_OR_RETURN(SDL_Renderer * renderer, GetScreenshotRenderer());
  int width = 0;
  int height = 0;
  RETURN_IF_ERROR(GetOutputSize(renderer, &width, &height));
  ASSIGN_OR_RETURN(
      const auto clipped,
      ClipRegion(region.value_or(CaptureRegion{0, 0, width, height}), width,
                 height));
  ASSIGN_OR_RETURN(auto surface, ReadFramebufferRegion(renderer, clipped));
  ASSIGN_OR_RETURN(const auto output, ScreenshotPath(preferred_path, resolved));
  if (output.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) {
      return absl::InternalError(absl::StrFormat(
          "Failed to create screenshot directory: %s", error.message()));
    }
  }
  RETURN_IF_ERROR(SaveSurface(surface.get(), output, resolved));
  std::error_code error;
  const auto bytes = std::filesystem::file_size(output, error);
  if (error) {
    return absl::InternalError(absl::StrFormat(
        "Failed to stat screenshot %s: %s", output.string(), error.message()));
  }
  ScreenshotArtifact artifact{output.string(), clipped.w, clipped.h,
                              static_cast<int64_t>(bytes)};
  if (reveal_to_user) {
    RevealScreenshot(output);
  }
  return artifact;
}

absl::StatusOr<ScreenshotArtifact> CaptureActiveWindow(
    const std::string& preferred_path, bool reveal_to_user,
    ScreenshotFormat format) {
  const ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx || !ctx->NavWindow) {
    return absl::FailedPreconditionError("No active ImGui window");
  }
  ASSIGN_OR_RETURN(const auto region, WindowRegion(*ctx->NavWindow));
  return CaptureHarnessScreenshotRegion(region, preferred_path, reveal_to_user,
                                        format);
}

absl::StatusOr<ScreenshotArtifact> CaptureWindowByName(
    const std::string& window_name, const std::string& preferred_path,
    bool reveal_to_user, ScreenshotFormat format) {
  if (!ImGui::GetCurrentContext()) {
    return absl::FailedPreconditionError("No ImGui context");
  }
  const ImGuiWindow* window = ImGui::FindWindowByName(window_name.c_str());
  if (!window) {
    return absl::NotFoundError(
        absl::StrFormat("Window '%s' not found", window_name));
  }
  ASSIGN_OR_RETURN(const auto region, WindowRegion(*window));
  return CaptureHarnessScreenshotRegion(region, preferred_path, reveal_to_user,
                                        format);
}

}  // namespace yaze::test

#endif  // YAZE_WITH_GRPC
