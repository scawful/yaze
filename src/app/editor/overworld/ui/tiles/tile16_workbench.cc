// Tile16 editor workbench UI (draw/layout). Domain logic: tile16/tile16_edit_session.
#include "app/editor/overworld/tile16_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/tile16_editor_action_state.h"
#include "app/editor/overworld/tile16_editor_shortcuts.h"
#include "app/editor/overworld/tile8_source_interaction.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/tile16_metadata.h"
#include "zelda3/overworld/tile16_renderer.h"
#include "zelda3/overworld/tile16_stamp.h"
#include "zelda3/overworld/tile16_usage_index.h"

namespace yaze {
namespace editor {

namespace {

const char* EditModeLabel(Tile16EditMode mode) {
  switch (mode) {
    case Tile16EditMode::kPaint:
      return "Paint";
    case Tile16EditMode::kPick:
      return "Pick";
    case Tile16EditMode::kUsageProbe:
      return "Usage Probe";
    default:
      return "Paint";
  }
}

gfx::TileInfo& TileInfoForQuadrant(gfx::Tile16* tile, int quadrant) {
  return zelda3::MutableTile16QuadrantInfo(*tile, quadrant);
}

void SyncTilesInfoArray(gfx::Tile16* tile) {
  zelda3::SyncTile16TilesInfo(tile);
}

}  // namespace

using namespace ImGui;

constexpr float kTile8DisplayScale = 4.0f;

void Tile16Editor::DrawContextMenu() {
  if (BeginPopup("##Tile16EditorContextMenu")) {
    if (BeginMenu(tr("View"))) {
      if (MenuItem(tr("Show Grid"), nullptr, session_.show_tile_grid())) {
        *session_.mutable_show_tile_grid() = !session_.show_tile_grid();
      }
      if (MenuItem(tr("Show Collision IDs"), nullptr,
                   session_.show_tile_collision_ids())) {
        *session_.mutable_show_tile_collision_ids() =
            !session_.show_tile_collision_ids();
        *tile8_source_canvas_.custom_labels_enabled() =
            session_.show_tile_collision_ids();
      }
      EndMenu();
    }

    if (BeginMenu(tr("Edit"))) {
      if (MenuItem(tr("Copy Current Tile16"), "Ctrl+C")) {
        status_ = CopyTile16ToClipboard(session_.current_tile16());
      }
      if (MenuItem(tr("Paste to Current Tile16"), "Ctrl+V")) {
        status_ = PasteTile16FromClipboard();
      }
      Separator();
      if (MenuItem(tr("Flip Horizontal"), "H")) {
        status_ = FlipTile16Horizontal();
      }
      if (MenuItem(tr("Flip Vertical"), "V")) {
        status_ = FlipTile16Vertical();
      }
      if (MenuItem(tr("Rotate"), "R")) {
        status_ = RotateTile16();
      }
      if (MenuItem(tr("Clear"), "Delete")) {
        status_ = ClearTile16();
      }
      EndMenu();
    }

    if (BeginMenu(tr("File"))) {
      if (MenuItem(tr("Write Pending to ROM"), "Ctrl+S")) {
        status_ = CommitAllChanges();
      }
      if (MenuItem(tr("Refresh Blockset Preview"), "Ctrl+Shift+S")) {
        status_ = CommitChangesToBlockset();
      }
      Separator();
      bool live_preview = session_.live_preview_enabled();
      if (MenuItem(tr("Live Preview"), nullptr, &live_preview)) {
        EnableLivePreview(live_preview);
      }
      EndMenu();
    }

    if (BeginMenu(tr("Scratch Space"))) {
      for (int i = 0; i < 4; i++) {
        std::string slot_name = "Slot " + std::to_string(i + 1);
        if (session_.scratch_slot(i).has_data) {
          if (MenuItem((slot_name + " (Load)").c_str())) {
            status_ = LoadTile16FromScratchSpace(i);
          }
          if (MenuItem((slot_name + " (Save)").c_str())) {
            status_ = SaveTile16ToScratchSpace(i);
          }
          if (MenuItem((slot_name + " (Clear)").c_str())) {
            status_ = ClearScratchSpace(i);
          }
        } else {
          if (MenuItem((slot_name + " (Save)").c_str())) {
            status_ = SaveTile16ToScratchSpace(i);
          }
        }
        if (i < 3)
          Separator();
      }
      EndMenu();
    }

    EndPopup();
  }
}
absl::Status Tile16Editor::UpdateTile16Edit() {
  static bool show_advanced_controls = false;
  static bool show_debug_info = false;

  gui::StyleVarGuard header_var_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(8, 4)},
       {ImGuiStyleVar_ItemSpacing, ImVec2(8, 4)}});

  const bool has_pending = has_pending_changes();
  const bool current_tile_pending = is_tile_modified(session_.current_tile16());
  const int pending_count = pending_changes_count();

  RETURN_IF_ERROR(DrawCompactActionStatusRow(has_pending, current_tile_pending,
                                             pending_count, &show_debug_info,
                                             &show_advanced_controls));

  ImGui::Separator();

  const int total_tiles =
      zelda3::ComputeTile16Count(session_.tile16_blockset());
  const float layout_width = ImGui::GetContentRegionAvail().x;
  const bool compact_layout = layout_width < 900.0f;
  const int layout_columns = compact_layout ? 1 : 2;

  if (ImGui::BeginTable(
          "##Tile16EditLayout", layout_columns,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
    if (compact_layout) {
      ImGui::TableSetupColumn("Tile16 Workbench",
                              ImGuiTableColumnFlags_WidthStretch, 1.0f);
    } else {
      ImGui::TableSetupColumn("Tile8 Source",
                              ImGuiTableColumnFlags_WidthStretch, 0.58f);
      ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch,
                              0.42f);
    }

    if (compact_layout) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::BeginGroup();
      gui::LayoutHelpers::SectionHeader(tr("Tile16 Editor"));
      RETURN_IF_ERROR(DrawTile16NavigationHeader(total_tiles));
      RETURN_IF_ERROR(DrawTile16EditorWorkbenchColumn(show_debug_info,
                                                      show_advanced_controls));
      ImGui::EndGroup();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::BeginGroup();
      gui::LayoutHelpers::SectionHeader(tr("Tile8 Source"));
      RETURN_IF_ERROR(DrawTile8SourcePanel(320.0f));
      ImGui::EndGroup();
    } else {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::BeginGroup();
      gui::LayoutHelpers::SectionHeader(tr("Tile8 Source"));
      RETURN_IF_ERROR(DrawTile8SourcePanel(0.0f));
      ImGui::EndGroup();

      ImGui::TableNextColumn();
      ImGui::BeginGroup();
      gui::LayoutHelpers::SectionHeader(tr("Tile16 Editor"));
      RETURN_IF_ERROR(DrawTile16NavigationHeader(total_tiles));
      RETURN_IF_ERROR(DrawTile16EditorWorkbenchColumn(show_debug_info,
                                                      show_advanced_controls));
      ImGui::EndGroup();
    }

    ImGui::EndTable();
  }

  DrawPaletteSettings();

  blockset_canvas_.ShowAdvancedCanvasProperties();
  blockset_canvas_.ShowScalingControls();
  tile8_source_canvas_.ShowAdvancedCanvasProperties();
  tile8_source_canvas_.ShowScalingControls();
  tile16_edit_canvas_.ShowAdvancedCanvasProperties();
  tile16_edit_canvas_.ShowScalingControls();

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawTile16NavigationHeader(int total_tiles) {
  gui::StyleVarGuard nav_spacing_guard(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

  ImGui::TextDisabled(tr("0x%03X / 0x%03X"), session_.current_tile16(),
                      std::max(0, total_tiles - 1));
  ImGui::SameLine();

  ImGui::SetNextItemWidth(80);
  if (ImGui::InputInt("##JumpToTile", session_.mutable_jump_to_tile_id(), 0,
                      0)) {
    *session_.mutable_jump_to_tile_id() =
        std::clamp(session_.jump_to_tile_id(), 0, total_tiles - 1);
    if (session_.jump_to_tile_id() != session_.current_tile16()) {
      RequestTileSwitch(session_.jump_to_tile_id());
      session_.set_scroll_to_current(true);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Tile ID (0-%d) - navigates as you type"),
                      total_tiles - 1);
  }

  const int total_pages = (total_tiles + Tile16EditSession::kTilesPerPage - 1) /
                          Tile16EditSession::kTilesPerPage;
  session_.set_current_page(session_.current_tile16() /
                            Tile16EditSession::kTilesPerPage);

  ImGui::SameLine();
  if (ImGui::Button("<<")) {
    RequestTileSwitch(0);
    session_.set_scroll_to_current(true);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(tr("First page"));

  ImGui::SameLine();
  if (ImGui::Button("<")) {
    int new_tile = std::max(
        0, session_.current_tile16() - Tile16EditSession::kTilesPerPage);
    RequestTileSwitch(new_tile);
    session_.set_scroll_to_current(true);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(tr("Previous page (PageUp)"));

  ImGui::SameLine();
  ImGui::TextDisabled(tr("Page %d/%d"), session_.current_page() + 1,
                      total_pages);

  ImGui::SameLine();
  if (ImGui::Button(">")) {
    int new_tile =
        std::min(total_tiles - 1,
                 session_.current_tile16() + Tile16EditSession::kTilesPerPage);
    RequestTileSwitch(new_tile);
    session_.set_scroll_to_current(true);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(tr("Next page (PageDown)"));

  ImGui::SameLine();
  if (ImGui::Button(">>")) {
    RequestTileSwitch(total_tiles - 1);
    session_.set_scroll_to_current(true);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(tr("Last page"));

  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
      int new_tile = std::max(
          0, session_.current_tile16() - Tile16EditSession::kTilesPerPage);
      RequestTileSwitch(new_tile);
      session_.set_scroll_to_current(true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
      int new_tile =
          std::min(total_tiles - 1, session_.current_tile16() +
                                        Tile16EditSession::kTilesPerPage);
      RequestTileSwitch(new_tile);
      session_.set_scroll_to_current(true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
      RequestTileSwitch(0);
      session_.set_scroll_to_current(true);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) {
      RequestTileSwitch(total_tiles - 1);
      session_.set_scroll_to_current(true);
    }

    if (!ImGui::GetIO().KeyCtrl) {
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) &&
          session_.current_tile16() > 0) {
        RequestTileSwitch(session_.current_tile16() - 1);
        session_.set_scroll_to_current(true);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) &&
          session_.current_tile16() < total_tiles - 1) {
        RequestTileSwitch(session_.current_tile16() + 1);
        session_.set_scroll_to_current(true);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) &&
          session_.current_tile16() >= Tile16EditSession::kTilesPerRow) {
        RequestTileSwitch(session_.current_tile16() -
                          Tile16EditSession::kTilesPerRow);
        session_.set_scroll_to_current(true);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) &&
          session_.current_tile16() + Tile16EditSession::kTilesPerRow <
              total_tiles) {
        RequestTileSwitch(session_.current_tile16() +
                          Tile16EditSession::kTilesPerRow);
        session_.set_scroll_to_current(true);
      }
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawTile16EditorWorkbenchColumn(
    bool show_debug_info, bool show_advanced_controls) {
  // Fixed size container to prevent canvas expansion
  if (ImGui::BeginChild(
          "##Tile16FixedCanvas", ImVec2(90, 90), true,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    // Configure canvas frame options for tile16 editor
    gui::CanvasFrameOptions tile16_edit_frame_opts;
    tile16_edit_frame_opts.canvas_size = ImVec2(64, 64);
    tile16_edit_frame_opts.draw_grid = session_.show_tile_grid();
    tile16_edit_frame_opts.grid_step =
        kTile8Size *
        kTile8DisplayScale;  // 8x8 source pixels at 4x preview scale.
    tile16_edit_frame_opts.draw_context_menu = true;
    tile16_edit_frame_opts.draw_overlay = true;
    tile16_edit_frame_opts.render_popups = true;
    tile16_edit_frame_opts.use_child_window = false;

    auto tile16_edit_rt =
        gui::BeginCanvas(tile16_edit_canvas_, tile16_edit_frame_opts);

    // Draw current tile16 bitmap with dynamic zoom
    if (session_.current_tile16_bmp().is_active()) {
      tile16_edit_canvas_.DrawBitmap(session_.current_tile16_bmp(), 0, 0,
                                     kTile8DisplayScale);
    }

    // Handle tile8 painting with improved hover preview
    if (session_.current_tile8() >= 0 &&
        session_.current_tile8() <
            static_cast<int>(
                session_.mutable_current_gfx_individual().size())) {
      // Create a display tile that shows the current palette selection
      if (!session_.mutable_tile8_preview_bitmap().is_active()) {
        session_.mutable_tile8_preview_bitmap().Create(
            8, 8, 8, std::vector<uint8_t>(kTile8PixelCount, 0));
      }

      // Get the original pixel data (already has sheet offsets from
      // ProcessGraphicsBuffer)
      auto& preview_data =
          session_.mutable_tile8_preview_bitmap().mutable_data();
      std::copy(
          session_.mutable_current_gfx_individual()[session_.current_tile8()]
              .begin(),
          session_.mutable_current_gfx_individual()[session_.current_tile8()]
              .end(),
          preview_data.begin());

      // Apply the correct sheet-aware palette slice for the preview
      const gfx::SnesPalette* display_palette = nullptr;
      if (session_.overworld_palette().size() >= 256) {
        display_palette = &session_.overworld_palette();
      } else if (session_.palette().size() >= 256) {
        display_palette = &session_.palette();
      } else {
        display_palette = session_.current_gfx_bmp()
                              ? &session_.current_gfx_bmp()->palette()
                              : &session_.palette();
      }

      if (display_palette && !display_palette->empty()) {
        session_.mutable_tile8_preview_bitmap().SetPalette(
            CreateRemappedPaletteForViewing(*display_palette,
                                            session_.current_palette()));
      }

      // Apply flips if needed
      if (session_.x_flip() || session_.y_flip()) {
        auto& data = session_.mutable_tile8_preview_bitmap().mutable_data();

        if (session_.x_flip()) {
          for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 4; ++x) {
              std::swap(data[y * 8 + x], data[y * 8 + (7 - x)]);
            }
          }
        }

        if (session_.y_flip()) {
          for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 8; ++x) {
              std::swap(data[y * 8 + x], data[(7 - y) * 8 + x]);
            }
          }
        }
      }

      // Push pixel changes to the existing surface before queuing texture work
      session_.mutable_tile8_preview_bitmap().UpdateSurfacePixels();

      // Queue texture creation/update on the persistent preview bitmap to
      // avoid dangling stack pointers in the arena queue
      const auto preview_command =
          session_.mutable_tile8_preview_bitmap().texture()
              ? gfx::Arena::TextureCommandType::UPDATE
              : gfx::Arena::TextureCommandType::CREATE;
      gfx::Arena::Get().QueueTextureCommand(
          preview_command, &session_.mutable_tile8_preview_bitmap());

      // CRITICAL FIX: Handle tile painting with simple click instead of
      // click+drag Draw the preview first
      tile16_edit_canvas_.DrawTilePainter(
          session_.mutable_tile8_preview_bitmap(), 8, kTile8DisplayScale);

      const bool left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

      if (left_clicked || right_clicked) {
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
        ImVec2 mouse_pos =
            ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

        // The hover tile painter and bitmap share a 0,0 origin, matching
        // ZScream/HMagic's direct preview hit-test behavior.
        const ImVec2 tile_position =
            Tile16PreviewDisplayPixelToTilePosition(mouse_pos);
        int tile_x = static_cast<int>(tile_position.x);
        int tile_y = static_cast<int>(tile_position.y);

        // Clamp to valid range (0-15 for 16x16 tile)
        tile_x = std::max(0, std::min(15, tile_x));
        tile_y = std::max(0, std::min(15, tile_y));

        util::logf(
            "Tile16 canvas click: (%.2f, %.2f) -> Tile16: (%d, %d), mode=%s",
            mouse_pos.x, mouse_pos.y, tile_x, tile_y,
            EditModeLabel(session_.edit_mode()));

        RETURN_IF_ERROR(HandleTile16CanvasClick(ImVec2(tile_x, tile_y),
                                                left_clicked, right_clicked));
      }
    }

    gui::EndCanvas(tile16_edit_canvas_, tile16_edit_rt, tile16_edit_frame_opts);
  }
  ImGui::EndChild();

  Separator();

  // === Compact Controls Section ===

  // Tile8 info and preview
  if (session_.current_tile8() >= 0 &&
      session_.current_tile8() <
          static_cast<int>(session_.mutable_current_gfx_individual().size())) {
    Text(tr("Tile8: %02X"), session_.current_tile8());
    SameLine();
    auto* tile8_texture = session_.mutable_tile8_preview_bitmap().texture();
    if (tile8_texture) {
      ImGui::Image((ImTextureID)(intptr_t)tile8_texture, ImVec2(24, 24));
    }

    const int sheet_idx = GetSheetIndexForTile8(session_.current_tile8());
    ImGui::SameLine();
    ImGui::TextDisabled(tr("G%d"), sheet_idx);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text(tr("Graphics chunk: %d"), sheet_idx);
      ImGui::EndTooltip();
    }
  }

  // Tile8 transform options in compact form
  Checkbox(tr("X Flip"), session_.mutable_x_flip());
  SameLine();
  Checkbox(tr("Y Flip"), session_.mutable_y_flip());
  SameLine();
  Checkbox(tr("Priority"), session_.mutable_priority_tile());

  Text(tr("Stamp:"));
  SameLine();
  if (ImGui::RadioButton(tr("1x"), session_.tile8_stamp_size() == 1)) {
    session_.set_tile8_stamp_size(1);
  }
  SameLine();
  if (ImGui::RadioButton(tr("2x"), session_.tile8_stamp_size() == 2)) {
    session_.set_tile8_stamp_size(2);
  }
  SameLine();
  if (ImGui::RadioButton(tr("4x"), session_.tile8_stamp_size() == 4)) {
    session_.set_tile8_stamp_size(4);
  }
  HOVER_HINT(
      "1x: paint one quadrant\n2x: fill current tile16 from a 2x2 tile8 "
      "block\n4x: stamp a 2x2 tile16 patch from a 4x4 tile8 block");

  Text(tr("Edit Mode:"));
  if (ImGui::RadioButton(tr("Paint (P)"),
                         session_.edit_mode() == Tile16EditMode::kPaint)) {
    session_.set_edit_mode(Tile16EditMode::kPaint);
  }
  SameLine();
  if (ImGui::RadioButton(tr("Pick (I)"),
                         session_.edit_mode() == Tile16EditMode::kPick)) {
    session_.set_edit_mode(Tile16EditMode::kPick);
  }
  SameLine();
  if (ImGui::RadioButton(tr("Usage (U)"),
                         session_.edit_mode() == Tile16EditMode::kUsageProbe)) {
    session_.set_edit_mode(Tile16EditMode::kUsageProbe);
    *session_.mutable_highlight_tile8_usage() = true;
  }
  HOVER_HINT(
      "Paint: left-click places Tile8 into Tile16.\n"
      "Pick: left-click samples Tile8 from Tile16.\n"
      "Usage: keeps usage overlay visible and samples on click.\n"
      "Right-click on Tile16 preview always samples.");

  if (session_.edit_mode() == Tile16EditMode::kUsageProbe ||
      session_.highlight_tile8_usage()) {
    if (ImGui::CollapsingHeader(tr("Usage Navigator"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      const float nav_height = 220.0f;
      if (BeginChild("##UsageBlocksetNav", ImVec2(0, nav_height), true,
                     ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        if (blockset_selector_.GetSelectedTileID() !=
            session_.current_tile16()) {
          blockset_selector_.SetSelectedTile(session_.current_tile16());
        }
        gui::CanvasFrameOptions opts;
        opts.draw_grid = session_.show_tile_grid();
        opts.grid_step = 32.0f;
        opts.draw_overlay = true;
        opts.use_child_window = false;
        auto rt = gui::BeginCanvas(blockset_canvas_, opts);
        if (session_.tile16_blockset_bmp() != nullptr) {
          auto result = blockset_selector_.Render(
              *session_.tile16_blockset_bmp(),
              session_.tile16_blockset_bmp()->is_active());
          if (result.selection_changed) {
            RequestTileSwitch(result.selected_tile);
          }
        }
        DrawTile8UsageOverlay();
        gui::EndCanvas(blockset_canvas_, rt, opts);
      }
      EndChild();
    }
  }

  Separator();

  RETURN_IF_ERROR(DrawBrushAndTilePaletteControls(show_debug_info));

  Separator();

  RETURN_IF_ERROR(DrawPrimaryActionControls());

  // Advanced controls (collapsible)
  if (show_advanced_controls) {
    Separator();
    Text(tr("Advanced:"));

    if (Button(tr("Palette Settings"), ImVec2(-1, 0))) {
      *session_.mutable_show_palette_settings() =
          !session_.show_palette_settings();
    }

    if (Button(tr("Analyze Data"), ImVec2(-1, 0))) {
      AnalyzeTile8SourceData();
    }
    HOVER_HINT("Analyze tile8 source data format and palette state");

    if (Button(tr("Manual Edit"), ImVec2(-1, 0))) {
      ImGui::OpenPopup("ManualTile8Editor");
    }

    if (Button(tr("Refresh Blockset"), ImVec2(-1, 0))) {
      RETURN_IF_ERROR(RefreshTile16Blockset());
    }

    // Scratch space in compact form
    Text(tr("Scratch:"));
    DrawScratchSpace();

    // Manual tile8 editor popup
    DrawManualTile8Inputs();
  }

  // Compact debug information panel
  if (show_debug_info) {
    Separator();
    Text(tr("Debug:"));
    ImGui::TextDisabled(tr("T16:%02X T8:%d Pal:%d"), session_.current_tile16(),
                        session_.current_tile8(), session_.current_palette());

    if (session_.current_tile8() >= 0) {
      int sheet_index = GetSheetIndexForTile8(session_.current_tile8());
      int actual_slot =
          GetActualPaletteSlot(session_.current_palette(), sheet_index);
      ImGui::TextDisabled(tr("Sheet:%d Slot:%d"), sheet_index, actual_slot);
    }

    // Compact palette mapping table
    if (ImGui::CollapsingHeader(tr("Palette Map"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginChild("##PaletteMappingScroll", ImVec2(0, 120), true);
      if (ImGui::BeginTable("##PalMap", 2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Btn", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("CGRAM", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 8; ++i) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%d", i);
          ImGui::TableNextColumn();
          ImGui::Text(tr("0x%02X"), GetActualPaletteSlot(i, 0));
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();
    }

    // Color preview - compact
    if (ImGui::CollapsingHeader(tr("Colors"))) {
      if (session_.overworld_palette().size() >= 256) {
        int actual_slot = GetActualPaletteSlotForCurrentTile16();
        ImGui::Text(tr("Slot %d:"), actual_slot);

        for (int i = 0;
             i < 8 && (actual_slot + i) <
                          static_cast<int>(session_.overworld_palette().size());
             ++i) {
          int color_index = actual_slot + i;
          auto color = session_.overworld_palette()[color_index];
          ImVec4 display_color = color.rgb();

          ImGui::ColorButton(absl::StrFormat("##c%d", i).c_str(), display_color,
                             ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(tr("%d:0x%04X"), color_index, color.snes());
          }

          if ((i + 1) % 4 != 0)
            ImGui::SameLine();
        }
      }
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawCompactActionStatusRow(
    bool has_pending, bool current_tile_pending, int pending_count,
    bool* show_debug_info, bool* show_advanced_controls) {
  const Tile16ActionControlState action_state =
      ComputeTile16ActionControlState(has_pending, current_tile_pending,
                                      session_.CanUndo(), session_.CanRedo());
  const float available_width = ImGui::GetContentRegionAvail().x;
  const int action_count = 7;
  const int columns = ComputeTile16CompactActionColumnCount(available_width);
  const int rows = ComputeTile16ActionRowCount(action_count, columns);
  const float row_height = ImGui::GetTextLineHeightWithSpacing() +
                           rows * ImGui::GetFrameHeightWithSpacing() +
                           ImGui::GetStyle().FramePadding.y * 2.0f;
  absl::Status action_status = absl::OkStatus();

  if (ImGui::BeginChild(
          "##Tile16CompactStatus", ImVec2(0, row_height), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::Text(tr("Tile16 0x%03X"), session_.current_tile16());
    ImGui::SameLine();
    if (has_pending) {
      ImGui::TextDisabled(tr("%s, %d pending"),
                          current_tile_pending ? "dirty" : "clean",
                          pending_count);
    } else {
      ImGui::TextDisabled(tr("clean"));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", EditModeLabel(session_.edit_mode()));
    if (show_debug_info != nullptr && *show_debug_info &&
        session_.has_rom_write_history()) {
      const auto seconds_since_write =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - session_.last_rom_write_time())
              .count();
      ImGui::SameLine();
      ImGui::TextDisabled(tr("| Last write: %d tile%s, %lds ago"),
                          session_.last_rom_write_count(),
                          session_.last_rom_write_count() == 1 ? "" : "s",
                          static_cast<long>(seconds_since_write));
    }

    if (ImGui::BeginTable("##Tile16CompactActions", columns,
                          ImGuiTableFlags_SizingStretchProp)) {
      int action_index = 0;
      auto next_action_cell = [&]() {
        if (action_index % columns == 0) {
          ImGui::TableNextRow();
        }
        ImGui::TableNextColumn();
        ++action_index;
      };

      next_action_cell();
      if (!action_state.can_write_pending) {
        ImGui::BeginDisabled();
      }
      if (gui::SuccessButton("Write Pending",
                             ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = CommitAllChanges();
      }
      if (!action_state.can_write_pending) {
        ImGui::EndDisabled();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Write all %d pending Tile16 edits to ROM"),
                          pending_count);
      }

      next_action_cell();
      if (!action_state.can_discard_current) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Discard Current"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        DiscardCurrentTileChanges();
      }
      if (!action_state.can_discard_current) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_discard_all) {
        ImGui::BeginDisabled();
      }
      if (gui::DangerButton("Discard All",
                            ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        DiscardAllChanges();
      }
      if (!action_state.can_discard_all) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_undo) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Undo"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = Undo();
      }
      if (!action_state.can_undo) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_redo) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Redo"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = Redo();
      }
      if (!action_state.can_redo) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      const char* advanced_label = *show_advanced_controls
                                       ? "Advanced*##Tile16AdvancedToggle"
                                       : "Advanced##Tile16AdvancedToggle";
      if (ImGui::Button(advanced_label,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        *show_advanced_controls = !*show_advanced_controls;
      }

      next_action_cell();
      const char* debug_label = *show_debug_info ? "Debug*##Tile16DebugToggle"
                                                 : "Debug##Tile16DebugToggle";
      if (ImGui::Button(debug_label,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        *show_debug_info = !*show_debug_info;
      }

      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
  return action_status;
}

absl::Status Tile16Editor::DrawBrushAndTilePaletteControls(
    bool show_debug_info) {
  // Palette selector - this is the paint brush palette for new placements.
  Text(tr("Brush Palette:"));
  if (show_debug_info) {
    SameLine();
    int actual_slot = GetActualPaletteSlotForCurrentTile16();
    ImGui::TextDisabled(tr("(Slot %d)"), actual_slot);
  }
  ImGui::TextDisabled(tr("Used for new tile8 placements"));

  // Compact palette grid
  ImGui::BeginGroup();
  float available_width = ImGui::GetContentRegionAvail().x;
  float button_size = ComputeTile16PaletteButtonSize(available_width);
  const gui::ButtonColorSet default_button_colors{
      gui::GetThemeColor(ImGuiCol_Button),
      gui::GetThemeColor(ImGuiCol_ButtonHovered),
      gui::GetThemeColor(ImGuiCol_ButtonActive),
  };
  const gui::ButtonColorSet selected_button_colors =
      gui::GetSuccessButtonColors();

  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (col > 0)
        ImGui::SameLine();

      int i = row * 4 + col;
      bool is_current = (session_.current_palette() == i);

      // Modern button styling with better visual hierarchy
      ImGui::PushID(i);

      const gui::ButtonColorSet& palette_button_colors =
          is_current ? selected_button_colors : default_button_colors;
      const ImVec4 palette_button_border =
          is_current ? gui::GetSuccessColor()
                     : gui::GetThemeColor(ImGuiCol_Border);
      gui::StyleColorGuard palette_btn_colors(
          {{ImGuiCol_Button, palette_button_colors.button},
           {ImGuiCol_ButtonHovered, palette_button_colors.hovered},
           {ImGuiCol_ButtonActive, palette_button_colors.active},
           {ImGuiCol_Border, palette_button_border}});
      gui::StyleVarGuard palette_btn_border(ImGuiStyleVar_FrameBorderSize,
                                            1.0f);

      if (ImGui::Button(absl::StrFormat("%d", i).c_str(),
                        ImVec2(button_size, button_size))) {
        if (session_.current_palette() != i) {
          session_.set_current_palette(i);
          auto status = RefreshAllPalettes();
          if (!status.ok()) {
            util::logf("Failed to refresh palettes: %s",
                       status.message().data());
          } else {
            util::logf("Palette successfully changed to %d",
                       session_.current_palette());
          }
        }
      }

      ImGui::PopID();

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        session_.set_current_palette(static_cast<uint8_t>(i));
        RETURN_IF_ERROR(ApplyPaletteToAll(session_.current_palette()));
      }

      // Tooltip with palette info
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (show_debug_info) {
          ImGui::Text(tr("Palette %d -> CGRAM 0x%02X"), i,
                      GetActualPaletteSlot(i, 0));
        } else {
          ImGui::Text(tr("Brush Palette %d"), i);
          ImGui::TextDisabled(tr("Applied to new tile8 placements"));
          ImGui::TextDisabled(tr("RMB: apply to all tile quadrants"));
          ImGui::TextDisabled(tr("Quadrant metadata is shown in strip below"));
          ImGui::TextDisabled(
              tr("Hotkeys: Ctrl+1..8 palette, 1..4 quadrant focus"));
          if (is_current) {
            gui::ThemedText("Active", gui::SemanticColor::Success);
          }
        }
        ImGui::EndTooltip();
      }
    }
  }
  ImGui::EndGroup();

  if (auto* tile_data = GetCurrentTile16Data(); tile_data != nullptr) {
    session_.set_active_quadrant(std::clamp(session_.active_quadrant(), 0, 3));
    Text(tr("Quadrant Focus:"));
    SameLine();
    ImGui::TextDisabled("1-4");
    static constexpr std::array<const char*, 4> kQuadrantLabels = {"TL", "TR",
                                                                   "BL", "BR"};
    const float quadrant_button_width = std::max(58.0f, button_size + 24.0f);
    const gui::ButtonColorSet active_quadrant_colors =
        gui::GetPrimaryButtonColors();
    for (int q = 0; q < 4; ++q) {
      if (q > 0) {
        SameLine();
      }

      const gfx::TileInfo& info = zelda3::Tile16QuadrantInfo(*tile_data, q);
      const uint8_t quadrant_palette = info.palette_;
      const bool is_active_quadrant = (session_.active_quadrant() == q);
      const bool matches_brush =
          (quadrant_palette == session_.current_palette());

      ImGui::PushID(100 + q);
      const gui::ButtonColorSet& quadrant_button_colors =
          is_active_quadrant ? active_quadrant_colors
                             : (matches_brush ? selected_button_colors
                                              : default_button_colors);
      const ImVec4 quadrant_border =
          is_active_quadrant
              ? gui::GetAccentColor()
              : (matches_brush ? gui::GetSuccessColor()
                               : gui::GetThemeColor(ImGuiCol_Border));
      gui::StyleColorGuard quadrant_btn_colors(
          {{ImGuiCol_Button, quadrant_button_colors.button},
           {ImGuiCol_ButtonHovered, quadrant_button_colors.hovered},
           {ImGuiCol_ButtonActive, quadrant_button_colors.active},
           {ImGuiCol_Border, quadrant_border}});
      gui::StyleVarGuard quadrant_btn_border(ImGuiStyleVar_FrameBorderSize,
                                             is_active_quadrant ? 2.0f : 1.0f);

      if (ImGui::Button(absl::StrFormat("%d %s:%d", q + 1, kQuadrantLabels[q],
                                        quadrant_palette)
                            .c_str(),
                        ImVec2(quadrant_button_width, 0))) {
        session_.set_active_quadrant(q);
        if (session_.current_palette() != quadrant_palette) {
          session_.set_current_palette(quadrant_palette);
          auto status = RefreshAllPalettes();
          if (!status.ok()) {
            util::logf("Failed to refresh palettes: %s",
                       status.message().data());
          }
        }
      }

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        session_.set_active_quadrant(q);
        RETURN_IF_ERROR(ApplyPaletteToQuadrant(q, session_.current_palette()));
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(tr("Quadrant %s metadata"), kQuadrantLabels[q]);
        ImGui::Separator();
        ImGui::Text(tr("Tile8: %02X"), info.id_);
        ImGui::Text(tr("Palette: %d"), quadrant_palette);
        ImGui::Text(tr("Flip: H:%s V:%s"), info.horizontal_mirror_ ? "Y" : "N",
                    info.vertical_mirror_ ? "Y" : "N");
        ImGui::Text(tr("Priority: %s"), info.over_ ? "Y" : "N");
        ImGui::TextDisabled(tr("LMB: set brush palette from this quadrant"));
        ImGui::TextDisabled(
            tr("RMB: apply current brush palette to this quadrant"));
        ImGui::TextDisabled(tr("Keys 1..4: focus TL/TR/BL/BR"));
        ImGui::EndTooltip();
      }

      ImGui::PopID();
    }

    const gfx::TileInfo& active_info =
        zelda3::Tile16QuadrantInfo(*tile_data, session_.active_quadrant());
    ImGui::TextDisabled(tr("Active %s: Tile8 %02X | P%d | H:%s V:%s | Pri:%s"),
                        kQuadrantLabels[session_.active_quadrant()],
                        active_info.id_, active_info.palette_,
                        active_info.horizontal_mirror_ ? "Y" : "N",
                        active_info.vertical_mirror_ ? "Y" : "N",
                        active_info.over_ ? "Y" : "N");

    if (Button(tr("Apply Brush to Active Quadrant"), ImVec2(-1, 0))) {
      RETURN_IF_ERROR(ApplyPaletteToQuadrant(session_.active_quadrant(),
                                             session_.current_palette()));
    }
    HOVER_HINT(
        "Copy the Brush Palette into the selected quadrant metadata.\n"
        "Use keys 1..4 to change active quadrant quickly.");
  }

  // Copy the current brush palette into all stored quadrant palette fields.
  if (Button(tr("Apply Brush to All Quadrants"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(ApplyPaletteToAll(session_.current_palette()));
  }
  HOVER_HINT(
      "Copy the Brush Palette into Tile Palette metadata for all 4 "
      "quadrants.\n"
      "Tip: right-click any brush palette button above for a one-step apply.");
  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawTile8SourcePanel(float preferred_height) {
  *session_.mutable_show_tile_collision_ids() =
      *tile8_source_canvas_.custom_labels_enabled();
  ImGui::Text(tr("Tile8 Source"));
  ImGui::SameLine();
  ImGui::Checkbox(tr("Grid##Tile16GridToggle"),
                  session_.mutable_show_tile_grid());
  ImGui::SameLine();
  if (ImGui::Checkbox(tr("IDs##Tile16CollisionIds"),
                      session_.mutable_show_tile_collision_ids())) {
    *tile8_source_canvas_.custom_labels_enabled() =
        session_.show_tile_collision_ids();
  }

  int source_bitmap_height = 0;
  tile8_source_canvas_.set_draggable(false);
  *tile8_source_canvas_.custom_labels_enabled() =
      session_.show_tile_collision_ids();
  if (session_.current_gfx_bmp() != nullptr &&
      session_.current_gfx_bmp()->is_active()) {
    *session_.mutable_tile8_source_display_scale() =
        ComputeTile8SourceDisplayScale(ImGui::GetContentRegionAvail().x,
                                       session_.current_gfx_bmp()->width());
    source_bitmap_height = session_.current_gfx_bmp()->height();
    tile8_source_canvas_.SetCanvasSize(
        ImVec2(session_.current_gfx_bmp()->width() *
                   session_.tile8_source_display_scale(),
               session_.current_gfx_bmp()->height() *
                   session_.tile8_source_display_scale()));
    ImGui::SameLine();
    ImGui::TextDisabled(tr("%.1fx"), session_.tile8_source_display_scale());
  }

  const float panel_height = ComputeTile8SourcePanelHeight(
      ImGui::GetContentRegionAvail().y, source_bitmap_height,
      session_.tile8_source_display_scale(), preferred_height);

  if (BeginChild("##Tile8SourceScrollable", ImVec2(0, panel_height), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    gui::CanvasFrameOptions tile8_frame_opts;
    tile8_frame_opts.draw_grid = session_.show_tile_grid();
    tile8_frame_opts.grid_step =
        8.0f * session_.tile8_source_display_scale();  // Tile8 grid
    tile8_frame_opts.draw_context_menu = true;
    tile8_frame_opts.draw_overlay = true;
    tile8_frame_opts.render_popups = true;
    tile8_frame_opts.use_child_window = false;

    auto tile8_rt = gui::BeginCanvas(tile8_source_canvas_, tile8_frame_opts);

    tile8_source_canvas_.DrawTileSelector(
        8.0F * session_.tile8_source_display_scale());

    const bool left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    // ZScream parity: hold right-click on tile8 source to show usage overlay.
    const bool temporary_usage = ComputeTile8UsageHighlight(
        ImGui::IsItemHovered(), ImGui::IsMouseDown(ImGuiMouseButton_Right));
    *session_.mutable_highlight_tile8_usage() =
        (session_.edit_mode() == Tile16EditMode::kUsageProbe) ||
        temporary_usage;

    if (left_clicked || right_clicked) {
      RETURN_IF_ERROR(HandleTile8SourceSelection(
          right_clicked, session_.tile8_source_display_scale()));
    }

    if (session_.current_gfx_bmp() != nullptr) {
      tile8_source_canvas_.DrawBitmap(*session_.current_gfx_bmp(), 2, 2,
                                      session_.tile8_source_display_scale());
    }

    gui::EndCanvas(tile8_source_canvas_, tile8_rt, tile8_frame_opts);
  }
  EndChild();

  return absl::OkStatus();
}

absl::Status Tile16Editor::HandleTile8SourceSelection(bool right_clicked,
                                                      float display_scale) {
  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = tile8_source_canvas_.zero_point();
  ImVec2 mouse_pos =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

  const int new_tile8 = ComputeTile8IndexFromCanvasMouse(
      mouse_pos.x, mouse_pos.y, session_.current_gfx_bmp()->width(),
      static_cast<int>(session_.mutable_current_gfx_individual().size()),
      display_scale);
  if (new_tile8 < 0 || new_tile8 == session_.current_tile8()) {
    return absl::OkStatus();
  }

  session_.set_current_tile8(new_tile8);
  RETURN_IF_ERROR(UpdateTile8Palette(session_.current_tile8()));
  if (right_clicked) {
    session_.set_tile8_usage_cache_dirty(true);
  }
  util::logf("Selected Tile8: %d", session_.current_tile8());

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawPrimaryActionControls() {
  // Local tile-shaping actions stay in the right column.
  if (Button(tr("Clear"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(ClearTile16());
  }

  if (Button(tr("Copy"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(CopyTile16ToClipboard(session_.current_tile16()));
  }

  if (Button(tr("Paste"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(PasteTile16FromClipboard());
  }

  return absl::OkStatus();
}

void Tile16Editor::DrawTile8UsageOverlay() {
  if (!session_.highlight_tile8_usage() || session_.current_tile8() < 0 ||
      session_.current_tile8() >= zelda3::kMaxTile8UsageId) {
    return;
  }

  if (session_.tile8_usage_cache_dirty()) {
    auto cache_status = session_.RebuildTile8UsageCache();
    if (!cache_status.ok()) {
      util::logf("Tile8 usage cache rebuild failed: %s",
                 cache_status.message().data());
      return;
    }
  }

  const auto& hits = session_.tile8_usage_cache()[session_.current_tile8()];
  if (hits.empty()) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 canvas_pos = blockset_canvas_.zero_point();
  const float scale = blockset_canvas_.GetGlobalScale();
  const float tile16_display = 32.0f * scale;
  const float quadrant_display = 16.0f * scale;
  const int tiles_per_row =
      std::max(1, session_.tile16_blockset_bmp()->width() / kTile16Size);

  for (const auto& hit : hits) {
    const int tile_x = hit.tile16_id % tiles_per_row;
    const int tile_y = hit.tile16_id / tiles_per_row;
    const int quad_x = hit.quadrant % 2;
    const int quad_y = hit.quadrant / 2;

    const ImVec2 min(
        canvas_pos.x + (tile_x * tile16_display) + (quad_x * quadrant_display),
        canvas_pos.y + (tile_y * tile16_display) + (quad_y * quadrant_display));
    const ImVec2 max(min.x + quadrant_display, min.y + quadrant_display);

    // Mirrors ZScream's right-click usage tint (purple, transparent).
    draw_list->AddRectFilled(min, max, IM_COL32(150, 0, 210, 80));
    draw_list->AddRect(min, max, IM_COL32(215, 170, 255, 180));
  }
}

void Tile16Editor::DrawPaletteSettings() {
  if (session_.show_palette_settings()) {
    if (Begin("Advanced Palette Settings",
              session_.mutable_show_palette_settings())) {
      Text(tr("Pixel Normalization & Color Correction:"));

      int mask_value = static_cast<int>(session_.palette_normalization_mask());
      if (SliderInt(tr("Normalization Mask"), &mask_value, 1, 255, "0x%02X")) {
        *session_.mutable_palette_normalization_mask() =
            static_cast<uint8_t>(mask_value);
      }

      Checkbox(tr("Auto Normalize Pixels"),
               session_.mutable_auto_normalize_pixels());

      if (Button(tr("Apply to All Graphics"))) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error: %s"), reload_result.message().data());
        }
      }

      SameLine();
      if (Button(tr("Reset Defaults"))) {
        *session_.mutable_palette_normalization_mask() = 0x0F;
        *session_.mutable_auto_normalize_pixels() = true;
        auto reload_result = LoadTile8();
        (void)reload_result;  // Suppress warning
      }

      Separator();
      Text(tr("Current State:"));
      static constexpr std::array<const char*, 7> palette_group_names = {
          "OW Main", "OW Aux", "OW Anim", "Dungeon",
          "Sprites", "Armor",  "Sword"};
      Text(tr("Palette Group: %d (%s)"), session_.current_palette_group(),
           (session_.current_palette_group() < 7)
               ? palette_group_names[session_.current_palette_group()]
               : "Unknown");
      Text(tr("Current Palette: %d"), session_.current_palette());

      Separator();
      Text(tr("Sheet-Specific Fixes:"));

      // Sheet-specific palette fixes
      static bool fix_sheet_0 = true;
      static bool fix_sprite_sheets = true;
      static bool use_transparent_for_terrain = false;

      if (Checkbox(tr("Fix Sheet 0 (Trees)"), &fix_sheet_0)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT(
          "Use direct palette for sheet 0 instead of transparent palette");

      if (Checkbox(tr("Fix Sprite Sheets"), &fix_sprite_sheets)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT("Use direct palette for sprite graphics sheets");

      if (Checkbox(tr("Transparent for Terrain"),
                   &use_transparent_for_terrain)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT("Force transparent palette for terrain graphics");

      Separator();
      Text(tr("Color Analysis:"));
      if (session_.current_tile8() >= 0 &&
          session_.current_tile8() <
              static_cast<int>(
                  session_.mutable_current_gfx_individual().size())) {
        Text(tr("Selected Tile8 Analysis:"));
        const auto& tile_data =
            session_.mutable_current_gfx_individual()[session_.current_tile8()];
        std::map<uint8_t, int> pixel_counts;
        for (uint8_t pixel : tile_data) {
          pixel_counts[pixel & 0x0F]++;  // Normalize to 4-bit
        }

        Text(tr("Pixel Value Distribution:"));
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          Text(tr("  Value %d (0x%X): %d pixels"), value, value, count);
        }

        Text(tr("Palette Colors Used:"));
        const gfx::SnesPalette* analysis_palette =
            session_.ResolveDisplayPalette();
        if (analysis_palette == nullptr || analysis_palette->empty()) {
          analysis_palette = session_.current_gfx_bmp()
                                 ? &session_.current_gfx_bmp()->palette()
                                 : &session_.palette();
        }
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          if (value < static_cast<int>(analysis_palette->size())) {
            auto color = (*analysis_palette)[value];
            ImVec4 display_color = color.rgb();
            ImGui::ColorButton(("##analysis" + std::to_string(value)).c_str(),
                               display_color, ImGuiColorEditFlags_NoTooltip,
                               ImVec2(16, 16));
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip(tr("Index %d: 0x%04X (%d pixels)"), value,
                                color.snes(), count);
            }
            if (value % 8 != 7)
              ImGui::SameLine();
          }
        }
      }

      // Enhanced ROM Palette Management Section
      Separator();
      if (CollapsingHeader(tr("ROM Palette Manager")) && session_.rom()) {
        Text(tr("Experimental ROM Palette Selection:"));
        HOVER_HINT(
            "Use ROM palettes to experiment with different color schemes");

        if (Button(tr("Open Enhanced Palette Editor"))) {
          tile16_edit_canvas_.ShowPaletteEditor();
        }
        SameLine();
        if (Button(tr("Show Color Analysis"))) {
          tile16_edit_canvas_.ShowColorAnalysis();
        }

        // Quick palette application
        static int quick_group = 0;
        static int quick_index = 0;

        SliderInt(tr("ROM Group"), &quick_group, 0, 6);
        SliderInt(tr("Palette Index"), &quick_index, 0, 7);

        if (Button(tr("Apply to Tile8 Source"))) {
          if (tile8_source_canvas_.ApplyROMPalette(quick_group, quick_index)) {
            util::logf("Applied ROM palette group %d, index %d to Tile8 source",
                       quick_group, quick_index);
          }
        }
        SameLine();
        if (Button(tr("Apply to Tile16 Editor"))) {
          if (tile16_edit_canvas_.ApplyROMPalette(quick_group, quick_index)) {
            util::logf(
                "Applied ROM palette group %d, index %d to Tile16 editor",
                quick_group, quick_index);
          }
        }
      }
    }
    End();
  }
}

void Tile16Editor::DrawScratchSpace() {
  Text(tr("Layout Scratch:"));
  for (int i = 0; i < 4; ++i) {
    ImGui::PushID(i);
    std::string slot_name = "S" + std::to_string(i + 1);

    if (Button((slot_name + " Save").c_str(), ImVec2(70, 20))) {
      status_ = SaveLayoutToScratch(i);
    }
    SameLine();

    bool can_load = session_.layout_scratch(i).in_use;
    if (!can_load) {
      ImGui::BeginDisabled();
    }
    if (Button((slot_name + " Load").c_str(), ImVec2(70, 20)) && can_load) {
      status_ = LoadLayoutFromScratch(i);
    }
    if (!can_load) {
      ImGui::EndDisabled();
    }
    SameLine();
    TextDisabled("%s", session_.layout_scratch(i).name.c_str());
    ImGui::PopID();
  }
}

void Tile16Editor::DrawManualTile8Inputs() {
  if (ImGui::BeginPopupModal("ManualTile8Editor", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(tr("Manual Tile8 Configuration for Tile16 %02X"),
                session_.current_tile16());
    ImGui::Separator();

    auto* tile_data = GetCurrentTile16Data();
    if (tile_data) {
      ImGui::Text(tr("Current Tile16 Staged Data:"));

      auto stage_current_tile = [&]() -> absl::Status {
        SyncTilesInfoArray(tile_data);
        RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
        RETURN_IF_ERROR(UpdateBlocksetBitmap());
        if (session_.live_preview_enabled()) {
          RETURN_IF_ERROR(UpdateOverworldTilemap());
        }
        MarkCurrentTileModified();
        return absl::OkStatus();
      };

      // Display and edit each quadrant using TileInfo structure
      const char* quadrant_names[] = {"Top-Left", "Top-Right", "Bottom-Left",
                                      "Bottom-Right"};

      for (int q = 0; q < 4; q++) {
        ImGui::Text(tr("%s Quadrant:"), quadrant_names[q]);
        ImGui::TextDisabled(tr("Tile Palette metadata + Tile8/flip flags"));

        // Get the current TileInfo for this quadrant
        gfx::TileInfo* tile_info = nullptr;
        switch (q) {
          case 0:
            tile_info = &tile_data->tile0_;
            break;
          case 1:
            tile_info = &tile_data->tile1_;
            break;
          case 2:
            tile_info = &tile_data->tile2_;
            break;
          case 3:
            tile_info = &tile_data->tile3_;
            break;
        }

        if (tile_info) {
          // Editable inputs for TileInfo components
          ImGui::PushID(q);

          int tile_id_int = static_cast<int>(tile_info->id_);
          if (ImGui::InputInt(tr("Tile8 ID"), &tile_id_int, 1, 10)) {
            tile_info->id_ =
                static_cast<uint16_t>(std::max(0, std::min(tile_id_int, 1023)));
          }

          int palette_int = static_cast<int>(tile_info->palette_);
          if (ImGui::SliderInt(tr("Tile Palette"), &palette_int, 0, 7)) {
            tile_info->palette_ = static_cast<uint8_t>(palette_int);
          }

          ImGui::Checkbox(tr("X Flip"), &tile_info->horizontal_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox(tr("Y Flip"), &tile_info->vertical_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox(tr("Priority"), &tile_info->over_);

          if (ImGui::Button(tr("Stage Quadrant Edit"))) {
            auto stage_result = stage_current_tile();
            if (!stage_result.ok()) {
              ImGui::Text(tr("Stage Error: %s"), stage_result.message().data());
            }
          }

          ImGui::PopID();
        }

        if (q < 3)
          ImGui::Separator();
      }

      ImGui::Separator();
      if (ImGui::Button(tr("Stage All Edits"))) {
        auto stage_result = stage_current_tile();
        if (!stage_result.ok()) {
          ImGui::Text(tr("Stage Error: %s"), stage_result.message().data());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Write Pending to ROM"))) {
        auto write_result = CommitAllChanges();
        if (!write_result.ok()) {
          ImGui::Text(tr("Write Error: %s"), write_result.message().data());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Refresh Display"))) {
        auto refresh_result = SetCurrentTile(session_.current_tile16());
        if (!refresh_result.ok()) {
          ImGui::Text(tr("Refresh Error: %s"), refresh_result.message().data());
        }
      }

    } else {
      ImGui::Text(tr("Tile16 data not accessible"));
      ImGui::Text(tr("Current tile16: %d"), session_.current_tile16());
      if (session_.rom()) {
        ImGui::Text(tr("Valid range: 0-4095 (4096 total tiles)"));
      }
    }

    ImGui::Separator();
    if (ImGui::Button(tr("Close"))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace yaze
