#include "app/editor/music/tracker_view.h"

#include <algorithm>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

namespace {
// Theme-aware color helpers
ImU32 GetColorNote() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  return ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.success));
}

ImU32 GetColorCommand() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  return ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.info));
}

ImU32 GetColorSubroutine() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  return ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.warning));
}

ImU32 GetColorBeatHighlight() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  auto color = theme.active_selection;
  color.alpha = 0.15f;  // Low opacity for subtle highlight
  return ImGui::GetColorU32(gui::ConvertColorToImVec4(color));
}

ImU32 GetColorSelection(bool is_range) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  auto color = theme.editor_selection;
  color.alpha = is_range ? 0.4f : 0.7f;
  return ImGui::GetColorU32(gui::ConvertColorToImVec4(color));
}
std::string DescribeCommand(uint8_t opcode) {
  switch (opcode) {
    case 0xE0:
      return "Set Instrument";
    case 0xE1:
      return "Set Pan";
    case 0xE5:
      return "Master Volume";
    case 0xE6:
      return "Master Volume Fade";
    case 0xE7:
      return "Set Tempo";
    case 0xE8:
      return "Tempo Fade";
    case 0xE9:
      return "Global Transpose";
    case 0xEA:
      return "Channel Transpose";
    case 0xEB:
      return "Tremolo On";
    case 0xEC:
      return "Tremolo Off";
    case 0xED:
      return "Channel Volume";
    case 0xEE:
      return "Channel Volume Fade";
    case 0xEF:
      return "Call Subroutine";
    case 0xF0:
      return "Vibrato Fade";
    case 0xF1:
      return "Pitch Env To";
    case 0xF2:
      return "Pitch Env From";
    case 0xF3:
      return "Pitch Env Off";
    case 0xF4:
      return "Tuning";
    case 0xF5:
      return "Echo Bits";
    case 0xF6:
      return "Echo Off";
    case 0xF7:
      return "Echo Params";
    case 0xF8:
      return "Echo Vol Fade";
    case 0xF9:
      return "Pitch Slide";
    case 0xFA:
      return "Percussion Patch";
    default:
      return "Command";
  }
}

// Command options for combo box
struct CommandOption {
  const char* name;
  uint8_t opcode;
};

constexpr CommandOption kCommandOptions[] = {
    {"Set Instrument", 0xE0},   {"Set Pan", 0xE1},
    {"Vibrato On", 0xE3},       {"Vibrato Off", 0xE4},
    {"Master Volume", 0xE5},    {"Master Volume Fade", 0xE6},
    {"Set Tempo", 0xE7},        {"Tempo Fade", 0xE8},
    {"Global Transpose", 0xE9}, {"Channel Transpose", 0xEA},
    {"Tremolo On", 0xEB},       {"Tremolo Off", 0xEC},
    {"Channel Volume", 0xED},   {"Channel Volume Fade", 0xEE},
    {"Call Subroutine", 0xEF},  {"Vibrato Fade", 0xF0},
    {"Pitch Env To", 0xF1},     {"Pitch Env From", 0xF2},
    {"Pitch Env Off", 0xF3},    {"Tuning", 0xF4},
    {"Echo Bits", 0xF5},        {"Echo Off", 0xF6},
    {"Echo Params", 0xF7},      {"Echo Vol Fade", 0xF8},
    {"Pitch Slide", 0xF9},      {"Percussion Patch", 0xFA},
};
}  // namespace

void TrackerView::Draw(MusicSong* song, const MusicBank* bank) {
  if (!song) {
    ImGui::TextDisabled("No song loaded");
    return;
  }

  // Handle input before drawing to avoid 1-frame lag
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    HandleNavigation();
    HandleKeyboardInput(song);
    HandleEditShortcuts(song);
  }

  DrawToolbar(song);
  ImGui::Separator();

  ImGui::BeginChild("TrackerGrid", ImVec2(0, 0), true);
  DrawGrid(song, bank);
  ImGui::EndChild();
}

