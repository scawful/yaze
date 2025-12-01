#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class MusicHelpPanel
 * @brief EditorPanel providing help documentation for the Music Editor
 */
class MusicHelpPanel : public EditorPanel {
 public:
  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.help"; }
  std::string GetDisplayName() const override { return "Help"; }
  std::string GetIcon() const override { return ICON_MD_HELP; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 99; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    ImGui::Text("Yaze Music Editor Guide");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::TextWrapped(
          "The Music Editor allows you to create and modify SNES music for "
          "Zelda 3.");
      ImGui::BulletText("Song Browser: Select and manage songs.");
      ImGui::BulletText("Tracker/Piano Roll: Edit note data.");
      ImGui::BulletText("Instrument Editor: Configure ADSR envelopes.");
      ImGui::BulletText("Sample Editor: Import and preview BRR samples.");
    }

    if (ImGui::CollapsingHeader("Tracker / Piano Roll")) {
      ImGui::Text("Controls:");
      ImGui::BulletText("Space: Play/Pause");
      ImGui::BulletText("Z, S, X...: Keyboard piano (C, C#, D...)");
      ImGui::BulletText("Shift+Arrows: Range selection");
      ImGui::BulletText("Ctrl+C/V: Copy/Paste (WIP)");
      ImGui::BulletText("Ctrl+Wheel: Zoom (Piano Roll)");
    }

    if (ImGui::CollapsingHeader("Instruments & Samples")) {
      ImGui::TextWrapped(
          "Instruments use BRR samples with an ADSR volume envelope.");
      ImGui::BulletText("ADSR: Attack, Decay, Sustain, Release.");
      ImGui::BulletText(
          "Loop Points: Define where the sample loops (in blocks of 16 "
          "samples).");
      ImGui::BulletText("Tuning: Adjust pitch multiplier ($1000 = 1.0x).");
    }

    if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
      ImGui::BulletText("Space: Play/Pause toggle");
      ImGui::BulletText("Escape: Stop playback");
      ImGui::BulletText("+/-: Increase/decrease speed");
      ImGui::BulletText("Arrow keys: Navigate in tracker/piano roll");
      ImGui::BulletText("Z,S,X,D,C,V,G,B,H,N,J,M: Piano keyboard (C to B)");
      ImGui::BulletText("Ctrl+Wheel: Zoom (Piano Roll)");
    }

    if (ImGui::CollapsingHeader("ASM Import/Export")) {
      ImGui::TextWrapped(
          "Songs can be exported to and imported from Oracle of "
          "Secrets-compatible ASM format.");
      ImGui::BulletText("Right-click a song in the browser to export/import.");
      ImGui::BulletText("Exported ASM can be assembled with Asar.");
      ImGui::BulletText("Import parses ASM labels and data directives.");
    }
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_
