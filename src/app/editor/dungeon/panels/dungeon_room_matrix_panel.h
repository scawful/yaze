#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomMatrixPanel
 * @brief WindowContent for displaying a visual 16x19 grid of all dungeon rooms
 *
 * This panel provides a compact overview of all 296 dungeon rooms in a matrix
 * layout. Users can click on cells to select and open rooms.
 *
 * Features:
 * - Responsive cell sizing based on panel width
 * - Palette-based coloring when room data is available
 * - Theme-aware selection highlighting
 *
 * @see WindowContent - Base interface
 */
class DungeonRoomMatrixPanel : public WindowContent {
 public:
  /**
   * @brief Construct a room matrix panel
   * @param current_room_id Pointer to the current room ID (for highlighting)
   * @param active_rooms Pointer to list of currently open rooms
   * @param on_room_selected Callback when a room is clicked
   * @param rooms Optional pointer to room array for palette-based coloring
   */
  DungeonRoomMatrixPanel(int* current_room_id, ImVector<int>* active_rooms,
                         std::function<void(int)> on_room_selected,
                         std::function<void(int, int)> on_room_swap = nullptr,
                         DungeonRoomStore* rooms = nullptr)
      : current_room_id_(current_room_id),
        active_rooms_(active_rooms),
        rooms_(rooms),
        on_room_selected_(std::move(on_room_selected)),
        on_room_swap_(std::move(on_room_swap)) {}

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_matrix"; }
  std::string GetDisplayName() const override { return "Room Matrix"; }
  std::string GetIcon() const override { return ICON_MD_GRID_VIEW; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 30; }
  float GetPreferredWidth() const override { return 440.0f; }

