#include "app/editor/music/song_browser_view.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace music {

using yaze::zelda3::music::MusicBank;

void SongBrowserView::Draw(MusicBank& bank) {
  // Search filter
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##SongFilter", ICON_MD_SEARCH " Search songs...",
                           search_buffer_, sizeof(search_buffer_));

  // Bank Space Management Section
  if (ImGui::CollapsingHeader(ICON_MD_STORAGE " Bank Space")) {
    ImGui::Indent(8.0f);

    // Check for expanded music patch
    if (bank.HasExpandedMusicPatch()) {
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_CHECK_CIRCLE " Oracle of Secrets expanded music detected");
      const auto& info = bank.GetExpandedBankInfo();
      ImGui::TextDisabled("Expanded bank at $%06X, Aux at $%06X",
                          info.main_rom_offset, info.aux_rom_offset);
      ImGui::Spacing();
    }

    // Display space for each bank
    static const char* bank_names[] = {"Overworld", "Dungeon", "Credits",
                                        "Expanded", "Auxiliary"};
    static const MusicBank::Bank banks[] = {
        MusicBank::Bank::Overworld, MusicBank::Bank::Dungeon,
        MusicBank::Bank::Credits, MusicBank::Bank::OverworldExpanded,
        MusicBank::Bank::Auxiliary};

    int num_banks = bank.HasExpandedMusicPatch() ? 5 : 3;

    for (int i = 0; i < num_banks; ++i) {
      auto space = bank.CalculateSpaceUsage(banks[i]);
      if (space.total_bytes == 0) continue;  // Skip empty/invalid banks

      // Progress bar color based on usage
      ImVec4 bar_color;
      if (space.is_critical) {
        bar_color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);  // Red
      } else if (space.is_warning) {
        bar_color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);  // Yellow
      } else {
        bar_color = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);  // Green
      }

      ImGui::Text("%s:", bank_names[i]);
      ImGui::SameLine(100);

      // Progress bar
      ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_color);
      float fraction = space.usage_percent / 100.0f;
      std::string overlay = absl::StrFormat(
          "%d / %d bytes (%.1f%%)", space.used_bytes, space.total_bytes,
          space.usage_percent);
      ImGui::ProgressBar(fraction, ImVec2(-1, 0), overlay.c_str());
      ImGui::PopStyleColor();

      // Warning/critical messages
      if (space.is_critical) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                           ICON_MD_ERROR " %s", space.recommendation.c_str());
      } else if (space.is_warning) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                           ICON_MD_WARNING " %s", space.recommendation.c_str());
      }
    }

    // Overall status
    ImGui::Spacing();
    if (!bank.AllSongsFit()) {
      ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                         ICON_MD_ERROR " Some banks are overflowing!");
      ImGui::TextDisabled("Songs won't fit in ROM. Remove or shorten songs.");
    } else {
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_CHECK " All songs fit in ROM");
    }

    ImGui::Unindent(8.0f);
  }

  ImGui::Separator();

  // Toolbar
  if (ImGui::Button(ICON_MD_ADD " New Song")) {
    int new_idx = bank.CreateNewSong("New Song", MusicBank::Bank::Dungeon);
    if (new_idx >= 0) {
      selected_song_index_ = new_idx;
      if (on_song_selected_) on_song_selected_(new_idx);
      if (on_edit_) on_edit_();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FILE_UPLOAD " Import")) {
    // TODO: Implement SPC/MML import
  }

  ImGui::Separator();

  ImGui::BeginChild("SongList", ImVec2(0, 0), true);

  // Vanilla Songs Section
  if (ImGui::CollapsingHeader(ICON_MD_LIBRARY_MUSIC " Vanilla Songs",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    for (size_t i = 0; i < bank.GetSongCount(); ++i) {
      const auto* song = bank.GetSong(static_cast<int>(i));
      if (!song || !bank.IsVanilla(static_cast<int>(i))) continue;

      // Filter check
      std::string display_name = absl::StrFormat("%02X: %s", i + 1, song->name);
      if (!MatchesSearch(display_name)) continue;

      // Icon + label
      std::string label =
          absl::StrFormat(ICON_MD_MUSIC_NOTE " %s##vanilla%zu", display_name, i);
      bool is_selected = (selected_song_index_ == static_cast<int>(i));
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        selected_song_index_ = static_cast<int>(i);
        if (on_song_selected_) {
          on_song_selected_(selected_song_index_);
        }
      }

      // Double-click opens tracker
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (on_open_tracker_) {
          on_open_tracker_(static_cast<int>(i));
        }
      }

      // Context menu for vanilla songs
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MD_MUSIC_NOTE " Open Tracker")) {
          if (on_open_tracker_) on_open_tracker_(static_cast<int>(i));
        }
        if (ImGui::MenuItem(ICON_MD_PIANO " Open Piano Roll")) {
          if (on_open_piano_roll_) on_open_piano_roll_(static_cast<int>(i));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Duplicate as Custom")) {
          bank.DuplicateSong(static_cast<int>(i));
          if (on_edit_) on_edit_();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_FILE_DOWNLOAD " Export to ASM...")) {
          if (on_export_asm_) on_export_asm_(static_cast<int>(i));
        }
        ImGui::EndPopup();
      }
    }
  }

  // Custom Songs Section
  if (ImGui::CollapsingHeader(ICON_MD_EDIT " Custom Songs",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    bool has_custom = false;
    for (size_t i = 0; i < bank.GetSongCount(); ++i) {
      const auto* song = bank.GetSong(static_cast<int>(i));
      if (!song || bank.IsVanilla(static_cast<int>(i))) continue;

      has_custom = true;

      // Filter check
      std::string display_name = absl::StrFormat("%02X: %s", i + 1, song->name);
      if (!MatchesSearch(display_name)) continue;

      // Custom song icon + label (different color)
      std::string label =
          absl::StrFormat(ICON_MD_AUDIOTRACK " %s##custom%zu", display_name, i);
      bool is_selected = (selected_song_index_ == static_cast<int>(i));

      // Highlight custom songs with a subtle green color
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        selected_song_index_ = static_cast<int>(i);
        if (on_song_selected_) {
          on_song_selected_(selected_song_index_);
        }
      }
      ImGui::PopStyleColor();

      // Double-click opens tracker
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (on_open_tracker_) {
          on_open_tracker_(static_cast<int>(i));
        }
      }

      // Context menu for custom songs (includes delete/rename)
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MD_MUSIC_NOTE " Open Tracker")) {
          if (on_open_tracker_) on_open_tracker_(static_cast<int>(i));
        }
        if (ImGui::MenuItem(ICON_MD_PIANO " Open Piano Roll")) {
          if (on_open_piano_roll_) on_open_piano_roll_(static_cast<int>(i));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Duplicate")) {
          bank.DuplicateSong(static_cast<int>(i));
          if (on_edit_) on_edit_();
        }
        if (ImGui::MenuItem(ICON_MD_DRIVE_FILE_RENAME_OUTLINE " Rename")) {
          rename_target_index_ = static_cast<int>(i);
          // TODO: Open rename popup
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_FILE_DOWNLOAD " Export to ASM...")) {
          if (on_export_asm_) on_export_asm_(static_cast<int>(i));
        }
        if (ImGui::MenuItem(ICON_MD_FILE_UPLOAD " Import from ASM...")) {
          if (on_import_asm_) on_import_asm_(static_cast<int>(i));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_DELETE " Delete")) {
          (void)bank.DeleteSong(static_cast<int>(i));
          if (selected_song_index_ == static_cast<int>(i)) {
            selected_song_index_ = -1;
          }
          if (on_edit_) on_edit_();
        }
        ImGui::EndPopup();
      }
    }

    if (!has_custom) {
      ImGui::TextDisabled("No custom songs yet");
      ImGui::TextDisabled("Click 'New Song' or duplicate a vanilla song");
    }
  }

  ImGui::EndChild();
}

bool SongBrowserView::MatchesSearch(const std::string& name) const {
  if (search_buffer_[0] == '\0') return true;

  // Case-insensitive search
  std::string lower_name = name;
  std::string lower_search(search_buffer_);
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                 ::tolower);
  std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(),
                 ::tolower);

  return lower_name.find(lower_search) != std::string::npos;
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
