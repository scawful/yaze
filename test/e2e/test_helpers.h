#ifndef YAZE_TEST_E2E_TEST_HELPERS_H
#define YAZE_TEST_E2E_TEST_HELPERS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"

namespace yaze {
namespace test {
namespace e2e {

/**
 * @brief Canvas interaction helpers for e2e tests
 */

// Click on canvas at world position (converts to screen coordinates)
void ClickCanvasAt(ImGuiTestContext* ctx, const std::string& canvas_id,
                   ImVec2 world_pos);

// Drag selection from start to end position on canvas
void DragCanvasSelection(ImGuiTestContext* ctx, const std::string& canvas_id,
                         ImVec2 start, ImVec2 end);

// Verify tile value at specific canvas coordinates
void VerifyCanvasTile(ImGuiTestContext* ctx, int x, int y,
                      uint16_t expected_tile);

/**
 * @brief Entity manipulation helpers
 */

// Create entity on canvas via context menu
void CreateEntityOnCanvas(ImGuiTestContext* ctx,
                          const std::string& entity_type, ImVec2 position);

// Select entity by ID in entity list
void SelectEntity(ImGuiTestContext* ctx, int entity_id);

// Verify entity properties match expected values
void VerifyEntityProperties(ImGuiTestContext* ctx, int entity_id,
                            const std::map<std::string, std::string>& props);

/**
 * @brief Editor state helpers
 */

// Open specific editor type via menu
void OpenEditor(ImGuiTestContext* ctx, editor::EditorType editor_type);

// Close specific editor
void CloseEditor(ImGuiTestContext* ctx, editor::EditorType editor_type);

// Verify editor window is active and visible
void VerifyEditorActive(ImGuiTestContext* ctx, editor::EditorType editor_type);

/**
 * @brief Keyboard shortcut helpers
 */

// Simulate keyboard shortcut (modifier + key)
void SimulateShortcut(ImGuiTestContext* ctx, ImGuiKey modifier, ImGuiKey key);

// Simulate undo (Ctrl+Z) or redo (Ctrl+Y)
void SimulateUndoRedo(ImGuiTestContext* ctx, bool is_redo = false);

/**
 * @brief ROM state validation helpers
 */

// Verify single byte in ROM matches expected value
void VerifyRomByteEquals(ImGuiTestContext* ctx, Rom* rom, uint32_t address,
                         uint8_t expected);

// Verify multiple bytes in ROM match expected values
void VerifyRomBytesEqual(ImGuiTestContext* ctx, Rom* rom, uint32_t address,
                         const std::vector<uint8_t>& expected);

// Validate overall ROM integrity (checksum, structure, etc.)
void VerifyRomIntegrity(ImGuiTestContext* ctx, Rom* rom);

/**
 * @brief Mock ROM generation
 */

// Create mock ROM for testing (deterministic data)
// Variants: "default", "zscustom_v3", "minimal", "corrupted", "large"
std::unique_ptr<Rom> CreateMockRomForTesting(
    const std::string& variant = "default");

/**
 * @brief Coordinate conversion helpers
 */

// Convert room tile coordinates to canvas pixel position
ImVec2 RoomCoordToCanvasPos(int x, int y);

// Convert overworld tile coordinates to canvas pixel position
ImVec2 OverworldCoordToCanvasPos(int x, int y);

/**
 * @brief Screenshot and visual validation
 */

// Capture screenshot of specific canvas
std::vector<uint8_t> CaptureCanvasScreenshot(ImGuiTestContext* ctx,
                                             const std::string& canvas_id);

// Load golden image from test data directory
std::vector<uint8_t> LoadGoldenImage(const std::string& image_name);

// Compare two images with similarity threshold (0.0-1.0)
bool CompareImages(const std::vector<uint8_t>& img1,
                   const std::vector<uint8_t>& img2, float threshold = 0.99f);

/**
 * @brief Test timing and synchronization
 */

// Wait for specific condition to be true (with timeout)
bool WaitForCondition(ImGuiTestContext* ctx,
                      std::function<bool()> condition, int max_frames = 60);

// Wait for window to appear
bool WaitForWindow(ImGuiTestContext* ctx, const std::string& window_name,
                   int max_frames = 30);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_TEST_HELPERS_H
