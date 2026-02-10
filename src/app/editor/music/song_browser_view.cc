#include "app/editor/music/song_browser_view.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace music {

using yaze::gui::GetErrorColor;
using yaze::gui::GetSuccessColor;
using yaze::gui::GetWarningColor;
using yaze::zelda3::music::MusicBank;

void SongBrowserView::Draw(MusicBank& bank) {
  // Search filter
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##SongFilter", ICON_MD_SEARCH " Search songs...",
                           search_buffer_, sizeof(search_buffer_));

  // Update filter cache if needed
  if (std::string(search_buffer_) != last_search_buffer_ ||
      (filtered_vanilla_indices_.empty() && filtered_custom_indices_.empty())) {
    last_search_buffer_ = search_buffer_;
    RebuildFilterCache(bank);
  }

  // Bank Space Management Section
  if (ImGui::CollapsingHeader(ICON_MD_STORAGE " Bank Space")) {
    ImGui::Indent(8.0f);

    // Check for expanded music patch
    if (bank.HasExpandedMusicPatch()) {
      ImGui::TextColored(GetSuccessColor(), ICON_MD_CHECK_CIRCLE
                         " Oracle of Secrets expanded music detected");
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
      if (space.total_bytes == 0)
        continue;  // Skip empty/invalid banks

      // Progress bar color based on usage
      ImVec4 bar_color =
          space.is_critical
              ? GetErrorColor()
              : (space.is_warning ? GetWarningColor() : GetSuccessColor());

      ImGui::Text("%s:", bank_names[i]);
      ImGui::SameLine(100);

      // Progress bar
      {
        gui::StyleColorGuard bar_guard(ImGuiCol_PlotHistogram, bar_color);
        float fraction = space.usage_percent / 100.0f;
        std::string overlay =
            absl::StrFormat("%d / %d bytes (%.1f%%)", space.used_bytes,
                            space.total_bytes, space.usage_percent);
        ImGui::ProgressBar(fraction, ImVec2(-1, 0), overlay.c_str());
      }

      // Warning/critical messages
      if (space.is_critical) {
        ImGui::TextColored(GetErrorColor(), ICON_MD_ERROR " %s",
                           space.recommendation.c_str());
      } else if (space.is_warning) {
        ImGui::TextColored(GetWarningColor(), ICON_MD_WARNING " %s",
                           space.recommendation.c_str());
      }
    }

    // Overall status
    ImGui::Spacing();
    if (!bank.AllSongsFit()) {
      ImGui::TextColored(GetErrorColor(),
                         ICON_MD_ERROR " Some banks are overflowing!");
      ImGui::TextDisabled("Songs won't fit in ROM. Remove or shorten songs.");
    } else {
      ImGui::TextColored(GetSuccessColor(),
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
      if (on_song_selected_)
        on_song_selected_(new_idx);
      if (on_edit_)
        on_edit_();
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
    const float item_height = ImGui::GetTextLineHeightWithSpacing();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered_vanilla_indices_.size()),
                  item_height);

    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        int i = filtered_vanilla_indices_[row];
        const auto* song = bank.GetSong(i);
        if (!song)
          continue;

        std::string display_name =
            absl::StrFormat("%02X: %s", i + 1, song->name);

        // Icon + label
        std::string label = absl::StrFormat(ICON_MD_MUSIC_NOTE " %s##vanilla%d",
                                            display_name, i);
        bool is_selected = (selected_song_index_ == i);

        // Push ID to avoid conflicts with same-named items across filter resets
        ImGui::PushID(i);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          selected_song_index_ = i;
          if (on_song_selected_) {
            on_song_selected_(selected_song_index_);
          }
        }

        // Double-click opens tracker
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          if (on_open_tracker_) {
            on_open_tracker_(i);
          }
        }

        // Context menu for vanilla songs
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem(ICON_MD_MUSIC_NOTE " Open Tracker")) {
            if (on_open_tracker_)
              on_open_tracker_(i);
          }
          if (ImGui::MenuItem(ICON_MD_PIANO " Open Piano Roll")) {
            if (on_open_piano_roll_)
              on_open_piano_roll_(i);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Duplicate as Custom")) {
            bank.DuplicateSong(i);
            if (on_edit_)
              on_edit_();
            // Force cache rebuild on next frame (or immediately).
            last_search_buffer_.clear();
          }
          ImGui::Separator();
          if (ImGui::MenuItem(ICON_MD_FILE_DOWNLOAD " Export to ASM...")) {
            if (on_export_asm_)
              on_export_asm_(i);
          }
          ImGui::EndPopup();
        }
        ImGui::PopID();
      }
    }
  }

  // Custom Songs Section
  if (ImGui::CollapsingHeader(ICON_MD_EDIT " Custom Songs",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (filtered_custom_indices_.empty()) {
      ImGui::TextDisabled("No custom songs match filter");
      ImGui::TextDisabled("Click 'New Song' or duplicate a vanilla song");
    } else {
      const float item_height = ImGui::GetTextLineHeightWithSpacing();
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(filtered_custom_indices_.size()),
                    item_height);

      while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
          int i = filtered_custom_indices_[row];
          const auto* song = bank.GetSong(i);
          if (!song)
            continue;

          std::string display_name =
              absl::StrFormat("%02X: %s", i + 1, song->name);

          // Custom song icon + label (different color)
          std::string label = absl::StrFormat(
              ICON_MD_AUDIOTRACK " %s##custom%d", display_name, i);
          bool is_selected = (selected_song_index_ == i);

          gui::StyleColorGuard text_guard(ImGuiCol_Text, GetSuccessColor());
          ImGui::PushID(i);
          if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_song_index_ = i;
            if (on_song_selected_) {
              on_song_selected_(selected_song_index_);
            }
          }

          // Double-click opens tracker
          if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (on_open_tracker_) {
              on_open_tracker_(i);
            }
          }

          // Context menu for custom songs (includes delete/rename)
          if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem(ICON_MD_MUSIC_NOTE " Open Tracker")) {
              if (on_open_tracker_)
                on_open_tracker_(i);
            }
            if (ImGui::MenuItem(ICON_MD_PIANO " Open Piano Roll")) {
              if (on_open_piano_roll_)
                on_open_piano_roll_(i);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Duplicate")) {
              bank.DuplicateSong(i);
              if (on_edit_)
                on_edit_();
              last_search_buffer_.clear();  // Force rebuild.
            }
            if (ImGui::MenuItem(ICON_MD_DRIVE_FILE_RENAME_OUTLINE " Rename")) {
              rename_target_index_ = i;
              // TODO: Open rename popup
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MD_FILE_DOWNLOAD " Export to ASM...")) {
              if (on_export_asm_)
                on_export_asm_(i);
            }
            if (ImGui::MenuItem(ICON_MD_FILE_UPLOAD " Import from ASM...")) {
              if (on_import_asm_)
                on_import_asm_(i);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MD_DELETE " Delete")) {
              (void)bank.DeleteSong(i);
              if (selected_song_index_ == i) {
                selected_song_index_ = -1;
              }
              if (on_edit_)
                on_edit_();
              last_search_buffer_.clear();  // Force rebuild.
            }
            ImGui::EndPopup();
          }
          ImGui::PopID();
        }
      }
    }
  }

  ImGui::EndChild();
}

bool SongBrowserView::MatchesSearch(const std::string& name) const {
  if (search_buffer_[0] == '\0')
    return true;

  // Case-insensitive search
  std::string lower_name = name;
  std::string lower_search(search_buffer_);
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                 ::tolower);
  std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(),
                 ::tolower);

  return lower_name.find(lower_search) != std::string::npos;
}

void SongBrowserView::RebuildFilterCache(const MusicBank& bank) {
  filtered_vanilla_indices_.clear();
  filtered_custom_indices_.clear();

  for (size_t i = 0; i < bank.GetSongCount(); ++i) {
    const auto* song = bank.GetSong(static_cast<int>(i));
    if (!song)
      continue;

    std::string display_name =
        absl::StrFormat("%02X: %s", i + 1, song->name);
    if (!MatchesSearch(display_name))
      continue;

    if (bank.IsVanilla(static_cast<int>(i))) {
      filtered_vanilla_indices_.push_back(static_cast<int>(i));
    } else {
      filtered_custom_indices_.push_back(static_cast<int>(i));
    }
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
