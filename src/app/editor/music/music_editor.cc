#include "music_editor.h"

#include "absl/strings/str_format.h"
#include "app/gfx/performance_profiler.h"
#include "app/editor/code/assembly_editor.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "imgui/imgui.h"

// ============================================================================
//
//                              IMPLEMENTATION PLAN
//
// This file implements the music editor for yaze. The full implementation will
// involve integrating three main components:
//
// 1. The UI (`MusicEditor` class):
//    - Built with ImGui, providing a piano roll, staff view, channel selectors,
//      and playback controls.
//
// 2. The Data Model (`zelda3::music::Tracker`):
//    - A legacy component from Hyrule Magic responsible for parsing the game's
//      unique music data format from the ROM. It loads songs, instruments,
//      and SPC commands into a structured format.
//
// 3. The Playback Engine (`emu::Apu`):
//    - A full-featured SNES APU (SPC700 + S-DSP) emulator. This will be used
//      to play back the music in real-time by loading it with the game's
//      sound driver, instrument/sample data, and feeding it the parsed music
//      commands.
//
// ----------------------------------------------------------------------------
//
//                              High-Level Plan
//
// I.   LOADING & DATA BINDING:
//      1. In `MusicEditor::Load()`, call `music_tracker_.LoadSongs(*rom())` to
//         parse all music data from the ROM.
//      2. Dynamically populate the "Select a song" dropdown from the list of
//         songs loaded by the `Tracker`.
//      3. When a song is selected, bind the UI to its data. The 8 channel tabs
//         will correspond to the 8 `SongPart`s of the selected `Song`.
//
// II.  VISUALIZATION (PIANO ROLL & STAFF):
//      1. For the selected channel, retrieve the linked list of `SpcCommand`s
//         from the `Tracker`.
//      2. Implement a function to traverse the `SpcCommand` list and calculate
//         the precise timing and pitch of each note. The `Tracker::GetBlockTime`
//         function is essential for this.
//      3. In `DrawPianoRoll()` and `DrawPianoStaff()`, render the notes based on
//         their calculated time and pitch. The X-axis will represent time, and
//         the Y-axis will represent pitch.
//      4. Non-note commands (e.g., volume, panning, instrument changes) should
//         be visualized in a separate "events" lane.
//
// III. PLAYBACK (APU EMULATION):
//      1. The `MusicEditor` will contain an instance of `emu::Apu`.
//      2. On "Play" press:
//         a. Initialize the APU (`apu_.Reset()`).
//         b. Load the SNES sound driver program into the APU's RAM. This data
//            is extracted by the `Tracker` (`snddat1`, `snddat2`).
//         c. Load all necessary instrument definitions (`insts`) and sample
//            data (`waves` converted to BRR format) into APU RAM.
//         d. Begin a playback loop (likely in a separate thread or via audio
//            callback).
//      3. Playback Loop:
//         a. At each step, determine the next `SpcCommand` to be played based
//            on the elapsed time.
//         b. "Send" the command to the emulated SPC700 by writing to the APU's
//            I/O ports (`apu_.in_ports_`). The sound driver running on the
//            SPC700 will interpret these commands.
//         c. Execute the APU for one frame's worth of cycles (`apu_.RunCycles()`).
//         d. Retrieve the generated audio buffer from the DSP (`apu_.GetSamples()`).
//         e. Queue the audio buffer for playback via the host's audio system (e.g., SDL).
//         f. Update the progress bar and the playback cursor on the piano roll.
//      4. On "Stop" press, terminate the playback loop.
//
// IV.  EDITING & SAVING:
//      1. Add interaction to the piano roll to create, delete, or modify notes.
//      2. These actions will manipulate the `SpcCommand` linked list in the
//         `Tracker`'s data structures.
//      3. The `AssemblyEditor` will provide a raw text view of the commands for
//         the selected channel, allowing for advanced editing.
//      4. Implement a "Save" function that calls `music_tracker_.SaveSongs()`
//         to serialize the modified `SpcCommand` data and write it back to the
//         ROM.
//
// ============================================================================

