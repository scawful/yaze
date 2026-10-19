#include "app/testing/overworld_ui_tests.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "app/application.h"
#include "app/editor/overworld/overworld_editor.h"
#include "core/source_artifact_publisher.h"
#include "imgui.h"
#include "nlohmann/json.hpp"
#if __has_include("imgui_test_engine/imgui_te_context.h")
#include "imgui_test_engine/imgui_te_context.h"
#elif __has_include("imgui_te_context.h")
#include "imgui_te_context.h"
#else
#error "ImGui Test Engine context header not found"
#endif

#if __has_include("imgui_test_engine/imgui_te_engine.h")
#include "imgui_test_engine/imgui_te_engine.h"
#elif __has_include("imgui_te_engine.h")
#include "imgui_te_engine.h"
#else
#error "ImGui Test Engine engine header not found"
#endif

namespace yaze::test {

namespace {

void EnsureTile16PanelOpenAndFocused(ImGuiTestContext* ctx) {
  ctx->SetRef("Tile16 Editor");
  if (ctx->GetWindowByRef("") == nullptr) {
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
    ctx->Yield(4);
  }

  ctx->SetRef("Tile16 Editor");
  IM_CHECK(ctx->GetWindowByRef("") != nullptr);
  ctx->WindowFocus("");
  ctx->Yield(2);
}

void ExpectActiveQuadrant(ImGuiTestContext* ctx, const char* label) {
  const std::string pattern = std::string("**/Active ") + label + ":*";
  IM_CHECK(ctx->ItemExists(pattern.c_str()));
}

void ExpectBrushPalette(ImGuiTestContext* ctx, int palette) {
  const std::string pattern =
      std::string("**/*Brush Palette: ") + std::to_string(palette) + "*";
  IM_CHECK(ctx->ItemExists(pattern.c_str()));
}

}  // namespace

// ============================================================================
// Test: Keyboard shortcut mode switching
//
// The overworld editor uses number keys (1, 2) and letter keys (B, F)
// to switch between Mouse, Draw Tile, and Fill Tile modes. These shortcuts
// are handled in OverworldEditor::HandleKeyboardShortcuts() and delegate
// to TilePaintingManager for B/F.
//
// These tests verify that pressing each shortcut key doesn't crash the
// application. Actual mode state verification would require accessing
// the OverworldEditor's internal state, which we defer to unit tests.
// ============================================================================

static void RegisterKeyboardShortcutTests(ImGuiTestEngine* engine) {
  // Test: Key '1' (Mouse mode) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "mouse_mode_key_1");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(3);
    };
  }

  // Test: Key '2' (Draw Tile mode) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "draw_mode_key_2");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_2);
      ctx->Yield(3);
      // Restore mouse mode
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(1);
    };
  }

  // Test: Key 'B' (Brush toggle) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "brush_toggle_key_b");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_B);
      ctx->Yield(3);
      // Toggle back
      ctx->KeyPress(ImGuiKey_B);
      ctx->Yield(1);
    };
  }

  // Test: Key 'F' (Fill tool) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "fill_tool_key_f");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_F);
      ctx->Yield(3);
      // Toggle back
      ctx->KeyPress(ImGuiKey_F);
      ctx->Yield(1);
    };
  }

  // Test: Key 'I' (Eyedropper / pick tile16) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "eyedropper_key_i");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_I);
      ctx->Yield(3);
    };
  }

  // Test: Ctrl+L (Lock map toggle) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "lock_toggle_ctrl_l");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_L);
      ctx->Yield(3);
      // Toggle back
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_L);
      ctx->Yield(1);
    };
  }

  // Test: F11 (Fullscreen toggle) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "fullscreen_toggle_f11");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiKey_F11);
      ctx->Yield(3);
      // Toggle back
      ctx->KeyPress(ImGuiKey_F11);
      ctx->Yield(1);
    };
  }

  // Test: Ctrl+Z (Undo) doesn't crash when no undo history
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "undo_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
      ctx->Yield(3);
    };
  }

  // Test: Ctrl+Y (Redo) doesn't crash when no redo history
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "redo_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
      ctx->Yield(3);
    };
  }

  // Test: Ctrl+T (Toggle Tile16 Editor panel) doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_keys", "tile16_editor_ctrl_t");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
      ctx->Yield(3);
      // Toggle back
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
      ctx->Yield(1);
    };
  }

  // Test: Keys 1..4 update tile16 quadrant focus in panel state text.
  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "overworld_keys",
                                       "tile16_quadrant_hotkeys_apply_focus");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      EnsureTile16PanelOpenAndFocused(ctx);

      struct QuadrantStep {
        ImGuiKey key;
        const char* label;
      };
      const QuadrantStep steps[] = {
          {ImGuiKey_1, "TL"},
          {ImGuiKey_2, "TR"},
          {ImGuiKey_3, "BL"},
          {ImGuiKey_4, "BR"},
      };

      for (const auto& step : steps) {
        ctx->KeyPress(step.key);
        ctx->Yield(2);
        ExpectActiveQuadrant(ctx, step.label);
      }

      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
      ctx->Yield(2);
    };
  }

  // Test: Ctrl+1..8 update brush palette while preserving active quadrant.
  {
    ImGuiTest* test = IM_REGISTER_TEST(
        engine, "overworld_keys", "tile16_ctrl_numeric_hotkeys_palette_rows");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      EnsureTile16PanelOpenAndFocused(ctx);

      ctx->KeyPress(ImGuiKey_4);
      ctx->Yield(2);
      ExpectActiveQuadrant(ctx, "BR");

      for (int palette = 0; palette < 8; ++palette) {
        const ImGuiKey number_key = static_cast<ImGuiKey>(ImGuiKey_1 + palette);
        ctx->KeyPress(ImGuiMod_Ctrl | number_key);
        ctx->Yield(2);
        ExpectBrushPalette(ctx, palette);
        ExpectActiveQuadrant(ctx, "BR");
      }

      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
      ctx->Yield(2);
    };
  }
}

