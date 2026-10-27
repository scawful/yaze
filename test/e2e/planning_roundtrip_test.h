#ifndef YAZE_TEST_E2E_PLANNING_ROUNDTRIP_TEST_H_
#define YAZE_TEST_E2E_PLANNING_ROUNDTRIP_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// Shared Planning workflow through the real Planning UI on a disposable
// project copy named by YAZE_PLANNING_E2E_PROJECT (its plan needs an
// agent_status section and steps D4, D6 and D7). The tests write that copy's
// plan and project file. Without the variable they log a skip and pass.
//
// PlanningRoundtrip/SharedWorkflow: agent status shown read-only beside the
// notes; a note typed in the Route window saved through File > Save Project
// changes only that note; an agent update applied while the editor is open
// (YAZE_PLANNING_E2E_Z3ED runs `z3ed plan-agent-apply`, otherwise the same
// core call) survives the next save; an outside title edit refuses the save,
// keeps the draft, and Reload shows the file.
//
// PlanningRoundtrip/ReopenPersists: run in a new process after
// SharedWorkflow; the notes, title and agent status are all still shown.
void RegisterPlanningRoundtripTests(ImGuiTestEngine* engine,
                                    Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_PLANNING_ROUNDTRIP_TEST_H_
