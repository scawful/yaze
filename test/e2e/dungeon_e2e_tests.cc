/**
 * @file dungeon_e2e_tests.cc
 * @brief Implementation of unified dungeon E2E test registration
 *
 * This file provides the RegisterDungeonE2ETests() function that registers
 * all dungeon-related E2E tests with the ImGuiTestEngine in a single call.
 *
 * This consolidates test registration that was previously scattered across
 * yaze_test.cc, making it easier to:
 * - Add new dungeon tests in one place
 * - Enable/disable dungeon test categories
 * - Maintain consistent test organization
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/dungeon_e2e_tests.h"

#include "app/controller.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace yaze {
namespace test {
namespace e2e {

void RegisterDungeonE2ETests(ImGuiTestEngine* engine, Controller* controller) {
  // =========================================================================
  // Smoke Tests (dungeon_editor_smoke_test.h)
  // =========================================================================
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E", "SmokeTest");
    test->TestFunc = E2ETest_DungeonEditorV2SmokeTest;
    test->UserData = controller;
  }

  // =========================================================================
  // Visual Verification Tests (dungeon_visual_verification_test.h)
  // =========================================================================
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Visual", "BasicRoomRendering");
    test->TestFunc = yaze::test::E2ETest_VisualVerification_BasicRoomRendering;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Visual", "LayerVisibility");
    test->TestFunc = yaze::test::E2ETest_VisualVerification_LayerVisibility;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Visual", "ObjectEditor");
    test->TestFunc = yaze::test::E2ETest_VisualVerification_ObjectEditor;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Visual", "MultiRoomNavigation");
    test->TestFunc = yaze::test::E2ETest_VisualVerification_MultiRoomNavigation;
    test->UserData = controller;
  }

  // =========================================================================
  // Object Drawing Tests (dungeon_object_drawing_test.h)
  // =========================================================================
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_ObjectDrawing", "BasicPlacement");
    test->TestFunc = yaze::test::E2ETest_DungeonObjectDrawing_BasicPlacement;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_ObjectDrawing", "MultiLayerObjects");
    test->TestFunc = yaze::test::E2ETest_DungeonObjectDrawing_MultiLayerObjects;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_ObjectDrawing", "ObjectDeletion");
    test->TestFunc = yaze::test::E2ETest_DungeonObjectDrawing_ObjectDeletion;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_ObjectDrawing", "ObjectRepositioning");
    test->TestFunc = yaze::test::E2ETest_DungeonObjectDrawing_ObjectRepositioning;
    test->UserData = controller;
  }

  // =========================================================================
  // Canvas Interaction Tests (dungeon_canvas_interaction_test.h)
  // =========================================================================
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Canvas", "PanZoom");
    test->TestFunc = E2ETest_DungeonCanvas_PanZoom;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Canvas", "ObjectSelection");
    test->TestFunc = E2ETest_DungeonCanvas_ObjectSelection;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Canvas", "GridSnap");
    test->TestFunc = E2ETest_DungeonCanvas_GridSnap;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Canvas", "MultiSelect");
    test->TestFunc = E2ETest_DungeonCanvas_MultiSelect;
    test->UserData = controller;
  }

  // =========================================================================
  // Layer Rendering Tests (dungeon_layer_rendering_test.h)
  // =========================================================================
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Layer", "ToggleBG1");
    test->TestFunc = yaze::test::E2ETest_DungeonLayers_ToggleBG1;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Layer", "ToggleBG2");
    test->TestFunc = yaze::test::E2ETest_DungeonLayers_ToggleBG2;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Layer", "AllLayersOff");
    test->TestFunc = yaze::test::E2ETest_DungeonLayers_AllLayersOff;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Layer", "PerRoomSettings");
    test->TestFunc = yaze::test::E2ETest_DungeonLayers_PerRoomSettings;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "DungeonE2E_Layer", "ObjectsAboveBackground");
    test->TestFunc = yaze::test::E2ETest_DungeonLayers_ObjectsAboveBackground;
    test->UserData = controller;
  }
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
