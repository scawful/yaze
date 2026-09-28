#ifndef YAZE_TEST_E2E_SETTINGS_DRAWER_TEST_H_
#define YAZE_TEST_E2E_SETTINGS_DRAWER_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// Every Settings entry point must open the right Settings drawer with real
// settings content: File > Settings, the menu-bar drawers button, the
// Ctrl/Cmd+, chord, and the command palette entries. Runs without a ROM,
// which is the state a fresh launch is in.
void RegisterSettingsDrawerTests(ImGuiTestEngine* engine,
                                 Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_SETTINGS_DRAWER_TEST_H_
