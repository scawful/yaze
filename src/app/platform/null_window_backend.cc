// null_window_backend.cc - Null Window Backend Implementation (Headless)

#include "app/platform/null_window_backend.h"
#include "app/gfx/resource/arena.h"
#include "app/platform/font_loader.h"
#include "util/log.h"
#include "imgui/imgui.h"

namespace yaze {
namespace platform {

absl::Status NullWindowBackend::Initialize(const WindowConfig& config) {
  LOG_INFO("NullWindowBackend", "Initializing headless window backend...");
  initialized_ = true;
  active_ = true;
  width_ = config.width > 0 ? config.width : 1280;
  height_ = config.height > 0 ? config.height : 720;
  return absl::OkStatus();
}

absl::Status NullWindowBackend::Shutdown() {
  LOG_INFO("NullWindowBackend", "Shutting down headless window backend");

  if (imgui_initialized_) {
     ShutdownImGui();
  }

  // CRITICAL: Shutdown graphics arena while renderer is still conceptually valid
  LOG_INFO("NullWindowBackend", "Shutting down graphics arena...");
  gfx::Arena::Get().Shutdown();

  initialized_ = false;
  return absl::OkStatus();
}

bool NullWindowBackend::PollEvent(WindowEvent& out_event) {
  // No events in headless mode
  return false;
}

void NullWindowBackend::ProcessNativeEvent(void* native_event) {
  // No-op
}

WindowStatus NullWindowBackend::GetStatus() const {
  WindowStatus status;
  status.is_active = active_;
  status.width = width_;
  status.height = height_;
  // All other flags false
  return status;
}

void NullWindowBackend::GetSize(int* width, int* height) const {
  if (width) *width = width_;
  if (height) *height = height_;
}

void NullWindowBackend::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
}

bool NullWindowBackend::InitializeRenderer(gfx::IRenderer* renderer) {
  // Return true to pretend renderer is initialized
  return true;
}

absl::Status NullWindowBackend::InitializeImGui(gfx::IRenderer* renderer) {
  // Even in headless, we might want a minimal ImGui context for test automation logic
  // that depends on ImGui::GetIO() or similar, though we won't render.
  if (ImGui::GetCurrentContext() == nullptr) {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Required for layout initialization
      io.IniFilename = nullptr;
      io.DisplaySize = ImVec2((float)width_, (float)height_);
      io.DeltaTime = 1.0f / 60.0f;

      // Load standard fonts so panels don't crash accessing them
      auto status = LoadPackageFonts();
      if (!status.ok()) {
          LOG_WARN("NullWindowBackend", "Failed to load package fonts: %s", status.ToString().c_str());
      }

      // Build font atlas so NewFrame doesn't crash
      unsigned char* pixels;
      int width, height;
      io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
      io.Fonts->SetTexID((ImTextureID)(intptr_t)1); // Dummy ID

      imgui_initialized_ = true;
  }
  return absl::OkStatus();
}

void NullWindowBackend::ShutdownImGui() {
  if (imgui_initialized_ && ImGui::GetCurrentContext() != nullptr) {
      ImGui::DestroyContext();
      imgui_initialized_ = false;
  }
}

void NullWindowBackend::NewImGuiFrame() {
  if (imgui_initialized_) {
      ImGuiIO& io = ImGui::GetIO();
      io.DisplaySize = ImVec2((float)width_, (float)height_);
      io.DeltaTime = 1.0f / 60.0f;
  }
}

void NullWindowBackend::RenderImGui(gfx::IRenderer* renderer) {
  if (imgui_initialized_ && ImGui::GetCurrentContext() != nullptr) {
      ImGui::Render();
  }
}

}  // namespace platform
}  // namespace yaze
