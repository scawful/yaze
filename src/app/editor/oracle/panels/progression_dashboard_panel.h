#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_PROGRESSION_DASHBOARD_PANEL_H
#define YAZE_APP_EDITOR_ORACLE_PANELS_PROGRESSION_DASHBOARD_PANEL_H

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "app/editor/core/content_registry.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "core/hack_manifest.h"
#include "core/oracle_progression.h"
#include "core/oracle_progression_loader.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze::editor {

/**
 * @class ProgressionDashboardPanel
 * @brief Visual dashboard of Oracle game state from SRAM data.
 *
 * Shows crystal tracker, game state phase, story flags, and dungeon
 * completion grid. Supports .srm file import and manual bit toggles
 * for testing.
 */
class ProgressionDashboardPanel : public EditorPanel {
 public:
  std::string GetId() const override { return "oracle.progression_dashboard"; }
  std::string GetDisplayName() const override {
    return "Progression Dashboard";
  }
  std::string GetIcon() const override { return ICON_MD_DASHBOARD; }
  std::string GetEditorCategory() const override { return "Oracle"; }
  PanelCategory GetPanelCategory() const override {
    return PanelCategory::CrossEditor;
  }
  float GetPreferredWidth() const override { return 400.0f; }

  void Draw(bool* p_open) override {
    (void)p_open;

    // Lazily resolve the manifest from the project context.
    if (!manifest_) {
      auto* project = ContentRegistry::Context::current_project();
      if (project && project->hack_manifest.loaded()) {
        manifest_ = &project->hack_manifest;
      }
    }

    DrawSrmImportControls();
    ImGui::Separator();

    DrawCrystalTracker();
    ImGui::Separator();
    DrawGameState();
    ImGui::Separator();
    DrawDungeonGrid();
    ImGui::Separator();
    DrawStoryFlags();
    ImGui::Separator();
    DrawManualControls();

    PushStateToManifestIfNeeded();
  }

 private:
  static bool StatesEqual(const core::OracleProgressionState& a,
                          const core::OracleProgressionState& b) {
    return a.crystal_bitfield == b.crystal_bitfield &&
           a.game_state == b.game_state && a.oosprog == b.oosprog &&
           a.oosprog2 == b.oosprog2 && a.side_quest == b.side_quest &&
           a.pendants == b.pendants;
  }

  void DrawSrmImportControls() {
    ImGui::Text("SRAM (.srm)");

    util::FileDialogOptions options;
    options.filters = {
        {"SRAM (.srm)", "srm"},
        {"All Files", "*"},
    };

    if (ImGui::Button("Import...")) {
      std::string file_path =
          util::FileDialogWrapper::ShowOpenFileDialog(options);
      if (!file_path.empty()) {
        auto state_or = core::LoadOracleProgressionFromSrmFile(file_path);
        if (state_or.ok()) {
          state_ = *state_or;
          game_state_slider_ = static_cast<int>(state_.game_state);
          loaded_srm_path_ = file_path;
          last_srm_error_.clear();
        } else {
          last_srm_error_ = std::string(state_or.status().message());
        }
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      state_ = core::OracleProgressionState();
      game_state_slider_ = 0;
      loaded_srm_path_.clear();
      last_srm_error_.clear();
      if (manifest_) {
        manifest_->ClearOracleProgressionState();
      }
    }

    if (!loaded_srm_path_.empty()) {
      const std::filesystem::path p(loaded_srm_path_);
      ImGui::TextDisabled("Loaded: %s", p.filename().string().c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", loaded_srm_path_.c_str());
      }
    } else {
      ImGui::TextDisabled("Loaded: (none)");
    }

    if (!last_srm_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Error: %s",
                         last_srm_error_.c_str());
    }
  }

  void PushStateToManifestIfNeeded() {
    if (!manifest_ || !manifest_->loaded()) {
      return;
    }

    const auto existing = manifest_->oracle_progression_state();
    if (existing.has_value() && StatesEqual(*existing, state_)) {
      return;
    }
    manifest_->SetOracleProgressionState(state_);
  }

  void DrawCrystalTracker() {
    ImGui::Text("Crystal Tracker");
    ImGui::Spacing();

    float item_width = 44.0f;

    for (int d = 1; d <= 7; ++d) {
      uint8_t mask = core::OracleProgressionState::GetCrystalMask(d);
      bool complete = (state_.crystal_bitfield & mask) != 0;

      ImVec4 color =
          complete ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)   // green
                   : ImVec4(0.3f, 0.3f, 0.3f, 0.6f);  // gray

      {
        gui::StyleColorGuard crystal_guard(
            {{ImGuiCol_Button, color},
             {ImGuiCol_ButtonHovered,
              ImVec4(color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f)}});

        char label[8];
        snprintf(label, sizeof(label), "D%d", d);
        if (ImGui::Button(label, ImVec2(item_width, 36.0f))) {
          // Toggle crystal bit for testing
          state_.crystal_bitfield ^= mask;
        }
      }

      if (d < 7) ImGui::SameLine();
    }

