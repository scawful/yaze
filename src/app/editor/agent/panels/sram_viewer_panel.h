#ifndef YAZE_APP_EDITOR_AGENT_PANELS_SRAM_VIEWER_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_SRAM_VIEWER_PANEL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/emu/mesen/mesen_socket_client.h"
#include "core/hack_manifest.h"

namespace yaze {
namespace project {
struct YazeProject;
}

namespace editor {

/**
 * @brief Panel for viewing Oracle of Secrets SRAM variables via Mesen2.
 *
 * Reads SRAM variable definitions from the hack manifest and displays their
 * live values from a running Mesen2 emulator. Supports auto-refresh, value
 * editing (poke), bitfield expansion for key addresses (Crystals, GameState),
 * and change highlighting.
 *
 * Variable grouping:
 *   - Story ($7EF3C5-$7EF3D7): GameState, OOSPROG, etc.
 *   - Dungeon ($7EF374, $7EF37A): Pendants, Crystals
 *   - Items ($7EF340-$7EF37F): Equipment and item counts
 */
class SramViewerPanel {
 public:
  SramViewerPanel();
  ~SramViewerPanel();

  void SetProject(project::YazeProject* project) { project_ = project; }

  void Draw();

 private:
  // Connection management (delegates to MesenClientRegistry)
  void DrawConnectionHeader();
  bool IsConnected() const;
  void Connect();
  void ConnectToPath(const std::string& socket_path);
  void Disconnect();
  void RefreshSocketList();

  // Data display
  void DrawVariableTable();
  void DrawGroupHeader(const char* label, uint32_t range_start,
                       uint32_t range_end);
  void DrawVariableRow(const core::SramVariable& var);
  void DrawCrystalBitfield(uint8_t value, uint32_t address);
  void DrawGameStateDropdown(uint8_t value, uint32_t address);

  // Data refresh
  void RefreshValues();
  void LoadVariablesFromManifest();

  // Poke (write) support
  void PokeValue(uint32_t address, uint8_t value);

  // Socket client
  std::shared_ptr<emu::mesen::MesenSocketClient> client_;

  // Project pointer for manifest access
  project::YazeProject* project_ = nullptr;

  // Cached SRAM variable definitions from manifest
  std::vector<core::SramVariable> variables_;
  bool variables_loaded_ = false;

  // Live value cache: address -> current byte value
  std::unordered_map<uint32_t, uint8_t> current_values_;

  // Previous values for change detection
  std::unordered_map<uint32_t, uint8_t> previous_values_;

  // Timestamp of last value change per address (for highlight fade)
  std::unordered_map<uint32_t, float> change_timestamps_;

  // UI state
  bool auto_refresh_ = true;
  float refresh_interval_ = 0.2f;  // 200ms
  float time_since_refresh_ = 0.0f;
  std::string connection_error_;
  std::vector<std::string> socket_paths_;
  int selected_socket_index_ = -1;
  char socket_path_buffer_[256] = {};
  std::string status_message_;
  char filter_text_[128] = {};

  // Edit state for poke dialog
  bool editing_active_ = false;
  uint32_t editing_address_ = 0;
  int editing_value_ = 0;

  // Section expansion state
  bool story_expanded_ = true;
  bool dungeon_expanded_ = true;
  bool items_expanded_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_SRAM_VIEWER_PANEL_H_