namespace yaze {
namespace editor {

void MusicEditor::Initialize() {
  // PLAN:
  // 1. Initialize the APU emulator instance.
  //    - `apu_.Init()`
  // 2. Initialize an audio device/stream using a library like SDL. This will
  //    provide a callback for feeding the APU's output samples.
  // 3. Set up the AssemblyEditor for displaying SPC command text.
  // apu_.Init();
  // TODO: Initialize SDL audio here.
}

absl::Status MusicEditor::Load() {
  gfx::ScopedTimer timer("MusicEditor::Load");
  // PLAN:
  // 1. Call `music_tracker_.LoadSongs(*rom())`. This is the main entry point
  //    for parsing all music data from the ROM into the tracker's structures.
  // 2. After loading, get the list of song names/IDs from the tracker.
  // 3. Populate `kGameSongs` (or a dynamic equivalent) with the song list
  //    to be displayed in the UI.
  // 4. Set the default selected song to the first one.
  // music_tracker_.LoadSongs(*rom());
  // // TODO: Populate song list dynamically.
  // song_names_.clear();
  // for (size_t i = 0; i < music_tracker_.songs.size(); ++i) {
  //   const auto& song = music_tracker_.songs[i];
  //   song_names_.push_back(absl::StrFormat("Song %zu (Addr: 0x%04X)", i + 1, song.addr));
  // }
  // if (!song_names_.empty()) {
  //   current_song_index_ = 0;
  // }
}

absl::Status MusicEditor::Update() {
  // PLAN:
  // 1. If a song is playing, call a new function, e.g., `UpdatePlayback()`,
  //    which will handle advancing the song and feeding commands to the APU.
  // 2. Draw the main UI. The state of the UI (e.g., which notes are displayed)
  //    will be derived from the `music_tracker_` data for the currently
  //    selected song and channel.
  if (is_playing_) {
    UpdatePlayback();
  }

  if (ImGui::BeginTable("MusicEditorColumns", 2, music_editor_flags_,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Assembly");
    ImGui::TableSetupColumn("Composition");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    // PLAN:
    // This assembly editor will display the raw SpcCommands for the selected
    // channel as text. Changes here would need to be parsed and reflected
    // back into the tracker's data model.
    assembly_editor_.InlineUpdate();
    DrawTrackerView();

    ImGui::TableNextColumn();
    DrawToolset();
    DrawInstrumentEditor();
    DrawSampleEditor();

    ImGui::EndTable();
  }

  return absl::OkStatus();
}









void MusicEditor::DrawToolset() {
  static int current_volume = 0;
  const int MAX_VOLUME = 100;

  gui::ItemLabel("Select a song to edit: ", gui::ItemLabelFlags::Left);
  // PLAN:
  // Replace `kGameSongs` with a dynamic list of songs from `music_tracker_`.
  // The `selected_option` will be the index into that list.
  ImGui::Combo("#songs_in_game", &current_song_index_, [](void* data, int idx, const char** out_text) {
    auto* vec = static_cast<std::vector<std::string>*>(data);
    if (idx < 0 || idx >= vec->size()) return false;
    *out_text = vec->at(idx).c_str();
    return true;
  }, static_cast<void*>(&song_names_), song_names_.size());

  gui::ItemLabel("Controls: ", gui::ItemLabelFlags::Left);
  if (ImGui::BeginTable("SongToolset", 6, toolset_table_flags_, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#play");
    ImGui::TableSetupColumn("#rewind");
    ImGui::TableSetupColumn("#fastforward");
    ImGui::TableSetupColumn("#volume");
    ImGui::TableSetupColumn("#debug");

    ImGui::TableSetupColumn("#slider");

    ImGui::TableNextColumn();
    if (ImGui::Button(is_playing_ ? ICON_MD_STOP : ICON_MD_PLAY_ARROW)) {
      // PLAN:
      // 1. Toggle `is_playing_`.
      // 2. If starting playback:
      //    a. Call a `StartPlayback()` method.
      //    b. This method will reset the APU, load the sound driver,
      //       instruments, and samples into APU RAM.
      //    c. It will then start the audio callback/thread.
      // 3. If stopping playback:
      //    a. Call a `StopPlayback()` method.
      //    b. This method will stop the audio callback/thread.
      if (is_playing_) {
        StopPlayback();
      } else {
        StartPlayback();
      }
      is_playing_ = !is_playing_;
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_FAST_REWIND)) {
      // PLAN: Seek backward in the song's command stream.
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_FAST_FORWARD)) {
      // PLAN: Seek forward in the song's command stream.
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_VOLUME_UP)) {
      // PLAN: Control master volume on the DSP.
    }

    // This is a temporary debug button.
    if (ImGui::Button(ICON_MD_ACCESS_TIME)) {
      music_tracker_.LoadSongs(*rom());
    }
    ImGui::TableNextColumn();
    ImGui::SliderInt("Volume", &current_volume, 0, 100);
    ImGui::EndTable();
  }

