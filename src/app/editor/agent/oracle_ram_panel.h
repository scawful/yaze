#ifndef YAZE_APP_EDITOR_AGENT_ORACLE_RAM_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_ORACLE_RAM_PANEL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/system/editor_panel.h"

namespace yaze {
namespace editor {

/**
 * @brief Panel for live monitoring of Oracle of Secrets RAM variables
 */
class OracleRamPanel : public EditorPanel {
 public:
  OracleRamPanel();
  ~OracleRamPanel() override = default;

  // EditorPanel implementation
  std::string GetId() const override { return "agent.oracle_ram"; }
  std::string GetDisplayName() const override { return "Oracle RAM"; }
  std::string GetIcon() const override; // Returns ICON_MD_MEMORY
  std::string GetEditorCategory() const override { return "Agent"; }
  PanelCategory GetPanelCategory() const override { return PanelCategory::Persistent; }
  
  void Draw(bool* p_open) override;
  void OnOpen() override;

 private:
  struct RamVariable {
    uint32_t address;
    std::string label;
    std::string description;
    uint8_t size; // 1 or 2 bytes
    uint16_t last_value = 0;
  };

  void InitializeVariables();
  void RefreshVariables();
  void DrawVariableTable();

  std::vector<RamVariable> variables_;
  double last_refresh_time_ = 0.0;
  bool auto_refresh_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_ORACLE_RAM_PANEL_H_
