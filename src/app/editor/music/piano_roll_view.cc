#include "app/editor/music/piano_roll_view.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "zelda3/music/song_data.h"

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
  ImU32 note_shadow;
  ImU32 background;
  ImU32 key_label;
  ImU32 beat_marker;
  ImU32 octave_line;
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
  // Shadow for notes - darker version of background
  auto shadow = theme.border_shadow;
  shadow.alpha = 0.4f;
  p.note_shadow = ImGui::GetColorU32(gui::ConvertColorToImVec4(shadow));
  p.background =
      ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.editor_background));
  auto label = theme.text_secondary;
  label.alpha = 0.85f;
  p.key_label = ImGui::GetColorU32(gui::ConvertColorToImVec4(label));
  // Beat markers - slightly brighter than grid
  auto beat = theme.accent;
  beat.alpha = 0.25f;
  p.beat_marker = ImGui::GetColorU32(gui::ConvertColorToImVec4(beat));
  // Octave divider lines
  auto octave = theme.separator;
  octave.alpha = 0.5f;
  p.octave_line = ImGui::GetColorU32(gui::ConvertColorToImVec4(octave));
  return p;
}

bool IsBlackKey(int semitone) {
  int s = semitone % 12;
  return (s == 1 || s == 3 || s == 6 || s == 8 || s == 10);
}

