#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CONTENT_H_
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CONTENT_H_

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "util/i18n/tr.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_room_composite.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/workspace/room_matrix_census.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/resource/bitmap_texture_queue.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/canvas/item_context_menu.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class RoomMatrixContent
 * @brief WindowContent for displaying a visual 16x19 grid of all dungeon rooms
 *
 * This panel provides a compact overview of all 296 dungeon rooms in a matrix
 * layout. Users can click on cells to select and open rooms.
 *
 * Features:
 * - Responsive cell sizing based on panel width
 * - Palette-based coloring when room data is available
 * - Theme-aware selection highlighting
 * - "Census" overlay (RoomMatrixCensusOverlay): owner colors, free /
 *   reclaimable outlines, legend and filter chips, largest free block
 *
 * @see WindowContent - Base interface
 */
class RoomMatrixContent : public WindowContent {
 public:
  /**
   * @brief Construct a room matrix panel
   * @param current_room_id Pointer to the current room ID (for highlighting)
   * @param active_rooms Pointer to list of currently open rooms
   * @param on_room_selected Callback when a room is clicked
   * @param rooms Optional pointer to room array for palette-based coloring
   */
  RoomMatrixContent(int* current_room_id, ImVector<int>* active_rooms,
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
  std::string GetWorkflowGroup() const override { return "Core"; }
  float GetPreferredWidth() const override { return 440.0f; }

  void SetRoomIntentCallback(
      std::function<void(int, RoomSelectionIntent)> callback) {
    on_room_intent_ = std::move(callback);
  }

  void SetRenderContextCallback(std::function<void(int)> callback) {
    prepare_render_context_ = std::move(callback);
  }

  // `request` opens the room data swap dialog for (first, second); `draw`
  // renders that dialog inside this panel's ID scope every frame.
  void SetRoomDataSwapHandlers(std::function<void(int, int)> request,
                               std::function<void()> draw) {
    on_room_data_swap_request_ = std::move(request);
    draw_room_data_swap_dialog_ = std::move(draw);
  }

  // Opens Clone / Import for (target, source).
  void SetRoomCloneRequestCallback(std::function<void(int, int)> callback) {
    on_room_clone_request_ = std::move(callback);
  }

  // Project dungeon ownership and warp tags for the census overlay.
  void SetCensusManifestProvider(
      RoomMatrixCensusOverlay::ManifestProvider provider) {
    census_.SetManifestProvider(std::move(provider));
  }
  RoomMatrixCensusOverlay& census_overlay() { return census_; }
  // The window manager can retain this panel after its editor is destroyed.
  void DetachRuntimeContext() {
    census_.SetEnabled(false);
    census_.SetManifestProvider({});
    SetRooms(nullptr);
    current_room_id_ = nullptr;
    active_rooms_ = nullptr;
    on_room_selected_ = {};
    on_room_swap_ = {};
    on_room_intent_ = {};
    prepare_render_context_ = {};
    on_room_data_swap_request_ = {};
    draw_room_data_swap_dialog_ = {};
    on_room_clone_request_ = {};
    navigation_shortcut_focus_ = false;
  }
  bool OwnsNavigationShortcutFocus() const {
    if (!navigation_shortcut_focus_ || ImGui::GetCurrentContext() == nullptr) {
      return false;
    }
    return navigation_shortcut_last_draw_frame_ >= ImGui::GetFrameCount() - 1;
  }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !active_rooms_)
      return;
    navigation_shortcut_last_draw_frame_ = ImGui::GetFrameCount();
    navigation_shortcut_focus_ =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    const auto& theme = AgentUI::GetTheme();

    // 16 wide x 19 tall = 304 cells (296 rooms + 8 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;  // 296 rooms (0x00-0x127)
    constexpr float kCellSpacing = 1.0f;

    Rom* census_rom = rooms_ ? rooms_->rom() : nullptr;
    census_.Update(census_rom);
    DrawMatrixHeader(census_rom);
    const bool census_on = census_.enabled() && census_.census() != nullptr;

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float panel_width = std::max(1.0f, avail.x);
    const float panel_height =
        std::max(1.0f, avail.y - ImGui::GetStyle().ItemSpacing.y);
    const float cell_size_by_width =
        (panel_width - kCellSpacing * (kRoomsPerRow - 1)) / kRoomsPerRow;
    const float cell_size_by_height =
        (panel_height - kCellSpacing * (kRoomsPerCol - 1)) / kRoomsPerCol;
    const float cell_size = std::max(
        1.0f, std::floor(std::min({cell_size_by_width, cell_size_by_height,
                                   24.0f * ImGui::GetFontSize() / 13.0f})));

