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
