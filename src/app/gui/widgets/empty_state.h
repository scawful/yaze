#ifndef YAZE_APP_GUI_WIDGETS_EMPTY_STATE_H_
#define YAZE_APP_GUI_WIDGETS_EMPTY_STATE_H_

#include <functional>

namespace yaze {
namespace gui {

/**
 * @brief Shared empty / disabled panel presentation.
 *
 * Widgets stay editor-agnostic: pass copy + optional action; the caller owns
 * what the action does (open ROM, switch editor, etc.).
 */
struct EmptyStateOptions {
  const char* icon = nullptr;
  const char* title = nullptr;
  const char* detail = nullptr;
  const char* action_label = nullptr;
  std::function<void()> on_action;
  /// Tighter vertical rhythm for drawers / inspectors.
  bool compact = false;
};

/**
 * @brief Draw a centered empty-state block.
 * @return true if the optional action button was clicked (also invokes
 *         @ref EmptyStateOptions::on_action when set).
 */
bool DrawEmptyState(const EmptyStateOptions& options);

// ---------------------------------------------------------------------------
// Shared copy presets — prefer these over ad-hoc "No ROM" strings.
// ---------------------------------------------------------------------------

EmptyStateOptions EmptyNoRom(bool compact = false);
EmptyStateOptions EmptyNoSelection(bool compact = false);
EmptyStateOptions EmptySelectInCanvas(bool compact = true);
EmptyStateOptions EmptyNoProject(bool compact = false);
EmptyStateOptions EmptyLoading(const char* what = "content",
                               bool compact = false);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_EMPTY_STATE_H_