void TrackerView::DrawToolbar(MusicSong* song) {
  ImGui::Text("%s", song->name.empty() ? "Untitled" : song->name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("(%d ticks)", song->GetTotalDuration());

  ImGui::SameLine();
  ImGui::Text("| Bank: %s", song->bank == 0
                                ? "Overworld"
                                : (song->bank == 1 ? "Dungeon" : "Credits"));

  ImGui::SameLine();
  ImGui::PushItemWidth(100);
  if (ImGui::DragInt("Ticks/Row", &ticks_per_row_, 1, 1, 96)) {
    if (ticks_per_row_ < 1)
      ticks_per_row_ = 1;
  }
  ImGui::PopItemWidth();
}

void TrackerView::DrawGrid(MusicSong* song, const MusicBank* bank) {
  if (song->segments.empty())
    return;

  // Use the first segment for now (TODO: Handle multiple segments)
  // Non-const reference to allow editing
  auto& segment = song->segments[0];
  uint32_t duration = segment.GetDuration();

  // Table setup: Row number + 8 Channels
  if (ImGui::BeginTable("TrackerTable", 9,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {

    // Header
    ImGui::TableSetupColumn("Tick", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    for (int i = 0; i < 8; ++i) {
      ImGui::TableSetupColumn(absl::StrFormat("Ch %d", i + 1).c_str());
    }
    ImGui::TableHeadersRow();

    // Rows
    int total_rows = (duration + ticks_per_row_ - 1) / ticks_per_row_;
    ImGuiListClipper clipper;
    clipper.Begin(total_rows, row_height_);

    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        int tick_start = row * ticks_per_row_;
        int tick_end = tick_start + ticks_per_row_;

        ImGui::TableNextRow();

        // Highlight every 4th row (beat)
        if (row % 4 == 0) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 GetColorBeatHighlight());
        }

        // Tick Number Column
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%04X", tick_start);

        // Channel Columns
        for (int ch = 0; ch < 8; ++ch) {
          ImGui::TableSetColumnIndex(ch + 1);

          // Selection drawing
          bool is_selected =
              (row == selected_row_ && (ch + 1) == selected_col_);

          // Calculate range selection
          if (selection_anchor_row_ != -1 && selection_anchor_col_ != -1) {
            int row_start = std::min(selected_row_, selection_anchor_row_);
            int row_end = std::max(selected_row_, selection_anchor_row_);
            int col_start = std::min(selected_col_, selection_anchor_col_);
            int col_end = std::max(selected_col_, selection_anchor_col_);

            if (row >= row_start && row <= row_end && (ch + 1) >= col_start &&
                (ch + 1) <= col_end) {
              ImGui::TableSetBgColor(
                  ImGuiTableBgTarget_CellBg,
                  GetColorSelection(true));  // Range selection
            }
          } else if (is_selected) {
            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_CellBg,
                GetColorSelection(false));  // Single selection

            // Auto-scroll to selection
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
              // Simple scroll-into-view logic could go here if needed,
              // but ImGui handles focused items usually well enough if using Selectable
            }
          }

          // Find event in this time range
          int event_index = -1;
          auto& track = segment.tracks[ch];
          for (size_t idx = 0; idx < track.events.size(); ++idx) {
            const auto& evt = track.events[idx];
            if (evt.tick >= tick_start && evt.tick < tick_end) {
              event_index = static_cast<int>(idx);
              break;
            }
            if (evt.tick >= tick_end)
              break;
          }

          DrawEventCell(track, event_index, ch, tick_start, bank);

          // Handle cell click for selection
          // Invisible button to capture clicks
          ImGui::PushID(row * 100 + ch);
          if (ImGui::Selectable("##cell", false,
                                ImGuiSelectableFlags_SpanAllColumns |
                                    ImGuiSelectableFlags_AllowOverlap,
                                ImVec2(0, row_height_))) {
            if (ImGui::GetIO().KeyShift) {
              if (selection_anchor_row_ == -1) {
                selection_anchor_row_ = selected_row_;
                selection_anchor_col_ = selected_col_;
              }
            } else {
              selection_anchor_row_ = -1;
              selection_anchor_col_ = -1;
            }
            selected_row_ = row;
            selected_col_ = ch + 1;
          }
          ImGui::PopID();
        }
      }
    }
    ImGui::EndTable();
  }
}