  const int SONG_DURATION = 120;
  static int current_time = 0;

  // PLAN:
  // 1. `SONG_DURATION` should be calculated dynamically for the selected song
  //    using `Tracker::GetBlockTime`.
  // 2. `current_time` should be updated continuously during playback based on
  //    the APU's cycle count or the number of samples played.
  gui::ItemLabel("Current Time: ", gui::ItemLabelFlags::Left);
  ImGui::Text("%d:%02d", current_time / 60, current_time % 60);
  ImGui::SameLine();
  ImGui::ProgressBar((float)current_time / SONG_DURATION);
}

void MusicEditor::DrawTrackerView() {
  // Basic FamiTracker-like layout
  ImGui::BeginChild("##TrackerView", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  // Channel Headers
  const int num_channels = 8; // SNES APU has 8 channels
  const float channel_header_width = 150.0f; // Adjust as needed

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

  // Draw channel headers
  for (int i = 0; i < num_channels; ++i) {
    ImGui::SameLine();
    ImGui::BeginGroup();
    // Channel button
    ImGui::Button(absl::StrFormat("CH %d", i + 1).data(), ImVec2(channel_header_width, 0));

    // Mute button
    ImGui::SameLine();
    if (channel_muted_[i]) {
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 0, 0, 255)); // Red for muted
    }
    if (ImGui::Button(ICON_MD_VOLUME_OFF)) {
      channel_muted_[i] = !channel_muted_[i];
    }
    if (channel_muted_[i]) {
      ImGui::PopStyleColor();
    }

    // Solo button
    ImGui::SameLine();
    if (channel_soloed_[i]) {
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 255, 255, 255)); // Cyan for soloed
    }
    if (ImGui::Button(ICON_MD_STAR)) {
      channel_soloed_[i] = !channel_soloed_[i];
    }
    if (channel_soloed_[i]) {
      ImGui::PopStyleColor();
    }

    ImGui::EndGroup();
  }
  ImGui::PopStyleVar(2);

  ImGui::Separator();

  // Pattern Data
  const int num_rows = 64; // Example: 64 rows per pattern
  const float row_height = ImGui::GetTextLineHeightWithSpacing();

  ImGui::BeginChild("##PatternData", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  for (int row = 0; row < num_rows; ++row) {
    // Highlight current row (playback position)
    if (row == current_pattern_index_) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255)); // Yellow
    }

    // Row number
    ImGui::Text("%02X", row);
    ImGui::SameLine();

    // for (int channel = 0; channel < num_channels; ++channel) {
    //   ImGui::BeginGroup();
    //   if (music_tracker_.songs.empty() || current_song_index_ >= music_tracker_.songs.size()) {
    //     ImGui::Text("%-4s", "---"); // Fixed width 4 for note
    //     ImGui::SameLine();
    //     ImGui::Text("%-2s", "--"); // Fixed width 2 for instrument
    //     ImGui::SameLine();
    //     ImGui::Text("%-4s", "----"); // Fixed width 4 for volume/effect
    //   } else {
    //     const auto& current_song = music_tracker_.songs[current_song_index_];
    //     // TODO: Need to get the SongPart for the current channel.
    //     // The Song struct has a `tbl` (table of SongPart pointers).
    //     // We need to map `channel` (0-7) to the correct `SongPart` in `current_song.tbl`.
    //     // For now, assume `current_song.tbl` is directly indexed by `channel`.
    //     if (channel < current_song.numparts) {
    //                 const auto* song_part = current_song.tbl[channel];
    //                 if (song_part) {
    //                   short spc_command_index = song_part->tbl[0]; // The SongPart struct has `tbl[8]` which are indices into the SpcCommand array. For now, we'll just use the first track (index 0) of the SongPart.
    //                   short current_row_time = 0; // Accumulate time to map to rows
          
    //                   // Iterate through SpcCommands
    //                   while (spc_command_index != -1 && spc_command_index < music_tracker_.m_size) {
    //                     const auto& spc_command = music_tracker_.current_spc_command_[spc_command_index];
          
    //                     // TODO: Map spc_command.tim and spc_command.tim2 to rows.
    //                     // For now, just display the command itself if it falls on the current row.
    //                     // This logic needs to be refined to correctly interpret command durations.
    //                     if (current_row_time == row) { // Simplified mapping for now
    //                       // TODO: Decode SpcCommand to Note, Instrument, Effect strings
    //                       ImGui::Text("%-4s", absl::StrFormat("%02X", spc_command.cmd).data()); // Display command byte
    //                       ImGui::SameLine();
    //                       ImGui::Text("%-2s", absl::StrFormat("%02X", spc_command.p1).data()); // Display p1
    //                       ImGui::SameLine();
    //                       ImGui::Text("%-4s", absl::StrFormat("%02X", spc_command.p2).data()); // Display p2
    //                     } else if (current_row_time > row) {
    //                       // If we've passed the current row, no more commands for this row
    //                       break;
    //                     }
          
    //                     // Advance time (simplified for now, needs proper GetBlockTime integration)
    //                     current_row_time++; // Assume each command takes 1 row for now
          
    //                     spc_command_index = spc_command.next;
    //                   }
          
    //                   // If no command was displayed for this row, display empty
    //                   if (current_row_time <= row) {
    //                     ImGui::Text("%-4s", "---");
    //                     ImGui::SameLine();
    //                     ImGui::Text("%-2s", "--");
    //                     ImGui::SameLine();
    //                     ImGui::Text("%-4s", "----");
    //                   }
          
    //                 } else {
    //                   ImGui::Text("%-4s", "---");
    //                   ImGui::SameLine();
    //                   ImGui::Text("%-2s", "--");
    //                   ImGui::SameLine();
    //                   ImGui::Text("%-4s", "----");
    //                 }        } else {
    //       ImGui::Text("%-4s", "---");
    //       ImGui::SameLine();
    //       ImGui::Text("%-2s", "--");
    //       ImGui::SameLine();
    //       ImGui::Text("%-4s", "----");
    //     }
    //   }
    //   ImGui::EndGroup();
    //   ImGui::SameLine();
    // }
    // ImGui::NewLine(); // Move to the next line after all channels for the row

    // if (row == current_pattern_index_) {
    //   ImGui::PopStyleColor();
    // }
  }
  ImGui::PopStyleVar();

  ImGui::EndChild(); // End PatternData child

  ImGui::EndChild(); // End TrackerView child
}

void MusicEditor::DrawInstrumentEditor() {
  // Implementation for instrument editing
  ImGui::Text("Instrument Editor");
  // ...
}

void MusicEditor::DrawSampleEditor() {
  // Implementation for sample editing
  ImGui::Text("Sample Editor");
  // ...
}

void MusicEditor::StartPlayback() {
  // Implementation for starting APU playback
  ImGui::Text("Starting Playback...");
  // ...
}

void MusicEditor::StopPlayback() {
  // Implementation for stopping APU playback
  ImGui::Text("Stopping Playback...");
  // ...
}

void MusicEditor::UpdatePlayback() {
  // Implementation for updating APU playback state
  // ...
}

}  // namespace editor
}  // namespace yaze