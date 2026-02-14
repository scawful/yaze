#include "controller.h"

#include "app/platform/sdl_compat.h"
#include "app/application.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <string>

#include "absl/status/status.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/events/core_events.h"
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
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#include "app/platform/ios/ios_platform_state.h"
#endif

namespace yaze {

absl::Status Controller::OnEntry(std::string filename) {
  // Create window backend using factory (auto-selects SDL2 or SDL3)
  auto backend_type = platform::WindowBackendFactory::GetDefaultType();
  auto renderer_type = gfx::RendererFactory::GetDefaultBackendType();

  const auto& app_config = Application::Instance().GetConfig();

  if (app_config.headless) {
    LOG_INFO("Controller", "Using Null Window Backend (Headless Mode)");
    backend_type = platform::WindowBackendType::Null;
    renderer_type = gfx::RendererBackendType::Null;
  }

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  backend_type = platform::WindowBackendType::IOS;
  renderer_type = gfx::RendererBackendType::Metal;
#endif

  window_backend_ = platform::WindowBackendFactory::Create(backend_type);
  if (!window_backend_) {
    return absl::InternalError("Failed to create window backend");
  }

  platform::WindowConfig config;
  config.title = "Yet Another Zelda3 Editor";
  config.resizable = true;
  config.high_dpi = false;  // Disabled to match legacy behavior (SDL_WINDOW_RESIZABLE only)
  
  if (app_config.service_mode) {
    LOG_INFO("Controller", "Starting in Service Mode (Hidden Window)");
    config.hidden = true;
  }

  RETURN_IF_ERROR(window_backend_->Initialize(config));

  // Create renderer via factory (auto-selects SDL2 or SDL3)
  renderer_ = gfx::RendererFactory::Create(renderer_type);
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

  editor_manager_.SetAssetLoadMode(Application::Instance().GetConfig().asset_load_mode);

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

      case platform::WindowEventType::Minimized:
      case platform::WindowEventType::Hidden:
      case platform::WindowEventType::FocusLost:
        editor_manager_.HandleHostVisibilityChanged(false);
        break;

      case platform::WindowEventType::Restored:
      case platform::WindowEventType::Shown:
      case platform::WindowEventType::Exposed:
      case platform::WindowEventType::FocusGained:
        editor_manager_.HandleHostVisibilityChanged(true);
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

  // Start new ImGui frame via backend (handles SDL2/SDL3 automatically)
  window_backend_->NewImGuiFrame();
  ImGui::NewFrame();

  // Advance any in-progress theme color transitions
  gui::ThemeManager::Get().UpdateTransition();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Calculate layout offsets for sidebars and status bar
  const float left_offset = editor_manager_.GetLeftLayoutOffset();
  const float right_offset = editor_manager_.GetRightLayoutOffset();
  float bottom_offset = editor_manager_.GetBottomLayoutOffset();

  float top_offset = 0.0f;
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  // On iOS, inset the dockspace by the safe area so content doesn't render
  // behind the notch/Dynamic Island (top) or home indicator (bottom).
  {
    const auto safe = platform::ios::GetSafeAreaInsets();
    top_offset = std::max(safe.top, platform::ios::GetOverlayTopInset());
    bottom_offset += safe.bottom;
  }
#endif

  // Adjust dockspace position and size for sidebars and status bar
  ImVec2 dockspace_pos = viewport->WorkPos;
  ImVec2 dockspace_size = viewport->WorkSize;

  dockspace_pos.x += left_offset;
  dockspace_pos.y += top_offset;
  dockspace_size.x -= (left_offset + right_offset);
  dockspace_size.y -= (bottom_offset + top_offset);

  ImGui::SetNextWindowPos(dockspace_pos);
  ImGui::SetNextWindowSize(dockspace_size);
  ImGui::SetNextWindowViewport(viewport->ID);

  // Check if menu bar should be visible (WASM can hide it for clean UI)
  bool show_menu_bar = true;
  if (editor_manager_.ui_coordinator()) {
    show_menu_bar = editor_manager_.ui_coordinator()->IsMenuBarVisible();
  }
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  show_menu_bar = false;
#endif

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
    editor_manager_.DrawMainMenuBar();  // Draw the fixed menu bar at the top
  }

  gui::DockSpaceRenderer::EndEnhancedDockSpace();

  if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
    bus->Publish(editor::FrameGuiBeginEvent::Create(ImGui::GetIO().DeltaTime));
  }
  ImGui::End();

#if !defined(__APPLE__) || (TARGET_OS_IPHONE != 1 && TARGET_IPHONE_SIMULATOR != 1)
  // Draw menu bar restore button when menu is hidden (WASM)
  if (!show_menu_bar && editor_manager_.ui_coordinator()) {
    editor_manager_.ui_coordinator()->DrawMenuBarRestoreButton();
  }
#endif
  gui::WidgetIdRegistry::Instance().BeginFrame();
  absl::Status update_status = editor_manager_.Update();
  gui::WidgetIdRegistry::Instance().EndFrame();
  RETURN_IF_ERROR(update_status);

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  {
    platform::ios::EditorStateSnapshot snap;
    auto* editor = editor_manager_.GetCurrentEditor();
    auto* rom = editor_manager_.GetCurrentRom();
    if (editor) {
      snap.can_undo = editor->undo_manager().CanUndo();
      snap.can_redo = editor->undo_manager().CanRedo();
      snap.editor_type =
          editor::kEditorNames[editor::EditorTypeIndex(editor->type())];
    }
    if (rom && rom->is_loaded()) {
      snap.can_save = true;
      snap.is_dirty = rom->dirty();
      snap.rom_title = rom->title();
    }
    platform::ios::PostEditorStateUpdate(snap);
  }
#endif

  return absl::OkStatus();
}

void Controller::DoRender() const {
  if (!window_backend_ || !renderer_) return;

  // Process pending texture commands (max 8 per frame for consistent performance)
  gfx::Arena::Get().ProcessTextureQueue(renderer_.get());

  if (Application::Instance().GetConfig().headless) {
    // In HEADLESS mode, we MUST still end the ImGui frame to satisfy assertions
    // even if we don't render to a window.
    ImGui::Render();
    ProcessScreenshotRequests();
    return;
  }

  renderer_->Clear();

  // Render ImGui draw data and handle viewports via backend
  // This MUST be called even in headless mode to end the ImGui frame
  window_backend_->RenderImGui(renderer_.get());

  renderer_->Present();

  // Process any pending screenshot requests on the main thread after present
  ProcessScreenshotRequests();

  // Get delta time AFTER render for accurate measurement
  float delta_time = TimingManager::Get().Update();

  // Gentle frame rate cap to prevent excessive CPU usage
  // Only delay if we're rendering faster than 144 FPS (< 7ms per frame)
  if (delta_time < 0.007f) {
#if TARGET_OS_IPHONE != 1
    SDL_Delay(1);  // Tiny delay to yield CPU without affecting ImGui timing
#endif
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
  // 3. LoadAssetsForMode() - initializes all editors and loads graphics
  // 4. Updates UI state (hides welcome screen, etc.)
  auto previous_mode = editor_manager_.asset_load_mode();
  editor_manager_.SetAssetLoadMode(AssetLoadMode::kFull);
  auto status = editor_manager_.OpenRomOrProject(rom_path);
  editor_manager_.SetAssetLoadMode(previous_mode);
  return status;
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
