#ifndef YAZE_TEST_E2E_EDITOR_SMOKE_TESTS_H_
#define YAZE_TEST_E2E_EDITOR_SMOKE_TESTS_H_

#include "imgui_test_engine/imgui_te_context.h"

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

void RegisterEditorSmokeTests(ImGuiTestEngine* engine, Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

void E2ETest_GraphicsEditorSmokeTest(ImGuiTestContext* ctx);
void E2ETest_SpriteEditorSmokeTest(ImGuiTestContext* ctx);
void E2ETest_MessageEditorSmokeTest(ImGuiTestContext* ctx);
void E2ETest_MusicEditorSmokeTest(ImGuiTestContext* ctx);
void E2ETest_EmulatorSaveStatesSmokeTest(ImGuiTestContext* ctx);

#endif  // YAZE_TEST_E2E_EDITOR_SMOKE_TESTS_H_
