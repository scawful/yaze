#ifndef YAZE_APP_EDITOR_AGENT_PANELS_MESEN_DEBUG_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_MESEN_DEBUG_PANEL_H_

#include <memory>
#include <string>
#include <vector>

#include "app/emu/mesen/mesen_socket_client.h"

namespace yaze {
namespace editor {

/**
 * @brief ImGui panel for Mesen2-OoS debugging integration
 *
 * Displays real-time ALTTP game state from a running Mesen2 emulator,
 * including Link position, health, active sprites, and game mode.
 */
class MesenDebugPanel {
 public:
  MesenDebugPanel();
  ~MesenDebugPanel();

  /**
   * @brief Draw the panel
   */
  void Draw();

  /**
   * @brief Set the socket client for Mesen2 communication
   */
  void SetClient(std::shared_ptr<emu::mesen::MesenSocketClient> client);

  /**
   * @brief Get connection status
   */
  bool IsConnected() const;

  /**
   * @brief Attempt to connect to Mesen2
   */
  void Connect();
  void ConnectToPath(const std::string& socket_path);

  /**
   * @brief Disconnect from Mesen2
   */
  void Disconnect();

 private:
  void DrawConnectionHeader();
  void DrawLinkState();
  void DrawSpriteList();
  void DrawGameMode();
  void DrawControlButtons();
  void DrawOverlayControls();
  void DrawStateControls();
  void RefreshSocketList();
  void RefreshState();

  std::shared_ptr<emu::mesen::MesenSocketClient> client_;

  // Cached state
  emu::mesen::GameState game_state_;
  std::vector<emu::mesen::SpriteInfo> sprites_;
  emu::mesen::MesenState emu_state_;
  emu::mesen::CpuState cpu_state_;

  // UI state
  bool auto_refresh_ = true;
  float refresh_interval_ = 0.1f;  // 100ms
  float time_since_refresh_ = 0.0f;
  bool show_all_sprites_ = false;
  bool show_cpu_state_ = false;
  std::string connection_error_;
  std::vector<std::string> socket_paths_;
  int selected_socket_index_ = -1;
  char socket_path_buffer_[256] = {};
  std::string status_message_;

  bool collision_overlay_enabled_ = false;
  int collision_map_index_ = 0;
  int save_state_slot_ = 0;

  // Section expansion state
  bool link_expanded_ = true;
  bool sprites_expanded_ = true;
  bool game_mode_expanded_ = true;
  bool cpu_expanded_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_MESEN_DEBUG_PANEL_H_
