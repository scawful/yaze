#include "controller.h"

#include "app/platform/sdl_compat.h"

#include <string>

#include "absl/status/status.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/renderer_factory.h"  // Use renderer factory for SDL2/SDL3 selection
#include "app/gfx/resource/arena.h"         // Add include for Arena
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/background_renderer.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/timing.h"
#include "app/platform/window.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace yaze {

absl::Status Controller::OnEntry(std::string filename) {
  // Create renderer FIRST (uses factory for SDL2/SDL3 selection)
  renderer_ = gfx::RendererFactory::Create();

  // Call CreateWindow with our renderer
  RETURN_IF_ERROR(CreateWindow(window_, renderer_.get(), SDL_WINDOW_RESIZABLE));

  // Initialize the graphics Arena with the renderer
  gfx::Arena::Get().Initialize(renderer_.get());

  // Set up audio for emulator
  editor_manager_.emulator().set_audio_buffer(window_.audio_buffer_.get());
  editor_manager_.emulator().set_audio_device_id(window_.audio_device_);

  // Initialize editor manager with renderer
  editor_manager_.Initialize(renderer_.get(), filename);

  active_ = true;
  return absl::OkStatus();
}

void Controller::SetStartupEditor(const std::string& editor_name,
                                  const std::string& cards) {
  // Process command-line flags for editor and cards
  // Example: --editor=Dungeon --cards="Rooms List,Room 0,Room 105"
  if (!editor_name.empty()) {
    editor_manager_.OpenEditorAndCardsFromFlags(editor_name, cards);
  }
}

void Controller::OnInput() {
  PRINT_IF_ERROR(HandleEvents(window_));
}

absl::Status Controller::OnLoad() {
  if (editor_manager_.quit() || !window_.active_) {
    active_ = false;
    return absl::OkStatus();
  }

#if TARGET_OS_IPHONE != 1
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Calculate layout offsets for sidebars
  const float left_offset = editor_manager_.GetLeftLayoutOffset();
  const float right_offset = editor_manager_.GetRightLayoutOffset();

  // Adjust dockspace position and size for sidebars
  ImVec2 dockspace_pos = viewport->WorkPos;
  ImVec2 dockspace_size = viewport->WorkSize;

  dockspace_pos.x += left_offset;
  dockspace_size.x -= (left_offset + right_offset);

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
  // Update timing at frame START for accurate delta detection
  float delta_time = TimingManager::Get().Update();
  TimingManager::Get().BeginFrame();

  // Adaptive texture processing based on frame budget
  // Process textures while we have time budget remaining (> 2ms before deadline)
  constexpr float kMinBudgetMs = 2.0f;  // Reserve 2ms for rendering
  int textures_processed = 0;

  while (gfx::Arena::Get().HasPendingTextures() &&
         TimingManager::Get().GetFrameBudgetRemainingMs() > kMinBudgetMs) {
    if (!gfx::Arena::Get().ProcessSingleTexture(renderer_.get())) {
      break;  // No more valid textures to process
    }
    textures_processed++;
    // Safety cap to prevent infinite loops
    if (textures_processed >= 32) break;
  }

  ImGui::Render();
  renderer_->Clear();
  ImGui_ImplSDLRenderer2_RenderDrawData(
      ImGui::GetDrawData(),
      static_cast<SDL_Renderer*>(renderer_->GetBackendRenderer()));
  renderer_->Present();

  // Gentle frame rate cap to prevent excessive CPU usage
  // Only delay if we're rendering faster than 144 FPS (< 7ms per frame)
  if (delta_time < 0.007f) {
    SDL_Delay(1);  // Tiny delay to yield CPU without affecting ImGui timing
  }
}

void Controller::OnExit() {
  renderer_->Shutdown();
  PRINT_IF_ERROR(ShutdownWindow(window_));
}

absl::Status Controller::LoadRomForTesting(const std::string& rom_path) {
  // Use EditorManager's OpenRomOrProject which handles the full initialization:
  // 1. Load ROM file into session
  // 2. ConfigureEditorDependencies()
  // 3. LoadAssets() - initializes all editors and loads graphics
  // 4. Updates UI state (hides welcome screen, etc.)
  return editor_manager_.OpenRomOrProject(rom_path);
}

}  // namespace yaze
