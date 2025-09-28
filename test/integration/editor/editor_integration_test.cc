#define IMGUI_DEFINE_MATH_OPERATORS

#include "integration/editor/editor_integration_test.h"

#include <SDL.h>

#include "app/core/window.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_imconfig.h"
#include "imgui_test_engine/imgui_te_ui.h"

namespace yaze {
namespace test {

EditorIntegrationTest::EditorIntegrationTest() 
    : engine_(nullptr), show_demo_window_(true) {}

EditorIntegrationTest::~EditorIntegrationTest() {
  if (engine_) {
    ImGuiTestEngine_Stop(engine_);
    ImGuiTestEngine_DestroyContext(engine_);
  }
}

absl::Status EditorIntegrationTest::Initialize() {
  RETURN_IF_ERROR(core::CreateWindow(window_, SDL_WINDOW_RESIZABLE));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Initialize Test Engine
  engine_ = ImGuiTestEngine_CreateContext();
  ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine_);
  test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
  test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(
      controller_.window(), yaze::core::Renderer::Get().renderer());
  ImGui_ImplSDLRenderer2_Init(yaze::core::Renderer::Get().renderer());

  // Register tests
  RegisterTests(engine_);
  ImGuiTestEngine_Start(engine_, ImGui::GetCurrentContext());
  controller_.set_active(true);

  // Set the default style
  yaze::gui::ColorsYaze();

  return absl::OkStatus();
}

int EditorIntegrationTest::RunTest() {
  auto status = Initialize();
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  while (controller_.IsActive()) {
    controller_.OnInput();
    auto status = Update();
    if (!status.ok()) {
      return EXIT_FAILURE;
    }
    controller_.DoRender();
  }

  return EXIT_SUCCESS;
}

absl::Status EditorIntegrationTest::Update() {
  ImGui::NewFrame();
  
  // Show test engine windows
  ImGuiTestEngine_ShowTestEngineWindows(engine_, &show_demo_window_);
  
  return absl::OkStatus();
}


// Helper methods for testing with a ROM
absl::Status EditorIntegrationTest::LoadTestRom(const std::string& filename) {
  test_rom_ = std::make_unique<Rom>();
  return test_rom_->LoadFromFile(filename);
}

absl::Status EditorIntegrationTest::SaveTestRom(const std::string& filename) {
  if (!test_rom_) {
    return absl::FailedPreconditionError("No test ROM loaded");
  }
  Rom::SaveSettings settings;
  settings.backup = false;
  settings.save_new = false;
  settings.filename = filename;
  return test_rom_->SaveToFile(settings);
}

absl::Status EditorIntegrationTest::TestEditorInitialize(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  editor->Initialize();
  return absl::OkStatus();
}

absl::Status EditorIntegrationTest::TestEditorLoad(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Load();
}

absl::Status EditorIntegrationTest::TestEditorSave(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Save();
}

absl::Status EditorIntegrationTest::TestEditorUpdate(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Update();
}

absl::Status EditorIntegrationTest::TestEditorCut(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Cut();
}

absl::Status EditorIntegrationTest::TestEditorCopy(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Copy();
}

absl::Status EditorIntegrationTest::TestEditorPaste(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Paste();
}

absl::Status EditorIntegrationTest::TestEditorUndo(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Undo();
}

absl::Status EditorIntegrationTest::TestEditorRedo(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Redo();
}

absl::Status EditorIntegrationTest::TestEditorFind(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Find();
}

absl::Status EditorIntegrationTest::TestEditorClear(editor::Editor* editor) {
  if (!editor) {
    return absl::InternalError("Editor is null");
  }
  return editor->Clear();
}

}  // namespace test
}  // namespace yaze 