int CountNotesInTrack(const MusicTrack& track) {
  int count = 0;
  for (const auto& evt : track.events) {
    if (evt.type == TrackEvent::Type::Note) count++;
  }
  return count;
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
    channel_colors_[0] = 0xFFFF6B6B; // Coral Red
    channel_colors_[1] = 0xFF4ECDC4; // Teal
    channel_colors_[2] = 0xFF45B7D1; // Sky Blue
    channel_colors_[3] = 0xFFF7DC6F; // Soft Yellow
    channel_colors_[4] = 0xFFBB8FCE; // Lavender
    channel_colors_[5] = 0xFF82E0AA; // Mint Green
    channel_colors_[6] = 0xFFF8B500; // Amber
    channel_colors_[7] = 0xFFE59866; // Peach
  }

  const RollPalette palette = GetPalette();
  active_segment_index_ =
      std::clamp(active_segment_index_, 0,
                 static_cast<int>(song->segments.size()) - 1);
  active_channel_index_ = std::clamp(active_channel_index_, 0, 7);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
  if (ImGui::BeginChild("##PianoRollToolbar", ImVec2(0, kToolbarHeight), 
                        ImGuiChildFlags_AlwaysUseWindowPadding, 
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    DrawToolbar(song, bank);
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();

  ImGui::Separator();

  ImGuiStyle& style = ImGui::GetStyle();
  float available_height = ImGui::GetContentRegionAvail().y;
  float reserved_for_status = kStatusBarHeight + style.ItemSpacing.y;
  float main_height = std::max(0.0f, available_height - reserved_for_status);

  if (ImGui::BeginChild("PianoRollMain", ImVec2(0, main_height), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    // === MAIN CONTENT ===
    const float layout_height = ImGui::GetContentRegionAvail().y;
    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX |
        ImGuiTableFlags_NoPadInnerX;
    if (ImGui::BeginTable("PianoRollLayout", 2, table_flags,
                          ImVec2(-FLT_MIN, layout_height))) {
      ImGui::TableSetupColumn("Channels",
                              ImGuiTableColumnFlags_NoHide |
                              ImGuiTableColumnFlags_NoReorder,
                              kChannelListWidth);
      ImGui::TableSetupColumn("Roll", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow(ImGuiTableRowFlags_None, layout_height);

      // --- Left Column: Channel List ---
      ImGui::TableSetColumnIndex(0);
      const ImGuiChildFlags channel_child_flags =
          ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding |
          ImGuiChildFlags_ResizeY;
      if (ImGui::BeginChild("PianoRollChannelList", ImVec2(0, 0), channel_child_flags,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawChannelList(song);
      }
      ImGui::EndChild();

      // --- Right Column: Piano Roll ---
      ImGui::TableSetColumnIndex(1);

      float total_height = (kNoteMaxPitch - kNoteMinPitch + 1) * key_height_;
      const auto& segment = song->segments[active_segment_index_];

      hovered_event_index_ = -1;
      hovered_channel_index_ = -1;
      hovered_segment_index_ = -1;

      RollCanvasContext canvas_ctx;
      bool canvas_open = BeginRollCanvas("PianoRollCanvas", &canvas_ctx);
      if (canvas_open) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = canvas_ctx.canvas_pos;
        ImVec2 canvas_size = canvas_ctx.canvas_size;
        float scroll_x = canvas_ctx.scroll_x;
        float scroll_y = canvas_ctx.scroll_y;

        ImVec2 key_origin(canvas_pos.x, canvas_pos.y - scroll_y);
        ImVec2 grid_origin(key_origin.x + key_width_ - scroll_x, key_origin.y);

        // Mouse wheel zoom support (Ctrl+Wheel = horizontal, Shift+Wheel = vertical)
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
          float wheel = ImGui::GetIO().MouseWheel;
          if (wheel != 0.0f) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;
            if (ctrl) {
              // Horizontal zoom
              float old_ppt = pixels_per_tick_;
              pixels_per_tick_ = std::clamp(pixels_per_tick_ + wheel * 0.5f, 0.5f, 10.0f);
              // Adjust scroll to keep mouse position stable
              ImVec2 mouse = ImGui::GetMousePos();
              float rel_x = mouse.x - canvas_pos.x + scroll_x;
              float new_scroll_x =
                  rel_x * (pixels_per_tick_ / old_ppt) - (mouse.x - canvas_pos.x);
              ImGui::SetScrollX(std::max(0.0f, new_scroll_x));
            } else if (shift) {
              // Vertical zoom
              float old_kh = key_height_;
              key_height_ = std::clamp(key_height_ + wheel * 2.0f, 6.0f, 20.0f);
              // Adjust scroll to keep mouse position stable
              ImVec2 mouse = ImGui::GetMousePos();
              float rel_y = mouse.y - canvas_pos.y + scroll_y;
              float new_scroll_y =
                  rel_y * (key_height_ / old_kh) - (mouse.y - canvas_pos.y);
              ImGui::SetScrollY(std::max(0.0f, new_scroll_y));
            }
            // Note: Without modifiers, let ImGui handle normal scrolling
          }
        }

        // Recalculate total_height after potential zoom changes
        total_height = (kNoteMaxPitch - kNoteMinPitch + 1) * key_height_;

        uint32_t duration = segment.GetDuration();
        if (duration == 0) duration = 1000; // fallback
        duration += 48;  // padding for edits
        float content_width = duration * pixels_per_tick_;
        
        ImGui::Dummy(ImVec2(key_width_ + content_width, total_height));
        
        int start_key_idx = static_cast<int>(scroll_y / key_height_);
        int num_keys = kNoteMaxPitch - kNoteMinPitch + 1;
        // Only calculate visible keys based on actual content height, not canvas size
        float visible_content_height =
            std::min(canvas_size.y, std::max(0.0f, total_height - scroll_y));
        int visible_keys =
            std::min(static_cast<int>(visible_content_height / key_height_) + 2,
                     num_keys - start_key_idx);
        
        // Clip all drawing to the actual content bounds
        float clip_bottom =
            std::min(canvas_pos.y + canvas_size.y, key_origin.y + total_height);
        
        // Key lane background - only fill actual content area
        draw_list->AddRectFilled(ImVec2(key_origin.x, std::max(key_origin.y, canvas_pos.y)),
                                 ImVec2(key_origin.x + key_width_, clip_bottom),
                                 palette.background);
        draw_list->PushClipRect(ImVec2(canvas_pos.x, canvas_pos.y),
                                ImVec2(canvas_pos.x + key_width_, clip_bottom),
                                true);
        
        // Draw piano keys
        for (int i = start_key_idx; i < std::min(num_keys, start_key_idx + visible_keys); ++i) {
          float y = total_height - (i + 1) * key_height_;
          ImVec2 k_min = ImVec2(key_origin.x, key_origin.y + y);
          ImVec2 k_max = ImVec2(key_origin.x + key_width_, key_origin.y + y + key_height_);
          
          int note_val = kNoteMinPitch + i;
          bool is_black = IsBlackKey(note_val);
          
          draw_list->AddRectFilled(k_min, k_max, is_black ? palette.black_key : palette.white_key);
          draw_list->AddRect(k_min, k_max, palette.grid_minor);
          
          // Show labels for all white keys (black keys are too narrow)
          if (!is_black) {
            Note n; n.pitch = static_cast<uint8_t>(note_val);
            draw_list->AddText(ImVec2(k_min.x + 4, k_min.y + 1), palette.key_label, n.GetNoteName().c_str());
          }
        }
        draw_list->PopClipRect();
        
        // Push clip rect for the entire grid area (notes + grid lines)
        draw_list->PushClipRect(
            ImVec2(canvas_pos.x + key_width_, canvas_pos.y),
            ImVec2(canvas_pos.x + canvas_size.x, clip_bottom),
                                true);
        
        // Draw grid and beat markers
        int ticks_per_beat = 72;
        int ticks_per_bar = ticks_per_beat * 4; // 4 beats per bar
        int start_tick = std::max(0, static_cast<int>(scroll_x / pixels_per_tick_) - 1);
        int visible_ticks =
            static_cast<int>(canvas_size.x / pixels_per_tick_) + 2;
        
        // Beat markers (major grid lines) - clipped to content height
        float grid_clip_bottom = std::min(grid_origin.y + total_height, clip_bottom);
        for (int t = start_tick; t < start_tick + visible_ticks; ++t) {
          if (t % ticks_per_beat == 0) {
            float x = grid_origin.x + t * pixels_per_tick_;
            bool is_bar = (t % ticks_per_bar == 0);
            draw_list->AddLine(ImVec2(x, std::max(grid_origin.y, canvas_pos.y)), 
                               ImVec2(x, grid_clip_bottom), 
                               is_bar ? palette.beat_marker : palette.grid_major,
                               is_bar ? 2.0f : 1.0f);
            
            // Draw bar/beat number at top
            if (is_bar && x > grid_origin.x) {
              int bar_num = t / ticks_per_bar + 1;
              std::string bar_label = absl::StrFormat("%d", bar_num);
              draw_list->AddText(ImVec2(x + 2, std::max(grid_origin.y, canvas_pos.y) + 2), 
                                 palette.key_label, bar_label.c_str());
            }
          }
        }
        
        // Horizontal key lines with octave emphasis
        for (int i = start_key_idx; i < start_key_idx + visible_keys && i < num_keys; ++i) {
          float y = total_height - (i + 1) * key_height_;
          float line_y = key_origin.y + y;
          if (line_y < canvas_pos.y || line_y > clip_bottom) continue;
          
          int note_val = kNoteMinPitch + i;
          bool is_octave = (note_val % 12 == 0);
          draw_list->AddLine(ImVec2(grid_origin.x, line_y), 
                             ImVec2(grid_origin.x + content_width, line_y), 
                             is_octave ? palette.octave_line : palette.grid_minor,
                             is_octave ? 1.5f : 1.0f);
        }
        
        // Push clip rect for note rendering (same as grid area)
        draw_list->PushClipRect(
            ImVec2(canvas_pos.x + key_width_, canvas_pos.y),
            ImVec2(canvas_pos.x + canvas_size.x, clip_bottom),
                                true);

        // Check for any solo'd channels
        bool any_solo = false;
        for (int ch = 0; ch < 8; ++ch) {
          if (channel_solo_[ch]) { any_solo = true; break; }
        }

        // Render channels
        // Pass 1: Inactive channels (ghost notes)
        int end_tick = start_tick + visible_ticks;
        
        for (int ch = 0; ch < 8; ++ch) {
          if (ch == active_channel_index_ || !channel_visible_[ch]) continue;
          if (channel_muted_[ch]) continue;
          if (any_solo && !channel_solo_[ch]) continue;
          
          const auto& track = segment.tracks[ch];
          ImU32 base_color = channel_colors_[ch];
          ImVec4 c = ImGui::ColorConvertU32ToFloat4(base_color);
          c.w = 0.3f; // Reduced opacity for ghost notes
          ImU32 ghost_color = ImGui::ColorConvertFloat4ToU32(c);

          // Optimization: Only draw visible notes
          auto it = std::lower_bound(track.events.begin(), track.events.end(), start_tick,
              [](const TrackEvent& e, int tick) { return e.tick + e.note.duration < tick; });

          for (; it != track.events.end(); ++it) {
            const auto& event = *it;
            if (event.tick > end_tick) break; // Stop if we're past the visible area

            if (event.type == TrackEvent::Type::Note) {
              int key_idx = event.note.pitch - kNoteMinPitch;
              // Simple culling for vertical visibility
              if (key_idx < start_key_idx || key_idx > start_key_idx + visible_keys) continue;

              float y = total_height - (key_idx + 1) * key_height_;
              float x = event.tick * pixels_per_tick_;
              float w = std::max(2.0f, event.note.duration * pixels_per_tick_);

              ImVec2 p_min = ImVec2(grid_origin.x + x, grid_origin.y + y + 1);
              ImVec2 p_max = ImVec2(p_min.x + w, p_min.y + key_height_ - 2);
              
              draw_list->AddRectFilled(p_min, p_max, ghost_color, 2.0f);
            }
          }
        }

        // Pass 2: Active channel (interactive)
        if (channel_visible_[active_channel_index_] && 
            !channel_muted_[active_channel_index_] &&
            (!any_solo || channel_solo_[active_channel_index_])) {
          const auto& track = segment.tracks[active_channel_index_];
          ImU32 active_color = channel_colors_[active_channel_index_];
          ImU32 hover_color = palette.note_hover;

          // Optimization: Only draw visible notes
          auto it = std::lower_bound(track.events.begin(), track.events.end(), start_tick,
              [](const TrackEvent& e, int tick) { return e.tick + e.note.duration < tick; });

          for (size_t idx = std::distance(track.events.begin(), it); idx < track.events.size(); ++idx) {
            const auto& event = track.events[idx];
            if (event.tick > end_tick) break; // Stop if we're past the visible area

            if (event.type == TrackEvent::Type::Note) {
              int key_idx = event.note.pitch - kNoteMinPitch;
              // Simple culling for vertical visibility
              if (key_idx < start_key_idx || key_idx > start_key_idx + visible_keys) continue;

              float y = total_height - (key_idx + 1) * key_height_;
              float x = event.tick * pixels_per_tick_;
              float w = std::max(4.0f, event.note.duration * pixels_per_tick_);

              ImVec2 p_min = ImVec2(grid_origin.x + x, grid_origin.y + y + 1);
              ImVec2 p_max = ImVec2(p_min.x + w, p_min.y + key_height_ - 2);
              bool hovered = ImGui::IsMouseHoveringRect(p_min, p_max);
              ImU32 color = hovered ? hover_color : active_color;

              // Draw shadow
              ImVec2 shadow_offset(2, 2);
              draw_list->AddRectFilled(
                  ImVec2(p_min.x + shadow_offset.x, p_min.y + shadow_offset.y),
                  ImVec2(p_max.x + shadow_offset.x, p_max.y + shadow_offset.y),
                  palette.note_shadow, 3.0f);
              
              // Draw note
              draw_list->AddRectFilled(p_min, p_max, color, 3.0f);
              draw_list->AddRect(p_min, p_max, palette.grid_major, 3.0f);
              
              // Draw resize handles for larger notes
              if (w > 10) {
                float handle_w = 4.0f;
                // Left handle indicator
                draw_list->AddRectFilled(
                    ImVec2(p_min.x, p_min.y), 
                    ImVec2(p_min.x + handle_w, p_max.y),
                    IM_COL32(255, 255, 255, 40), 2.0f);
                // Right handle indicator
                draw_list->AddRectFilled(
                    ImVec2(p_max.x - handle_w, p_min.y), 
                    ImVec2(p_max.x, p_max.y),
                    IM_COL32(255, 255, 255, 40), 2.0f);
              }

              if (hovered) {
                hovered_event_index_ = static_cast<int>(idx);
                hovered_channel_index_ = active_channel_index_;
                hovered_segment_index_ = active_segment_index_;
                ImGui::SetTooltip("Ch %d | %s\nTick: %d | Dur: %d",
                                  active_channel_index_ + 1,
                                  event.note.GetNoteName().c_str(), 
                                  event.tick,
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
          
          // Update status bar info
          if (rel_x >= 0 && rel_y >= 0 && rel_y <= total_height) {
            status_tick_ = static_cast<int>(rel_x / pixels_per_tick_);
            int pitch_idx = static_cast<int>(rel_y / key_height_);
            status_pitch_ = kNoteMaxPitch - pitch_idx;
            Note temp_note; temp_note.pitch = static_cast<uint8_t>(status_pitch_);
            status_note_name_ = temp_note.GetNoteName();
            
            // Draw crosshair - clipped to content area
            ImU32 crosshair_color = IM_COL32(255, 255, 255, 60);
            float crosshair_top = std::max(grid_origin.y, canvas_pos.y);
            float crosshair_bottom = std::min(grid_origin.y + total_height, clip_bottom);
            draw_list->AddLine(ImVec2(mp.x, crosshair_top),
                               ImVec2(mp.x, crosshair_bottom),
                               crosshair_color);
            draw_list->AddLine(ImVec2(grid_origin.x, mp.y),
                               ImVec2(grid_origin.x + content_width, mp.y),
                               crosshair_color);
          } else {
            status_tick_ = -1;
            status_pitch_ = -1;
          }
        } else {
          status_tick_ = -1;
          status_pitch_ = -1;
        }
        
        draw_list->PopClipRect();  // End note rendering clip

        HandleMouseInput(song, active_channel_index_, active_segment_index_, grid_origin, ImVec2(content_width, total_height));

        // Draw playback cursor (on top of everything) - show even when paused
        if (is_playing_ || is_paused_) {
          // Calculate segment start tick for current segment
          uint32_t segment_start = 0;
          for (int i = 0; i < active_segment_index_; ++i) {
            segment_start += song->segments[i].GetDuration();
          }
          DrawPlaybackCursor(draw_list, grid_origin, total_height, segment_start);

          // Auto-scroll to follow playback if enabled (only when actively playing)
          if (is_playing_ && !is_paused_ && follow_playback_ &&
              playback_tick_ >= segment_start) {
            uint32_t local_tick = playback_tick_ - segment_start;
            float cursor_x = local_tick * pixels_per_tick_;
            float visible_width = canvas_size.x - key_width_;
            if (cursor_x > scroll_x + visible_width - 100 ||
                cursor_x < scroll_x + 50) {
              ImGui::SetScrollX(std::max(0.0f, cursor_x - visible_width / 3));
            }
          }
        }
      }
      EndRollCanvas();
    
    ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  // === STATUS BAR (Fixed Height) ===
  ImGui::Separator();
  DrawStatusBar(song);

  // Context menus
  if (ImGui::BeginPopup("PianoRollNoteContext")) {
    if (song && context_target_.segment >= 0 && context_target_.channel >= 0 &&
        context_target_.event_index >= 0 &&
        context_target_.segment < (int)song->segments.size()) {
      auto& track = song->segments[context_target_.segment].tracks[context_target_.channel];
      if (context_target_.event_index < (int)track.events.size()) {
        auto& evt = track.events[context_target_.event_index];
        if (evt.type == TrackEvent::Type::Note) {
          ImGui::Text(ICON_MD_MUSIC_NOTE " Note %s", evt.note.GetNoteName().c_str());
          ImGui::Text("Tick: %d", evt.tick);
          ImGui::Separator();

          // Velocity slider (0-127)
          ImGui::Text("Velocity:");
          ImGui::SameLine();
          int velocity = evt.note.velocity;
          ImGui::SetNextItemWidth(120);
          if (ImGui::SliderInt("##velocity", &velocity, 0, 127)) {
            evt.note.velocity = static_cast<uint8_t>(velocity);
            if (on_edit_) on_edit_();
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Articulation/velocity (0 = default)");
          }

          // Duration slider (1-192 ticks, quarter = 72)
          ImGui::Text("Duration:");
          ImGui::SameLine();
          int duration = evt.note.duration;
          ImGui::SetNextItemWidth(120);
          if (ImGui::SliderInt("##duration", &duration, 1, 192)) {
            evt.note.duration = static_cast<uint8_t>(duration);
            if (on_edit_) on_edit_();
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Duration in ticks (quarter = 72)");
          }

          ImGui::Separator();
          ImGui::Text("Quick Duration:");
          if (ImGui::MenuItem("Whole (288)")) {
            evt.note.duration = 0xFE;  // Max duration
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Half (144)")) {
            evt.note.duration = 144;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Quarter (72)")) {
            evt.note.duration = kDurationQuarter;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Eighth (36)")) {
            evt.note.duration = kDurationEighth;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("Sixteenth (18)")) {
            evt.note.duration = kDurationSixteenth;
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem("32nd (9)")) {
            evt.note.duration = kDurationThirtySecond;
            if (on_edit_) on_edit_();
          }

          ImGui::Separator();
          if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Duplicate")) {
            TrackEvent copy = evt;
            copy.tick += evt.note.duration;
            track.InsertEvent(copy);
            if (on_edit_) on_edit_();
          }
          if (ImGui::MenuItem(ICON_MD_DELETE " Delete", "Del")) {
            track.RemoveEvent(context_target_.event_index);
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
      ImGui::Text(ICON_MD_ADD " Add Note");
      ImGui::Separator();
      if (ImGui::MenuItem("Quarter note")) {
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
      if (ImGui::MenuItem("Eighth note")) {
        auto& t =
            song->segments[empty_context_.segment].tracks[empty_context_.channel];
        TrackEvent evt = TrackEvent::MakeNote(empty_context_.tick,
                                              empty_context_.pitch,
                                              kDurationEighth);
        t.InsertEvent(evt);
        if (on_edit_) on_edit_();
        if (on_note_preview_) on_note_preview_(evt, empty_context_.segment,
                                               empty_context_.channel);
      }
      if (ImGui::MenuItem("Sixteenth note")) {
        auto& t =
            song->segments[empty_context_.segment].tracks[empty_context_.channel];
        TrackEvent evt = TrackEvent::MakeNote(empty_context_.tick,
                                              empty_context_.pitch,
                                              kDurationSixteenth);
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
  // --- Transport Group ---
  if (song) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW)) {
      if (on_segment_preview_) {
        on_segment_preview_(*song, active_segment_index_);
      }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play Segment");
  }

  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();

  // --- Song/Segment Group ---
  if (song) {
    ImGui::TextDisabled(ICON_MD_MUSIC_NOTE);
    ImGui::SameLine();
    ImGui::Text("%s", song->name.empty() ? "Untitled" : song->name.c_str());
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    std::string seg_label = absl::StrFormat("Seg %d/%d",
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

  // --- Instrument Group ---
  if (bank) {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    ImGui::TextDisabled(ICON_MD_PIANO);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
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
      ImGui::SetTooltip("Instrument for new notes");
    }
  }

  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();

  // --- Zoom Group ---
  ImGui::TextDisabled(ICON_MD_ZOOM_IN);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
  ImGui::SliderFloat("##ZoomX", &pixels_per_tick_, 0.5f, 10.0f, "%.1f");
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Horizontal Zoom (px/tick)");
  
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
  ImGui::SliderFloat("##ZoomY", &key_height_, 6.0f, 20.0f, "%.0f");
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Vertical Zoom (px/key)");

  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();

  // --- Snap/Grid Group ---
  ImGui::TextDisabled(ICON_MD_GRID_ON);
  ImGui::SameLine();
  
  bool snap_active = snap_enabled_;
  if (snap_active) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  }
  if (ImGui::Button("Snap")) {
    snap_enabled_ = !snap_enabled_;
  }
  if (snap_active) {
    ImGui::PopStyleColor();
  }
  
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 2.5f);
  const char* snap_labels[] = {"1/4", "1/8", "1/16"};
  int snap_idx = 2;
  if (snap_ticks_ == kDurationQuarter) snap_idx = 0;
  else if (snap_ticks_ == kDurationEighth) snap_idx = 1;

  if (ImGui::Combo("##SnapValue", &snap_idx, snap_labels, IM_ARRAYSIZE(snap_labels))) {
    snap_enabled_ = true;
    snap_ticks_ = (snap_idx == 0) ? kDurationQuarter
                : (snap_idx == 1) ? kDurationEighth
                                  : kDurationSixteenth;
  }
}

void PianoRollView::DrawChannelList(const MusicSong* song) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
  
  ImGui::TextDisabled(ICON_MD_QUEUE_MUSIC " CHANNELS");
  ImGui::Separator();
  
  const auto& segment = song->segments[active_segment_index_];
  
  for (int i = 0; i < 8; ++i) {
    ImGui::PushID(i);
    
    bool is_active = (active_channel_index_ == i);
    int note_count = CountNotesInTrack(segment.tracks[i]);
    
    // Highlight active channel row
    if (is_active) {
      ImVec2 row_min = ImGui::GetCursorScreenPos();
      ImVec2 row_max = ImVec2(row_min.x + ImGui::GetContentRegionAvail().x, 
                              row_min.y + ImGui::GetTextLineHeightWithSpacing() + 4);
      ImGui::GetWindowDrawList()->AddRectFilled(row_min, row_max, 
          IM_COL32(255, 255, 255, 20), 3.0f);
    }
    
    // Color indicator
    ImVec4 col_v4 = ImGui::ColorConvertU32ToFloat4(channel_colors_[i]);
    if (ImGui::ColorButton("##Col", col_v4, 
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                           ImVec2(14, 14))) {
      ImGui::OpenPopup("ChannelColorPicker");
    }
    if (ImGui::BeginPopup("ChannelColorPicker")) {
      if (ImGui::ColorPicker4("##picker", (float*)&col_v4, 
                              ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) {
        channel_colors_[i] = ImGui::ColorConvertFloat4ToU32(col_v4);
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    
    // Visibility toggle
    const char* vis_icon = channel_visible_[i] ? ICON_MD_VISIBILITY : ICON_MD_VISIBILITY_OFF;
    if (ImGui::SmallButton(vis_icon)) {
      channel_visible_[i] = !channel_visible_[i];
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle visibility");
    ImGui::SameLine();
    
    // Mute button
    bool muted = channel_muted_[i];
    if (muted) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    if (ImGui::SmallButton("M")) {
      channel_muted_[i] = !channel_muted_[i];
    }
    if (muted) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute");
    ImGui::SameLine();
    
    // Solo button
    bool solo = channel_solo_[i];
    if (solo) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
    if (ImGui::SmallButton("S")) {
      channel_solo_[i] = !channel_solo_[i];
    }
    if (solo) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Solo");
    ImGui::SameLine();
    
    // Channel name + note count
    std::string label = absl::StrFormat("Ch%d", i + 1);
    if (ImGui::Selectable(label.c_str(), is_active, 0, ImVec2(35, 0))) {
      active_channel_index_ = i;
      channel_visible_[i] = true;
    }
    
    // Note count badge
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", note_count);
    
    ImGui::PopID();
  }
  
  ImGui::PopStyleVar();
  
  // Channel controls legend
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextDisabled("M=Mute S=Solo");
}

void PianoRollView::DrawStatusBar(const MusicSong* /*song*/) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
  if (ImGui::BeginChild("##PianoRollStatusBar", ImVec2(0, kStatusBarHeight), 
                        ImGuiChildFlags_AlwaysUseWindowPadding,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    
    // Mouse position info
    if (status_tick_ >= 0 && status_pitch_ >= 0) {
      ImGui::Text(ICON_MD_MOUSE " Tick: %d | Pitch: %s (%d)", 
                  status_tick_, status_note_name_.c_str(), status_pitch_);
    } else {
      ImGui::TextDisabled(ICON_MD_MOUSE " Hover over grid...");
    }
    
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 420);

    // Keyboard hints
    ImGui::TextDisabled("Click: Add | Drag: Move | Ctrl+Wheel: Zoom X | Shift+Wheel: Zoom Y");
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
}

void PianoRollView::HandleMouseInput(MusicSong* song, int active_channel, int active_segment,
                                     const ImVec2& grid_origin, const ImVec2& grid_size) {
  if (!song) return;
  if (active_segment < 0 || active_segment >= static_cast<int>(song->segments.size())) return;
  auto& track = song->segments[active_segment].tracks[active_channel];

  ImVec2 mouse_pos = ImGui::GetMousePos();

  // Mouse to grid conversion
  float rel_x = mouse_pos.x - grid_origin.x;
  float rel_y = mouse_pos.y - grid_origin.y;
  int tick = static_cast<int>(std::lround(rel_x / pixels_per_tick_));
  int pitch_idx = static_cast<int>(std::lround(rel_y / key_height_));
  uint8_t pitch = static_cast<uint8_t>(kNoteMaxPitch - pitch_idx);
  (void)grid_size;  // Used for bounds reference

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

    // Otherwise add a note with snap
    int snapped_tick = snap_enabled_ ? snap_tick(tick) : tick;
    TrackEvent new_note = TrackEvent::MakeNote(snapped_tick, pitch, 
                                               snap_enabled_ ? snap_ticks_ : kDurationQuarter);
    track.InsertEvent(new_note);
    if (on_edit_) on_edit_();
    if (on_note_preview_) {
      on_note_preview_(new_note, active_segment, active_channel);
    }
  }
}

bool PianoRollView::BeginRollCanvas(const char* id, RollCanvasContext* ctx) {
  const ImGuiChildFlags child_flags =
      ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding |
      ImGuiChildFlags_ResizeY;
  const ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar;
  bool open = ImGui::BeginChild(id, ImVec2(0, 0), child_flags, window_flags);
  if (!open) return false;

  ctx->canvas_pos = ImGui::GetCursorScreenPos();
  ctx->canvas_size = ImGui::GetContentRegionAvail();
  ctx->scroll_x = ImGui::GetScrollX();
  ctx->scroll_y = ImGui::GetScrollY();
  return true;
}

void PianoRollView::EndRollCanvas() { ImGui::EndChild(); }

void PianoRollView::DrawPlaybackCursor(ImDrawList* draw_list,
                                        const ImVec2& grid_origin,
                                        float grid_height,
                                        uint32_t segment_start_tick) {
  // Only draw if playback tick is in or past current segment
  if (playback_tick_ < segment_start_tick) return;

  // Calculate cursor position relative to segment start
  uint32_t local_tick = playback_tick_ - segment_start_tick;
  float cursor_x = grid_origin.x + local_tick * pixels_per_tick_;

  // Different colors for playing vs paused state
  ImU32 cursor_color, glow_color;
  if (is_paused_) {
    // Orange/amber for paused state
    cursor_color = IM_COL32(255, 180, 50, 255);
    glow_color = IM_COL32(255, 180, 50, 80);
  } else {
    // Bright red for active playback
    cursor_color = IM_COL32(255, 100, 100, 255);
    glow_color = IM_COL32(255, 100, 100, 80);
  }

  // Glow layer (thicker, semi-transparent)
  draw_list->AddLine(ImVec2(cursor_x, grid_origin.y),
                     ImVec2(cursor_x, grid_origin.y + grid_height),
                     glow_color, 6.0f);

  // Main cursor line
  draw_list->AddLine(ImVec2(cursor_x, grid_origin.y),
                     ImVec2(cursor_x, grid_origin.y + grid_height),
                     cursor_color, 2.0f);

  // Top indicator - triangle when playing, pause bars when paused
  const float tri_size = 8.0f;
  if (is_paused_) {
    // Pause bars indicator
    const float bar_width = 3.0f;
    const float bar_height = tri_size * 1.5f;
    const float bar_gap = 4.0f;
    draw_list->AddRectFilled(
        ImVec2(cursor_x - bar_gap - bar_width, grid_origin.y - bar_height),
        ImVec2(cursor_x - bar_gap, grid_origin.y),
        cursor_color);
    draw_list->AddRectFilled(
        ImVec2(cursor_x + bar_gap, grid_origin.y - bar_height),
        ImVec2(cursor_x + bar_gap + bar_width, grid_origin.y),
        cursor_color);
  } else {
    // Triangle indicator for active playback
    draw_list->AddTriangleFilled(
        ImVec2(cursor_x, grid_origin.y),
        ImVec2(cursor_x - tri_size, grid_origin.y - tri_size),
        ImVec2(cursor_x + tri_size, grid_origin.y - tri_size),
        cursor_color);
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
