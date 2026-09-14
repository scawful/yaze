#include "app/service/screenshot_utils.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "app/platform/sdl_compat.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#ifdef YAZE_USE_SDL3
#include "imgui/backends/imgui_impl_sdlrenderer3.h"
#else
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#endif
#ifdef YAZE_SCREENSHOT_TEST_HAS_PNG
#include <png.h>
#endif

namespace yaze::test {
namespace {

class ScreenshotUtilsTest : public ::testing::Test {
 protected:
  static constexpr int kWidth = 128;
  static constexpr int kHeight = 96;

  void SetUp() override {
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_screenshot_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directories(directory_));
    surface_ =
        platform::CreateSurface(kWidth, kHeight, 32, SDL_PIXELFORMAT_RGBA32);
    ASSERT_NE(surface_, nullptr) << SDL_GetError();
    renderer_ = SDL_CreateSoftwareRenderer(surface_);
    ASSERT_NE(renderer_, nullptr) << SDL_GetError();
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
    ImGui::GetStyle().WindowMinSize = ImVec2(1, 1);
#ifdef YAZE_USE_SDL3
    backend_initialized_ = ImGui_ImplSDLRenderer3_Init(renderer_);
#else
    backend_initialized_ = ImGui_ImplSDLRenderer2_Init(renderer_);
#endif
    ASSERT_TRUE(backend_initialized_);
    Frame();
  }

  void TearDown() override {
    if (context_ != nullptr) {
      ImGui::SetCurrentContext(context_);
      if (backend_initialized_) {
#ifdef YAZE_USE_SDL3
        ImGui_ImplSDLRenderer3_Shutdown();
#else
        ImGui_ImplSDLRenderer2_Shutdown();
#endif
      }
      ImGui::DestroyContext(context_);
      ImGui::SetCurrentContext(previous_context_);
    }
    if (renderer_ != nullptr)
      SDL_DestroyRenderer(renderer_);
    platform::DestroySurface(surface_);
    std::error_code error;
    if (!directory_.empty())
      std::filesystem::remove_all(directory_, error);
    EXPECT_FALSE(error) << error.message();
  }

  static std::array<uint8_t, 4> Pixel(int x, int y) {
    return {static_cast<uint8_t>(x * 3 + 11), static_cast<uint8_t>(y * 5 + 29),
            static_cast<uint8_t>(x * 7 + y * 11 + 13), 255};
  }