    const float grid_width =
        kRoomsPerRow * cell_size + kCellSpacing * (kRoomsPerRow - 1);
    const float grid_height =
        kRoomsPerCol * cell_size + kCellSpacing * (kRoomsPerCol - 1);
    const float x_offset = std::max(0.0f, (panel_width - grid_width) * 0.5f);
    const float y_offset = std::max(0.0f, (panel_height - grid_height) * 0.5f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 grid_origin = ImGui::GetCursorScreenPos();
    ImVec2 canvas_pos = grid_origin;
    canvas_pos.x += x_offset;
    canvas_pos.y += y_offset;

    int room_index = 0;
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = room_index;
        bool is_valid_room = (room_id < kTotalRooms);
        const bool matches_filter =
            MatchesSearchFilter(room_id) &&
            (!census_on || census_.MatchesChips(room_id));

        ImVec2 cell_min =
            ImVec2(canvas_pos.x + col * (cell_size + kCellSpacing),
                   canvas_pos.y + row * (cell_size + kCellSpacing));
        ImVec2 cell_max =
            ImVec2(cell_min.x + cell_size, cell_min.y + cell_size);

        if (is_valid_room) {
          // Keep the palette hue stable whether a room is loaded or unopened.
          ImU32 bg_color = census_on ? census_.FillColor(room_id)
                                     : GetRoomColor(room_id, theme);
          if (!matches_filter) {
            bg_color = BlendRoomColor(
                bg_color, ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker),
                0.72f);
          }

          bool is_current = (*current_room_id_ == room_id);
          bool is_open = rooms_ && rooms_->GetIfLoaded(room_id);
          for (int i = 0; i < active_rooms_->Size; i++) {
            if ((*active_rooms_)[i] == room_id) {
              is_open = true;
              break;
            }
          }

          if ((is_open || is_current) && matches_filter) {
            ImVec4 color = ImGui::ColorConvertU32ToFloat4(bg_color);
            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
            ImGui::ColorConvertHSVtoRGB(h, s, v + (1.0f - v) * 0.22f, color.x,
                                        color.y, color.z);
            bg_color = ImGui::ColorConvertFloat4ToU32(color);
          }
          draw_list->AddRectFilled(cell_min, cell_max, bg_color);
          // An inset outline stays inside the cell, including the last row.
          if (is_current || is_open) {
            const ImU32 outline =
                is_current ? ImGui::ColorConvertFloat4ToU32(
                                 theme.dungeon_selection_primary)
                           : IM_COL32(255, 255, 255, matches_filter ? 130 : 45);
            draw_list->AddRect(ImVec2(cell_min.x + 1, cell_min.y + 1),
                               ImVec2(cell_max.x - 1, cell_max.y - 1), outline,
                               0, 0, is_current ? 2.0f : 1.0f);
          }

          if (census_on) {
            DrawCensusCellMarks(draw_list, room_id, cell_min, cell_max,
                                matches_filter, theme);
          }

          // Scale the three-digit IDs to the actual cell; never spill into
          // adjacent rooms. Tiny docks retain full IDs in the hover tooltip.
          if (cell_size >= 14.0f) {
            const std::string label = absl::StrFormat("%03X", room_id);
            const ImVec2 normal_size = ImGui::CalcTextSize(label.c_str());
            const float scale =
                std::min(1.0f, (cell_size - 2.0f) / normal_size.x);
            const ImVec2 text_pos(
                cell_min.x + (cell_size - normal_size.x * scale) * 0.5f,
                cell_min.y + (cell_size - normal_size.y * scale) * 0.5f);
            const ImVec4 color = ImGui::ColorConvertU32ToFloat4(bg_color);
            const float luminance =
                color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
            const ImU32 ink = luminance > 0.55f ? IM_COL32(15, 15, 15, 255)
                                                : IM_COL32(245, 245, 245, 255);
            draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
                               text_pos, ink, label.c_str());
          }

          // Handle clicks
          ImGui::SetCursorScreenPos(cell_min);
          char btn_id[32];
          snprintf(btn_id, sizeof(btn_id), "##room%d", room_id);
          ImGui::InvisibleButton(btn_id, ImVec2(cell_size, cell_size));
          gui::AutoRegisterLastItem("button",
                                    absl::StrFormat("Room 0x%03X", room_id));

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

