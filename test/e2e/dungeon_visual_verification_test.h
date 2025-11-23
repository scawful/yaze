#ifndef YAZE_TEST_E2E_DUNGEON_VISUAL_VERIFICATION_TEST_H_
#define YAZE_TEST_E2E_DUNGEON_VISUAL_VERIFICATION_TEST_H_

struct ImGuiTestContext;

namespace yaze {
namespace test {

// E2E visual verification tests for dungeon rendering
void E2ETest_VisualVerification_BasicRoomRendering(ImGuiTestContext* ctx);
void E2ETest_VisualVerification_LayerVisibility(ImGuiTestContext* ctx);
void E2ETest_VisualVerification_ObjectEditor(ImGuiTestContext* ctx);
void E2ETest_VisualVerification_MultiRoomNavigation(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_DUNGEON_VISUAL_VERIFICATION_TEST_H_
