#ifndef YAZE_TEST_E2E_DEAD_CLICK_REGRESSION_TEST_H_
#define YAZE_TEST_E2E_DEAD_CLICK_REGRESSION_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// Mouse-driven checks for the three "dead click" symptoms: a main-menu
// MenuItem click changes state, a Select-mode right-click on the overworld
// canvas opens its context popup, and a panel close X closes the panel.
// All three act on mouse release, so they are driven with real injected
// mouse events, not by calling the callbacks directly.
void RegisterDeadClickRegressionTests(ImGuiTestEngine* engine,
                                      Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_DEAD_CLICK_REGRESSION_TEST_H_
