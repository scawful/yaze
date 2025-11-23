#ifndef YAZE_TEST_E2E_DUNGEON_E2E_TESTS_H_
#define YAZE_TEST_E2E_DUNGEON_E2E_TESTS_H_

/**
 * @file dungeon_e2e_tests.h
 * @brief Unified header for all dungeon E2E tests
 *
 * This header provides a single include point for all dungeon-related E2E tests.
 * It also provides a registration function that can be called to register all
 * dungeon tests with the ImGuiTestEngine in a single call.
 *
 * Test Categories:
 * - Smoke Tests: Basic functionality validation (dungeon_editor_smoke_test.h)
 * - Visual Verification: AI-powered rendering verification
 *                        (dungeon_visual_verification_test.h)
 * - Object Drawing: Object placement and manipulation
 *                   (dungeon_object_drawing_test.h)
 * - Canvas Interaction: Mouse/keyboard interaction on canvas
 *                       (dungeon_canvas_interaction_test.h)
 * - Layer Rendering: Layer visibility and rendering order
 *                    (dungeon_layer_rendering_test.h)
 *
 * Usage:
 *   #include "e2e/dungeon_e2e_tests.h"
 *
 *   // In test setup (replaces individual test registrations):
 *   yaze::test::e2e::RegisterDungeonE2ETests(engine, &controller);
 */

#include "imgui_test_engine/imgui_te_context.h"

// Include all dungeon E2E test headers
#include "e2e/dungeon_canvas_interaction_test.h"
#include "e2e/dungeon_editor_smoke_test.h"
#include "e2e/dungeon_layer_rendering_test.h"
#include "e2e/dungeon_object_drawing_test.h"
#include "e2e/dungeon_visual_verification_test.h"

// Forward declarations
struct ImGuiTestEngine;

namespace yaze {

// Forward declarations
class Controller;

namespace test {
namespace e2e {

/**
 * @brief Register all dungeon E2E tests with the test engine
 *
 * This function registers all dungeon-related E2E tests including:
 * - DungeonEditorV2 smoke tests (1 test)
 * - Visual verification tests (4 tests)
 * - Object drawing tests (4 tests)
 * - Canvas interaction tests (4 tests)
 * - Layer rendering tests (5 tests)
 *
 * Total: 18 dungeon E2E tests
 *
 * @param engine The ImGuiTestEngine instance to register tests with
 * @param controller Pointer to the application controller (used as UserData)
 */
void RegisterDungeonE2ETests(ImGuiTestEngine* engine, Controller* controller);

// =============================================================================
// Test Index (by category and source file)
// =============================================================================

// --- Smoke Tests (dungeon_editor_smoke_test.h) ---
// E2ETest_DungeonEditorV2SmokeTest - Basic card-based UI validation

// --- Visual Verification (dungeon_visual_verification_test.h) ---
// yaze::test::E2ETest_VisualVerification_BasicRoomRendering
// yaze::test::E2ETest_VisualVerification_LayerVisibility
// yaze::test::E2ETest_VisualVerification_ObjectEditor
// yaze::test::E2ETest_VisualVerification_MultiRoomNavigation

// --- Object Drawing (dungeon_object_drawing_test.h) ---
// yaze::test::E2ETest_DungeonObjectDrawing_BasicPlacement
// yaze::test::E2ETest_DungeonObjectDrawing_MultiLayerObjects
// yaze::test::E2ETest_DungeonObjectDrawing_ObjectDeletion
// yaze::test::E2ETest_DungeonObjectDrawing_ObjectRepositioning

// --- Canvas Interaction (dungeon_canvas_interaction_test.h) ---
// E2ETest_DungeonCanvas_PanZoom
// E2ETest_DungeonCanvas_ObjectSelection
// E2ETest_DungeonCanvas_GridSnap
// E2ETest_DungeonCanvas_MultiSelect

// --- Layer Rendering (dungeon_layer_rendering_test.h) ---
// yaze::test::E2ETest_DungeonLayers_ToggleBG1
// yaze::test::E2ETest_DungeonLayers_ToggleBG2
// yaze::test::E2ETest_DungeonLayers_AllLayersOff
// yaze::test::E2ETest_DungeonLayers_PerRoomSettings
// yaze::test::E2ETest_DungeonLayers_ObjectsAboveBackground

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_DUNGEON_E2E_TESTS_H_
