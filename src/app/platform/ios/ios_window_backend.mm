#include "app/platform/ios/ios_window_backend.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#import <CoreFoundation/CoreFoundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#endif

#include <algorithm>
#include <string>

#include "app/gfx/backend/metal_renderer.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/platform/font_loader.h"
#include "app/platform/ios/ios_platform_state.h"
#include "imgui/backends/imgui_impl_metal.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace platform {

namespace {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
UIEdgeInsets GetSafeAreaInsets(MTKView* view) {
  if (!view) {
    return UIEdgeInsetsZero;
  }
  if (@available(iOS 11.0, *)) {
    return view.safeAreaInsets;
  }
  return UIEdgeInsetsZero;
}

void ApplyTouchStyle(MTKView* view) {
  struct TouchStyleBaseline {
    bool initialized = false;
    ImVec2 touch_extra = ImVec2(0.0f, 0.0f);
    ImVec2 frame_padding = ImVec2(0.0f, 0.0f);
    ImVec2 item_spacing = ImVec2(0.0f, 0.0f);
    float scrollbar_size = 0.0f;
    float grab_min_size = 0.0f;
  };

  static TouchStyleBaseline baseline;

  ImGuiStyle& style = ImGui::GetStyle();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  io.ConfigWindowsResizeFromEdges = false;
  if (!baseline.initialized) {
    baseline.touch_extra = style.TouchExtraPadding;
    baseline.frame_padding = style.FramePadding;
    baseline.item_spacing = style.ItemSpacing;
    baseline.scrollbar_size = style.ScrollbarSize;
    baseline.grab_min_size = style.GrabMinSize;
    baseline.initialized = true;
  }

  float scale = ios::GetTouchScale();
  if (scale < 0.75f) {
    scale = 0.75f;
  } else if (scale > 1.6f) {
    scale = 1.6f;
  }

  const float frame_height = ImGui::GetFrameHeight();
  const float target_height = std::max(44.0f * scale, frame_height);
  const float touch_extra =
      std::clamp((target_height - frame_height) * 0.5f, 0.0f, 16.0f * scale);
  style.TouchExtraPadding =
      ImVec2(std::max(baseline.touch_extra.x * scale, touch_extra),
             std::max(baseline.touch_extra.y * scale, touch_extra));

  const float font_size = ImGui::GetFontSize();
  if (font_size > 0.0f) {
    style.ScrollbarSize = std::max(baseline.scrollbar_size * scale,
                                   font_size * 1.1f * scale);
    style.GrabMinSize =
        std::max(baseline.grab_min_size * scale, font_size * 0.9f * scale);
    style.FramePadding.x = std::max(baseline.frame_padding.x * scale,
                                    font_size * 0.55f * scale);
    style.FramePadding.y = std::max(baseline.frame_padding.y * scale,
                                    font_size * 0.35f * scale);
    style.ItemSpacing.x = std::max(baseline.item_spacing.x * scale,
                                   font_size * 0.45f * scale);
    style.ItemSpacing.y = std::max(baseline.item_spacing.y * scale,
                                   font_size * 0.35f * scale);
  }

  // Window chrome sizing for touch-friendly interaction
  style.WindowRounding = 8.0f * scale;
  style.PopupRounding = 6.0f * scale;
  style.TabRounding = 4.0f * scale;
  style.ScrollbarRounding = 6.0f * scale;
  style.TabCloseButtonMinWidthUnselected = 44.0f * scale;

  // Prevent tiny windows on iPad — minimum 200x150 ensures usability
  style.WindowMinSize = ImVec2(200.0f * scale, 150.0f * scale);

  const UIEdgeInsets insets = GetSafeAreaInsets(view);
  const float safe_x = std::max(insets.left, insets.right);
  const float safe_y = std::max(insets.top, insets.bottom);
  const float overlay_top = ios::GetOverlayTopInset();
  const float padded_top = std::max(safe_y, overlay_top);
  style.DisplaySafeAreaPadding = ImVec2(safe_x, padded_top);
  ios::SetSafeAreaInsets(insets.left, insets.right, insets.top,
                         insets.bottom);
}
#endif
}  // namespace