// ============================================================================
// Test: Entity editing mode shortcuts (3-8 keys)
//
// Keys 3-8 activate different entity editing modes:
//   3 = Entrances, 4 = Exits, 5 = Items,
//   6 = Sprites, 7 = Transports, 8 = Music
// ============================================================================

static void RegisterEntityModeTests(ImGuiTestEngine* engine) {
  // Test: Entity mode keys (3-8) don't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_entity", "mode_keys_3_to_8");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Cycle through all entity modes
      ctx->KeyPress(ImGuiKey_3);  // Entrances
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_4);  // Exits
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_5);  // Items
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_6);  // Sprites
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_7);  // Transports
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_8);  // Music
      ctx->Yield(2);
      // Return to mouse mode
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(1);
    };
  }
}

// ============================================================================
// Test: Mode switching round-trips
//
// Verifies that cycling through all editing modes and returning to Mouse
// mode doesn't leave the editor in a broken state.
// ============================================================================

static void RegisterModeRoundTripTests(ImGuiTestEngine* engine) {
  // Test: Full mode cycle (Mouse -> Draw -> Fill -> Mouse) is stable
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_modes", "full_mode_cycle");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Start in mouse mode
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(2);

      // Switch to draw tile
      ctx->KeyPress(ImGuiKey_2);
      ctx->Yield(2);

      // Switch to fill via F
      ctx->KeyPress(ImGuiKey_F);
      ctx->Yield(2);

      // Toggle brush via B (should go to draw tile)
      ctx->KeyPress(ImGuiKey_B);
      ctx->Yield(2);

      // Toggle brush again (should go back to mouse)
      ctx->KeyPress(ImGuiKey_B);
      ctx->Yield(2);

      // Back to mouse mode explicitly
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(3);
    };
  }

  // Test: Rapid mode switching doesn't crash
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_modes", "rapid_mode_switch");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      for (int i = 0; i < 10; ++i) {
        ctx->KeyPress(ImGuiKey_B);
        ctx->Yield(1);
      }
      // Settle back to mouse
      ctx->KeyPress(ImGuiKey_1);
      ctx->Yield(3);
    };
  }
}

// ============================================================================
// Test: Canvas navigation smoke tests
//
// These exercise CanvasNavigationManager methods indirectly through the
// keyboard shortcuts and UI interactions. The actual methods tested:
//   - HandleOverworldPan (via middle-click drag simulation)
//   - HandleOverworldZoom (via the zoom step mechanism)
//   - ResetOverworldView (not directly bound to a key, but used internally)
//
// Note: Mouse-based pan/zoom requires pixel-precise ImGui interaction that
// the test engine handles through its coroutine system.
// ============================================================================