    ImGui::Text("Crystals: %d / 7", state_.GetCrystalCount());
  }

  void DrawGameState() {
    ImGui::Text("Game State");
    ImGui::Spacing();

    // Phase labels
    const char* phases[] = {"Start", "Loom Beach", "Kydrog Complete",
                            "Farore Rescued"};
    int phase_count = 4;

    float bar_width = ImGui::GetContentRegionAvail().x;
    float segment = bar_width / static_cast<float>(phase_count);

    ImVec2 bar_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (int i = 0; i < phase_count; ++i) {
      ImVec2 seg_min(bar_pos.x + segment * i, bar_pos.y);
      ImVec2 seg_max(bar_pos.x + segment * (i + 1), bar_pos.y + 24.0f);

      ImU32 fill = (i <= state_.game_state)
                       ? IM_COL32(60, 140, 200, 220)
                       : IM_COL32(50, 50, 50, 180);

      draw_list->AddRectFilled(seg_min, seg_max, fill, 3.0f);
      draw_list->AddRect(seg_min, seg_max, IM_COL32(80, 80, 80, 255), 3.0f);

      // Label
      ImVec2 text_pos(seg_min.x + 4, seg_min.y + 4);
      draw_list->AddText(text_pos, IM_COL32(220, 220, 220, 255), phases[i]);
    }

    ImGui::Dummy(ImVec2(0, 30));
    ImGui::Text("Phase: %s", state_.GetGameStateName().c_str());
  }

  void DrawDungeonGrid() {
    ImGui::Text("Dungeon Completion");
    ImGui::Spacing();

    struct DungeonInfo {
      const char* label;
      int number;  // 0 means special (FOS/SOP/SOW)
    };

    DungeonInfo dungeons[] = {
        {"D1 Mushroom Grotto", 1},  {"D2 Tail Palace", 2},
        {"D3 Kalyxo Castle", 3},    {"D4 Zora Temple", 4},
        {"D5 Glacia Estate", 5},    {"D6 Goron Mines", 6},
        {"D7 Dragon Ship", 7},      {"FOS Fortress", 0},
        {"SOP Shrine of Power", 0}, {"SOW Shrine of Wisdom", 0},
    };

    ImGui::Columns(2, "dungeon_grid", false);
    for (const auto& dungeon : dungeons) {
      bool complete = false;
      if (dungeon.number >= 1 && dungeon.number <= 7) {
        complete = state_.IsDungeonComplete(dungeon.number);
      }

      ImVec4 color =
          complete ? ImVec4(0.1f, 0.6f, 0.2f, 1.0f)   // green
                   : ImVec4(0.25f, 0.25f, 0.25f, 1.0f);  // dark

      {
        gui::StyleColorGuard header_guard(ImGuiCol_Header, color);
        ImGui::Selectable(dungeon.label, complete,
                          ImGuiSelectableFlags_Disabled);
      }
      ImGui::NextColumn();
    }
    ImGui::Columns(1);
  }

  void DrawStoryFlags() {
    if (!ImGui::TreeNode("Story Flags")) return;

    // OOSPROG bits
    ImGui::Text("OOSPROG ($7EF3D6): 0x%02X", state_.oosprog);
    DrawBitGrid("oosprog", state_.oosprog, oosprog_labels_);

    ImGui::Spacing();

    // OOSPROG2 bits
    ImGui::Text("OOSPROG2 ($7EF3C6): 0x%02X", state_.oosprog2);
    DrawBitGrid("oosprog2", state_.oosprog2, oosprog2_labels_);

    ImGui::Spacing();

    // Side quest
    ImGui::Text("SideQuest ($7EF3D7): 0x%02X", state_.side_quest);

    ImGui::TreePop();
  }

  static void DrawBitGrid(const char* id_prefix, uint8_t value,
                          const char* const* labels) {
    for (int bit = 0; bit < 8; ++bit) {
      bool set = (value & (1 << bit)) != 0;
      ImVec4 color = set ? ImVec4(0.3f, 0.7f, 0.3f, 1.0f)
                         : ImVec4(0.2f, 0.2f, 0.2f, 0.6f);

      char buf[64];
      snprintf(buf, sizeof(buf), "%s##%s_%d", labels[bit], id_prefix, bit);

      {
        gui::StyleColorGuard bit_guard(ImGuiCol_Button, color);
        ImGui::SmallButton(buf);
      }
      if (bit < 7) ImGui::SameLine();
    }
  }

  void DrawManualControls() {
    if (!ImGui::TreeNode("Manual Controls")) return;

    ImGui::SliderInt("Game State", &game_state_slider_, 0, 3);
    if (game_state_slider_ != state_.game_state) {
      state_.game_state = static_cast<uint8_t>(game_state_slider_);
    }

    int crystal_int = state_.crystal_bitfield;
    if (ImGui::SliderInt("Crystal Bits", &crystal_int, 0, 127)) {
      state_.crystal_bitfield = static_cast<uint8_t>(crystal_int);
    }

    if (ImGui::Button("Clear All")) {
      state_ = core::OracleProgressionState();
      game_state_slider_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Complete All")) {
      state_.crystal_bitfield = 0x7F;
      state_.game_state = 3;
      game_state_slider_ = 3;
    }

    ImGui::TreePop();
  }

  core::OracleProgressionState state_;
  int game_state_slider_ = 0;

  core::HackManifest* manifest_ = nullptr;
  std::string loaded_srm_path_;
  std::string last_srm_error_;

  // Bit labels for flag grids
  static constexpr const char* oosprog_labels_[8] = {
      "Bit0", "HallOfSecrets", "PendantQuest", "Bit3",
      "ElderMet", "Bit5",       "Bit6",         "FortressComplete",
  };

  static constexpr const char* oosprog2_labels_[8] = {
      "Bit0",           "Bit1",          "KydrogEncounter", "Bit3",
      "DekuSoulFreed",  "BookOfSecrets", "Bit6",            "Bit7",
  };
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_PROGRESSION_DASHBOARD_PANEL_H