absl::Status IOSWindowBackend::Initialize(const WindowConfig& config) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  metal_view_ = ios::GetMetalView();
  if (!metal_view_) {
    return absl::FailedPreconditionError("Metal view not set");
  }

  title_ = config.title;
  status_.is_active = true;
  status_.is_focused = true;
  status_.is_fullscreen = config.fullscreen;

  auto* view = static_cast<MTKView*>(metal_view_);
  status_.width = static_cast<int>(view.bounds.size.width);
  status_.height = static_cast<int>(view.bounds.size.height);

  initialized_ = true;
  return absl::OkStatus();
#else
  (void)config;
  return absl::FailedPreconditionError(
      "IOSWindowBackend is only available on iOS");
#endif
}

absl::Status IOSWindowBackend::Shutdown() {
  ShutdownImGui();

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (command_queue_) {
    CFRelease(command_queue_);
    command_queue_ = nullptr;
  }
#endif

  metal_view_ = nullptr;
  initialized_ = false;
  return absl::OkStatus();
}

bool IOSWindowBackend::IsInitialized() const {
  return initialized_;
}

bool IOSWindowBackend::PollEvent(WindowEvent& out_event) {
  out_event = WindowEvent{};
  return false;
}

void IOSWindowBackend::ProcessNativeEvent(void* native_event) {
  (void)native_event;
}

WindowStatus IOSWindowBackend::GetStatus() const {
  WindowStatus status = status_;
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (metal_view_) {
    auto* view = static_cast<MTKView*>(metal_view_);
    status.width = static_cast<int>(view.bounds.size.width);
    status.height = static_cast<int>(view.bounds.size.height);
  }
#endif
  return status;
}

bool IOSWindowBackend::IsActive() const {
  return status_.is_active;
}

void IOSWindowBackend::SetActive(bool active) {
  status_.is_active = active;
}

void IOSWindowBackend::GetSize(int* width, int* height) const {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (metal_view_) {
    auto* view = static_cast<MTKView*>(metal_view_);
    if (width) {
      *width = static_cast<int>(view.bounds.size.width);
    }
    if (height) {
      *height = static_cast<int>(view.bounds.size.height);
    }
    return;
  }
#endif

  if (width) {
    *width = 0;
  }
  if (height) {
    *height = 0;
  }
}

void IOSWindowBackend::SetSize(int width, int height) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (metal_view_) {
    auto* view = static_cast<MTKView*>(metal_view_);
    view.drawableSize = CGSizeMake(width, height);
  }
#else
  (void)width;
  (void)height;
#endif
}

std::string IOSWindowBackend::GetTitle() const {
  return title_;
}

void IOSWindowBackend::SetTitle(const std::string& title) {
  title_ = title;
}

void IOSWindowBackend::ShowWindow() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (metal_view_) {
    auto* view = static_cast<MTKView*>(metal_view_);
    view.hidden = NO;
  }
#endif
  status_.is_active = true;
}

void IOSWindowBackend::HideWindow() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (metal_view_) {
    auto* view = static_cast<MTKView*>(metal_view_);
    view.hidden = YES;
  }
#endif
  status_.is_active = false;
}

bool IOSWindowBackend::InitializeRenderer(gfx::IRenderer* renderer) {
  if (!renderer || !metal_view_) {
    return false;
  }

  if (renderer->GetBackendRenderer()) {
    return true;
  }

  auto* metal_renderer = dynamic_cast<gfx::MetalRenderer*>(renderer);
  if (metal_renderer) {
    metal_renderer->SetMetalView(metal_view_);
  } else {
    LOG_WARN("IOSWindowBackend", "Non-Metal renderer selected on iOS");
  }

  return renderer->Initialize(nullptr);
}

SDL_Window* IOSWindowBackend::GetNativeWindow() {
  return nullptr;
}

