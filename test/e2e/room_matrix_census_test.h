#ifndef YAZE_TEST_E2E_ROOM_MATRIX_CENSUS_TEST_H_
#define YAZE_TEST_E2E_ROOM_MATRIX_CENSUS_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// Room Matrix "Census" overlay: a real click on the toggle computes the census,
// and hovering a free room shows its census tooltip. Driven with injected
// mouse events like the dead-click regressions.
void RegisterRoomMatrixCensusTests(ImGuiTestEngine* engine,
                                   Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_ROOM_MATRIX_CENSUS_TEST_H_