  void SetRoomIntentCallback(
      std::function<void(int, RoomSelectionIntent)> callback) {
    on_room_intent_ = std::move(callback);
  }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !active_rooms_)
      return;

    const auto& theme = AgentUI::GetTheme();

    // 16 wide x 19 tall = 304 cells (296 rooms + 8 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;  // 296 rooms (0x00-0x127)
    constexpr float kCellSpacing = 1.0f;

    DrawMatrixSummary(theme, kTotalRooms);

    ImGui::PushID("RoomMatrixFilter");
    ImGui::SetNextItemWidth(
        std::max(140.0f, ImGui::GetContentRegionAvail().x - 82.0f));
    ImGui::InputTextWithHint("##Search", ICON_MD_SEARCH " Filter room id/name",
                             search_filter_, IM_ARRAYSIZE(search_filter_));
    ImGui::SameLine();
    const bool has_filter = search_filter_[0] != '\0';
    if (!has_filter) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_MD_CLOSE "##ClearFilter")) {
      search_filter_[0] = '\0';
    }
    if (!has_filter) {
      ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Clear filter");
    }
    ImGui::PopID();

    DrawMatrixLegend(theme);
    ImGui::Spacing();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float panel_width = std::max(1.0f, avail.x);
    const float panel_height = std::max(1.0f, avail.y);
    const float cell_size_by_width =
        (panel_width - kCellSpacing * (kRoomsPerRow - 1)) / kRoomsPerRow;
    const float cell_size_by_height =
        (panel_height - kCellSpacing * (kRoomsPerCol - 1)) / kRoomsPerCol;
    const float cell_size = std::clamp(
        std::min(cell_size_by_width, cell_size_by_height), 12.0f, 24.0f);

    const float grid_width =
        kRoomsPerRow * cell_size + kCellSpacing * (kRoomsPerRow - 1);
    const float grid_height =
        kRoomsPerCol * cell_size + kCellSpacing * (kRoomsPerCol - 1);
    const float x_offset = std::max(0.0f, (panel_width - grid_width) * 0.5f);
    const float y_offset = std::max(0.0f, (panel_height - grid_height) * 0.5f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    canvas_pos.x += x_offset;
    canvas_pos.y += y_offset;

    int room_index = 0;
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = room_index;
        bool is_valid_room = (room_id < kTotalRooms);
        const bool matches_filter = MatchesSearchFilter(room_id);

        ImVec2 cell_min =
            ImVec2(canvas_pos.x + col * (cell_size + kCellSpacing),
                   canvas_pos.y + row * (cell_size + kCellSpacing));
        ImVec2 cell_max =
            ImVec2(cell_min.x + cell_size, cell_min.y + cell_size);

        if (is_valid_room) {
          // Get color based on room palette if available, else use algorithmic
          ImU32 bg_color = GetRoomColor(room_id, theme);
          if (!matches_filter) {
            bg_color = BlendRoomColor(
                bg_color, ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker),
                0.72f);
          }

          bool is_current = (*current_room_id_ == room_id);
          bool is_open = false;
          for (int i = 0; i < active_rooms_->Size; i++) {
            if ((*active_rooms_)[i] == room_id) {
              is_open = true;
              break;
            }
          }

          // Draw cell background
          draw_list->AddRectFilled(cell_min, cell_max, bg_color);

          // Draw outline based on state using theme colors
          if (is_current) {
            // Add glow effect for current room (outer glow layers)
            ImVec4 glow_color = theme.dungeon_selection_primary;
            glow_color.w = matches_filter ? 0.3f : 0.18f;
            ImVec2 glow_min(cell_min.x - 2, cell_min.y - 2);
            ImVec2 glow_max(cell_max.x + 2, cell_max.y + 2);
            draw_list->AddRect(glow_min, glow_max,
                               ImGui::ColorConvertFloat4ToU32(glow_color), 0.0f,
                               0, 3.0f);

            // Inner bright border
            ImU32 sel_color =
                ImGui::ColorConvertFloat4ToU32(theme.dungeon_selection_primary);
            draw_list->AddRect(cell_min, cell_max, sel_color, 0.0f, 0, 2.5f);
          } else if (is_open) {
            ImU32 open_color = ImGui::ColorConvertFloat4ToU32(
                theme.dungeon_grid_cell_selected);
            draw_list->AddRect(cell_min, cell_max, open_color, 0.0f, 0, 2.0f);
          } else {
            ImU32 border_color =
                ImGui::ColorConvertFloat4ToU32(theme.dungeon_grid_cell_border);
            if (!matches_filter) {
              border_color = BlendRoomColor(
                  border_color,
                  ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker), 0.5f);
            }
            draw_list->AddRect(cell_min, cell_max, border_color, 0.0f, 0, 1.0f);
          }

          // Draw room ID (only if cell is large enough)
          if (cell_size >= 18.0f) {
            char label[8];
            snprintf(label, sizeof(label), "%02X", room_id);
            ImVec2 text_size = ImGui::CalcTextSize(label);
            ImVec2 text_pos =
                ImVec2(cell_min.x + (cell_size - text_size.x) * 0.5f,
                       cell_min.y + (cell_size - text_size.y) * 0.5f);
            ImVec4 text_color_vec = theme.dungeon_grid_text;
            if (!matches_filter) {
              text_color_vec.w *= 0.55f;
            }
            ImU32 text_color = ImGui::ColorConvertFloat4ToU32(text_color_vec);
            draw_list->AddText(text_pos, text_color, label);
          }

          // Handle clicks
          ImGui::SetCursorScreenPos(cell_min);
          char btn_id[32];
          snprintf(btn_id, sizeof(btn_id), "##room%d", room_id);
          ImGui::InvisibleButton(btn_id, ImVec2(cell_size, cell_size));

          if (ImGui::IsItemClicked()) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              // Double-click: open as standalone panel
              if (on_room_intent_) {
                on_room_intent_(room_id, RoomSelectionIntent::kOpenStandalone);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            } else if (on_room_selected_) {
              on_room_selected_(room_id);
            }
          }

          if (ImGui::BeginPopupContextItem()) {
            const bool can_swap =
                on_room_swap_ && current_room_id_ && *current_room_id_ >= 0 &&
                *current_room_id_ < kTotalRooms && *current_room_id_ != room_id;

            std::string open_label =
                is_open ? "Focus Room" : "Open in Workbench";
            if (ImGui::MenuItem(open_label.c_str())) {
              if (on_room_intent_) {
                on_room_intent_(room_id,
                                RoomSelectionIntent::kFocusInWorkbench);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            }

            if (ImGui::MenuItem("Open as Panel")) {
              if (on_room_intent_) {
                on_room_intent_(room_id, RoomSelectionIntent::kOpenStandalone);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            }

            if (ImGui::MenuItem("Swap With Current Room", nullptr, false,
                                can_swap)) {
              on_room_swap_(*current_room_id_, room_id);
            }

            ImGui::Separator();

            char id_buf[16];
            snprintf(id_buf, sizeof(id_buf), "0x%02X", room_id);
            if (ImGui::MenuItem("Copy Room ID")) {
              ImGui::SetClipboardText(id_buf);
            }

            const std::string& room_label = zelda3::GetRoomLabel(room_id);
            if (ImGui::MenuItem("Copy Room Name")) {
              ImGui::SetClipboardText(room_label.c_str());
            }

            ImGui::EndPopup();
          }

          // Tooltip with room info and thumbnail preview
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            // Use unified ResourceLabelProvider for room names
            ImGui::Text("%s", zelda3::GetRoomLabel(room_id).c_str());
            ImGui::TextDisabled("Room 0x%02X", room_id);

            if (is_current) {
              ImGui::TextColored(theme.dungeon_selection_primary,
                                 ICON_MD_MY_LOCATION " Current workbench room");
            } else if (is_open) {
              ImGui::TextColored(theme.status_success,
                                 ICON_MD_TAB " Open in editor");
            }

            if (rooms_) {
              auto* loaded_room = rooms_->GetIfLoaded(room_id);
              if (loaded_room != nullptr) {
                // Show palette info
                ImGui::TextDisabled("Palette: %d | Blockset: %d",
                                    loaded_room->palette(),
                                    loaded_room->blockset());

                // Show thumbnail preview of the room
                auto& room = *loaded_room;
                zelda3::RoomLayerManager layer_mgr;
                layer_mgr.ApplyLayerMerging(room.layer_merging());
                auto& preview_bitmap = room.GetCompositeBitmap(layer_mgr);
                if (preview_bitmap.is_active() &&
                    preview_bitmap.texture() != 0) {
                  ImGui::Separator();
                  // Render at thumbnail size (80x80 from 512x512)
                  constexpr float kThumbnailSize = 80.0f;
                  ImGui::Image((ImTextureID)(intptr_t)preview_bitmap.texture(),
                               ImVec2(kThumbnailSize, kThumbnailSize));
                }
              }
            }

            ImGui::Separator();
            ImGui::TextDisabled("Click to %s", is_open ? "focus" : "open");
            ImGui::TextDisabled("Double-click to open as panel");
            ImGui::TextDisabled("Right-click for actions");
            ImGui::EndTooltip();
          }
        } else {
          // Empty cell
          draw_list->AddRectFilled(
              cell_min, cell_max,
              ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker));
        }

        room_index++;
      }
    }

    // Advance cursor past the grid
    ImGui::Dummy(
        ImVec2(panel_width, std::max(grid_height + y_offset, panel_height)));
  }

  void SetRooms(DungeonRoomStore* rooms) { rooms_ = rooms; }

 private:
  void DrawMatrixSummary(const AgentUITheme& theme, int total_rooms) const {
    const int open_rooms = active_rooms_ ? active_rooms_->Size : 0;
    const int current_room = current_room_id_ ? *current_room_id_ : -1;
    const std::string current_label = current_room >= 0
                                          ? zelda3::GetRoomLabel(current_room)
                                          : "No room selected";

    ImGui::SeparatorText("Navigator");
    ImGui::Text("%s", current_label.c_str());
    ImGui::TextDisabled("Current: 0x%02X", std::max(0, current_room));
    ImGui::SameLine();
    ImGui::TextDisabled("%d open", open_rooms);

    if (current_room >= 0) {
      if (auto room_meta = GetRoomMetadata(current_room);
          room_meta.has_value()) {
        ImGui::SameLine();
        ImGui::TextColored(theme.status_success, "Pal %d / Blockset %d",
                           room_meta->first, room_meta->second);
      }
    }
  }

  void DrawMatrixLegend(const AgentUITheme& theme) const {
    ImGui::SeparatorText("Legend");
    DrawLegendSwatch(theme.dungeon_selection_primary,
                     ICON_MD_MY_LOCATION " Current");
    ImGui::SameLine();
    DrawLegendSwatch(theme.dungeon_grid_cell_selected, ICON_MD_TAB " Open");
    ImGui::SameLine();
    DrawLegendSwatch(theme.dungeon_grid_cell_border,
                     ICON_MD_GRID_VIEW " Other");
  }

  void DrawLegendSwatch(const ImVec4& color, const char* label) const {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float size = ImGui::GetFrameHeight() - 6.0f;
    const ImVec2 min = ImVec2(pos.x, pos.y + 3.0f);
    const ImVec2 max = ImVec2(pos.x + size, pos.y + size + 3.0f);
    draw_list->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(color),
                             3.0f);
    draw_list->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
    ImGui::Dummy(ImVec2(size + 6.0f, size + 6.0f));
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextUnformatted(label);
  }

  bool MatchesSearchFilter(int room_id) const {
    if (search_filter_[0] == '\0') {
      return true;
    }

    const std::string filter = NormalizeForSearch(search_filter_);
    char id_buf[16];
    snprintf(id_buf, sizeof(id_buf), "%02X", room_id);
    char hex_buf[16];
    snprintf(hex_buf, sizeof(hex_buf), "0x%02X", room_id);

    const std::string room_label =
        NormalizeForSearch(zelda3::GetRoomLabel(room_id));
    const std::string id_text = NormalizeForSearch(id_buf);
    const std::string hex_text = NormalizeForSearch(hex_buf);
    return room_label.find(filter) != std::string::npos ||
           id_text.find(filter) != std::string::npos ||
           hex_text.find(filter) != std::string::npos;
  }

  static std::string NormalizeForSearch(const std::string& value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (unsigned char ch : value) {
      lowered.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lowered;
  }

  static ImU32 BlendRoomColor(ImU32 source, ImU32 target, float blend) {
    ImVec4 src = ImGui::ColorConvertU32ToFloat4(source);
    ImVec4 dst = ImGui::ColorConvertU32ToFloat4(target);
    src.x = (src.x * (1.0f - blend)) + (dst.x * blend);
    src.y = (src.y * (1.0f - blend)) + (dst.y * blend);
    src.z = (src.z * (1.0f - blend)) + (dst.z * blend);
    src.w = 1.0f;
    return ImGui::ColorConvertFloat4ToU32(src);
  }

  std::optional<std::pair<int, int>> GetRoomMetadata(int room_id) const {
    if (!rooms_ || room_id < 0 ||
        room_id >= static_cast<int>(DungeonRoomStore::kRoomCount)) {
      return std::nullopt;
    }

    if (const auto* room = rooms_->GetIfMaterialized(room_id);
        room != nullptr) {
      return std::make_pair(static_cast<int>(room->palette()),
                            static_cast<int>(room->blockset()));
    }

    Rom* rom = rooms_->rom();
    if (rom == nullptr || !rom->is_loaded()) {
      return std::nullopt;
    }

    zelda3::Room header_room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
    return std::make_pair(static_cast<int>(header_room.palette()),
                          static_cast<int>(header_room.blockset()));
  }

  /**
   * @brief Get color for a room from dominant preview color, with fallback.
   */
  ImU32 GetRoomColor(int room_id, const AgentUITheme& theme) {
    auto sample_dominant_color =
        [](const gfx::Bitmap& bitmap) -> std::optional<ImU32> {
      if (!bitmap.is_active() || bitmap.width() <= 0 || bitmap.height() <= 0 ||
          bitmap.data() == nullptr || bitmap.surface() == nullptr ||
          bitmap.surface()->format == nullptr ||
          bitmap.surface()->format->palette == nullptr) {
        return std::nullopt;
      }

      constexpr int kSampleStep = 16;
      std::array<uint32_t, 256> histogram{};
      SDL_Palette* palette = bitmap.surface()->format->palette;
      const uint8_t* pixels = bitmap.data();
      const int width = bitmap.width();
      const int height = bitmap.height();

      for (int y = 0; y < height; y += kSampleStep) {
        for (int x = 0; x < width; x += kSampleStep) {
          const uint8_t idx = pixels[(y * width) + x];
          if (idx == 255) {
            continue;
          }
          histogram[idx]++;
        }
      }

      uint32_t best_count = 0;
      uint8_t best_index = 0;
      for (int i = 0; i < palette->ncolors && i < 256; ++i) {
        if (histogram[static_cast<size_t>(i)] > best_count) {
          best_count = histogram[static_cast<size_t>(i)];
          best_index = static_cast<uint8_t>(i);
        }
      }

      if (best_count == 0) {
        return std::nullopt;
      }
      const SDL_Color& c = palette->colors[best_index];
      return IM_COL32(c.r, c.g, c.b, 255);
    };

    auto soften_color = [&](ImU32 color, float blend = 0.32f) -> ImU32 {
      ImVec4 src = ImGui::ColorConvertU32ToFloat4(color);
      const ImVec4 bg = theme.panel_bg_darker;
      src.x = (src.x * (1.0f - blend)) + (bg.x * blend);
      src.y = (src.y * (1.0f - blend)) + (bg.y * blend);
      src.z = (src.z * (1.0f - blend)) + (bg.z * blend);
      src.w = 1.0f;
      return ImGui::ColorConvertFloat4ToU32(src);
    };

    auto palette_fallback_color = [&](int palette_id,
                                      int blockset_id) -> ImU32 {
      const ImVec4 bg = theme.panel_bg_darker;
      const float palette_mix =
          0.26f + (static_cast<float>(palette_id & 0x07) * 0.055f);
      const float blockset_mix =
          0.08f + (static_cast<float>(blockset_id & 0x0F) * 0.018f);
      ImVec4 tint = theme.dungeon_selection_primary;
      tint.x = std::clamp(tint.x + blockset_mix, 0.0f, 1.0f);
      tint.y = std::clamp(tint.y + (palette_mix * 0.35f), 0.0f, 1.0f);
      tint.z = std::clamp(tint.z - (blockset_mix * 0.2f), 0.0f, 1.0f);

      ImVec4 mixed;
      mixed.x = std::clamp(
          (bg.x * (1.0f - palette_mix)) + (tint.x * palette_mix), 0.0f, 1.0f);
      mixed.y = std::clamp(
          (bg.y * (1.0f - palette_mix)) + (tint.y * palette_mix), 0.0f, 1.0f);
      mixed.z = std::clamp(
          (bg.z * (1.0f - palette_mix)) + (tint.z * palette_mix), 0.0f, 1.0f);
      mixed.w = 1.0f;
      return ImGui::ColorConvertFloat4ToU32(mixed);
    };

    // If room data is available and loaded, sample the actual room bitmap and
    // choose its most frequent indexed color.
    if (rooms_) {
      auto* room = rooms_->GetIfLoaded(room_id);
      if (room != nullptr) {
        zelda3::RoomLayerManager layer_mgr;
        layer_mgr.ApplyLayerMerging(room->layer_merging());
        auto& composite = room->GetCompositeBitmap(layer_mgr);
        if (auto composite_color = sample_dominant_color(composite);
            composite_color.has_value()) {
          return soften_color(composite_color.value());
        }

        if (auto bg1_color = sample_dominant_color(room->bg1_buffer().bitmap());
            bg1_color.has_value()) {
          return soften_color(bg1_color.value());
        }

        return palette_fallback_color(room->palette(), room->blockset());
      }

      if (auto room_meta = GetRoomMetadata(room_id); room_meta.has_value()) {
        return palette_fallback_color(room_meta->first, room_meta->second);
      }
    }

    // Fallback: neutral deterministic color buckets (no rainbow hue wheel).
    const auto clamp01 = [](float v) {
      return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
    };

    const ImVec4 dark = theme.panel_bg_darker;
    const ImVec4 mid = theme.panel_bg_color;
    const float group_mix =
        0.16f + (static_cast<float>((room_id >> 4) & 0x03) * 0.08f);
    const float step = static_cast<float>(room_id & 0x07) * 0.0125f;

    ImVec4 fallback;
    fallback.x = clamp01(dark.x + (mid.x - dark.x) * group_mix + step);
    fallback.y = clamp01(dark.y + (mid.y - dark.y) * group_mix + step);
    fallback.z = clamp01(dark.z + (mid.z - dark.z) * group_mix + step);
    fallback.w = 1.0f;
    return ImGui::ColorConvertFloat4ToU32(fallback);
  }

  int* current_room_id_ = nullptr;
  ImVector<int>* active_rooms_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<void(int, int)> on_room_swap_;
  std::function<void(int, RoomSelectionIntent)> on_room_intent_;
  char search_filter_[64] = "";
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
