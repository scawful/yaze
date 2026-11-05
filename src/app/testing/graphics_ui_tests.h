#ifndef YAZE_APP_TESTING_GRAPHICS_UI_TESTS_H_
#define YAZE_APP_TESTING_GRAPHICS_UI_TESTS_H_

struct ImGuiTestEngine;

namespace yaze::test {

// Disabled unless exactly one explicit disposable-project gate is selected.
// The native owner supplies actual fixture hashes and owns process/window
// admission, normal Quit and separate-process reopen; this adds no transport.
void RegisterGraphicsSourceUITests(ImGuiTestEngine* engine);

}  // namespace yaze::test

#endif  // YAZE_APP_TESTING_GRAPHICS_UI_TESTS_H_
