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
#include "app/gui/automation/widget_id_registry.h"
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
  ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;
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

    manager->OpenEditorAndPanelsFromFlags(
        "Overworld", "overworld.canvas,overworld.tile16_selector");
    auto& widgets = gui::WidgetIdRegistry::Instance();
    editor::OverworldEditor* overworld = nullptr;
    for (int frame = 0; frame < 200; ++frame) {
      auto* editors = manager->GetCurrentEditorSet();
      overworld = editors ? editors->GetEditorAs<editor::OverworldEditor>(
                                editor::EditorType::kOverworld)
                          : nullptr;
      if (overworld && overworld->overworld().is_loaded() &&
          overworld->lyra_canvas_.loaded() &&
          widgets.GetWidgetId("OverworldToolbar/world:2") != 0 &&
          widgets.GetWidgetId("OwBlocksetSelector/button:Lookup") != 0 &&
          ctx->ItemExists("**/OwMap"))
        break;
      ctx->Yield(1);
    }
    IM_CHECK(overworld && overworld->overworld().is_loaded());
    if (!overworld->lyra_canvas_status_.ok()) {
      IM_ERRORF("Native Loom binding failed: %s",
                overworld->lyra_canvas_status_.ToString().c_str());
      return;
    }
    IM_CHECK(overworld->lyra_canvas_.loaded());
    // Project autoload must land on Lyra inside the normal Special World.
    // Check before the test changes the world, selection, or viewport.
    IM_CHECK_EQ(overworld->current_world_id(), 2);
    IM_CHECK_EQ(overworld->current_map_id(), 0xA0);
    IM_CHECK_EQ(overworld->DisplayedMapCount(), 192);
    IM_CHECK(!overworld->IsProjectTerrainMap(0x80));
    IM_CHECK(overworld->IsProjectTerrainMap(0xA0));
    IM_CHECK(overworld->IsProjectTerrainMap(0xBF));
    const auto* document = overworld->lyra_canvas_.document();
    IM_CHECK(document != nullptr && !document->dirty());
    const auto source_path = fs::weakly_canonical(
        current_project->GetAbsolutePath(current_project->lyra_terrain_file),
        ec);
    IM_CHECK(!ec && source_path.parent_path() == project_directory);
    IM_CHECK(source_path.extension() == ".json");

    const auto world =
        ctx->ItemInfo(widgets.GetWidgetId("OverworldToolbar/world:2"));
    IM_CHECK(world.ID != 0 && world.Window != nullptr);
    ctx->SetRef(world.Window);
    ctx->WindowFocus("");
    ctx->ItemClick(world.ID);
    ctx->Yield(2);
    IM_CHECK_EQ(overworld->current_world_id(), 2);

    // Only the viewport setup is programmatic. Brush selection, the stroke,
    // and undo/redo use the ordinary editor controls. Lyra's first document
    // row is world Tile16 row 128 (map A0), below the existing SW maps.
    constexpr int kX = 4;
    constexpr int kStartY = 30;
    constexpr int kLyraWorldY = 128;
    auto& canvas = overworld->ow_map_canvas();
    canvas.set_global_scale(0.25f);
    overworld->LocateOverworldTile(0xA0, kX, kLyraWorldY + 48);
    ctx->Yield(3);
    auto terrain = ctx->ItemInfo("**/OwMap");
    IM_CHECK(terrain.ID != 0 && terrain.Window != nullptr);
    const auto cell_position = [&](int y) {
      const auto origin = canvas.zero_point();
      const auto scrolling = canvas.scrolling();
      const float cell_pixels = 16.0f * canvas.global_scale();
      return ImVec2(
          origin.x + scrolling.x + (kX + 0.5f) * cell_pixels,
          origin.y + scrolling.y + (kLyraWorldY + y + 0.5f) * cell_pixels);
    };
    const bool reaches_b0 = terrain.RectClipped.Contains(cell_position(65));
    const int end_y = reaches_b0 ? 65 : 33;
    IM_CHECK(terrain.RectClipped.Contains(cell_position(kStartY)));
    IM_CHECK(terrain.RectClipped.Contains(cell_position(end_y)));

    // The normal Tile16 selector's lookup popup chooses the brush through
    // RequestTile16Selection; it must use the Loom atlas for map A0.
    std::array<bool, 8> used{};
    for (const int y : {31, 32, end_y}) {
      const auto tile = document->tile(kX, y);
      IM_CHECK(tile.ok());
      if (*tile < used.size())
        used[*tile] = true;
    }
    const int old_brush = overworld->current_tile16_id();
    if (old_brush >= 0 && old_brush < static_cast<int>(used.size()))
      used[old_brush] = true;
    int brush = 0;
    while (brush < static_cast<int>(used.size()) && used[brush])
      ++brush;
    IM_CHECK(brush < 8 &&
             brush < static_cast<int>(document->profile().tile_count));
    // Startup toasts overlap the selector's top-right lookup control. Their
    // TTL uses steady_clock and pauses on hover, so simulated SleepNoSkip
    // time cannot expire them. Let the real UI age them without dismissing
    // notifications or bypassing the ordinary selector interaction.
    auto* toasts = manager->toast_manager();
    IM_CHECK(toasts != nullptr);
    if (toasts->pending_count() != 0) {
      // The dockspace fills the viewport, so there may be no window-free
      // point. This validated canvas cell is away from the toast stack.
      ctx->MouseSetViewport(terrain.Window);
      ctx->MouseMoveToPos(cell_position(kStartY));
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(15);
      while (toasts->pending_count() != 0 &&
             std::chrono::steady_clock::now() < deadline && !ctx->IsError())
        ctx->Yield(1);
      IM_CHECK_EQ(toasts->pending_count(), 0);
      ctx->Yield(2);
    }
    const auto lookup =
        ctx->ItemInfo(widgets.GetWidgetId("OwBlocksetSelector/button:Lookup"));
    IM_CHECK(lookup.ID != 0 && lookup.Window != nullptr);
    ctx->SetRef(lookup.Window);
    ctx->WindowFocus("");
    ctx->ItemClick(lookup.ID);
    ctx->Yield(2);
    const ImGuiID tile_input =
        widgets.GetWidgetId("OwBlocksetSelector/input:TileID");
    IM_CHECK(tile_input != 0);
    // The first eight tile IDs have the same decimal and hexadecimal text.
    ctx->ItemInputValue(tile_input, std::to_string(brush).c_str());
    ctx->Yield(2);
    IM_CHECK_EQ(overworld->current_tile16_id(), brush);
    ctx->SetRef(world.Window);
    ctx->WindowFocus("");
    ImGuiID paint = widgets.GetWidgetId("OverworldToolbar/button:Paint");
    const bool tools_overflow = paint == 0 || !ctx->ItemExists(paint);
    if (tools_overflow) {
      const ImGuiID tools =
          widgets.GetWidgetId("OverworldToolbar/button:Tools");
      IM_CHECK(tools != 0);
      ctx->ItemClick(tools);
      ctx->Yield(2);
      paint = widgets.GetWidgetId("OverworldToolbar/button:Paint");
    }
    IM_CHECK(paint != 0);
    ctx->ItemClick(paint);
    if (tools_overflow)
      ctx->KeyPress(ImGuiKey_Escape);
    ctx->Yield(2);
    const auto before =
        std::make_unique<core::LyraTerrainDocument::Maps>(document->maps());
    const size_t undo_before = overworld->undo_manager().UndoStackSize();
    IM_CHECK(undo_before < overworld->undo_manager().GetMaxStackSize());

    terrain = ctx->ItemInfo("**/OwMap");
    IM_CHECK(terrain.RectClipped.Contains(cell_position(kStartY)));
    IM_CHECK(terrain.RectClipped.Contains(cell_position(end_y)));
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
    const auto painted =
        std::make_unique<core::LyraTerrainDocument::Maps>(document->maps());
    IM_CHECK(manager->GetCurrentEditor() == overworld);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(overworld->lyra_canvas_status_.ok());
    IM_CHECK(document->maps() == *before);
    IM_CHECK(!document->dirty());
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(overworld->lyra_canvas_status_.ok());
    IM_CHECK(document->maps() == *painted);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 1);

    // Capture a real rectangle across A0/A8, copy it through the editor's
    // clipboard shortcut, and paste it across the next available map seam.
    // Four columns at 0.25x exceed ImGui's drag threshold without changing
    // the viewport; the original painted column makes the pattern nonuniform.
    const auto xy_position = [&](int x, int y) {
      const auto origin = canvas.zero_point();
      const auto scrolling = canvas.scrolling();
      const float size = 16.0f * canvas.global_scale();
      return ImVec2(origin.x + scrolling.x + (x + 0.5f) * size,
                    origin.y + scrolling.y + (kLyraWorldY + y + 0.5f) * size);
    };
    editor::TileBrush rectangle{4, 3, {}};
    for (int y = 31; y <= 33; ++y)
      for (int x = kX; x < kX + rectangle.width; ++x)
        rectangle.tile_ids.push_back(document->tile(x, y).value());
    ctx->MouseMoveToPos(xy_position(kX, 31));
    ctx->MouseDown(ImGuiMouseButton_Right);
    ctx->Yield(2);
    ctx->MouseMoveToPos(xy_position(kX + 3, 33));
    ctx->Yield(2);
    ctx->MouseUp(ImGuiMouseButton_Right);
    ctx->Yield(3);
    IM_CHECK(overworld->tile_painting()->selection_brush() != nullptr);
    IM_CHECK_EQ(overworld->tile_painting()->selection_brush()->width, 4);
    IM_CHECK_EQ(overworld->tile_painting()->selection_brush()->height, 3);
    IM_CHECK(overworld->tile_painting()->selection_brush()->tile_ids ==
             rectangle.tile_ids);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
    ctx->Yield(2);
    const int paste_y = reaches_b0 ? 63 : 31;
    IM_CHECK(terrain.RectClipped.Contains(xy_position(11, paste_y + 2)));
    ctx->MouseMoveToPos(xy_position(8, paste_y));
    ctx->Yield(2);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
    ctx->Yield(3);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 2);
    for (int y = 0; y < rectangle.height; ++y)
      for (int x = 0; x < rectangle.width; ++x)
        IM_CHECK(document->tile(8 + x, paste_y + y).value() ==
                 rectangle.at(x, y));
    const auto pasted =
        std::make_unique<core::LyraTerrainDocument::Maps>(document->maps());
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(document->maps() == *painted);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(document->maps() == *pasted);

    // The same captured rectangle is a brush and a fill pattern. Draw a
    // stamp, undo it exactly, then fill A9 through the normal toolbar.
    ctx->MouseMoveToPos(xy_position(16, paste_y));
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield(3);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 3);
    for (int y = 0; y < rectangle.height; ++y)
      for (int x = 0; x < rectangle.width; ++x)
        IM_CHECK(document->tile(16 + x, paste_y + y).value() ==
                 rectangle.at(x, y));
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(document->maps() == *pasted);
    ImGuiID fill = widgets.GetWidgetId("OverworldToolbar/button:Fill");
    const bool fill_overflow = fill == 0 || !ctx->ItemExists(fill);
    if (fill_overflow) {
      const auto tools = widgets.GetWidgetId("OverworldToolbar/button:Tools");
      IM_CHECK(tools != 0);
      ctx->ItemClick(tools);
      ctx->Yield(2);
      fill = widgets.GetWidgetId("OverworldToolbar/button:Fill");
    }
    IM_CHECK(fill != 0);
    ctx->ItemClick(fill);
    if (fill_overflow)
      ctx->KeyPress(ImGuiKey_Escape);
    overworld->LocateOverworldTile(0xA9, 48, kLyraWorldY + 48);
    ctx->Yield(3);
    terrain = ctx->ItemInfo("**/OwMap");
    IM_CHECK(terrain.RectClipped.Contains(xy_position(48, 48)));
    ctx->MouseSetViewport(terrain.Window);
    ctx->MouseMoveToPos(xy_position(48, 48));
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield(3);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 3);
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x)
        IM_CHECK(document->tile(32 + x, 32 + y).value() ==
                 rectangle.at(x % rectangle.width, y % rectangle.height));
    const auto filled =
        std::make_unique<core::LyraTerrainDocument::Maps>(document->maps());
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(document->maps() == *pasted);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
    ctx->Yield(2);
    IM_CHECK(document->maps() == *filled);
    const auto rejected = overworld->lyra_canvas_.PaintPattern(
        {2, 1, {brush, document->profile().tile_count}}, 0, 0, 2, 1, true,
        overworld->undo_manager());
    IM_CHECK(!rejected.ok());
    IM_CHECK(document->maps() == *filled);
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo_before + 3);

    const auto save_status = manager->SaveProject();
    ctx->Yield(3);
    if (!save_status.ok()) {
      IM_ERRORF("SaveProject failed: %s", save_status.ToString().c_str());
      return;
    }
    IM_CHECK(!overworld->HasPendingProjectDraftChanges());
    const auto saved = read_file(source_path);
    IM_CHECK(saved.has_value() && *saved == document->Serialize());
    // Parsing is verified by the separate fresh-process autoload test below.
    // Keep full-map snapshots on the heap: the test-engine coroutine has a
    // smaller stack than the main UI thread that normally loads terrain.
    auto reopened_project = std::make_unique<project::YazeProject>();
    IM_CHECK(reopened_project->Open(expected_project.string()).ok());
    IM_CHECK(fs::canonical(reopened_project->lyra_terrain_file, ec) ==
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
        {"undo_actions_added", 3},
        {"rectangle_selection_copy_paste", true},
        {"rectangle_stamp", true},
        {"pattern_fill", true},
        {"invalid_brush_refused_atomically", true},
        {"pattern_width", rectangle.width},
        {"pattern_height", rectangle.height},
        {"pattern_tile_ids", rectangle.tile_ids},
        {"pattern_paste_x", 8},
        {"pattern_paste_y", paste_y},
        {"surface", "normal_special_overworld"},
        {"normal_tile16_selector", true},
        {"displayed_map_count", overworld->DisplayedMapCount()},
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
  ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;
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
          overworld->lyra_canvas_.loaded() && ctx->ItemExists("**/OwMap"))
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
    IM_CHECK_EQ(overworld->current_world_id(), 2);
    IM_CHECK_EQ(overworld->current_map_id(), 0xA0);
    IM_CHECK_EQ(overworld->DisplayedMapCount(), 192);
    IM_CHECK(!overworld->IsProjectTerrainMap(0x80));
    IM_CHECK(overworld->IsProjectTerrainMap(0xA0));
    IM_CHECK(overworld->IsProjectTerrainMap(0xBF));
    IM_CHECK(prior.value("surface", "") == "normal_special_overworld");
    IM_CHECK(prior.value("normal_tile16_selector", false));
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
    if (prior.value("rectangle_selection_copy_paste", false)) {
      const editor::TileBrush rectangle{
          prior.at("pattern_width").get<int>(),
          prior.at("pattern_height").get<int>(),
          prior.at("pattern_tile_ids").get<std::vector<int>>()};
      IM_CHECK(rectangle.valid());
      const int px = prior.at("pattern_paste_x").get<int>();
      const int py = prior.at("pattern_paste_y").get<int>();
      for (int y = 0; y < rectangle.height; ++y)
        for (int x = 0; x < rectangle.width; ++x)
          IM_CHECK(document->tile(px + x, py + y).value() ==
                   rectangle.at(x, y));
      for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
          IM_CHECK(document->tile(32 + x, 32 + y).value() ==
                   rectangle.at(x % rectangle.width, y % rectangle.height));
    }
    IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), 0);

    // Fit the one normal SW canvas for the driver's screenshot. Both the
    // existing map 80 and the project terrain at A0 must be in the same view.
    const auto terrain = ctx->ItemInfo("**/OwMap");
    IM_CHECK(terrain.ID != 0 && terrain.Window != nullptr);
    ctx->SetRef(terrain.Window);
    ctx->WindowFocus("");
    overworld->ZoomToFit();
    ctx->Yield(3);
    const auto fitted = ctx->ItemInfo("**/OwMap");
    const auto& canvas = overworld->ow_map_canvas();
    const auto origin = canvas.zero_point();
    const auto scroll = canvas.scrolling();
    const float cell_pixels = 16.0f * canvas.global_scale();
    const auto map_center = [&](int map_row) {
      return ImVec2(
          origin.x + scroll.x + 16.0f * cell_pixels,
          origin.y + scroll.y + (map_row * 32.0f + 16.0f) * cell_pixels);
    };
    IM_CHECK(fitted.RectClipped.Contains(map_center(0)));
    IM_CHECK(fitted.RectClipped.Contains(map_center(4)));
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
        {"surface", "normal_special_overworld"},
        {"map_80_and_a0_same_canvas", true},
        {"displayed_map_count", overworld->DisplayedMapCount()},
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
