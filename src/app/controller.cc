#include "controller.h"

#include "app/platform/sdl_compat.h"

#include <string>

#include "absl/status/status.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/renderer_factory.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/background_renderer.h"
#include "app/gui/core/theme_manager.h"
#include "app/emu/emulator.h"
#include "app/platform/iwindow.h"
#include "app/platform/timing.h"
#include "app/service/screenshot_utils.h"
#include "imgui/imgui.h"

namespace yaze {

absl::Status Controller::OnEntry(std::string filename) {
  // Create window backend using factory (auto-selects SDL2 or SDL3)
  window_backend_ = platform::WindowBackendFactory::Create(
      platform::WindowBackendFactory::GetDefaultType());

  platform::WindowConfig config;
  config.title = "Yet Another Zelda3 Editor";
  config.resizable = true;
  config.high_dpi = false;  // Disabled to match legacy behavior (SDL_WINDOW_RESIZABLE only)

  RETURN_IF_ERROR(window_backend_->Initialize(config));

  // Create renderer via factory (auto-selects SDL2 or SDL3)
  renderer_ = gfx::RendererFactory::Create();
  if (!window_backend_->InitializeRenderer(renderer_.get())) {
    return absl::InternalError("Failed to initialize renderer");
  }

  // Initialize ImGui via backend (handles SDL2/SDL3 automatically)
  RETURN_IF_ERROR(window_backend_->InitializeImGui(renderer_.get()));

  // Initialize the graphics Arena with the renderer
  gfx::Arena::Get().Initialize(renderer_.get());

  // Set up audio for emulator (using backend's audio resources)
  auto audio_buffer = window_backend_->GetAudioBuffer();
  if (audio_buffer) {
    editor_manager_.emulator().set_audio_buffer(audio_buffer.get());
  }
  editor_manager_.emulator().set_audio_device_id(window_backend_->GetAudioDevice());

  // Initialize editor manager with renderer
  editor_manager_.Initialize(renderer_.get(), filename);

  active_ = true;
  return absl::OkStatus();
}

void Controller::SetStartupEditor(const std::string& editor_name,
                                  const std::string& panels) {
  // Process command-line flags for editor and panels
  // Example: --editor=Dungeon --open_panels="dungeon.room_list,Room 0"
  if (!editor_name.empty()) {
    editor_manager_.OpenEditorAndPanelsFromFlags(editor_name, panels);
  }
}

void Controller::OnInput() {
  if (!window_backend_) return;

  platform::WindowEvent event;
  while (window_backend_->PollEvent(event)) {
    switch (event.type) {
      case platform::WindowEventType::Quit:
      case platform::WindowEventType::Close:
        active_ = false;
        break;
      default:
        // Other events are handled by ImGui via ProcessNativeEvent
        // which is called inside PollEvent
        break;
    }

    // Forward native SDL events to emulator input for event-based paths
    if (event.has_native_event) {
      editor_manager_.emulator().input_manager().ProcessEvent(
          static_cast<void*>(&event.native_event));
    }
  }
}

absl::Status Controller::OnLoad() {
  if (!window_backend_) {
    return absl::InternalError("Window backend not initialized");
  }

  if (editor_manager_.quit() || !window_backend_->IsActive()) {
    active_ = false;
    return absl::OkStatus();
  }

#if TARGET_OS_IPHONE != 1
  // Start new ImGui frame via backend (handles SDL2/SDL3 automatically)
  window_backend_->NewImGuiFrame();
  ImGui::NewFrame();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Calculate layout offsets for sidebars and status bar
  const float left_offset = editor_manager_.GetLeftLayoutOffset();
  const float right_offset = editor_manager_.GetRightLayoutOffset();
  const float bottom_offset = editor_manager_.GetBottomLayoutOffset();

  // Adjust dockspace position and size for sidebars and status bar
  ImVec2 dockspace_pos = viewport->WorkPos;
  ImVec2 dockspace_size = viewport->WorkSize;

  dockspace_pos.x += left_offset;
  dockspace_size.x -= (left_offset + right_offset);
  dockspace_size.y -= bottom_offset;  // Reserve space for status bar at bottom

  ImGui::SetNextWindowPos(dockspace_pos);
  ImGui::SetNextWindowSize(dockspace_size);
  ImGui::SetNextWindowViewport(viewport->ID);

  // Check if menu bar should be visible (WASM can hide it for clean UI)
  bool show_menu_bar = true;
  if (editor_manager_.ui_coordinator()) {
    show_menu_bar = editor_manager_.ui_coordinator()->IsMenuBarVisible();
  }

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
  if (show_menu_bar) {
    window_flags |= ImGuiWindowFlags_MenuBar;
  }
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  // Create DockSpace with adjusted size
  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  gui::DockSpaceRenderer::BeginEnhancedDockSpace(
      dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

  if (show_menu_bar) {
    editor_manager_.DrawMenuBar();  // Draw the fixed menu bar at the top
  }

  gui::DockSpaceRenderer::EndEnhancedDockSpace();
  ImGui::End();

  // Draw menu bar restore button when menu is hidden (WASM)
  if (!show_menu_bar && editor_manager_.ui_coordinator()) {
    editor_manager_.ui_coordinator()->DrawMenuBarRestoreButton();
  }
#endif
  gui::WidgetIdRegistry::Instance().BeginFrame();
  absl::Status update_status = editor_manager_.Update();
  gui::WidgetIdRegistry::Instance().EndFrame();
  RETURN_IF_ERROR(update_status);
  return absl::OkStatus();
}

void Controller::DoRender() const {
  if (!window_backend_ || !renderer_) return;

  // Process pending texture commands (max 8 per frame for consistent performance)
  gfx::Arena::Get().ProcessTextureQueue(renderer_.get());

  renderer_->Clear();
  
  // Render ImGui draw data and handle viewports via backend
  window_backend_->RenderImGui(renderer_.get());

  renderer_->Present();

  // Process any pending screenshot requests on the main thread after present
  ProcessScreenshotRequests();

  // Get delta time AFTER render for accurate measurement
  float delta_time = TimingManager::Get().Update();

  // Gentle frame rate cap to prevent excessive CPU usage
  // Only delay if we're rendering faster than 144 FPS (< 7ms per frame)
  if (delta_time < 0.007f) {
    SDL_Delay(1);  // Tiny delay to yield CPU without affecting ImGui timing
  }
}

void Controller::OnExit() {
  if (renderer_) {
    renderer_->Shutdown();
  }
  if (window_backend_) {
    window_backend_->Shutdown();
  }
}

absl::Status Controller::LoadRomForTesting(const std::string& rom_path) {
  // Use EditorManager's OpenRomOrProject which handles the full initialization:
  // 1. Load ROM file into session
  // 2. ConfigureEditorDependencies()
  // 3. LoadAssets() - initializes all editors and loads graphics
  // 4. Updates UI state (hides welcome screen, etc.)
  return editor_manager_.OpenRomOrProject(rom_path);
}

void Controller::RequestScreenshot(const ScreenshotRequest& request) {
  std::lock_guard<std::mutex> lock(screenshot_mutex_);
  screenshot_requests_.push(request);
}

void Controller::ProcessScreenshotRequests() const {
#ifdef YAZE_WITH_GRPC
  std::lock_guard<std::mutex> lock(screenshot_mutex_);
  while (!screenshot_requests_.empty()) {
    auto request = screenshot_requests_.front();
    screenshot_requests_.pop();

    // Perform capture on main thread
    auto result = test::CaptureHarnessScreenshot(request.preferred_path);
    if (request.callback) {
      request.callback(result);
    }
  }
#endif
}

}  // namespace yaze