absl::Status IOSWindowBackend::InitializeImGui(gfx::IRenderer* renderer) {
  if (imgui_initialized_) {
    return absl::OkStatus();
  }

  if (!renderer) {
    return absl::InvalidArgumentError("Renderer is null");
  }

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!metal_view_) {
    return absl::FailedPreconditionError("Metal view not set");
  }

  auto* view = static_cast<MTKView*>(metal_view_);
  id<MTLDevice> device = view.device;
  if (!device) {
    device = MTLCreateSystemDefaultDevice();
    view.device = device;
  }

  if (!device) {
    return absl::InternalError("Failed to create Metal device");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;

  if (auto ini_path = util::PlatformPaths::GetImGuiIniPath(); ini_path.ok()) {
    static std::string ini_path_str;
    if (ini_path_str.empty()) {
      ini_path_str = ini_path->string();
    }
    io.IniFilename = ini_path_str.c_str();
  } else {
    io.IniFilename = nullptr;
    LOG_WARN("IOSWindowBackend", "Failed to resolve ImGui ini path: %s",
             ini_path.status().ToString().c_str());
  }

  if (!ImGui_ImplMetal_Init(device)) {
    return absl::InternalError("ImGui_ImplMetal_Init failed");
  }

  auto font_status = LoadPackageFonts();
  if (!font_status.ok()) {
    ImGui_ImplMetal_Shutdown();
    ImGui::DestroyContext();
    return font_status;
  }
  gui::ColorsYaze();
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  ApplyTouchStyle(view);
#endif

  if (!command_queue_) {
    id<MTLCommandQueue> queue = [device newCommandQueue];
    command_queue_ = (__bridge_retained void*)queue;
  }

  imgui_initialized_ = true;
  LOG_INFO("IOSWindowBackend", "ImGui initialized with Metal backend");
  return absl::OkStatus();
#else
  return absl::FailedPreconditionError(
      "IOSWindowBackend is only available on iOS");
#endif
}

void IOSWindowBackend::ShutdownImGui() {
  if (!imgui_initialized_) {
    return;
  }

  ImGui_ImplMetal_Shutdown();
  ImGui::DestroyContext();

  imgui_initialized_ = false;
}

void IOSWindowBackend::NewImGuiFrame() {
  if (!imgui_initialized_) {
    return;
  }

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  auto* view = static_cast<MTKView*>(metal_view_);
  if (!view) {
    return;
  }

  ApplyTouchStyle(view);

  auto* render_pass = view.currentRenderPassDescriptor;
  if (!render_pass) {
    return;
  }

  ImGui_ImplMetal_NewFrame(render_pass);
#endif
}

void IOSWindowBackend::RenderImGui(gfx::IRenderer* renderer) {
  if (!imgui_initialized_) {
    return;
  }

  // Clamp all floating windows within safe area to prevent off-screen drift.
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (ctx && vp) {
      const ImGuiStyle& style = ImGui::GetStyle();
      const ImVec2 safe = style.DisplaySafeAreaPadding;
      const ImVec2 rect_pos(vp->WorkPos.x + safe.x, vp->WorkPos.y + safe.y);
      const ImVec2 rect_size(vp->WorkSize.x - safe.x * 2.0f,
                             vp->WorkSize.y - safe.y - safe.x);

      for (ImGuiWindow* win : ctx->Windows) {
        if (!win || win->Hidden || win->IsFallbackWindow) continue;
        if (win->DockIsActive || win->DockNodeAsHost) continue;
        if (win->Flags & ImGuiWindowFlags_NoMove) continue;
        if (win->Flags & ImGuiWindowFlags_ChildWindow) continue;

        auto result = gui::LayoutHelpers::ClampWindowToRect(
            win->Pos, win->Size, rect_pos, rect_size, 48.0f);
        if (result.clamped) {
          ImGui::SetWindowPos(win, result.pos, ImGuiCond_Always);
        }
      }
    }
  }
#endif

  ImGui::Render();

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  (void)renderer;
  auto* view = static_cast<MTKView*>(metal_view_);
  if (!view || !view.currentDrawable) {
    return;
  }

  auto* render_pass = view.currentRenderPassDescriptor;
  if (!render_pass || !command_queue_) {
    return;
  }

  id<MTLCommandQueue> queue =
      (__bridge id<MTLCommandQueue>)command_queue_;
  id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder =
      [command_buffer renderCommandEncoderWithDescriptor:render_pass];

  ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), command_buffer, encoder);
  [encoder endEncoding];
  [command_buffer presentDrawable:view.currentDrawable];
  [command_buffer commit];
#else
  (void)renderer;
#endif
}

uint32_t IOSWindowBackend::GetAudioDevice() const {
  return 0;
}

std::shared_ptr<int16_t> IOSWindowBackend::GetAudioBuffer() const {
  return nullptr;
}

std::string IOSWindowBackend::GetBackendName() const {
  return "iOS-Metal";
}

int IOSWindowBackend::GetSDLVersion() const {
  return 0;
}

}  // namespace platform
}  // namespace yaze
