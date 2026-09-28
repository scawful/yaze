#ifndef YAZE_TEST_E2E_MENU_TILEMAP_EDITOR_TEST_H_
#define YAZE_TEST_E2E_MENU_TILEMAP_EDITOR_TEST_H_

struct ImGuiTestEngine;

namespace yaze {
class Controller;

namespace test {
namespace e2e {

// Opens the Screen editor's "Menu Tilemap (2bpp)" panel, loads a synthetic
// tilemap file from a temp dir (bypassing the native Open dialog via a
// testing-only hook), paints one cell with a real mouse click on the
// canvas, undoes it, saves with the real Ctrl/Cmd+S keyboard path, and
// checks the saved bytes match the pristine original -- i.e. the click
// actually reached MenuTilemapDocument::SetCell() and Undo actually
// reverted it, not just that the underlying model works in isolation
// (that's what the unit tests already cover).
void RegisterMenuTilemapEditorTests(ImGuiTestEngine* engine,
                                    Controller* controller);

}  // namespace e2e
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_MENU_TILEMAP_EDITOR_TEST_H_