  void Frame(const char* window_name = nullptr, float scale = 1.0f,
             ImVec2 viewport_origin = ImVec2(0, 0)) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kWidth / scale, kHeight / scale);
    io.DisplayFramebufferScale = ImVec2(scale, scale);
#ifdef YAZE_USE_SDL3
    ImGui_ImplSDLRenderer3_NewFrame();
#else
    ImGui_ImplSDLRenderer2_NewFrame();
#endif
    ImGui::NewFrame();
    // Model the main platform viewport's desktop-space origin without creating
    // a native window. The renderer still contains only its framebuffer.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    viewport->Pos = viewport_origin;
    viewport->WorkPos = viewport_origin;
    if (window_name != nullptr) {
      ImGui::SetNextWindowPos(
          ImVec2(viewport_origin.x + 5, viewport_origin.y + 7));
      ImGui::SetNextWindowSize(ImVec2(12, 10));
      ImGui::Begin(
          window_name, nullptr,
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
      ImGui::End();
    }
    ImGui::Render();
#ifdef YAZE_USE_SDL3
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
#else
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
#endif
    // Paint a stable, asymmetric framebuffer after the genuine backend frame;
    // capture correctness must not depend on window theme or font rasterizing.
    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kWidth; ++x) {
        const auto color = Pixel(x, y);
        SDL_SetRenderDrawColor(renderer_, color[0], color[1], color[2],
                               color[3]);
#ifdef YAZE_USE_SDL3
        SDL_RenderPoint(renderer_, static_cast<float>(x),
                        static_cast<float>(y));
#else
        SDL_RenderDrawPoint(renderer_, x, y);
#endif
      }
    }
  }

  std::string Path(const char* name) const {
    return (directory_ / name).string();
  }

  void ExpectMetadata(const ScreenshotArtifact& artifact, int width,
                      int height) {
    EXPECT_EQ(artifact.width, width);
    EXPECT_EQ(artifact.height, height);
    EXPECT_EQ(artifact.file_size_bytes,
              std::filesystem::file_size(artifact.file_path));
  }

  std::array<uint8_t, 8> Signature(const std::string& path) {
    std::array<uint8_t, 8> bytes{};
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    EXPECT_EQ(file.gcount(), static_cast<std::streamsize>(bytes.size()));
    return bytes;
  }

  void ExpectPixels(const uint8_t* pixels, size_t stride, int width, int height,
                    int origin_x = 0, int origin_y = 0) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto expected = Pixel(origin_x + x, origin_y + y);
        const uint8_t* actual = pixels + y * stride + x * 4;
        ASSERT_EQ((std::array<uint8_t, 4>{actual[0], actual[1], actual[2],
                                          actual[3]}),
                  expected)
            << "at " << x << "," << y;
      }
    }
  }

  void ExpectBmp(const ScreenshotArtifact& artifact, int width, int height,
                 int origin_x = 0, int origin_y = 0) {
    ExpectMetadata(artifact, width, height);
    const auto signature = Signature(artifact.file_path);
    ASSERT_EQ(signature[0], 'B');
    ASSERT_EQ(signature[1], 'M');
    SDL_Surface* decoded = platform::LoadBMP(artifact.file_path.c_str());
    ASSERT_NE(decoded, nullptr) << SDL_GetError();
    SDL_Surface* rgba =
        platform::ConvertSurfaceFormat(decoded, SDL_PIXELFORMAT_RGBA32);
    platform::DestroySurface(decoded);
    ASSERT_NE(rgba, nullptr) << SDL_GetError();
    EXPECT_EQ(rgba->w, width);
    EXPECT_EQ(rgba->h, height);
    if (rgba->w == width && rgba->h == height) {
      ExpectPixels(static_cast<const uint8_t*>(rgba->pixels), rgba->pitch,
                   width, height, origin_x, origin_y);
    }
    platform::DestroySurface(rgba);
  }

  std::filesystem::path directory_;
  SDL_Surface* surface_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  bool backend_initialized_ = false;
};