          if (on_room_data_swap_request_) {
            if (ImGui::BeginDragDropSource()) {
              ImGui::SetDragDropPayload(kRoomDragPayload, &room_id,
                                        sizeof(room_id));
              ImGui::Text(ICON_MD_SWAP_HORIZ " Room %03X", room_id);
              ImGui::TextDisabled("%s",
                                  tr("Drop on another room to swap data"));
              ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
              if (const ImGuiPayload* payload =
                      ImGui::AcceptDragDropPayload(kRoomDragPayload);
                  payload && payload->DataSize == sizeof(int)) {
                const int source_room = *static_cast<const int*>(payload->Data);
                if (source_room != room_id) {
                  on_room_data_swap_request_(source_room, room_id);
                }
              }
              ImGui::EndDragDropTarget();
            }
          }

          gui::ItemContextMenu(nullptr, [&, room_id]() {
            const bool has_other_current =
                current_room_id_ && *current_room_id_ >= 0 &&
                *current_room_id_ < kTotalRooms && *current_room_id_ != room_id;
            const bool can_swap = on_room_swap_ && has_other_current;
            const int current_room = current_room_id_ ? *current_room_id_ : -1;

            std::vector<gui::MenuItemSpec> items;
            items.emplace_back(is_open ? "Focus Room" : "Open in Workbench",
                               ICON_MD_ARROW_FORWARD, [this, room_id]() {
                                 if (on_room_intent_) {
                                   on_room_intent_(
                                       room_id,
                                       RoomSelectionIntent::kFocusInWorkbench);
                                 } else if (on_room_selected_) {
                                   on_room_selected_(room_id);
                                 }
                               });
            items.emplace_back(
                tr("Open as Panel"), ICON_MD_OPEN_IN_NEW, [this, room_id]() {
                  if (on_room_intent_) {
                    on_room_intent_(room_id,
                                    RoomSelectionIntent::kOpenStandalone);
                  } else if (on_room_selected_) {
                    on_room_selected_(room_id);
                  }
                });
            items.back().separator_after = true;

            items.push_back(gui::CopyToClipboardItem(
                tr("Copy Room ID"), absl::StrFormat("0x%03X", room_id)));
            items.push_back(gui::CopyToClipboardItem(
                tr("Copy Room Name"), zelda3::GetRoomLabel(room_id)));
            items.back().separator_after = true;

            items.push_back(gui::MenuItemSpec::Conditional(
                tr("Show in Current Panel"),
                [this, room_id]() {
                  on_room_swap_(*current_room_id_, room_id);
                },
                [can_swap]() { return can_swap; }));
            items.back().icon = ICON_MD_OPEN_IN_BROWSER;
            items.back().tooltip =
                tr("Replace the room shown in the current panel");
            items.back().separator_after = true;

            const std::string current_label =
                has_other_current ? absl::StrFormat("%03X", current_room)
                                  : std::string(tr("Current Room"));
            const bool can_swap_data =
                on_room_data_swap_request_ && has_other_current;
            items.push_back(gui::MenuItemSpec::Conditional(
                absl::StrCat(tr("Swap Data With"), " ", current_label, "..."),
                [this, room_id, current_room]() {
                  on_room_data_swap_request_(current_room, room_id);
                },
                [can_swap_data]() { return can_swap_data; }));
            items.back().icon = ICON_MD_SWAP_HORIZ;
            items.back().tooltip =
                tr("Exchange room contents. Tip: drag one room onto another.");
            const bool can_clone = on_room_clone_request_ && has_other_current;
            items.push_back(gui::MenuItemSpec::Conditional(
                absl::StrCat(tr("Clone Into"), " ", current_label, "..."),
                [this, room_id, current_room]() {
                  on_room_clone_request_(current_room, room_id);
                },
                [can_clone]() { return can_clone; }));
            items.back().icon = ICON_MD_CONTENT_COPY;
            items.back().tooltip =
                tr("Replace the current room's contents with this room's");
            return items;
          });

