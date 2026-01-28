#ifndef YAZE_APP_EDITOR_AGENT_PANELS_ORACLE_STATE_LIBRARY_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_ORACLE_STATE_LIBRARY_PANEL_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/emu/mesen/mesen_socket_client.h"

namespace yaze {
namespace editor {

/**
 * @brief State entry from the Oracle save state library
 */
struct StateEntry {
  std::string id;
  std::string label;
  std::string path;
  std::string status;  // "draft", "canon", "deprecated"
  std::string md5;
  std::string captured_by;
  std::string verified_by;
  std::string verified_at;
  std::string deprecated_reason;
  std::vector<std::string> tags;

  // Metadata
  int module = 0;
  int room = 0;
  int area = 0;
  bool indoors = false;
  int link_x = 0;
  int link_y = 0;
  int health = 0;
  int max_health = 0;
  int rupees = 0;
  std::string location;
  std::string summary;
};

/**
 * @brief ImGui panel for Oracle of Secrets save state library management
 *
 * Provides UI for:
 * - Viewing all states in the library with status badges
 * - Loading states into Mesen2 emulator
 * - Verifying and promoting draft states to canon
 * - Deprecating bad states
 * - Filtering by status and tags
 */
class OracleStateLibraryPanel {
 public:
  OracleStateLibraryPanel();
  ~OracleStateLibraryPanel();

  /**
   * @brief Draw the panel
   */
  void Draw();

  /**
   * @brief Set the Mesen socket client for emulator communication
   */
  void SetClient(std::shared_ptr<emu::mesen::MesenSocketClient> client);

  /**
   * @brief Refresh the state library from disk
   */
  void RefreshLibrary();

  /**
   * @brief Load a state into the emulator
   */
  absl::Status LoadState(const std::string& state_id);

  /**
   * @brief Verify and promote a state to canon
   */
  absl::Status VerifyState(const std::string& state_id);

  /**
   * @brief Deprecate a state
   */
  absl::Status DeprecateState(const std::string& state_id,
                               const std::string& reason);

 private:
  void DrawToolbar();
  void DrawStateList();
  void DrawStateDetails();
  void DrawVerificationDialog();
  void LoadManifest();
  void SaveManifest();

  std::shared_ptr<emu::mesen::MesenSocketClient> client_;

  // Library data
  std::vector<StateEntry> entries_;
  std::string library_root_;
  std::string manifest_path_;

  // UI state
  int selected_index_ = -1;
  bool show_deprecated_ = false;
  bool show_draft_ = true;
  bool show_canon_ = true;
  char filter_text_[128] = {};
  char tag_filter_[64] = {};

  // Verification dialog state
  bool show_verify_dialog_ = false;
  std::string verify_target_id_;
  char verify_notes_[256] = {};

  // Deprecation dialog state
  bool show_deprecate_dialog_ = false;
  std::string deprecate_target_id_;
  char deprecate_reason_[256] = {};

  // Status
  std::string status_message_;
  bool status_is_error_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_ORACLE_STATE_LIBRARY_PANEL_H_
