#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_

#include <string>
#include "util/i18n/tr.h"

#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class MusicHelpPanel
 * @brief WindowContent providing help documentation for the Music Editor
 */
class MusicHelpPanel : public WindowContent {
 public:
  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "music.help"; }
  std::string GetDisplayName() const override { return "Help"; }
  std::string GetIcon() const override { return ICON_MD_HELP; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 99; }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    ImGui::Text(tr("Yaze Music Editor Guide"));
    ImGui::Separator();

    if (ImGui::CollapsingHeader(tr("Overview"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::TextWrapped(
          tr("The Music Editor allows you to create and modify SNES music for "
             "Zelda 3."));
      ImGui::BulletText(tr("Song Browser: Select and manage songs."));
      ImGui::BulletText(tr("Tracker/Piano Roll: Edit note data."));
      ImGui::BulletText(tr("Instrument Editor: Configure ADSR envelopes."));
      ImGui::BulletText(tr("Sample Editor: Import and preview BRR samples."));
    }

    if (ImGui::CollapsingHeader(tr("Tracker / Piano Roll"))) {
      ImGui::Text(tr("Controls:"));
      ImGui::BulletText(tr("Space: Play/Pause"));
      ImGui::BulletText(tr("Z, S, X...: Keyboard piano (C, C#, D...)"));
      ImGui::BulletText(tr("Shift+Arrows: Range selection"));
      ImGui::BulletText(tr("Ctrl+C/V: Copy/Paste (WIP)"));
      ImGui::BulletText(tr("Ctrl+Wheel: Zoom (Piano Roll)"));
    }

    if (ImGui::CollapsingHeader(tr("Instruments & Samples"))) {
      ImGui::TextWrapped(
          tr("Instruments use BRR samples with an ADSR volume envelope."));
      ImGui::BulletText(tr("ADSR: Attack, Decay, Sustain, Release."));
      ImGui::BulletText(
          tr("Loop Points: Define where the sample loops (in blocks of 16 "
             "samples)."));
      ImGui::BulletText(tr("Tuning: Adjust pitch multiplier ($1000 = 1.0x)."));
    }

    if (ImGui::CollapsingHeader(tr("Keyboard Shortcuts"))) {
      ImGui::BulletText(tr("Space: Play/Pause toggle"));
      ImGui::BulletText(tr("Escape: Stop playback"));
      ImGui::BulletText(tr("+/-: Increase/decrease speed"));
      ImGui::BulletText(tr("Arrow keys: Navigate in tracker/piano roll"));
      ImGui::BulletText(tr("Z,S,X,D,C,V,G,B,H,N,J,M: Piano keyboard (C to B)"));
      ImGui::BulletText(tr("Ctrl+Wheel: Zoom (Piano Roll)"));
    }

    if (ImGui::CollapsingHeader(tr("ASM Import/Export"))) {
      ImGui::TextWrapped(
          tr("Songs can be exported to and imported from Oracle of "
             "Secrets-compatible ASM format."));
      ImGui::BulletText(
          tr("Right-click a song in the browser to export/import."));
      ImGui::BulletText(tr("Exported ASM can be assembled with Asar."));
      ImGui::BulletText(tr("Import parses ASM labels and data directives."));
    }
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_HELP_PANEL_H_