          // Tooltip with room info and thumbnail preview
          if (ImGui::IsItemHovered() && !ImGui::GetDragDropPayload()) {
            ImGui::BeginTooltip();
            // Use unified ResourceLabelProvider for room names
            ImGui::Text("%s", zelda3::GetRoomLabel(room_id).c_str());
            ImGui::TextDisabled(tr("Room 0x%02X"), room_id);
            if (census_on) {
              ImGui::Separator();
              census_.DrawTooltip(room_id);
              ImGui::Separator();
            }

            if (is_current) {
              ImGui::TextColored(theme.dungeon_selection_primary,
                                 ICON_MD_MY_LOCATION " Current workbench room");
            } else if (is_open) {
              ImGui::TextColored(theme.status_success,
                                 ICON_MD_TAB " Open in editor");
            }

            if (rooms_) {
              zelda3::Room* room = rooms_->GetIfLoaded(room_id);
              if (room == nullptr) {
                room = HoverPreviewRoom(room_id);
              }
              if (room != nullptr) {
                ImGui::TextDisabled(
                    "Palette: 0x%02X | Blockset: 0x%02X | Spriteset: 0x%02X",
                    room->palette(), room->blockset(), room->spriteset());
                ImGui::TextDisabled(
                    "Objects: %zu | Sprites: %zu | Doors: %zu | Chests: %zu",
                    room->GetTileObjects().size(), room->GetSprites().size(),
                    room->GetDoors().size(), room->GetChests().size());

                if (prepare_render_context_)
                  prepare_render_context_(room_id);
                auto& preview_bitmap = PrepareCanonicalRoomComposite(
                    *room, tooltip_composite_output_);
                gfx::EnsureCompositeBitmapTextureQueued(preview_bitmap);
                gfx::Arena::Get().ProcessTextureQueue(nullptr);
                if (preview_bitmap.is_active() &&
                    preview_bitmap.texture() != 0) {
                  ImGui::Separator();
                  constexpr float kThumbnailSize = 144.0f;
                  ImGui::Image((ImTextureID)(intptr_t)preview_bitmap.texture(),
                               ImVec2(kThumbnailSize, kThumbnailSize));
                }
              }
            }

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

    // Cell buttons move the cursor. Reserve the grid once from its origin,
    // rather than adding another full grid below the last cell.
    ImGui::SetCursorScreenPos(grid_origin);
    ImGui::Dummy(ImVec2(panel_width, grid_height + y_offset));
    if (draw_room_data_swap_dialog_) {
      draw_room_data_swap_dialog_();
    }
  }

  void SetRooms(DungeonRoomStore* rooms) {
    if (rooms_ != rooms) {
      rooms_ = rooms;
      census_.Invalidate();
      tooltip_composite_output_.Retire();
      hover_preview_room_.reset();
    }
  }

 private:
  static constexpr const char* kRoomDragPayload = "YAZE_DUNGEON_ROOM_ID";

  // Unopened rooms preview from a tooltip-owned copy. Replacing the shared
  // room would drop pending imported collision and mark the room as open.
  zelda3::Room* HoverPreviewRoom(int room_id) {
    Rom* rom = rooms_ ? rooms_->rom() : nullptr;
    if (rom == nullptr || !rom->is_loaded()) {
      return nullptr;
    }
    if (!hover_preview_room_ || hover_preview_room_->id() != room_id) {
      tooltip_composite_output_.Retire();
      hover_preview_room_ =
          std::make_unique<zelda3::Room>(zelda3::LoadRoomFromRom(rom, room_id));
      hover_preview_room_->SetGameData(rooms_->game_data());
    }
    return hover_preview_room_.get();
  }

