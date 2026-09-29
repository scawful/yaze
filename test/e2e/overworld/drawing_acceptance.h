#ifndef YAZE_TEST_E2E_OVERWORLD_DRAWING_ACCEPTANCE_H
#define YAZE_TEST_E2E_OVERWORLD_DRAWING_ACCEPTANCE_H

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "app/controller.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/features.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace yaze::test {

// Opt-in ROM fixture: never mutate the source passed by the operator. The test
// copies it before loading and retains the destination for independent review.
inline void RegisterOverworldDrawingAcceptance(ImGuiTestEngine* engine,
                                               Controller* controller) {
  if (!std::getenv("YAZE_DRAWING_ACCEPTANCE_DIR")) {
    return;
  }
  for (const char* name : {"LargeDrawing", "WideDrawing", "TallDrawing",
                           "PaletteUndoAfterNavigation"}) {
    auto* test = IM_REGISTER_TEST(engine, "OverworldAcceptance", name);
    test->UserData = controller;
    test->TestFunc = [](ImGuiTestContext* ctx) {
      namespace fs = std::filesystem;
      const char* source = std::getenv("YAZE_DRAWING_ACCEPTANCE_ROM");
      const char* directory = std::getenv("YAZE_DRAWING_ACCEPTANCE_DIR");
      IM_CHECK(source && directory);
      const fs::path root = fs::path(directory) / ctx->Test->Name;
      fs::create_directories(root);
      const fs::path input = root / "input.sfc";
      const fs::path output = root / "saved.sfc";
      IM_CHECK(!fs::exists(input) && !fs::exists(output));
      fs::copy_file(source, input);
      const auto read = [](const fs::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(stream), {});
      };
      const auto protected_bytes = read(source);
      auto* ctl = static_cast<Controller*>(ctx->Test->UserData);
      IM_CHECK(ctl->LoadRomForTesting(input.string()).ok());
      auto* manager = ctl->editor_manager();
      manager->OpenEditorAndPanelsFromFlags("Overworld", "");
      auto* editor =
          manager->GetCurrentEditorSet()->GetEditorAs<editor::OverworldEditor>(
              editor::EditorType::kOverworld);
      IM_CHECK(editor != nullptr);
      auto* windows = manager->GetWindowManager();
      const auto window = windows->GetWorkspaceWindowName(
          manager->GetCurrentSessionId(), "overworld.canvas");
      ctx->Yield(10);
      ctx->WindowFocus(window.c_str());
      const std::string name = ctx->Test->Name;
      const auto size = name == "WideDrawing" ? zelda3::AreaSizeEnum::WideArea
                        : name == "TallDrawing"
                            ? zelda3::AreaSizeEnum::TallArea
                            : zelda3::AreaSizeEnum::LargeArea;
      IM_CHECK(editor->overworld().ConfigureMultiAreaMap(0x30, size).ok());
      const int source_x = name == "TallDrawing" ? 15 : 31;
      int source_y = 222;
      if (name == "WideDrawing") {
        // Select a nonuniform patch so a one-row paste cannot be a no-op.
        const auto& tiles = editor->overworld().GetMapTiles(0);
        bool differs = false;
        for (int row = 193; row < 220 && !differs; ++row) {
          for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 2; ++x)
              differs |= tiles[source_x + x][row + y] !=
                         tiles[source_x + x][row + y + 1];
          if (differs)
            source_y = row;
        }
        IM_CHECK(differs);
      }
      editor->ow_map_canvas().set_global_scale(0.5f);
      editor->JumpToMap(name == "WideDrawing"   ? 0x31
                        : name == "TallDrawing" ? 0x38
                                                : 0x39);
      ctx->Yield(10);
      if (name == "PaletteUndoAfterNavigation") {
        const int original =
            editor->overworld().overworld_map(0x30)->area_palette();
        const int changed = original ^ 1;
        auto* canvas_window = ImGui::FindWindowByName(window.c_str());
        IM_CHECK(canvas_window != nullptr);
        // Match the real quick-properties field's ID stack, including the area.
        ImGuiID field = ImGui::GetIDWithSeed("OverworldAppearance", nullptr,
                                             canvas_window->ID);
        field = ImGui::GetIDWithSeed(0x30, field);
        field = ImGui::GetIDWithSeed("Palette", nullptr, field);
        field = ImGui::GetIDWithSeed("##Value", nullptr, field);
        // Replace one selected digit: a single edit, with no intermediate hex value.
        ctx->ItemClick(field);
        ctx->KeyPress(ImGuiKey_End);
        ctx->KeyPress(ImGuiMod_Shift | ImGuiKey_LeftArrow);
        const char digit[] = {"0123456789ABCDEF"[changed & 0x0F], 0};
        ctx->KeyChars(digit);
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(3);
        for (int map_id : {0x30, 0x31, 0x38, 0x39})
          IM_CHECK_EQ(editor->overworld().overworld_map(map_id)->area_palette(),
                      changed);
        const auto changed_palette = editor->map_bitmap(0x30).palette();
        // Keep the pointer off the canvas so cursor-follow cannot undo the jump.
        ctx->MouseMoveToPos(
            ImVec2(canvas_window->Pos.x + 8, canvas_window->Pos.y + 8));
        editor->JumpToMap(0x00);
        ctx->Yield(10);
        IM_CHECK_EQ(editor->current_map_id(), 0x00);
        const auto other_id =
            editor->overworld().overworld_map(0)->area_palette();
        const auto other_palette = editor->map_bitmap(0).palette();
        const auto other_atlas = editor->tile16_blockset().atlas.palette();
        IM_CHECK(editor->Undo().ok());
        ctx->Yield(3);
        IM_CHECK_EQ(editor->current_map_id(), 0x00);
        for (int map_id : {0x30, 0x31, 0x38, 0x39}) {
          const auto* map = editor->overworld().overworld_map(map_id);
          IM_CHECK_EQ(map->area_palette(), original);
          IM_CHECK(editor->map_bitmap(map_id).palette() ==
                   map->current_palette());
          IM_CHECK(editor->map_bitmap(map_id).palette() != changed_palette);
        }
        IM_CHECK_EQ(editor->overworld().overworld_map(0)->area_palette(),
                    other_id);
        IM_CHECK(editor->map_bitmap(0).palette() == other_palette);
        IM_CHECK(editor->tile16_blockset().atlas.palette() == other_atlas);
        IM_CHECK(editor->Redo().ok());
        ctx->Yield(3);
        for (int map_id : {0x30, 0x31, 0x38, 0x39}) {
          IM_CHECK_EQ(editor->overworld().overworld_map(map_id)->area_palette(),
                      changed);
          IM_CHECK(editor->map_bitmap(map_id).palette() == changed_palette);
        }
        IM_CHECK_EQ(editor->current_map_id(), 0x00);
        IM_CHECK_EQ(editor->overworld().overworld_map(0)->area_palette(),
                    other_id);
        IM_CHECK(editor->map_bitmap(0).palette() == other_palette);
        IM_CHECK(editor->tile16_blockset().atlas.palette() == other_atlas);
        IM_CHECK(read(source) == protected_bytes);
        IM_CHECK(read(input) == protected_bytes);
        ctx->LogInfo(
            "Palette UI edit, unrelated-area navigation, undo/redo passed");
        return;
      }
      auto& canvas = editor->ow_map_canvas();
      const auto screen = [&](int x, int y) {
        const auto origin = canvas.zero_point();
        const auto scroll = canvas.scrolling();
        const float scale = canvas.global_scale();
        return ImVec2(origin.x + scroll.x + (x * 16 + 8) * scale,
                      origin.y + scroll.y + (y * 16 + 8) * scale);
      };
      auto& world = editor->overworld().GetMapTiles(0);
      const auto baseline = world;
      const bool dirty_before = editor->rom()->dirty();
      const bool undo_before = editor->CanUndo();
      // Cross the area boundary (all four quadrants for Large), then paste
      // the captured 2x3 brush one row lower.
      ctx->MouseMoveToPos(screen(source_x, source_y));
      ctx->Yield(3);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->MouseMoveToPos(screen(source_x + 1, source_y + 2));
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(3);
      const auto* brush = editor->tile_painting()->selection_brush();
      IM_CHECK(brush != nullptr);
      IM_CHECK_EQ(brush->width, 2);
      IM_CHECK_EQ(brush->height, 3);
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 2; ++x)
          IM_CHECK_EQ(brush->at(x, y), baseline[source_x + x][source_y + y]);
      IM_CHECK(world == baseline);
      IM_CHECK_EQ(editor->rom()->dirty(), dirty_before);
      IM_CHECK_EQ(editor->CanUndo(), undo_before);
      IM_CHECK(editor->Copy().ok());
      ctx->MouseMoveToPos(screen(source_x, source_y + 1));
      ctx->Yield(3);
      IM_CHECK(editor->Paste().ok());
      ctx->Yield(3);
      const auto painted = world;
      IM_CHECK(painted != baseline);
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 2; ++x)
          IM_CHECK_EQ(world[source_x + x][source_y + 1 + y],
                      baseline[source_x + x][source_y + y]);
      IM_CHECK(editor->Undo().ok());
      ctx->Yield(3);
      IM_CHECK(world == baseline);
      IM_CHECK(editor->Redo().ok());
      ctx->Yield(3);
      IM_CHECK(world == painted);
      IM_CHECK(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
      const auto save = editor->Save();
      ctx->LogInfo("Editor save: %s", save.ToString().c_str());
      IM_CHECK(save.ok());
      IM_CHECK(editor->rom()
                   ->SaveToFile(
                       {.filename = output.string(), .require_backup = true})
                   .ok());
      auto reopened = std::make_unique<Rom>();
      IM_CHECK(reopened->LoadFromFile(output.string()).ok());
      auto overworld = std::make_unique<zelda3::Overworld>(reopened.get());
      overworld->SetGameData(manager->GetCurrentGameData());
      IM_CHECK(overworld->Load(reopened.get()).ok());
      IM_CHECK(overworld->GetMapTiles(0) == painted);
      IM_CHECK(overworld->overworld_map(0x30)->area_size() == size);
      IM_CHECK(read(source) == protected_bytes);
      IM_CHECK(read(input) == protected_bytes);
      ctx->LogInfo(
          "Capture/paste/undo/redo and saved destination reopen passed: %s",
          output.string().c_str());
    };
  }
}

}  // namespace yaze::test
#endif