TEST_F(ScreenshotUtilsTest,
       PngExtensionProducesPngPixelsOrExplicitUnsupported) {
  const auto path = Path("frame.png");
  const auto captured = CaptureHarnessScreenshot(path, false);
#ifdef YAZE_SCREENSHOT_TEST_HAS_PNG
  ASSERT_TRUE(captured.ok()) << captured.status();
  EXPECT_EQ(captured->file_path, path);
  ExpectMetadata(*captured, kWidth, kHeight);
  ASSERT_EQ(Signature(path),
            (std::array<uint8_t, 8>{137, 80, 78, 71, 13, 10, 26, 10}));
  png_image decoded{};
  decoded.version = PNG_IMAGE_VERSION;
  ASSERT_TRUE(png_image_begin_read_from_file(&decoded, path.c_str()))
      << decoded.message;
  decoded.format = PNG_FORMAT_RGBA;
  std::vector<uint8_t> pixels(PNG_IMAGE_SIZE(decoded));
  const bool read =
      png_image_finish_read(&decoded, nullptr, pixels.data(), 0, nullptr);
  EXPECT_TRUE(read) << decoded.message;
  EXPECT_EQ(decoded.width, kWidth);
  EXPECT_EQ(decoded.height, kHeight);
  if (read && decoded.width == kWidth && decoded.height == kHeight) {
    ExpectPixels(pixels.data(), kWidth * 4, kWidth, kHeight);
  }
  png_image_free(&decoded);
#else
  EXPECT_EQ(captured.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_FALSE(std::filesystem::exists(path));
#endif
}

TEST_F(ScreenshotUtilsTest, BmpExtensionPreservesFormatAndPixels) {
  const auto captured = CaptureHarnessScreenshot(Path("frame.bmp"), false);
  ASSERT_TRUE(captured.ok()) << captured.status();
  ExpectBmp(*captured, kWidth, kHeight);
}

TEST_F(ScreenshotUtilsTest, PartialNegativeRegionClipsRatherThanShiftsExtent) {
  const auto captured = CaptureHarnessScreenshotRegion(
      CaptureRegion{-3, -2, 11, 9}, Path("clipped.bmp"), false);
  ASSERT_TRUE(captured.ok()) << captured.status();
  ExpectBmp(*captured, 8, 7);
}

TEST_F(ScreenshotUtilsTest, NamedWindowUsesViewportOriginAndFramebufferScale) {
  for (const float scale : {1.0f, 2.0f}) {
    SCOPED_TRACE(scale);
    Frame("CaptureTarget", scale, ImVec2(100, 200));
    Frame("CaptureTarget", scale, ImVec2(100, 200));
    const auto captured =
        CaptureWindowByName("CaptureTarget", Path("window.bmp"), false);
    ASSERT_TRUE(captured.ok()) << captured.status();
    ExpectBmp(*captured, 12 * scale, 10 * scale, 5 * scale, 7 * scale);
  }
}

TEST_F(ScreenshotUtilsTest, MissingHiddenAndInactiveWindowsLeaveNoArtifact) {
  Frame("HiddenTarget");
  Frame("HiddenTarget");
  auto* window = ImGui::FindWindowByName("HiddenTarget");
  ASSERT_NE(window, nullptr);
  for (const char* name : {"MissingTarget", "HiddenTarget", "InactiveTarget"}) {
    SCOPED_TRACE(name);
    if (std::string(name) == "HiddenTarget") {
      window->Hidden = true;  // E.g. an inactive dock tab in the current frame.
    } else if (std::string(name) == "InactiveTarget") {
      Frame(name);
      Frame();  // Keep the cached ImGui window, but do not submit it this frame.
    }
    const auto path = (directory_ / (std::string(name) + ".bmp")).string();
    const auto captured = CaptureWindowByName(name, path, false);
    EXPECT_FALSE(captured.ok());
    EXPECT_FALSE(std::filesystem::exists(path));
  }
}

TEST_F(ScreenshotUtilsTest, InvalidFormatOrExtensionLeavesNoArtifact) {
  struct InvalidRequest {
    const char* name;
    ScreenshotFormat format;
  };
  for (const auto& request : {
           InvalidRequest{"mismatch.png", ScreenshotFormat::kBmp},
           InvalidRequest{"mismatch.bmp", ScreenshotFormat::kPng},
           InvalidRequest{"unsupported.jpg", ScreenshotFormat::kAuto},
           InvalidRequest{"unknown.bmp", static_cast<ScreenshotFormat>(99)},
       }) {
    SCOPED_TRACE(request.name);
    const auto path = Path(request.name);
    const auto captured = CaptureHarnessScreenshot(path, false, request.format);
    EXPECT_EQ(captured.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_FALSE(std::filesystem::exists(path));
  }
  EXPECT_TRUE(std::filesystem::is_empty(directory_));
}

TEST_F(ScreenshotUtilsTest, ExplicitBmpAppendsExtensionAndReturnsAbsolutePath) {
  const auto extensionless = directory_ / "explicit";
  // Do not change the process-wide working directory shared with other tests.
  const auto relative = std::filesystem::relative(extensionless);
  ASSERT_FALSE(relative.empty());
  const auto captured = CaptureHarnessScreenshot(relative.string(), false,
                                                 ScreenshotFormat::kBmp);
  ASSERT_TRUE(captured.ok()) << captured.status();
  EXPECT_TRUE(std::filesystem::path(captured->file_path).is_absolute());
  EXPECT_TRUE(std::filesystem::equivalent(captured->file_path,
                                          extensionless.string() + ".bmp"));
  EXPECT_FALSE(std::filesystem::exists(extensionless));
  ExpectBmp(*captured, kWidth, kHeight);
}

TEST_F(ScreenshotUtilsTest,
       MissingContextAndUnsupportedBackendLeaveNoArtifact) {
  const auto path = Path("guard.bmp");
  ImGui::SetCurrentContext(nullptr);
  const auto no_context = CaptureHarnessScreenshot(path, false);
  const auto no_active = CaptureActiveWindow(path, false);
  const auto no_named = CaptureWindowByName("Target", path, false);
  ImGui::SetCurrentContext(context_);
  EXPECT_EQ(no_context.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(no_active.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(no_named.status().code(), absl::StatusCode::kFailedPrecondition);

  ImGuiIO& io = ImGui::GetIO();
  const char* backend_name = io.BackendRendererName;
  void* backend_data = io.BackendRendererUserData;
  io.BackendRendererName = "imgui_impl_metal";
  // An unsupported backend must be rejected before interpreting its data.
  io.BackendRendererUserData = reinterpret_cast<void*>(uintptr_t{1});
  const auto unsupported = CaptureHarnessScreenshot(path, false);
  io.BackendRendererName = backend_name;
  io.BackendRendererUserData = nullptr;
  const auto no_renderer = CaptureHarnessScreenshot(path, false);
  io.BackendRendererUserData = backend_data;
  EXPECT_EQ(unsupported.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(no_renderer.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_TRUE(std::filesystem::is_empty(directory_));
}

TEST_F(ScreenshotUtilsTest, CollapsedAndDetachedWindowsLeaveNoArtifact) {
  Frame("Target");
  Frame("Target");
  auto* window = ImGui::FindWindowByName("Target");
  ASSERT_NE(window, nullptr);
  const auto path = Path("window.bmp");
  window->Collapsed = true;
  const auto collapsed = CaptureWindowByName("Target", path, false);
  window->Collapsed = false;
  EXPECT_EQ(collapsed.status().code(), absl::StatusCode::kFailedPrecondition);

  ImGuiViewportP detached;
  auto* original_viewport = window->Viewport;
  window->Viewport = &detached;
  const auto other_viewport = CaptureWindowByName("Target", path, false);
  window->Viewport = original_viewport;
  EXPECT_EQ(other_viewport.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_TRUE(std::filesystem::is_empty(directory_));
}

TEST_F(ScreenshotUtilsTest, ClipsRightAndBottomWithoutIntegerOverflow) {
  constexpr int kMax = std::numeric_limits<int>::max();
  for (const auto region :
       {CaptureRegion{120, 90, 20, 20}, CaptureRegion{120, 90, kMax, kMax}}) {
    SCOPED_TRACE(region.width);
    const auto captured =
        CaptureHarnessScreenshotRegion(region, Path("edge.bmp"), false);
    ASSERT_TRUE(captured.ok()) << captured.status();
    ExpectBmp(*captured, 8, 6, 120, 90);
  }
  for (const auto region : {CaptureRegion{kMax - 2, kMax - 2, kMax, kMax},
                            CaptureRegion{-kMax, 0, kMax, 7}}) {
    SCOPED_TRACE(region.x);
    const auto path = Path("outside.bmp");
    const auto captured = CaptureHarnessScreenshotRegion(region, path, false);
    EXPECT_EQ(captured.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_FALSE(std::filesystem::exists(path));
  }
}

TEST_F(ScreenshotUtilsTest, NondefaultRendererViewportIsPreservedByReadback) {
  // The fixture has already painted the full framebuffer. This viewport is
  // disjoint from the requested crop and must not shift or truncate readback.
  const SDL_Rect viewport{40, 40, 10, 10};
#ifdef YAZE_USE_SDL3
  ASSERT_TRUE(SDL_SetRenderViewport(renderer_, &viewport)) << SDL_GetError();
#else
  ASSERT_EQ(SDL_RenderSetViewport(renderer_, &viewport), 0) << SDL_GetError();
#endif
  for (const bool region_only : {false, true}) {
    SCOPED_TRACE(region_only);
    const auto captured =
        region_only ? CaptureHarnessScreenshotRegion(CaptureRegion{3, 5, 11, 7},
                                                     Path("region.bmp"), false)
                    : CaptureHarnessScreenshot(Path("full.bmp"), false);
    SDL_Rect restored{};
#ifdef YAZE_USE_SDL3
    ASSERT_TRUE(SDL_GetRenderViewport(renderer_, &restored)) << SDL_GetError();
#else
    SDL_RenderGetViewport(renderer_, &restored);
#endif
    EXPECT_EQ(restored.x, viewport.x);
    EXPECT_EQ(restored.y, viewport.y);
    EXPECT_EQ(restored.w, viewport.w);
    EXPECT_EQ(restored.h, viewport.h);
    ASSERT_TRUE(captured.ok()) << captured.status();
    if (region_only) {
      ExpectBmp(*captured, 11, 7, 3, 5);
    } else {
      ExpectBmp(*captured, kWidth, kHeight);
    }
  }
}

}  // namespace
}  // namespace yaze::test