  std::function<void(int)> prepare_render_context_;
  std::unique_ptr<zelda3::Room> hover_preview_room_;
  void DrawMatrixHeader(Rom* rom) {
    const float button_width = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const bool has_filter = search_filter_[0] != '\0';
    ImGui::SetNextItemWidth(
        std::max(1.0f, ImGui::GetContentRegionAvail().x -
                           (button_width + spacing) * (has_filter ? 2 : 1)));
    ImGui::InputTextWithHint("##RoomSearch", "Room ID or name...",
                             search_filter_, IM_ARRAYSIZE(search_filter_));
    if (has_filter) {
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CLOSE "##ClearFilter", ImVec2(button_width, 0)))
        search_filter_[0] = '\0';
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tr("Clear room filter"));
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_MORE_HORIZ "##MatrixOptions",
                      ImVec2(button_width, 0)))
      ImGui::OpenPopup("##MatrixOptionsPopup");
    gui::AutoRegisterLastItem("button", "Room matrix options");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(tr("Room colors and census"));
    if (ImGui::BeginPopup("##MatrixOptionsPopup")) {
      ImGui::TextDisabled("Room palette colors (same swatch as ZScream)");
      ImGui::TextUnformatted("Brighter + thin outline: open room");
      ImGui::TextUnformatted("Accent outline: current room");
      ImGui::Separator();
      census_.DrawControls(rom);
      ImGui::EndPopup();
    }
    const std::string caption = absl::StrFormat(
        "%03X  %s", *current_room_id_, zelda3::GetRoomLabel(*current_room_id_));
    // Clip long custom names to one line, with the full name on hover.
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x,
                      ImGui::GetTextLineHeight());
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
    draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  caption.c_str());
    draw->PopClipRect();
    ImGui::Dummy(size);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(caption.c_str());
      ImGui::TextDisabled("%d loaded rooms",
                          rooms_ ? static_cast<int>(rooms_->LoadedCount())
                                 : active_rooms_->Size);
      if (const auto meta = GetRoomMetadata(*current_room_id_))
        ImGui::TextDisabled("Palette %02X / Room GFX %02X", meta->first,
                            meta->second);
      ImGui::TextDisabled(
          "Click a room to open. Double-click for a separate panel.");
      if (on_room_data_swap_request_)
        ImGui::TextDisabled("Drag a room onto another to swap their data.");
      ImGui::EndTooltip();
    }
  }

  // Census marks: free/reclaimable outline, largest-free-block halo, orphan
  // corner.
  void DrawCensusCellMarks(ImDrawList* draw_list, int room_id,
                           const ImVec2& cell_min, const ImVec2& cell_max,
                           bool matches_filter,
                           const AgentUITheme& theme) const {
    const ImU32 dim_target =
        ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker);
    if (census_.InLargestFreeBlock(room_id)) {
      draw_list->AddRect(
          ImVec2(cell_min.x - 1.5f, cell_min.y - 1.5f),
          ImVec2(cell_max.x + 1.5f, cell_max.y + 1.5f),
          RoomMatrixCensusOverlay::BlockHaloColor(matches_filter ? 0.5f : 0.2f),
          0.0f, 0, 2.0f);
    }
    if (auto outline = census_.StatusOutline(room_id); outline.has_value()) {
      ImU32 color = *outline;
      if (!matches_filter) {
        color = BlendRoomColor(color, dim_target, 0.55f);
      }
      draw_list->AddRect(ImVec2(cell_min.x + 1.0f, cell_min.y + 1.0f),
                         ImVec2(cell_max.x - 1.0f, cell_max.y - 1.0f), color,
                         0.0f, 0, 2.0f);
    }
    if (census_.IsOrphan(room_id)) {
      const float corner = std::max(4.0f, (cell_max.x - cell_min.x) * 0.3f);
      ImU32 color = RoomMatrixCensusOverlay::OrphanMarkColor();
      if (!matches_filter) {
        color = BlendRoomColor(color, dim_target, 0.55f);
      }
      draw_list->AddTriangleFilled(ImVec2(cell_max.x - corner, cell_min.y),
                                   ImVec2(cell_max.x, cell_min.y),
                                   ImVec2(cell_max.x, cell_min.y + corner),
                                   color);
    }
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

    if (const auto* room = rooms_->GetIfLoaded(room_id); room != nullptr) {
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

  // ZScream's MapPicturebox uses CGRAM [4, 2]: the fourth opaque
  // color in the first dungeon row (stored index 3; transparent is implicit).
  // Resolve the palette through the same two-level lookup as Room rendering.
  // Header-only reads avoid materializing/rendering all 296 rooms, and reading
  // GameData each frame keeps unsaved palette edits visible immediately.
  ImU32 GetRoomColor(int room_id, const AgentUITheme& theme) const {
    if (rooms_ && rooms_->rom() && rooms_->rom()->is_loaded() &&
        rooms_->game_data() && room_id >= 0 &&
        room_id < static_cast<int>(DungeonRoomStore::kRoomCount)) {
      const auto& palettes = rooms_->game_data()->palette_groups.dungeon_main;
      if (!palettes.empty()) {
        int palette_id;
        if (const auto* room = rooms_->GetIfLoaded(room_id)) {
          palette_id = room->ResolveDungeonPaletteId();
        } else {
          auto header = zelda3::LoadRoomHeaderFromRom(rooms_->rom(), room_id);
          header.SetGameData(rooms_->game_data());
          palette_id = header.ResolveDungeonPaletteId();
        }
        const auto& palette = palettes.palette_ref(palette_id);
        if (palette.size() > 3) {
          const auto color = palette[3].rgb();
          return IM_COL32(color.x, color.y, color.z, 255);
        }
      }
    }
    return ImGui::ColorConvertFloat4ToU32(theme.panel_bg_color);
  }

  int* current_room_id_ = nullptr;
  ImVector<int>* active_rooms_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<void(int, int)> on_room_swap_;
  std::function<void(int, RoomSelectionIntent)> on_room_intent_;
  std::function<void(int, int)> on_room_data_swap_request_;
  std::function<void()> draw_room_data_swap_dialog_;
  std::function<void(int, int)> on_room_clone_request_;
  RoomCompositeOutput tooltip_composite_output_;
  RoomMatrixCensusOverlay census_;
  bool navigation_shortcut_focus_ = false;
  int navigation_shortcut_last_draw_frame_ = -1;
  char search_filter_[64] = "";
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_ROOM_MATRIX_CONTENT_H_
