#include "app/editor/music/piano_roll_view.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

namespace {
struct RollPalette {
  ImU32 white_key;
  ImU32 black_key;
  ImU32 grid_major;
  ImU32 grid_minor;
  ImU32 note;
  ImU32 note_hover;
  ImU32 background;
  ImU32 key_label;
};

RollPalette GetPalette() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  RollPalette p;
  p.white_key = ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.surface));
  p.black_key = ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.child_bg));
  auto grid = theme.separator;
  grid.alpha = 0.35f;
  p.grid_major = ImGui::GetColorU32(gui::ConvertColorToImVec4(grid));
  grid.alpha = 0.18f;
  p.grid_minor = ImGui::GetColorU32(gui::ConvertColorToImVec4(grid));
  p.note = ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.accent));
  auto hover = theme.accent;
  hover.alpha = 0.85f;
  p.note_hover = ImGui::GetColorU32(gui::ConvertColorToImVec4(hover));
  p.background =
      ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.editor_background));
  auto label = theme.text_secondary;
  label.alpha = 0.85f;
  p.key_label = ImGui::GetColorU32(gui::ConvertColorToImVec4(label));
  return p;
}

bool IsBlackKey(int semitone) {
  int s = semitone % 12;
  return (s == 1 || s == 3 || s == 6 || s == 8 || s == 10);
}
}  // namespace

