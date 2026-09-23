#include "tile16_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/tile16/tile16_editor_action_state.h"
#include "app/editor/overworld/tile16/tile16_editor_shortcuts.h"
#include "app/editor/overworld/tile16/tile8_source_interaction.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
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

using namespace ImGui;

absl::Status Tile16Editor::Initialize(
    gfx::Bitmap& tile16_blockset_bmp, gfx::Bitmap& current_gfx_bmp,
    std::array<uint8_t, 0x200>& all_tiles_types) {
  session_.set_on_tile_selection_changed(
      [this](int tile_id) { blockset_selector_.SetSelectedTile(tile_id); });
  session_.set_on_tile8_sheet_resized([this](float width, float height) {
    tile8_source_canvas_.SetCanvasSize(ImVec2(width, height));
  });

  RETURN_IF_ERROR(session_.InitializeBitmaps(tile16_blockset_bmp,
                                             current_gfx_bmp, all_tiles_types));

  tile16_edit_canvas_.InitializeDefaults();
  tile8_source_canvas_.InitializeDefaults();

  blockset_selector_.AttachCanvas(&blockset_canvas_);
  blockset_selector_.SetTileCount(zelda3::kNumTile16Individual);

  tile16_edit_canvas_.SetAutoResize(false);
  tile8_source_canvas_.SetAutoResize(false);

  if (session_.rom()) {
    tile16_edit_canvas_.InitializePaletteEditor(session_.rom());
    tile8_source_canvas_.InitializePaletteEditor(session_.rom());
  }

  ImVector<std::string> tile16_names;
  for (int i = 0; i < 0x200; ++i) {
    std::string str = util::HexByte(all_tiles_types[i]);
    tile16_names.push_back(str);
  }
  *tile8_source_canvas_.mutable_labels(0) = tile16_names;
  *tile8_source_canvas_.custom_labels_enabled() =
      session_.mutable_show_tile_collision_ids();

  gui::AddTableColumn(tile_edit_table_, "##tile16ID", [&]() {
    Text(tr("Tile16: %02X"), session_.current_tile16());
  });
  gui::AddTableColumn(tile_edit_table_, "##tile8ID", [&]() {
    Text(tr("Tile8: %02X"), session_.current_tile8());
  });
  gui::AddTableColumn(tile_edit_table_, "##tile16Flip", [&]() {
    Checkbox(tr("X Flip"), session_.mutable_x_flip());
    Checkbox(tr("Y Flip"), session_.mutable_y_flip());
    Checkbox(tr("Priority"), session_.mutable_priority_tile());
  });

  return absl::OkStatus();
}

absl::Status Tile16Editor::Update() {
  // Legacy MenuBar shell removed — single chrome path is UpdateAsPanel().
  return UpdateAsPanel();
}

void Tile16Editor::DrawTile16Editor() {
  status_ = UpdateTile16Edit();
}