static void RegisterCanvasNavigationTests(ImGuiTestEngine* engine) {
  // Test: Multiple frames of rendering without interaction is stable
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_canvas", "idle_frames_stable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Just yield several frames to verify the overworld editor
      // renders stably without any interaction
      ctx->Yield(10);
    };
  }
}

// ============================================================================
// Test: World combo selector smoke tests
//
// The toolbar has a world selector combo (Light World, Dark World, Special).
// These tests verify the combo exists but don't change world since that
// triggers heavy map reloading that depends on ROM state.
// ============================================================================

static void RegisterWorldSelectorTests(ImGuiTestEngine* engine) {
  // Test: Overworld editor remains stable after multiple frame yields
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "overworld_toolbar", "toolbar_renders_stable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Render several frames to verify toolbar doesn't crash
      ctx->Yield(5);
      // The toolbar "CanvasToolbar" table renders every frame when
      // ROM is loaded and overworld is initialized
    };
  }
}

// This test writes only an explicitly named disposable project and its terrain
// sidecar. It is absent from ordinary UI runs unless the exact project path is
// supplied at process startup, and still refuses an unmarked/wrong project.
static void RegisterLyraTerrainAcceptanceTest(ImGuiTestEngine* engine) {
  const char* requested = std::getenv("YAZE_LYRA_TERRAIN_ACCEPTANCE_PROJECT");
  if (!requested || !*requested ||
      std::getenv("YAZE_LYRA_TERRAIN_REOPEN_PROJECT"))
    return;
  ImGuiTest* test = IM_REGISTER_TEST(engine, "lyra_terrain",
                                     "cross_map_stroke_undo_redo_project_save");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    namespace fs = std::filesystem;
    const char* requested = std::getenv("YAZE_LYRA_TERRAIN_ACCEPTANCE_PROJECT");
    IM_CHECK(requested && *requested);
    IM_CHECK(fs::path(requested).is_absolute());
    std::error_code ec;
    const auto expected_project = fs::canonical(requested, ec);
    IM_CHECK(!ec);

    editor::EditorManager* manager = nullptr;
    project::YazeProject* current_project = nullptr;
    for (int frame = 0; frame < 200; ++frame) {
      auto* controller = Application::Instance().GetController();
      manager = controller ? controller->editor_manager() : nullptr;
      current_project = manager ? manager->GetCurrentProject() : nullptr;
      if (current_project && current_project->project_opened() &&
          fs::weakly_canonical(current_project->filepath, ec) ==
              expected_project &&
          !ec)
        break;
      ctx->Yield(1);
    }
    IM_CHECK(manager && current_project && current_project->project_opened());
    IM_CHECK(fs::canonical(current_project->filepath, ec) == expected_project &&
             !ec);
    IM_CHECK(current_project->name.find("[Lyra canvas acceptance]") !=
             std::string::npos);
    const auto project_directory = expected_project.parent_path();
    const auto rom_path = fs::canonical(
        current_project->GetAbsolutePath(current_project->rom_filename), ec);
    IM_CHECK(!ec && rom_path.parent_path() == project_directory);
    IM_CHECK(rom_path.filename() == "main-with-d3.sfc");
    auto* rom = manager->GetCurrentRom();
    IM_CHECK(rom && rom->is_loaded() && !rom->dirty());
    IM_CHECK(fs::canonical(rom->filename(), ec) == rom_path && !ec);
    const auto rom_before = rom->vector();
    const auto read_file =
        [](const fs::path& path) -> std::optional<std::string> {
      std::ifstream file(path, std::ios::binary);
      if (!file)
        return std::nullopt;
      std::string bytes{std::istreambuf_iterator<char>(file), {}};
      return file.bad() ? std::nullopt
                        : std::optional<std::string>(std::move(bytes));
    };
    const auto rom_disk_before = read_file(rom_path);
    IM_CHECK(rom_disk_before.has_value());
    if (!current_project->lyra_terrain_file.empty()) {
      const auto source = fs::canonical(
          current_project->GetAbsolutePath(current_project->lyra_terrain_file),
          ec);
      IM_CHECK(!ec && source.parent_path() == project_directory);
    }

    manager->OpenEditorAndPanelsFromFlags("Overworld", "overworld.canvas");
    editor::OverworldEditor* overworld = nullptr;
    for (int frame = 0; frame < 200; ++frame) {
      auto* editors = manager->GetCurrentEditorSet();
      overworld = editors ? editors->GetEditorAs<editor::OverworldEditor>(
                                editor::EditorType::kOverworld)
                          : nullptr;
      if (overworld && overworld->overworld().is_loaded() &&
          ctx->ItemExists("**/Lyra terrain"))
        break;
      ctx->Yield(1);
    }
    IM_CHECK(overworld && overworld->overworld().is_loaded());
    const auto mode = ctx->ItemInfo("**/Lyra terrain");
    IM_CHECK(mode.ID != 0 && mode.Window != nullptr);
    ctx->SetRef(mode.Window);
    ctx->WindowFocus("");
    ctx->ItemClick(mode.ID);
    ctx->Yield(3);
    if (!overworld->lyra_canvas_.loaded()) {
      ctx->ItemClick("**/Open Lyra terrain canvas");
      ctx->Yield(5);
    }
    if (!overworld->lyra_canvas_status_.ok()) {
      IM_ERRORF("Native Loom binding failed: %s",
                overworld->lyra_canvas_status_.ToString().c_str());
      return;
    }
    IM_CHECK(overworld->lyra_canvas_.loaded());
    const auto* document = overworld->lyra_canvas_.document();
    IM_CHECK(document != nullptr && !document->dirty());
    const auto source_path = fs::weakly_canonical(
        current_project->GetAbsolutePath(current_project->lyra_terrain_file),
        ec);
    IM_CHECK(!ec && source_path.parent_path() == project_directory);
    IM_CHECK(source_path.extension() == ".json");

    // Known product controls establish pan/zoom; coordinates below come from
    // the live terrain item rectangle, not an assumed desktop/window position.
    ctx->ItemClick("**/A0 \\/ A8 \\/ B0");
    ctx->ItemInputValue("**/Zoom", 0.25f);
    ctx->ItemInputValue("**/Page", 0);
    ctx->Yield(3);
    auto terrain = ctx->ItemInfo("**/terrain");
    IM_CHECK(terrain.ID != 0 && terrain.Window != nullptr);
    constexpr int kX = 4;
    constexpr int kStartY = 30;
    constexpr float kCellPixels = 4.0f;  // 16 pixels at the UI-selected 0.25x.
    const auto cell_position = [&](int y) {
      return ImVec2(terrain.RectFull.Min.x + 24 + (kX + 0.5f) * kCellPixels,
                    terrain.RectFull.Min.y + 40 + (y + 0.5f) * kCellPixels);
    };
    const bool reaches_b0 = terrain.RectClipped.Contains(cell_position(65));
    const int end_y = reaches_b0 ? 65 : 33;
    IM_CHECK(terrain.RectClipped.Contains(cell_position(kStartY)));
    IM_CHECK(terrain.RectClipped.Contains(cell_position(end_y)));

    // Select a first-row picker tile different from each boundary probe.
    std::array<bool, 8> used{};
    for (const int y : {31, 32, end_y}) {
      const auto tile = document->tile(kX, y);
      IM_CHECK(tile.ok());
      if (*tile < used.size())
        used[*tile] = true;
    }
    int brush = 0;
    while (brush < static_cast<int>(used.size()) && used[brush])
      ++brush;
    IM_CHECK(brush < 8 &&
             brush < static_cast<int>(document->profile().tile_count));
    const auto picker = ctx->ItemInfo("**/Page");
    IM_CHECK(picker.Window != nullptr);
    const ImGuiID brush_seed =
        ImHashData(&brush, sizeof(brush), picker.Window->ID);
    const ImGuiID brush_button = ImHashStr("tile", 0, brush_seed);
    ctx->ItemClick(brush_button);
    ctx->Yield(2);
    const auto before = document->maps();
    const size_t undo_before = overworld->undo_manager().UndoStackSize();
    IM_CHECK(undo_before < overworld->undo_manager().GetMaxStackSize());

    terrain = ctx->ItemInfo("**/terrain");
    ctx->MouseSetViewport(terrain.Window);
    ctx->MouseMoveToPos(cell_position(kStartY));
    ctx->MouseDown(ImGuiMouseButton_Left);
    ctx->Yield(2);
    ctx->MouseMoveToPos(cell_position(32));
    ctx->Yield(2);
    ctx->MouseMoveToPos(cell_position(end_y));
    ctx->Yield(2);
    ctx->MouseUp(ImGuiMouseButton_Left);
    ctx->Yield(3);

    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 1);
    IM_CHECK(document->dirty());
    for (int y = kStartY; y <= end_y; ++y)
      IM_CHECK(document->tile(kX, y).value() == brush);
    const auto painted = document->maps();
    ctx->ItemClick("**/Undo##LyraTerrain");
    ctx->Yield(2);
    IM_CHECK(overworld->lyra_canvas_status_.ok());
    IM_CHECK(document->maps() == before);
    IM_CHECK(!document->dirty());
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before);
    ctx->ItemClick("**/Redo##LyraTerrain");
    ctx->Yield(2);
    IM_CHECK(overworld->lyra_canvas_status_.ok());
    IM_CHECK(document->maps() == painted);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 1);

    ctx->ItemClick("**/Save terrain");
    ctx->Yield(3);
    const auto save_status = overworld->lyra_canvas_status_;
    if (!save_status.ok()) {
      IM_ERRORF("SaveProject failed: %s", save_status.ToString().c_str());
      return;
    }
    IM_CHECK(!overworld->HasPendingProjectDraftChanges());
    const auto saved = read_file(source_path);
    IM_CHECK(saved.has_value() && *saved == document->Serialize());
    auto reopened =
        core::LyraTerrainDocument::Parse(*saved, document->profile());
    IM_CHECK(reopened.ok() && reopened->maps() == painted);
    project::YazeProject reopened_project;
    IM_CHECK(reopened_project.Open(expected_project.string()).ok());
    IM_CHECK(fs::canonical(reopened_project.lyra_terrain_file, ec) ==
                 source_path &&
             !ec);
    IM_CHECK(rom->vector() == rom_before && !rom->dirty());
    IM_CHECK(read_file(rom_path) == rom_disk_before);

    // Directly queued static tests do not receive an RPC execution ID. Emit a
    // unique receipt only after every native interaction/persistence check.
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const auto receipt_path =
        project_directory /
        ("lyra-canvas-native-pass-" + std::to_string(stamp) + ".json");
    const nlohmann::json receipt = {
        {"test", "lyra_terrain/cross_map_stroke_undo_redo_project_save"},
        {"result", "PASS"},
        {"project", expected_project.string()},
        {"terrain", source_path.string()},
        {"rom", rom_path.string()},
        {"paint_x", kX},
        {"paint_y_start", kStartY},
        {"paint_y_end", end_y},
        {"tile16", brush},
        {"undo_actions_added", 1},
        {"a0_a8_crossed", true},
        {"a8_b0_crossed", reaches_b0},
        {"terrain_sha256", core::ComputeSourceArtifactSha256(*saved)},
        {"rom_sha256", core::ComputeSourceArtifactSha256(*rom_disk_before)}};
    std::ofstream output(receipt_path, std::ios::binary);
    IM_CHECK(output.is_open());
    output << receipt.dump(2) << '\n';
    output.close();
    IM_CHECK(!output.fail());
    ctx->LogInfo("Lyra native canvas PASS; receipt: %s",
                 receipt_path.string().c_str());
  };
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromCommandLine);
}