void PianoRollView::Draw(MusicSong* song, const MusicBank* bank) {
  if (!song || song->segments.empty()) {
    ImGui::TextDisabled("No song loaded");
    return;
  }

  // Initialize channel colors if needed
  if (channel_colors_.empty()) {
    channel_colors_.resize(8);
    channel_colors_[0] = 0xFF5555FF; // Red
    channel_colors_[1] = 0xFF55FF55; // Green
    channel_colors_[2] = 0xFFFF5555; // Blue
    channel_colors_[3] = 0xFFFFFF55; // Yellow
    channel_colors_[4] = 0xFF55FFFF; // Cyan
    channel_colors_[5] = 0xFFFF55FF; // Magenta
    channel_colors_[6] = 0xFFFFAA55; // Orange
    channel_colors_[7] = 0xFFAAFF55; // Lime
  }

  const RollPalette palette = GetPalette();
  active_segment_index_ =
      std::clamp(active_segment_index_, 0,
                 static_cast<int>(song->segments.size()) - 1);
  active_channel_index_ = std::clamp(active_channel_index_, 0, 7);

  DrawToolbar(song, bank);
  
  // Main Layout Table
  if (ImGui::BeginTable("PianoRollLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Roll", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    // --- Left Column: Channel List ---
    ImGui::TableSetColumnIndex(0);
    
    ImGui::TextDisabled("CHANNELS");
    ImGui::Separator();
    
    for (int i = 0; i < 8; ++i) {
      ImGui::PushID(i);
      
      // Visibility toggle
      bool visible = channel_visible_[i];
      if (ImGui::Checkbox("##Vis", &visible)) {
        channel_visible_[i] = visible;
      }
      ImGui::SameLine();
      
      // Color picker
      ImVec4 col_v4 = ImGui::ColorConvertU32ToFloat4(channel_colors_[i]);
      if (ImGui::ColorEdit4("##Col", (float*)&col_v4, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf)) {
        channel_colors_[i] = ImGui::ColorConvertFloat4ToU32(col_v4);
      }
      ImGui::SameLine();
      
      // Selection
      std::string label = absl::StrFormat("Ch %d", i + 1);
      bool is_active = (active_channel_index_ == i);
      if (ImGui::Selectable(label.c_str(), is_active)) {
        active_channel_index_ = i;
        // Ensure active channel is visible
        channel_visible_[i] = true;
      }
      
      ImGui::PopID();
    }

    // --- Right Column: Piano Roll ---
    ImGui::TableSetColumnIndex(1);
    
    float total_height = (kNoteMaxPitch - kNoteMinPitch + 1) * key_height_;
    const auto& segment = song->segments[active_segment_index_];

    hovered_event_index_ = -1;
    hovered_channel_index_ = -1;
    hovered_segment_index_ = -1;

    if (ImGui::BeginChild("PianoRollCanvas", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
      ImVec2 canvas_size = ImGui::GetContentRegionAvail();
      
      float scroll_x = ImGui::GetScrollX();
      float scroll_y = ImGui::GetScrollY();
      
      uint32_t duration = segment.GetDuration();
      if (duration == 0) duration = 1000; // fallback
      duration += 48;  // padding for edits
      float content_width = duration * pixels_per_tick_;
      
      ImGui::Dummy(ImVec2(content_width + key_width_, total_height));
      
      ImVec2 key_origin = ImVec2(canvas_pos.x - scroll_x, canvas_pos.y - scroll_y);
      ImVec2 grid_origin = ImVec2(key_origin.x + key_width_, key_origin.y);
      
      int start_key_idx = (int)(scroll_y / key_height_);
      int visible_keys = (int)(canvas_size.y / key_height_) + 1;
      int num_keys = kNoteMaxPitch - kNoteMinPitch + 1;
      
      // key lane background
      draw_list->AddRectFilled(key_origin,
                               ImVec2(key_origin.x + key_width_, key_origin.y + total_height),
                               palette.background);
      draw_list->PushClipRect(ImVec2(canvas_pos.x, canvas_pos.y),
                              ImVec2(canvas_pos.x + key_width_, canvas_pos.y + total_height),
                              true);
      
      for (int i = start_key_idx; i < std::min(num_keys, start_key_idx + visible_keys); ++i) {
        float y = total_height - (i + 1) * key_height_;
        ImVec2 k_min = ImVec2(key_origin.x, key_origin.y + y);
        ImVec2 k_max = ImVec2(key_origin.x + key_width_, key_origin.y + y + key_height_);
        
        int note_val = kNoteMinPitch + i;
        bool is_black = IsBlackKey(note_val);
        
        draw_list->AddRectFilled(k_min, k_max, is_black ? palette.black_key : palette.white_key);
        draw_list->AddRect(k_min, k_max, palette.grid_minor);
        
        if (note_val % 12 == 0) {
          Note n; n.pitch = note_val;
          draw_list->AddText(ImVec2(k_min.x + 4, k_min.y), palette.key_label, n.GetNoteName().c_str());
        }
      }
      draw_list->PopClipRect();
      
      int ticks_per_beat = 72;
      int start_tick = (int)(scroll_x / pixels_per_tick_);
      int visible_ticks = (int)(canvas_size.x / pixels_per_tick_) + 2;
      
      for (int t = start_tick; t < start_tick + visible_ticks; ++t) {
          if (t % ticks_per_beat == 0) {
              float x = grid_origin.x + t * pixels_per_tick_;
              draw_list->AddLine(ImVec2(x, grid_origin.y), ImVec2(x, grid_origin.y + total_height), palette.grid_major);
          }
      }
      
      for (int i = start_key_idx; i < std::min(num_keys, start_key_idx + visible_keys); ++i) {
          float y = total_height - (i + 1) * key_height_;
          draw_list->AddLine(ImVec2(grid_origin.x, key_origin.y + y), 
                             ImVec2(grid_origin.x + content_width, key_origin.y + y), palette.grid_minor);
      }

      // Render channels
      // Pass 1: Inactive channels (ghost notes)
      for (int ch = 0; ch < 8; ++ch) {
        if (ch == active_channel_index_ || !channel_visible_[ch]) continue;
        
        const auto& track = segment.tracks[ch];
        ImU32 base_color = channel_colors_[ch];
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(base_color);
        c.w = 0.3f; // Reduced opacity
        ImU32 ghost_color = ImGui::ColorConvertFloat4ToU32(c);

        for (const auto& event : track.events) {
          if (event.type == TrackEvent::Type::Note) {
            int key_idx = event.note.pitch - kNoteMinPitch;
            float y = total_height - (key_idx + 1) * key_height_;
            float x = event.tick * pixels_per_tick_;
            float w = event.note.duration * pixels_per_tick_;

            ImVec2 p_min = ImVec2(grid_origin.x + x, grid_origin.y + y);
            ImVec2 p_max = ImVec2(p_min.x + w, p_min.y + key_height_);
            
            // Simple render for ghost notes
            draw_list->AddRectFilled(p_min, p_max, ghost_color, 3.0f);
          }
        }
      }

      // Pass 2: Active channel (interactive)
      if (channel_visible_[active_channel_index_]) {
        const auto& track = segment.tracks[active_channel_index_];
        ImU32 active_color = channel_colors_[active_channel_index_];
        ImU32 hover_color = palette.note_hover;

        for (size_t idx = 0; idx < track.events.size(); ++idx) {
          const auto& event = track.events[idx];
          if (event.type == TrackEvent::Type::Note) {
            int key_idx = event.note.pitch - kNoteMinPitch;
            float y = total_height - (key_idx + 1) * key_height_;
            float x = event.tick * pixels_per_tick_;
            float w = event.note.duration * pixels_per_tick_;

            ImVec2 p_min = ImVec2(grid_origin.x + x, grid_origin.y + y);
            ImVec2 p_max = ImVec2(p_min.x + w, p_min.y + key_height_);
            bool hovered = ImGui::IsMouseHoveringRect(p_min, p_max);
            ImU32 color = hovered ? hover_color : active_color;

            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, color, 3.0f);
            ImGui::GetWindowDrawList()->AddRect(p_min, p_max, palette.grid_major, 3.0f);

            if (hovered) {
              hovered_event_index_ = static_cast<int>(idx);
              hovered_channel_index_ = active_channel_index_;
              hovered_segment_index_ = active_segment_index_;
              ImGui::SetTooltip("Ch %d\nPitch: %s\nTick: %d\nDur: %d",
                                active_channel_index_ + 1,
                                event.note.GetNoteName().c_str(), event.tick,
                                event.note.duration);
            }
          }
        }
      }
      
      // Crosshair under mouse for alignment
      if (ImGui::IsWindowHovered()) {
        ImVec2 mp = ImGui::GetMousePos();
        float rel_x = mp.x - grid_origin.x;
        float rel_y = mp.y - grid_origin.y;
        if (rel_x >= 0 && rel_y >= 0 && rel_y <= total_height) {
          draw_list->AddLine(ImVec2(mp.x, grid_origin.y),
                             ImVec2(mp.x, grid_origin.y + total_height),
                             palette.grid_minor);
          draw_list->AddLine(ImVec2(grid_origin.x, mp.y),
                             ImVec2(grid_origin.x + content_width, mp.y),
                             palette.grid_minor);
        }
      }

      HandleMouseInput(song, active_channel_index_, active_segment_index_, grid_origin, ImVec2(content_width, total_height));
    }
    ImGui::EndChild();
    
    ImGui::EndTable();
  }

  // Context menu for notes
  if (ImGui::BeginPopup("PianoRollNoteContext")) {
    if (song && context_target_.segment >= 0 && context_target_.channel >= 0 &&
        context_target_.event_index >= 0 &&
        context_target_.segment < (int)song->segments.size()) {
      auto& track = song->segments[context_target_.segment].tracks[context_target_.channel];
      if (context_target_.event_index < (int)track.events.size()) {
        auto& evt = track.events[context_target_.event_index];
        if (evt.type == TrackEvent::Type::Note) {
          ImGui::Text("Note %s", evt.note.GetNoteName().c_str());
          ImGui::Separator();
          if (ImGui::MenuItem("Delete")) {
            track.RemoveEvent(context_target_.event_index);
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Set Quarter")) {
            evt.note.duration = kDurationQuarter;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Set Eighth")) {
            evt.note.duration = kDurationEighth;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Duplicate")) {
            TrackEvent copy = evt;
            copy.tick += evt.note.duration;
            track.InsertEvent(copy);
            if (on_edit_) on_edit_();
          }
        }
      }
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("PianoRollEmptyContext")) {
    if (song && empty_context_.segment >= 0 &&
        empty_context_.segment < (int)song->segments.size() &&
        empty_context_.channel >= 0 && empty_context_.channel < 8 &&
        empty_context_.tick >= 0) {
      ImGui::Text("Add note");
      ImGui::Separator();
      if (ImGui::MenuItem("Insert default note")) {
        auto& t =
            song->segments[empty_context_.segment].tracks[empty_context_.channel];
        TrackEvent evt = TrackEvent::MakeNote(empty_context_.tick,
                                              empty_context_.pitch,
                                              snap_enabled_ ? snap_ticks_ : kDurationQuarter);
        t.InsertEvent(evt);
        if (on_edit_) on_edit_();
        if (on_note_preview_) on_note_preview_(evt, empty_context_.segment,
                                               empty_context_.channel);
      }
      if (ImGui::MenuItem("Insert quarter")) {
        auto& t =
            song->segments[empty_context_.segment].tracks[empty_context_.channel];
        TrackEvent evt = TrackEvent::MakeNote(empty_context_.tick,
                                              empty_context_.pitch,
                                              kDurationQuarter);
        t.InsertEvent(evt);
        if (on_edit_) on_edit_();
        if (on_note_preview_) on_note_preview_(evt, empty_context_.segment,
                                               empty_context_.channel);
      }
    }
    ImGui::EndPopup();
  }
}

void PianoRollView::DrawToolbar(const MusicSong* song, const MusicBank* bank) {
  ImGui::Text("Piano Roll");
  if (song) {
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", song->name.c_str());
  }

  // Segment selector
  if (song) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    std::string seg_label = absl::StrFormat("Segment %d/%d##SegSelect",
                                            active_segment_index_ + 1,
                                            (int)song->segments.size());
    if (ImGui::BeginCombo("##SegmentSelect", seg_label.c_str())) {
      for (int i = 0; i < (int)song->segments.size(); ++i) {
        bool is_selected = (i == active_segment_index_);
        std::string label = absl::StrFormat("Segment %d", i + 1);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          active_segment_index_ = i;
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }

  // Instrument Selector (New)
  if (bank) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    const auto* inst = bank->GetInstrument(preview_instrument_index_);
    std::string preview = inst ? absl::StrFormat("%02X: %s", preview_instrument_index_, inst->name.c_str()) 
                               : absl::StrFormat("%02X", preview_instrument_index_);
    if (ImGui::BeginCombo("##InstSelect", preview.c_str())) {
      for (size_t i = 0; i < bank->GetInstrumentCount(); ++i) {
        const auto* item = bank->GetInstrument(i);
        bool is_selected = (static_cast<int>(i) == preview_instrument_index_);
        if (ImGui::Selectable(absl::StrFormat("%02X: %s", i, item->name.c_str()).c_str(), is_selected)) {
          preview_instrument_index_ = static_cast<int>(i);
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Select instrument for new notes / preview");
    }
  }

  if (song) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play Segment")) {
      if (on_segment_preview_) {
        on_segment_preview_(*song, active_segment_index_);
      }
    }
  }

  ImGui::SameLine();
  ImGui::PushItemWidth(110);
  ImGui::SliderFloat("Zoom X", &pixels_per_tick_, 0.5f, 10.0f, "%.1f px/tick");
  ImGui::SameLine();
  ImGui::SliderFloat("Zoom Y", &key_height_, 6.0f, 20.0f, "%.0f px/key");
  ImGui::PopItemWidth();

  ImGui::SameLine();
  ImGui::Checkbox("Snap", &snap_enabled_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  const char* snap_labels[] = {"1/4", "1/8", "1/16", "Off"};
  int snap_idx = 2;
  if (snap_ticks_ == kDurationQuarter) snap_idx = 0;
  else if (snap_ticks_ == kDurationEighth) snap_idx = 1;
  else if (!snap_enabled_) snap_idx = 3;

  if (ImGui::Combo("##SnapValue", &snap_idx, snap_labels, IM_ARRAYSIZE(snap_labels))) {
    if (snap_idx == 3) {
      snap_enabled_ = false;
    } else {
      snap_enabled_ = true;
      snap_ticks_ = (snap_idx == 0) ? kDurationQuarter
                  : (snap_idx == 1) ? kDurationEighth
                                    : kDurationSixteenth;
    }
  }
}

void PianoRollView::HandleMouseInput(MusicSong* song, int active_channel, int active_segment,
                                     const ImVec2& grid_origin, const ImVec2& grid_size) {
  if (!song) return;
  if (active_segment < 0 || active_segment >= static_cast<int>(song->segments.size())) return;
  auto& track = song->segments[active_segment].tracks[active_channel];

  ImVec2 mouse_pos = ImGui::GetMousePos();
  float content_width = grid_size.x;
  float total_height = grid_size.y;
  int num_keys = static_cast<int>(total_height / key_height_);

  // Mouse to grid conversion
  float rel_x = mouse_pos.x - grid_origin.x;
  float rel_y = mouse_pos.y - grid_origin.y;
  int tick = static_cast<int>(std::lround(rel_x / pixels_per_tick_));
  int pitch_idx = static_cast<int>(std::lround(rel_y / key_height_));
  uint8_t pitch = static_cast<uint8_t>(kNoteMaxPitch - pitch_idx);

  auto snap_tick = [this](int t) {
    if (!snap_enabled_) return t;
    return (t / snap_ticks_) * snap_ticks_;
  };

  // Drag release
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && dragging_event_index_ != -1) {
    if (drag_moved_ && on_edit_) {
      on_edit_();
      drag_moved_ = false;
    }
    dragging_event_index_ = -1;
    drag_mode_ = 0;
  }

  // Handle drag update
  if (dragging_event_index_ != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    if (drag_segment_index_ < 0 || drag_segment_index_ >= static_cast<int>(song->segments.size())) return;
    auto& drag_track = song->segments[drag_segment_index_].tracks[drag_channel_index_];
    if (dragging_event_index_ >= static_cast<int>(drag_track.events.size())) return;

    int delta_ticks = static_cast<int>(
        std::lround((mouse_pos.x - drag_start_mouse_.x) / pixels_per_tick_));
    int delta_pitch = static_cast<int>(
        std::lround((drag_start_mouse_.y - mouse_pos.y) / key_height_));

    TrackEvent updated = drag_original_event_;
    if (drag_mode_ == 1) {  // Move
      updated.tick = snap_tick(std::max(0, drag_original_event_.tick + delta_ticks));
      int new_pitch = drag_original_event_.note.pitch + delta_pitch;
      new_pitch = std::clamp(new_pitch,
                             static_cast<int>(kNoteMinPitch),
                             static_cast<int>(kNoteMaxPitch));
      updated.note.pitch = static_cast<uint8_t>(new_pitch);
    } else if (drag_mode_ == 2) {  // Resize left
      int new_tick = snap_tick(std::max(0, drag_original_event_.tick + delta_ticks));
      int new_duration = drag_original_event_.note.duration - delta_ticks;
      updated.tick = new_tick;
      updated.note.duration = std::max(1, new_duration);
    } else if (drag_mode_ == 3) {  // Resize right
      int new_duration = drag_original_event_.note.duration + delta_ticks;
      updated.note.duration = std::max(1, new_duration);
    }

    drag_track.RemoveEvent(dragging_event_index_);
    drag_track.InsertEvent(updated);

    // Find updated index
    for (size_t i = 0; i < drag_track.events.size(); ++i) {
      const auto& evt = drag_track.events[i];
      if (evt.type == TrackEvent::Type::Note && evt.tick == updated.tick &&
          evt.note.pitch == updated.note.pitch &&
          evt.note.duration == updated.note.duration) {
        dragging_event_index_ = static_cast<int>(i);
        break;
      }
    }

    drag_moved_ = true;
    return;
  }

  // Left click handling (selection / add note)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      ImGui::IsWindowHovered() && tick >= 0 &&
      pitch_idx >= 0 && pitch_idx <= (kNoteMaxPitch - kNoteMinPitch)) {
    // If clicked on existing note, begin drag
    if (hovered_event_index_ >= 0 &&
        hovered_segment_index_ == active_segment &&
        hovered_channel_index_ == active_channel) {
      dragging_event_index_ = hovered_event_index_;
      drag_segment_index_ = active_segment;
      drag_channel_index_ = active_channel;
      drag_original_event_ = track.events[dragging_event_index_];
      drag_start_mouse_ = ImGui::GetMousePos();
      drag_mode_ = 1;

      // Detect edge hover for resize
      float left_x = drag_original_event_.tick * pixels_per_tick_;
      float right_x = (drag_original_event_.tick + drag_original_event_.note.duration) * pixels_per_tick_;
      float dist_left = std::fabs((grid_origin.x + left_x) - mouse_pos.x);
      float dist_right = std::fabs((grid_origin.x + right_x) - mouse_pos.x);
      const float edge_threshold = 6.0f;
      if (dist_left < edge_threshold) drag_mode_ = 2;
      else if (dist_right < edge_threshold) drag_mode_ = 3;

      if (on_note_preview_) {
        on_note_preview_(drag_original_event_, active_segment, active_channel);
      }
      return;
    }

    // Otherwise add a note
    TrackEvent new_note = TrackEvent::MakeNote(tick, pitch, kDurationQuarter);
    track.InsertEvent(new_note);
    if (on_edit_) on_edit_();
    if (on_note_preview_) {
      on_note_preview_(new_note, active_segment, active_channel);
    }
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
