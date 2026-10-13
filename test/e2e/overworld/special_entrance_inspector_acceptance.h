#ifndef YAZE_TEST_E2E_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_ACCEPTANCE_H_
#define YAZE_TEST_E2E_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_ACCEPTANCE_H_

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/controller.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "zelda3/overworld/special_entrance_inspector.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::test {

// Opt-in, read-only workflow on a disposable copy of the supplied ROM.
inline void RegisterSpecialEntranceInspectorAcceptance(ImGuiTestEngine* engine,
                                                       Controller* controller) {
  if (!std::getenv("YAZE_SPECIAL_ENTRANCE_ACCEPTANCE_DIR")) {
    return;
  }
  auto* test = IM_REGISTER_TEST(engine, "OverworldAcceptance",
                                "SpecialEntranceInspection");
  test->UserData = controller;
  test->TestFunc = [](ImGuiTestContext* ctx) {
    namespace fs = std::filesystem;
    const char* source = std::getenv("YAZE_SPECIAL_ENTRANCE_ACCEPTANCE_ROM");
    const char* directory = std::getenv("YAZE_SPECIAL_ENTRANCE_ACCEPTANCE_DIR");
    IM_CHECK(source && directory);
    const fs::path input = fs::path(directory) / "inspector-input.sfc";
    IM_CHECK(!fs::exists(input));
    fs::create_directories(input.parent_path());
    fs::copy_file(source, input);
    const auto read = [](const fs::path& path) {
      std::ifstream stream(path, std::ios::binary);
      return std::vector<char>(std::istreambuf_iterator<char>(stream), {});
    };
    const auto before = read(input);
    auto* ctl = static_cast<Controller*>(ctx->Test->UserData);
    IM_CHECK(ctl->LoadRomForTesting(input.string()).ok());
    auto* manager = ctl->editor_manager();
    manager->OpenEditorAndPanelsFromFlags("Overworld", "");
    auto* editor =
        manager->GetCurrentEditorSet()->GetEditorAs<editor::OverworldEditor>(
            editor::EditorType::kOverworld);
    IM_CHECK(editor != nullptr);
    auto* windows = manager->GetWindowManager();
    const size_t session_id = manager->GetCurrentSessionId();
    IM_CHECK(windows->OpenWindow(session_id, "overworld.entity_workbench"));
    editor->JumpToMap(0x8A);  // Child of $81; matches live on screen $81.
    ctx->Yield(10);
    const auto title = windows->GetWorkspaceWindowName(
        session_id, "overworld.entity_workbench");
    ctx->WindowFocus(title.c_str());
    ctx->ItemClick("**/Inspect special entrances");
    ctx->Yield(2);
    const auto inspection = zelda3::InspectSpecialEntrances(
        *ctl->GetCurrentRom(), editor->overworld(), 0x8A);
    IM_CHECK(inspection.available && !inspection.matches.empty());
    const auto& match = inspection.matches.front();
    const int destination = inspection.rows[match.row_index].destination_area;
    const auto name = zelda3::Zelda3Labels::GetLabel(
        "overworld_map", destination,
        absl::StrFormat("Special area $%02X", destination));
    const auto label =
        absl::StrFormat("**/%s ($%02X) - tile (%d,%d)", name, destination,
                        match.world_x, match.world_y);
    ctx->ItemClick(label.c_str());
    ctx->Yield(2);
    IM_CHECK_EQ(editor->current_map_id(), 0x81);
    IM_CHECK(read(input) == before);
    IM_CHECK(read(source) == before);
  };
}

}  // namespace yaze::test

#endif  // YAZE_TEST_E2E_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_ACCEPTANCE_H_