void TrackerView::DrawEventCell(MusicTrack& track, int event_index,
                                int channel_idx, uint16_t tick,
                                const MusicBank* bank) {
  TrackEvent* event_ptr =
      (event_index >= 0 && event_index < static_cast<int>(track.events.size()))
          ? &track.events[event_index]
          : nullptr;

  bool has_event = event_ptr != nullptr;

  if (!has_event) {
    ImGui::TextDisabled("...");
  } else {
    auto& event = *event_ptr;
    switch (event.type) {
      case TrackEvent::Type::Note:
        ImGui::TextColored(ImColor(GetColorNote()), "%s",
                           event.note.GetNoteName().c_str());
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Note: %s\nDuration: %d\nVelocity: %d",
                            event.note.GetNoteName().c_str(),
                            event.note.duration, event.note.velocity);
        }
        break;

      case TrackEvent::Type::Command: {
        ImU32 color = GetColorCommand();
        std::string label = absl::StrFormat("CMD %02X", event.command.opcode);
        std::string tooltip = DescribeCommand(event.command.opcode);

        // Improved display for common commands
        if (event.command.opcode == 0xE0) {  // Set Instrument
          if (bank) {
            const auto* inst = bank->GetInstrument(event.command.params[0]);
            if (inst) {
              label = absl::StrFormat("Instr: %s", inst->name.c_str());
              tooltip =
                  absl::StrFormat("Set Instrument: %s (ID %02X)",
                                  inst->name.c_str(), event.command.params[0]);
            } else {
              label = absl::StrFormat("Instr: %02X", event.command.params[0]);
            }
          } else {
            label = absl::StrFormat("Instr: %02X", event.command.params[0]);
          }
        } else if (event.command.opcode == 0xE1) {  // Set Pan
          int pan = event.command.params[0];
          if (pan == 0x0A)
            label = "Pan: Center";
          else if (pan < 0x0A)
            label = absl::StrFormat("Pan: L%d", 0x0A - pan);
          else
            label = absl::StrFormat("Pan: R%d", pan - 0x0A);
        } else if (event.command.opcode == 0xE7) {  // Set Tempo
          label = absl::StrFormat("Tempo: %d", event.command.params[0]);
        } else if (event.command.opcode == 0xED) {  // Channel Volume
          label = absl::StrFormat("Vol: %d", event.command.params[0]);
        } else if (event.command.opcode == 0xE5) {  // Master Volume
          label = absl::StrFormat("M.Vol: %d", event.command.params[0]);
        }

        ImGui::TextColored(ImColor(color), "%s", label.c_str());
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s\nOpcode: %02X\nParams: %02X %02X %02X",
                            tooltip.c_str(), event.command.opcode,
                            event.command.params[0], event.command.params[1],
                            event.command.params[2]);
        }
        break;
      }

      case TrackEvent::Type::SubroutineCall:
        ImGui::TextColored(ImColor(GetColorSubroutine()), "CALL");
        break;

      case TrackEvent::Type::End:
        ImGui::TextDisabled("END");
        break;
    }
  }

  bool hovered = ImGui::IsItemHovered();
  const bool double_clicked =
      hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  const bool right_clicked =
      hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

  // If empty and double-clicked, insert a default note
  if (!has_event && double_clicked) {
    TrackEvent evt = TrackEvent::MakeNote(
        tick, static_cast<uint8_t>(kNoteMinPitch + 36), kDurationSixteenth);
    track.InsertEvent(evt);
    if (on_edit_)
      on_edit_();
    return;
  }

  const std::string popup_id =
      absl::StrFormat("EditEvent##%d_%d_%d", channel_idx, tick, event_index);
  if ((double_clicked || right_clicked) && has_event) {
    ImGui::OpenPopup(popup_id.c_str());
  }

  if (ImGui::BeginPopup(popup_id.c_str())) {
    if (!has_event) {
      ImGui::TextDisabled("Empty");
      ImGui::EndPopup();
      return;
    }

    auto& event = *event_ptr;
    if (event.type == TrackEvent::Type::Note) {
      static int edit_pitch = kNoteMinPitch + 36;
      static int edit_duration = kDurationSixteenth;
      edit_pitch = event.note.pitch;
      edit_duration = event.note.duration;

      static std::vector<std::string> note_labels;
      if (note_labels.empty()) {
        for (int p = kNoteMinPitch; p <= kNoteMaxPitch; ++p) {
          Note n;
          n.pitch = static_cast<uint8_t>(p);
          note_labels.push_back(n.GetNoteName());
        }
      }
      int idx = edit_pitch - kNoteMinPitch;
      idx = std::clamp(idx, 0, static_cast<int>(note_labels.size()) - 1);

      ImGui::Text("Edit Note");
      if (ImGui::BeginCombo("Pitch", note_labels[idx].c_str())) {
        for (int i = 0; i < static_cast<int>(note_labels.size()); ++i) {
          bool sel = (i == idx);
          if (ImGui::Selectable(note_labels[i].c_str(), sel)) {
            idx = i;
            edit_pitch = kNoteMinPitch + i;
          }
          if (sel)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if (ImGui::SliderInt("Duration", &edit_duration, 1, 0xFF)) {
        edit_duration = std::clamp(edit_duration, 1, 0xFF);
      }

      if (ImGui::Button("Apply")) {
        event.note.pitch = static_cast<uint8_t>(edit_pitch);
        event.note.duration = static_cast<uint8_t>(edit_duration);
        if (on_edit_)
          on_edit_();
        ImGui::CloseCurrentPopup();
      }
    } else if (event.type == TrackEvent::Type::Command) {
      int current_cmd_idx = 0;
      for (size_t i = 0; i < IM_ARRAYSIZE(kCommandOptions); ++i) {
        if (kCommandOptions[i].opcode == event.command.opcode) {
          current_cmd_idx = static_cast<int>(i);
          break;
        }
      }
      ImGui::Text("Edit Command");
      if (ImGui::BeginCombo("Opcode", kCommandOptions[current_cmd_idx].name)) {
        for (size_t i = 0; i < IM_ARRAYSIZE(kCommandOptions); ++i) {
          bool sel = (static_cast<int>(i) == current_cmd_idx);
          if (ImGui::Selectable(kCommandOptions[i].name, sel)) {
            current_cmd_idx = static_cast<int>(i);
          }
          if (sel)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      int p0 = event.command.params[0];
      int p1 = event.command.params[1];
      int p2 = event.command.params[2];

      uint8_t opcode = kCommandOptions[current_cmd_idx].opcode;

      if (opcode == 0xE0 && bank) {  // Set Instrument
        // Instrument selector
        const auto* inst = bank->GetInstrument(p0);
        std::string preview =
            inst ? absl::StrFormat("%02X: %s", p0, inst->name.c_str())
                 : absl::StrFormat("%02X", p0);
        if (ImGui::BeginCombo("Instrument", preview.c_str())) {
          for (size_t i = 0; i < bank->GetInstrumentCount(); ++i) {
            const auto* item = bank->GetInstrument(i);
            bool is_selected = (static_cast<int>(i) == p0);
            if (ImGui::Selectable(
                    absl::StrFormat("%02X: %s", i, item->name.c_str()).c_str(),
                    is_selected)) {
              p0 = static_cast<int>(i);
            }
            if (is_selected)
              ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
      } else {
        ImGui::InputInt("Param 0 (hex)", &p0, 1, 4,
                        ImGuiInputTextFlags_CharsHexadecimal);
      }

      ImGui::InputInt("Param 1 (hex)", &p1, 1, 4,
                      ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::InputInt("Param 2 (hex)", &p2, 1, 4,
                      ImGuiInputTextFlags_CharsHexadecimal);

      if (ImGui::Button("Apply")) {
        event.command.opcode = opcode;
        event.command.params[0] = static_cast<uint8_t>(p0 & 0xFF);
        event.command.params[1] = static_cast<uint8_t>(p1 & 0xFF);
        event.command.params[2] = static_cast<uint8_t>(p2 & 0xFF);
        if (on_edit_)
          on_edit_();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", DescribeCommand(event.command.opcode).c_str());
    } else {
      ImGui::TextDisabled("Unsupported edit type");
    }

    ImGui::EndPopup();
  }
}

void TrackerView::HandleNavigation() {
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
    if (ImGui::GetIO().KeyShift && selection_anchor_row_ == -1) {
      selection_anchor_row_ = selected_row_;
      selection_anchor_col_ = selected_col_;
    }
    if (!ImGui::GetIO().KeyShift && selection_anchor_row_ != -1) {
      selection_anchor_row_ = -1;
      selection_anchor_col_ = -1;
    }
    selected_row_ = std::max(0, selected_row_ - 1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
    if (ImGui::GetIO().KeyShift && selection_anchor_row_ == -1) {
      selection_anchor_row_ = selected_row_;
      selection_anchor_col_ = selected_col_;
    }
    if (!ImGui::GetIO().KeyShift && selection_anchor_row_ != -1) {
      selection_anchor_row_ = -1;
      selection_anchor_col_ = -1;
    }
    selected_row_++;  // Limit checked against song length later
  }
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
    if (ImGui::GetIO().KeyShift && selection_anchor_row_ == -1) {
      selection_anchor_row_ = selected_row_;
      selection_anchor_col_ = selected_col_;
    }
    if (!ImGui::GetIO().KeyShift && selection_anchor_row_ != -1) {
      selection_anchor_row_ = -1;
      selection_anchor_col_ = -1;
    }
    selected_col_ = std::max(1, selected_col_ - 1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
    if (ImGui::GetIO().KeyShift && selection_anchor_row_ == -1) {
      selection_anchor_row_ = selected_row_;
      selection_anchor_col_ = selected_col_;
    }
    if (!ImGui::GetIO().KeyShift && selection_anchor_row_ != -1) {
      selection_anchor_row_ = -1;
      selection_anchor_col_ = -1;
    }
    selected_col_ = std::min(8, selected_col_ + 1);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
    selected_row_ = std::max(0, selected_row_ - 16);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
    selected_row_ += 16;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    selected_row_ = 0;
  }
}

void TrackerView::HandleKeyboardInput(MusicSong* song) {
  if (!song || song->segments.empty())
    return;

  if (selected_row_ < 0 || selected_col_ < 1 || selected_col_ > 8)
    return;
  int ch = selected_col_ - 1;

  auto& track = song->segments[0].tracks[ch];
  int tick = selected_row_ * ticks_per_row_;

  // Helper to trigger undo
  auto TriggerEdit = [this]() {
    if (on_edit_)
      on_edit_();
  };

  // Handle Note Entry
  // Mapping: Z=C, S=C#, X=D, D=D#, C=E, V=F, G=F#, B=G, H=G#, N=A, J=A#, M=B
  // Octave +1: Q, 2, W, 3, E, R, 5, T, 6, Y, 7, U
  struct KeyNote {
    ImGuiKey key;
    int semitone;
    int octave_offset;
  };
  static const KeyNote key_map[] = {
      {ImGuiKey_Z, 0, 0}, {ImGuiKey_S, 1, 0},  {ImGuiKey_X, 2, 0},
      {ImGuiKey_D, 3, 0}, {ImGuiKey_C, 4, 0},  {ImGuiKey_V, 5, 0},
      {ImGuiKey_G, 6, 0}, {ImGuiKey_B, 7, 0},  {ImGuiKey_H, 8, 0},
      {ImGuiKey_N, 9, 0}, {ImGuiKey_J, 10, 0}, {ImGuiKey_M, 11, 0},
      {ImGuiKey_Q, 0, 1}, {ImGuiKey_2, 1, 1},  {ImGuiKey_W, 2, 1},
      {ImGuiKey_3, 3, 1}, {ImGuiKey_E, 4, 1},  {ImGuiKey_R, 5, 1},
      {ImGuiKey_5, 6, 1}, {ImGuiKey_T, 7, 1},  {ImGuiKey_6, 8, 1},
      {ImGuiKey_Y, 9, 1}, {ImGuiKey_7, 10, 1}, {ImGuiKey_U, 11, 1}};

  static int base_octave = 4;  // Default octave

  // Octave Control
  if (ImGui::IsKeyPressed(ImGuiKey_F1))
    base_octave = std::max(1, base_octave - 1);
  if (ImGui::IsKeyPressed(ImGuiKey_F2))
    base_octave = std::min(6, base_octave + 1);

  // Check note keys
  for (const auto& kn : key_map) {
    if (ImGui::IsKeyPressed(kn.key)) {
      TriggerEdit();

      int octave = base_octave + kn.octave_offset;
      if (octave > 6)
        octave = 6;

      uint8_t pitch = kNoteMinPitch + (octave - 1) * 12 + kn.semitone;

      // Check for existing event at this tick
      bool found = false;
      for (auto& evt : track.events) {
        if (evt.tick == tick) {
          if (evt.type == TrackEvent::Type::Note) {
            evt.note.pitch = pitch;  // Update pitch, keep duration
          } else {
            // Replace command/other with note
            evt = TrackEvent::MakeNote(tick, pitch, kDurationSixteenth);
          }
          found = true;
          break;
        }
      }

      if (!found) {
        track.InsertEvent(
            TrackEvent::MakeNote(tick, pitch, kDurationSixteenth));
      }

      // Auto-advance
      selected_row_++;
      return;
    }
  }

  // Deletion
  if (ImGui::IsKeyPressed(ImGuiKey_Delete) ||
      ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
    bool changed = false;

    // Handle range deletion if selected
    if (selection_anchor_row_ != -1) {
      // TODO: Implement range deletion logic
    } else {
      // Single cell deletion
      for (size_t i = 0; i < track.events.size(); ++i) {
        if (track.events[i].tick == tick) {
          TriggerEdit();
          track.RemoveEvent(i);
          changed = true;
          break;
        }
      }
    }

    if (changed) {
      selected_row_++;
    }
  }

  // Special keys
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    // Insert Key Off / Rest
    TriggerEdit();
    // TODO: Check existing and set to Rest (0xC9) or insert Rest
  }
}

void TrackerView::HandleEditShortcuts(MusicSong* song) {
  if (!song || song->segments.empty())
    return;
  if (selected_row_ < 0 || selected_col_ < 1 || selected_col_ > 8)
    return;

  int ch = selected_col_ - 1;
  auto& track = song->segments[0].tracks[ch];
  int tick = selected_row_ * ticks_per_row_;

  auto TriggerEdit = [this]() {
    if (on_edit_)
      on_edit_();
  };

  // Insert simple SetInstrument command (Cmd+I / Ctrl+I)
  if (ImGui::IsKeyPressed(ImGuiKey_I) &&
      (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper)) {
    TriggerEdit();
    // default instrument 0
    TrackEvent cmd = TrackEvent::MakeCommand(tick, 0xE0, 0x00);
    track.InsertEvent(cmd);
  }

  // Insert SetPan (Cmd+P)
  if (ImGui::IsKeyPressed(ImGuiKey_P) &&
      (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper)) {
    TriggerEdit();
    TrackEvent cmd = TrackEvent::MakeCommand(tick, 0xE1, 0x10);  // center pan
    track.InsertEvent(cmd);
  }

  // Insert Channel Volume (Cmd+V)
  if (ImGui::IsKeyPressed(ImGuiKey_V) &&
      (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper)) {
    TriggerEdit();
    TrackEvent cmd = TrackEvent::MakeCommand(tick, 0xED, 0x7F);
    track.InsertEvent(cmd);
  }

  // Quick duration tweak for the note at this tick (Alt+[ / Alt+])
  if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket) && ImGui::GetIO().KeyAlt) {
    for (auto& evt : track.events) {
      if (evt.tick == tick && evt.type == TrackEvent::Type::Note) {
        TriggerEdit();
        evt.note.duration = std::max<uint16_t>(1, evt.note.duration - 6);
        break;
      }
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightBracket) && ImGui::GetIO().KeyAlt) {
    for (auto& evt : track.events) {
      if (evt.tick == tick && evt.type == TrackEvent::Type::Note) {
        TriggerEdit();
        evt.note.duration = evt.note.duration + 6;
        break;
      }
    }
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
