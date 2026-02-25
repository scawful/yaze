#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_PANEL_H_
#define YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_PANEL_H_

#include <future>
#include <optional>
#include <string>

#include "app/editor/oracle/panels/oracle_validation_view_model.h"
#include "app/editor/system/editor_panel.h"

namespace yaze {
class Rom;
}

namespace yaze::editor {

class OracleValidationPanel : public EditorPanel {
 public:
  ~OracleValidationPanel() override;

  std::string GetId() const override;
  std::string GetDisplayName() const override;
  std::string GetIcon() const override;
  std::string GetEditorCategory() const override;
  PanelCategory GetPanelCategory() const override;
  float GetPreferredWidth() const override;

  void Draw(bool* p_open) override;

 private:
  static std::string DefaultRomPath();
  void PollPendingResult();
  Rom* GetRom() const;
  void LaunchRun(oracle_validation::RunMode mode);

  void DrawOptions();
  void DrawActionButtons();
  void DrawResults();
  void DrawSmokeCards(const oracle_validation::SmokeResult& smoke);
  void DrawPreflightCards(const oracle_validation::PreflightResult& preflight);
  void DrawRawOutput(const oracle_validation::OracleRunResult& result);

  std::string rom_path_ = DefaultRomPath();
  int min_d6_track_rooms_ = 4;
  bool write_report_ = false;
  std::string report_path_;
  std::string required_collision_rooms_ = "0x25,0x27";

  bool running_ = false;
  std::future<oracle_validation::OracleRunResult> pending_;
  std::optional<oracle_validation::OracleRunResult> last_result_;
  std::string status_message_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_PANEL_H_
