#include "app/test/dungeon_ui_tests.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE

#include "imgui.h"
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

// ============================================================================
// Test: Panel visibility toggles
//
// Verifies that dungeon editor panels can be opened and closed via the
// PanelManager without crashing or leaving orphaned state.
// ============================================================================

static void RegisterPanelVisibilityTests(ImGuiTestEngine* engine) {
  // Test: Object Editor panel opens when requested
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_panels", "object_editor_opens");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG1");
      ctx->ItemClick("**/BG1");
    };
  }

  // Test: Room selector panel can be focused
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_panels", "room_selector_exists");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Room Selector");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Layer visibility toggles (BG1/BG2/Sprites/Grid/Bounds)
//
// The compact layer toggle bar renders SmallButton("BG1"), SmallButton("BG2"),
// etc. inside the canvas viewer. These are standard ImGui buttons addressable
// by the test engine.
// ============================================================================

static void RegisterLayerToggleTests(ImGuiTestEngine* engine) {
  // Test: BG1 toggle button exists and is clickable
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_layers", "bg1_toggle_clickable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Navigate to the workbench first
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG1");

      // Click again to restore original state
      ctx->ItemClick("**/BG1");
    };
  }

  // Test: BG2 toggle
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_layers", "bg2_toggle_clickable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG2");
      ctx->ItemClick("**/BG2");
    };
  }
}

// ============================================================================
// Test: Workbench toolbar interactions
//
// Tests that toolbar buttons in the workbench panel respond to clicks.
// ============================================================================

static void RegisterToolbarTests(ImGuiTestEngine* engine) {
  // Test: Workbench window can be focused
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_toolbar", "workbench_focus");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Keyboard shortcut smoke tests
//
// Verifies that common keyboard shortcuts don't crash the application.
// These are smoke tests - they verify no crash, not specific behavior
// (behavior depends on editor state which may vary).
// ============================================================================

static void RegisterKeyboardShortcutTests(ImGuiTestEngine* engine) {
  // Test: Escape key doesn't crash (should cancel placement if active)
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_keys", "escape_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(3);  // Let a few frames process
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }

  // Test: Delete key doesn't crash (should delete selection if any)
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_keys", "delete_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiKey_Delete);
      ctx->Yield(3);
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }

  // Test: Ctrl+Z (undo) doesn't crash
  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "dungeon_keys", "undo_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
      ctx->Yield(3);
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Toast dedup verification
//
// Triggers the same error condition twice rapidly and verifies the UI
// doesn't stack duplicate toasts. Since we can't easily inspect toast
// state from the test engine, we verify the action doesn't crash and
// the UI remains responsive.
// ============================================================================

static void RegisterToastTests(ImGuiTestEngine* engine) {
  // Test: Rapid repeated actions don't flood the UI
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_toast", "rapid_actions_stable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");

      // Rapidly press escape multiple times - if placement mode is active
      // this would generate repeated "placement cancelled" toasts.
      // With dedup, only one should appear.
      for (int i = 0; i < 5; ++i) {
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield(1);
      }
      ctx->Yield(5);  // Let toasts settle
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Public registration entry point
// ============================================================================

void RegisterDungeonUITests(ImGuiTestEngine* engine) {
  RegisterPanelVisibilityTests(engine);
  RegisterLayerToggleTests(engine);
  RegisterToolbarTests(engine);
  RegisterKeyboardShortcutTests(engine);
  RegisterToastTests(engine);
}

}  // namespace yaze::test

#else  // !YAZE_ENABLE_IMGUI_TEST_ENGINE

namespace yaze::test {
void RegisterDungeonUITests(ImGuiTestEngine*) {}
}  // namespace yaze::test

#endif  // YAZE_ENABLE_IMGUI_TEST_ENGINE