absl::Status Tile16Editor::UpdateAsPanel() {
  if (!session_.map_blockset_loaded()) {
    return absl::InvalidArgumentError("Blockset not initialized, open a ROM.");
  }

  // Menu button for context menu
  if (Button(ICON_MD_MENU " Menu")) {
    OpenPopup("##Tile16EditorContextMenu");
  }
  SameLine();
  TextDisabled(tr("Right-click for more options"));

  // Context menu
  DrawContextMenu();

  // About popup
  if (BeginPopupModal("About Tile16 Editor", NULL,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text(tr("Tile16 Editor for Link to the Past"));
    Text(tr("This editor allows you to edit 16x16 tiles used in the game."));
    Text(tr("Features:"));
    BulletText(
        tr("Edit Tile16 graphics by placing 8x8 tiles in the quadrants"));
    BulletText(tr("Copy and paste Tile16 graphics"));
    BulletText(tr("Save and load Tile16 graphics to/from scratch space"));
    BulletText(tr("Preview Tile16 graphics at a larger size"));
    Separator();
    if (Button(tr("Close"))) {
      CloseCurrentPopup();
    }
    EndPopup();
  }

  // Handle keyboard shortcuts (shared implementation)
  HandleKeyboardShortcuts();

  DrawTile16Editor();
  if (!status_.ok()) {
    ImGui::TextWrapped("Tile16 edit failed: %s", status_.message().data());
  }
  DrawPaletteSettings();
  RETURN_IF_ERROR(UpdateLivePreview());

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateBlockset() {
  gui::BeginPadding(2);
  gui::BeginChildWithScrollbar("##Tile16EditorBlocksetScrollRegion");

  // Tile ID search/jump bar
  if (blockset_selector_.DrawFilterBar()) {
    RequestTileSwitch(blockset_selector_.GetSelectedTileID());
  }

  // Configure canvas frame options for blockset view
  gui::CanvasFrameOptions frame_opts;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 32.0f;  // Tile16 grid
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  auto canvas_rt = gui::BeginCanvas(blockset_canvas_, frame_opts);
  gui::EndPadding();

  // Ensure selector is synced with current selection
  if (blockset_selector_.GetSelectedTileID() != session_.current_tile16()) {
    blockset_selector_.SetSelectedTile(session_.current_tile16());
  }

  if (session_.tile16_blockset_bmp() == nullptr) {
    return absl::FailedPreconditionError(
        "Tile16 blockset bitmap not initialized");
  }

  // Render the selector widget (handles bitmap, grid, highlights, interaction)
  auto result =
      blockset_selector_.Render(*session_.tile16_blockset_bmp(), true);

  if (result.selection_changed) {
    // Selection does not create an edit or interrupt document history.
    RequestTileSwitch(result.selected_tile);
    util::logf("Selected Tile16 from blockset: %d", result.selected_tile);
  }

  gui::EndCanvas(blockset_canvas_, canvas_rt, frame_opts);
  EndChild();

  return absl::OkStatus();
}

void Tile16Editor::HandleKeyboardShortcuts() {
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::IsAnyItemActive()) {
    const ImGuiIO& io = ImGui::GetIO();
#if defined(__APPLE__)
    const bool platform_primary_held = io.KeyCtrl || io.KeySuper;
#else
    const bool platform_primary_held = io.KeyCtrl;
#endif
    const bool ctrl_held = platform_primary_held ||
                           ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                           ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    // Editing shortcuts (only fire without Ctrl to avoid conflicts)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
      status_ = ClearTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H) && !ctrl_held) {
      status_ = FlipTile16Horizontal();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V) && !ctrl_held) {
      status_ = FlipTile16Vertical();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R) && !ctrl_held) {
      status_ = RotateTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F) && !ctrl_held) {
      if (session_.current_tile8() >= 0 &&
          session_.current_tile8() <
              static_cast<int>(
                  session_.mutable_current_gfx_individual().size())) {
        status_ = FillTile16WithTile8(session_.current_tile8());
      }
    }

    if (!ctrl_held) {
      if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        session_.set_edit_mode(Tile16EditMode::kPaint);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_I)) {
        session_.set_edit_mode(Tile16EditMode::kPick);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_U)) {
        session_.set_edit_mode(Tile16EditMode::kUsageProbe);
      }
    }

    // Palette shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
      status_ = CyclePalette(false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      status_ = CyclePalette(true);
    }

    // Numeric shortcuts:
    //  - 1..4 focus tile16 quadrants
    //  - Ctrl+1..8 switch brush palette rows
    for (int i = 0; i < 8; ++i) {
      if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i))) {
        continue;
      }
      const Tile16NumericShortcutResult shortcut =
          ResolveTile16NumericShortcut(ctrl_held, i);
      if (shortcut.quadrant_focus.has_value()) {
        session_.set_active_quadrant(*shortcut.quadrant_focus);
      }
      if (shortcut.palette_id.has_value()) {
        session_.set_current_palette(*shortcut.palette_id);
        status_ = RefreshAllPalettes();
      }
    }

    // Ctrl-modified shortcuts
    if (ctrl_held) {
      if (!session_.has_shared_history() && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        status_ = io.KeyShift ? Redo() : Undo();
      }
      if (!session_.has_shared_history() && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        status_ = Redo();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_C)) {
        status_ = CopyTile16ToClipboard(session_.current_tile16());
      }
      if (ImGui::IsKeyPressed(ImGuiKey_V)) {
        status_ = PasteTile16FromClipboard();
      }
    }
  }
}

absl::Status Tile16Editor::DrawToCurrentTile16(ImVec2 pos,
                                               const gfx::Bitmap* source_tile) {
  return session_.DrawToCurrentTile16({pos.x, pos.y}, source_tile);
}

absl::Status Tile16Editor::HandleTile16CanvasClick(const ImVec2& tile_position,
                                                   bool left_click,
                                                   bool right_click) {
  return session_.HandleTile16CanvasClick({tile_position.x, tile_position.y},
                                          left_click, right_click);
}

ImVec2 Tile16Editor::Tile16PreviewDisplayPixelToTilePosition(
    const ImVec2& display_position) {
  const Tile16LocalPos local =
      Tile16EditSession::Tile16PreviewDisplayPixelToTilePosition(
          {display_position.x, display_position.y});
  return ImVec2(local.x, local.y);
}

absl::Status Tile16Editor::PickTile8FromTile16(const ImVec2& position) {
  return session_.PickTile8FromTile16({position.x, position.y});
}

absl::Status Tile16Editor::LoadLayoutFromScratch(int slot) {
  RETURN_IF_ERROR(session_.LoadLayoutFromScratch(slot));
  blockset_selector_.SetSelectedTile(session_.current_tile16());
  selected_tiles_.clear();
  selected_tiles_.reserve(64);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int tile_id = session_.layout_scratch(slot).tile_layout[y][x];
      if (tile_id >= 0) {
        selected_tiles_.push_back(tile_id);
      }
    }
  }
  selection_start_tile_ = session_.current_tile16();
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
