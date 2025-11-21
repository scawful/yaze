#ifndef YAZE_APP_GUI_CANVAS_CANVAS_POPUP_H
#define YAZE_APP_GUI_CANVAS_CANVAS_POPUP_H

#include <functional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief State for a single persistent popup
 *
 * POD struct representing the state of a popup that persists across frames.
 * Popups remain open until explicitly closed or the user dismisses them.
 */
struct PopupState {
  // Unique popup identifier (used with ImGui::OpenPopup/BeginPopup)
  std::string popup_id;

  // Whether the popup is currently open
  bool is_open = false;

  // Callback that renders the popup content
  // Should call ImGui::BeginPopup(popup_id) / ImGui::EndPopup()
  std::function<void()> render_callback;

  // Whether the popup should persist across frames
  bool persist = true;

  // Default constructor
  PopupState() = default;

  // Constructor with id and callback
  PopupState(const std::string& id, std::function<void()> callback)
      : popup_id(id), is_open(false), render_callback(std::move(callback)) {}
};

/**
 * @brief Registry for managing persistent popups
 *
 * Maintains a collection of popups and their lifecycle. Handles opening,
 * closing, and rendering popups across frames.
 *
 * This class is designed to be embedded in Canvas or used standalone for
 * testing and custom UI components.
 */
class PopupRegistry {
 public:
  PopupRegistry() = default;

  /**
   * @brief Open a persistent popup
   *
   * If the popup already exists, updates its callback and reopens it.
   * If the popup is new, adds it to the registry and opens it.
   *
   * @param popup_id Unique identifier for the popup
   * @param render_callback Function that renders the popup content
   */
  void Open(const std::string& popup_id, std::function<void()> render_callback);

  /**
   * @brief Close a persistent popup
   *
   * Marks the popup as closed. It will be removed from the registry on the
   * next render pass.
   *
   * @param popup_id Identifier of the popup to close
   */
  void Close(const std::string& popup_id);

  /**
   * @brief Check if a popup is currently open
   *
   * @param popup_id Identifier of the popup to check
   * @return true if popup is open, false otherwise
   */
  bool IsOpen(const std::string& popup_id) const;

  /**
   * @brief Render all active popups
   *
   * Iterates through all open popups and calls their render callbacks.
   * Automatically removes popups that have been closed by the user.
   *
   * This should be called once per frame, typically at the end of the
   * frame after all other rendering is complete.
   */
  void RenderAll();

  /**
   * @brief Get the number of active popups
   *
   * @return Number of open popups in the registry
   */
  size_t GetActiveCount() const;

  /**
   * @brief Clear all popups from the registry
   *
   * Closes all popups and removes them from the registry.
   * Useful for cleanup or resetting state.
   */
  void Clear();

  /**
   * @brief Get direct access to the popup list (for migration/debugging)
   *
   * @return Reference to the internal popup vector
   */
  std::vector<PopupState>& GetPopups() { return popups_; }
  const std::vector<PopupState>& GetPopups() const { return popups_; }

 private:
  // Internal storage for popup states
  std::vector<PopupState> popups_;

  // Helper to find a popup by ID
  std::vector<PopupState>::iterator FindPopup(const std::string& popup_id);
  std::vector<PopupState>::const_iterator FindPopup(
      const std::string& popup_id) const;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_POPUP_H