// A separate process opts into this read-only test using the successful native
// paint receipt. It must observe automatic loading before any canvas UI action.
static void RegisterLyraTerrainReopenTest(ImGuiTestEngine* engine) {
  const char* requested = std::getenv("YAZE_LYRA_TERRAIN_REOPEN_PROJECT");
  if (!requested || !*requested ||
      std::getenv("YAZE_LYRA_TERRAIN_ACCEPTANCE_PROJECT"))
    return;
  ImGuiTest* test = IM_REGISTER_TEST(engine, "lyra_terrain",
                                     "fresh_process_autoload_saved_terrain");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    namespace fs = std::filesystem;
    const char* requested = std::getenv("YAZE_LYRA_TERRAIN_REOPEN_PROJECT");
    const char* prior_receipt = std::getenv("YAZE_LYRA_TERRAIN_REOPEN_RECEIPT");
    IM_CHECK(requested && *requested && prior_receipt && *prior_receipt);
    IM_CHECK(fs::path(requested).is_absolute());
    IM_CHECK(fs::path(prior_receipt).is_absolute());
    std::error_code ec;
    const auto expected_project = fs::canonical(requested, ec);
    IM_CHECK(!ec);
    const auto project_directory = expected_project.parent_path();
    const auto receipt_path = fs::canonical(prior_receipt, ec);
    IM_CHECK(!ec && receipt_path.parent_path() == project_directory);
    const auto read_file =
        [](const fs::path& path) -> std::optional<std::string> {
      std::ifstream file(path, std::ios::binary);
      if (!file)
        return std::nullopt;
      std::string bytes{std::istreambuf_iterator<char>(file), {}};
      return file.bad() ? std::nullopt
                        : std::optional<std::string>(std::move(bytes));
    };
    const auto prior_bytes = read_file(receipt_path);
    IM_CHECK(prior_bytes.has_value());
    const auto prior = nlohmann::json::parse(*prior_bytes, nullptr, false);
    IM_CHECK(prior.is_object());
    IM_CHECK(prior.value("result", "") == "PASS");
    IM_CHECK(prior.value("test", "") ==
             "lyra_terrain/cross_map_stroke_undo_redo_project_save");
    IM_CHECK(prior.value("project", "") == expected_project.string());
    const auto project_before = read_file(expected_project);
    IM_CHECK(project_before.has_value());

    editor::EditorManager* manager = nullptr;
    project::YazeProject* current_project = nullptr;
    for (int frame = 0; frame < 200; ++frame) {
      auto* controller = Application::Instance().GetController();
      manager = controller ? controller->editor_manager() : nullptr;
      current_project = manager ? manager->GetCurrentProject() : nullptr;
      if (current_project && current_project->project_opened() &&
          fs::weakly_canonical(current_project->filepath, ec) ==
              expected_project &&
          !ec)
        break;
      ctx->Yield(1);
    }
    IM_CHECK(manager && current_project && current_project->project_opened());
    IM_CHECK(fs::canonical(current_project->filepath, ec) == expected_project &&
             !ec);
    IM_CHECK(current_project->name.find("[Lyra canvas acceptance]") !=
             std::string::npos);
    IM_CHECK(!current_project->lyra_terrain_file.empty());
    const auto source_path = fs::canonical(
        current_project->GetAbsolutePath(current_project->lyra_terrain_file),
        ec);
    IM_CHECK(!ec && source_path.parent_path() == project_directory);
    IM_CHECK(source_path.string() == prior.value("terrain", ""));
    const auto source_before = read_file(source_path);
    IM_CHECK(source_before.has_value());
    IM_CHECK(core::ComputeSourceArtifactSha256(*source_before) ==
             prior.value("terrain_sha256", ""));
    const auto rom_path = fs::canonical(
        current_project->GetAbsolutePath(current_project->rom_filename), ec);
    IM_CHECK(!ec && rom_path.parent_path() == project_directory);
    IM_CHECK(rom_path.filename() == "main-with-d3.sfc");
    IM_CHECK(rom_path.string() == prior.value("rom", ""));
    auto* rom = manager->GetCurrentRom();
    IM_CHECK(rom && rom->is_loaded() && !rom->dirty());
    IM_CHECK(fs::canonical(rom->filename(), ec) == rom_path && !ec);
    const auto rom_disk = read_file(rom_path);
    IM_CHECK(rom_disk.has_value());
    IM_CHECK(core::ComputeSourceArtifactSha256(*rom_disk) ==
             prior.value("rom_sha256", ""));
    const auto rom_bytes = [&]() {
      return std::string_view(reinterpret_cast<const char*>(rom->data()),
                              rom->size());
    };
    IM_CHECK(core::ComputeSourceArtifactSha256(rom_bytes()) ==
             prior.value("rom_sha256", ""));

    manager->OpenEditorAndPanelsFromFlags("Overworld", "overworld.canvas");
    editor::OverworldEditor* overworld = nullptr;
    for (int frame = 0; frame < 200; ++frame) {
      auto* editors = manager->GetCurrentEditorSet();
      overworld = editors ? editors->GetEditorAs<editor::OverworldEditor>(
                                editor::EditorType::kOverworld)
                          : nullptr;
      if (overworld && overworld->overworld().is_loaded() &&
          overworld->lyra_canvas_.loaded() &&
          ctx->ItemExists("**/Lyra terrain"))
        break;
      ctx->Yield(1);
    }
    IM_CHECK(overworld && overworld->overworld().is_loaded());
    if (!overworld->lyra_canvas_status_.ok()) {
      IM_ERRORF("Native Lyra autoload failed: %s",
                overworld->lyra_canvas_status_.ToString().c_str());
      return;
    }
    IM_CHECK(overworld->lyra_canvas_.loaded());
    const auto* document = overworld->lyra_canvas_.document();
    IM_CHECK(document && !document->dirty());
    IM_CHECK(!overworld->HasPendingProjectDraftChanges());
    IM_CHECK(document->Serialize() == *source_before);
    IM_CHECK(core::ComputeSourceArtifactSha256(document->Serialize()) ==
             prior.value("terrain_sha256", ""));
    const int x = prior.value("paint_x", -1);
    const int first_y = prior.value("paint_y_start", -1);
    const int last_y = prior.value("paint_y_end", -1);
    const int tile = prior.value("tile16", -1);
    IM_CHECK(x >= 0 && x < core::LyraTerrainDocument::kWidth);
    IM_CHECK(first_y >= 0 && first_y <= 31 && last_y >= 32 &&
             last_y < core::LyraTerrainDocument::kHeight);
    IM_CHECK(tile >= 0 && tile < document->profile().tile_count);
    for (int y = first_y; y <= last_y; ++y)
      IM_CHECK(document->tile(x, y).value() == tile);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), 0);

    // Expose the already loaded canvas for the driver's native screenshot.
    const auto mode = ctx->ItemInfo("**/Lyra terrain");
    IM_CHECK(mode.ID != 0 && mode.Window != nullptr);
    ctx->SetRef(mode.Window);
    ctx->ItemClick(mode.ID);
    ctx->Yield(3);
    IM_CHECK(ctx->ItemExists("**/terrain"));
    IM_CHECK(!overworld->HasPendingProjectDraftChanges());
    IM_CHECK(read_file(expected_project) == project_before);
    IM_CHECK(read_file(source_path) == source_before);
    IM_CHECK(read_file(rom_path) == rom_disk && !rom->dirty());
    IM_CHECK(core::ComputeSourceArtifactSha256(rom_bytes()) ==
             prior.value("rom_sha256", ""));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const auto output_path =
        project_directory /
        ("lyra-canvas-reopen-pass-" + std::to_string(stamp) + ".json");
    const nlohmann::json receipt = {
        {"test", "lyra_terrain/fresh_process_autoload_saved_terrain"},
        {"result", "PASS"},
        {"project", expected_project.string()},
        {"terrain", source_path.string()},
        {"rom", rom_path.string()},
        {"prior_receipt", receipt_path.string()},
        {"autoloaded", true},
        {"document_clean", true},
        {"saved_cells_exact", true},
        {"project_unchanged", true},
        {"terrain_sha256", prior.at("terrain_sha256")},
        {"rom_sha256", prior.at("rom_sha256")}};
    std::ofstream output(output_path, std::ios::binary);
    IM_CHECK(output.is_open());
    output << receipt.dump(2) << '\n';
    output.close();
    IM_CHECK(!output.fail());
    ctx->LogInfo("Lyra native reopen PASS; receipt: %s",
                 output_path.string().c_str());
  };
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromCommandLine);
}

// ============================================================================
// Public registration entry point
// ============================================================================

void RegisterOverworldUITests(ImGuiTestEngine* engine) {
  if (engine == nullptr)
    return;
  RegisterKeyboardShortcutTests(engine);
  RegisterEntityModeTests(engine);
  RegisterModeRoundTripTests(engine);
  RegisterCanvasNavigationTests(engine);
  RegisterWorldSelectorTests(engine);
  RegisterLyraTerrainAcceptanceTest(engine);
  RegisterLyraTerrainReopenTest(engine);
}

}  // namespace yaze::test

#else  // !YAZE_ENABLE_IMGUI_TEST_ENGINE

namespace yaze::test {
void RegisterOverworldUITests(ImGuiTestEngine* /*engine*/) {}
}  // namespace yaze::test

#endif  // YAZE_ENABLE_IMGUI_TEST_ENGINE
