#ifndef YAZE_TEST_E2E_ASSEMBLY_LAYOUT_TEST_H_
#define YAZE_TEST_E2E_ASSEMBLY_LAYOUT_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// The Disassembly panel docked by the Assembly default layout must keep its
// bank preview actions inside the panel at a laptop-sized window. Needs a
// test ROM (YAZE_TEST_ROM_VANILLA); skipped without one.
void RegisterAssemblyLayoutTests(ImGuiTestEngine* engine,
                                 Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_ASSEMBLY_LAYOUT_TEST_H_
