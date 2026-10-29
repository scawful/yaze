#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/registry/undo_action.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "util/macro.h"
#include "util/platform_paths.h"

namespace yaze::editor {
namespace {

absl::StatusOr<std::filesystem::path> GetScratchPadPath() {
  ASSIGN_OR_RETURN(auto config_dir, util::PlatformPaths::GetConfigDirectory());
  return config_dir / "ScratchPad.dat";
}

// DrawBitmap offsets are screen pixels; scrolling is already in screen space.
// Canvas's old drawn_tile_position is owned by its bitmap painter, which this
// Tile16 workspace does not invoke.
ImVec2 ScratchMousePosition(const gui::Canvas& canvas) {
  const float scale = std::max(0.01f, canvas.GetGlobalScale());
  const auto mouse = ImGui::GetIO().MousePos;
  const auto origin = canvas.zero_point();
  const auto scroll = canvas.scrolling();
  return ImVec2((mouse.x - origin.x - scroll.x - 2) / scale,
                (mouse.y - origin.y - scroll.y - 2) / scale);
}

uint64_t ScratchPreviewRevision(const ScratchPage& page, gfx::Tilemap& atlas,
                                bool loaded) {
  uint64_t hash = 1469598103934665603ULL;
  const auto add = [&hash](uint64_t value) {
    hash = (hash ^ value) * 1099511628211ULL;
  };
  add(loaded);
  add(atlas.atlas.width());
  add(atlas.atlas.height());
  for (size_t i = 0; i < atlas.atlas.palette().size(); ++i)
    add(atlas.atlas.palette()[i].snes());
  if (loaded && page.in_use) {
    std::set<int> ids;
    for (int y = 0; y < page.height; ++y)
      for (int x = 0; x < page.width; ++x)
        ids.insert(page.tile_data[x][y]);
    for (const int id : ids) {
      add(id);
      for (const auto pixel : gfx::GetTilemapData(atlas, id))
        add(pixel);
    }
  }
  return hash;
}

class ScratchWorkspaceUndoAction final : public UndoAction {
 public:
  using Apply = std::function<absl::Status(const ScratchWorkspaceState&)>;
  ScratchWorkspaceUndoAction(ScratchWorkspaceState before,
                             ScratchWorkspaceState after, Apply apply,
                             std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)),
        description_(std::move(description)) {}

  absl::Status Undo() override { return apply_(before_); }
  absl::Status Redo() override { return apply_(after_); }
  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override {
    return sizeof(*this) +
           (before_.pages.size() + after_.pages.size()) * sizeof(ScratchPage);
  }

 private:
  ScratchWorkspaceState before_;
  ScratchWorkspaceState after_;
  Apply apply_;
  std::string description_;
};

}  // namespace

absl::Status OverworldEditor::LoadScratchPad() {
  if (scratch_initialized_)
    return RebuildScratchBitmapFromTileData();
  const auto path = GetScratchPadPath();
  if (!path.ok()) {
    scratch_load_status_ = path.status();
    return scratch_load_status_;
  }
  scratch_load_status_ = scratch_workspace_.Load(*path);
  if (!scratch_load_status_.ok())
    return scratch_load_status_;
  scratch_initialized_ = true;
  scratch_path_ = *path;
  scratch_save_status_ = absl::OkStatus();
  scratch_space_dirty_ = false;
  scratch_edit_before_.reset();
  return RebuildScratchBitmapFromTileData();
}

absl::Status OverworldEditor::SaveScratchPad() {
  if (scratch_path_.empty())
    return absl::FailedPreconditionError("Scratch workspace is not loaded");
  return scratch_workspace_.Save(scratch_path_);
}

absl::Status OverworldEditor::FlushScratchPadIfDirty() {
  if (!scratch_space_dirty_)
    return absl::OkStatus();
  scratch_save_status_ = SaveScratchPad();
  RETURN_IF_ERROR(scratch_save_status_);
  scratch_space_dirty_ = false;
  return absl::OkStatus();
}

void OverworldEditor::MarkScratchSpaceDirty() {
  scratch_space_dirty_ = true;
  scratch_save_status_ = absl::OkStatus();
}

void OverworldEditor::BeginScratchEdit() {
  if (!scratch_edit_before_) {
    FinalizePaintOperation();
    scratch_edit_before_ = scratch_workspace_.state();
  }
}

void OverworldEditor::FinishScratchEdit(const std::string& description) {
  scratch_paint_gesture_owned_ = false;
  if (!scratch_edit_before_)
    return;
  if (!(*scratch_edit_before_ == scratch_workspace_.state())) {
    MarkScratchSpaceDirty();
    undo_manager_.Push(std::make_unique<ScratchWorkspaceUndoAction>(
        std::move(*scratch_edit_before_), scratch_workspace_.state(),
        [this](const ScratchWorkspaceState& state) {
          return RestoreScratchState(state);
        },
        description));
  }
  scratch_edit_before_.reset();
}

absl::Status OverworldEditor::RestoreScratchState(
    const ScratchWorkspaceState& state) {
  scratch_workspace_.Restore(state);
  MarkScratchSpaceDirty();
  RETURN_IF_ERROR(RebuildScratchBitmapFromTileData());
  // Keep the Undo operation usable after a save failure; dirty state protects
  // the recoverable snapshot and normal Save retries persistence.
  (void)FlushScratchPadIfDirty();
  return absl::OkStatus();
}

absl::Status OverworldEditor::RebuildScratchBitmapFromTileData() {
  const auto& page = scratch_workspace_.current();
  const int width = page.width * 32;
  const int height = page.height * 32;
  std::vector<uint8_t> pixels(width * height, 0);
  if (map_blockset_loaded_ && page.in_use) {
    for (int ty = 0; ty < page.height; ++ty) {
      for (int tx = 0; tx < page.width; ++tx) {
        const auto tile =
            gfx::GetTilemapData(tile16_blockset_, page.tile_data[tx][ty]);
        if (tile.size() != 16 * 16)
          continue;
        for (int y = 0; y < 32; ++y)
          for (int x = 0; x < 32; ++x)
            pixels[(ty * 32 + y) * width + tx * 32 + x] =
                tile[(y / 2) * 16 + x / 2];
      }
    }
  }
  scratch_bitmap_.Create(width, height, 8, pixels);
  // The Tile16 atlas already carries the currently selected area's palette.
  // Watch its actual colors, since in-place palette/Tile16 edits do not create
  // a new Bitmap resource generation.
  scratch_bitmap_.SetPalette(tile16_blockset_.atlas.palette());
  if (all_gfx_loaded_) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &scratch_bitmap_);
  }
  scratch_preview_map_ = current_map_;
  scratch_preview_revision_ =
      ScratchPreviewRevision(page, tile16_blockset_, map_blockset_loaded_);
  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawScratchSpace() {
  using namespace ImGui;
  if (is_lyra_preview()) {
    TextDisabled("Lyra preview: scratch editing unavailable");
    return absl::OkStatus();
  }
  if (!scratch_load_status_.ok()) {
    TextWrapped("Scratch workspace retained on disk: %s",
                std::string(scratch_load_status_.message()).c_str());
    if (Button(tr("Retry loading scratch workspace")))
      return LoadScratchPad();
    return absl::OkStatus();
  }

  const auto report = [this](absl::Status status) {
    scratch_action_status_ = std::move(status);
  };
  TextUnformatted(ICON_MD_BRUSH " Scratch Workspace");
  TextWrapped(
      "Named layouts stay outside the ROM. Tile16 references preview "
      "with the current area's graphics and palette.");
  if (BeginCombo("Layout", scratch_workspace_.current().name.c_str())) {
    for (const auto& page : scratch_workspace_.state().pages) {
      PushID(std::to_string(page.id).c_str());
      if (Selectable(page.name.c_str(),
                     page.id == scratch_workspace_.state().selected_id)) {
        FinishScratchEdit("Edit scratch layout");
        scratch_workspace_.Select(page.id);
        MarkScratchSpaceDirty();
        report(RebuildScratchBitmapFromTileData());
      }
      PopID();
    }
    EndCombo();
  }
  BeginDisabled(scratch_workspace_.state().pages.size() >=
                ScratchWorkspace::kMaxPages);
  if (Button(tr("New layout"))) {
    BeginScratchEdit();
    const auto status = scratch_workspace_.AddPage();
    FinishScratchEdit("New scratch layout");
    report(status.ok() ? RebuildScratchBitmapFromTileData() : status);
  }
  SameLine();
  if (Button(tr("Duplicate"))) {
    BeginScratchEdit();
    const auto status = scratch_workspace_.AddPage(true);
    FinishScratchEdit("Duplicate scratch layout");
    report(status.ok() ? RebuildScratchBitmapFromTileData() : status);
  }
  SameLine();
  if (Button(tr("Capture selection")))
    report(SaveCurrentSelectionToScratch());
  EndDisabled();
  BeginDisabled(scratch_workspace_.state().pages.size() <= 1);
  if (Button(tr("Remove layout"))) {
    BeginScratchEdit();
    const auto status = scratch_workspace_.DeletePage();
    FinishScratchEdit("Remove scratch layout");
    report(status.ok() ? RebuildScratchBitmapFromTileData() : status);
  }
  EndDisabled();

  const auto& page = scratch_workspace_.current();
  if (scratch_name_id_ != page.id || !IsAnyItemActive()) {
    scratch_name_buffer_.fill('\0');
    std::memcpy(scratch_name_buffer_.data(), page.name.data(),
                page.name.size());
    scratch_name_id_ = page.id;
  }
  if (InputText("Name", scratch_name_buffer_.data(),
                scratch_name_buffer_.size(),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
    BeginScratchEdit();
    report(scratch_workspace_.Rename(scratch_name_buffer_.data()));
    FinishScratchEdit("Rename scratch layout");
  }
  int dimensions[] = {page.width, page.height};
  if (InputInt2("Size in tiles", dimensions)) {
    BeginScratchEdit();
    const auto resize = scratch_workspace_.Resize(dimensions[0], dimensions[1]);
    FinishScratchEdit("Resize scratch layout");
    if (resize.ok())
      report(RebuildScratchBitmapFromTileData());
    else
      report(resize);
  }

  BeginDisabled(!page.in_use);
  if (Button(tr("Copy layout as brush")))
    report(LoadScratchToSelection());
  EndDisabled();
  SameLine();
  if (Button(tr("Clear layout")))
    report(ClearScratchSpace());
  SameLine();
  if (Button(tr("Refresh preview")))
    report(RebuildScratchBitmapFromTileData());

  TextWrapped(
      "Left-drag paints the selected Tile16. A map selection stamps "
      "on click. Right-click picks a Tile16. Copy a layout, hover "
      "the map, and use Paste to place it.");
  if (!scratch_bitmap_.is_active() || scratch_preview_map_ != current_map_ ||
      scratch_preview_revision_ !=
          ScratchPreviewRevision(page, tile16_blockset_, map_blockset_loaded_))
    report(RebuildScratchBitmapFromTileData());

  gui::CanvasFrameOptions options;
  options.canvas_size = ImVec2(page.width * 32 + 4, page.height * 32 + 4);
  options.draw_grid = true;
  options.grid_step = 32.0f;
  options.draw_context_menu = false;
  options.draw_overlay = true;
  options.render_popups = false;
  options.use_child_window = false;
  gui::BeginChildWithScrollbar(
      "##ScratchSpaceScrollRegion",
      ImVec2(0, std::max(150.0f, GetContentRegionAvail().y - 28)));
  auto canvas = gui::BeginCanvas(scratch_canvas_, options);
  scratch_canvas_.DrawBitmap(scratch_bitmap_, 2, 2, canvas.scale);
  if (map_blockset_loaded_)
    scratch_canvas_.DrawTileSelector(32);

  const auto* active_brush = selection_brush();
  const ImVec2 origin = scratch_canvas_.zero_point();
  const ImVec2 scroll = scratch_canvas_.scrolling();
  const float scale = canvas.scale;

  // Render existing selection rectangle if active
  if (scratch_select_active_) {
    const float x0 =
        origin.x + scroll.x + (scratch_select_min_.x * 32.0f + 2.0f) * scale;
    const float y0 =
        origin.y + scroll.y + (scratch_select_min_.y * 32.0f + 2.0f) * scale;
    const float x1 = origin.x + scroll.x +
                     ((scratch_select_max_.x + 1.0f) * 32.0f + 2.0f) * scale;
    const float y1 = origin.y + scroll.y +
                     ((scratch_select_max_.y + 1.0f) * 32.0f + 2.0f) * scale;
    canvas.draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                                    IM_COL32(80, 140, 240, 50));
    canvas.draw_list->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                              IM_COL32(255, 255, 255, 220), 0.0f, 0, 2.0f);
  }

  if (canvas.hovered && map_blockset_loaded_) {
    const auto cursor = ScratchMousePosition(scratch_canvas_);
    const int x = static_cast<int>(std::floor(cursor.x / 32));
    const int y = static_cast<int>(std::floor(cursor.y / 32));

    // Right mouse drag-selection (Tile16 Select Rect)
    if (IsMouseClicked(ImGuiMouseButton_Right) && x >= 0 && y >= 0 &&
        x < page.width && y < page.height) {
      scratch_selecting_ = true;
      scratch_drag_start_ = ImVec2(x, y);
      scratch_select_min_ = scratch_drag_start_;
      scratch_select_max_ = scratch_drag_start_;
    }

    if (scratch_selecting_ && IsMouseDragging(ImGuiMouseButton_Right)) {
      int cur_x = std::clamp(x, 0, page.width - 1);
      int cur_y = std::clamp(y, 0, page.height - 1);
      int min_x = std::min(static_cast<int>(scratch_drag_start_.x), cur_x);
      int min_y = std::min(static_cast<int>(scratch_drag_start_.y), cur_y);
      int max_x = std::max(static_cast<int>(scratch_drag_start_.x), cur_x);
      int max_y = std::max(static_cast<int>(scratch_drag_start_.y), cur_y);
      scratch_select_min_ = ImVec2(min_x, min_y);
      scratch_select_max_ = ImVec2(max_x, max_y);

      // Draw active dragging rect
      const float x0 = origin.x + scroll.x + (min_x * 32.0f + 2.0f) * scale;
      const float y0 = origin.y + scroll.y + (min_y * 32.0f + 2.0f) * scale;
      const float x1 =
          origin.x + scroll.x + ((max_x + 1.0f) * 32.0f + 2.0f) * scale;
      const float y1 =
          origin.y + scroll.y + ((max_y + 1.0f) * 32.0f + 2.0f) * scale;
      canvas.draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                                      IM_COL32(100, 160, 255, 70));
      canvas.draw_list->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                                IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
    }

    if (scratch_selecting_ && !IsMouseDown(ImGuiMouseButton_Right)) {
      scratch_selecting_ = false;
      int start_x = static_cast<int>(scratch_drag_start_.x);
      int start_y = static_cast<int>(scratch_drag_start_.y);
      int end_x = std::clamp(x, 0, page.width - 1);
      int end_y = std::clamp(y, 0, page.height - 1);

      if (start_x == end_x && start_y == end_y) {
        // Single right click: sample Tile16
        RequestTile16Selection(page.tile_data[start_x][start_y]);
        scratch_select_active_ = false;
      } else {
        int min_x = std::min(start_x, end_x);
        int max_x = std::max(start_x, end_x);
        int min_y = std::min(start_y, end_y);
        int max_y = std::max(start_y, end_y);
        TileBrush brush;
        brush.width = max_x - min_x + 1;
        brush.height = max_y - min_y + 1;
        brush.tile_ids.reserve(brush.width * brush.height);
        for (int row = min_y; row <= max_y; ++row) {
          for (int col = min_x; col <= max_x; ++col) {
            brush.tile_ids.push_back(page.tile_data[col][row]);
          }
        }
        SetSelectionBrush(std::move(brush));
        scratch_select_active_ = true;
        scratch_select_min_ = ImVec2(min_x, min_y);
        scratch_select_max_ = ImVec2(max_x, max_y);
      }
    }

    // Brush stamp or single-tile paint
    if (x >= 0 && y >= 0 && x < page.width && y < page.height) {
      if (active_brush && active_brush->valid()) {
        // Draw brush ghost preview
        const float bx0 = origin.x + scroll.x + (x * 32.0f + 2.0f) * scale;
        const float by0 = origin.y + scroll.y + (y * 32.0f + 2.0f) * scale;
        const float bx1 = origin.x + scroll.x +
                          ((x + active_brush->width) * 32.0f + 2.0f) * scale;
        const float by1 = origin.y + scroll.y +
                          ((y + active_brush->height) * 32.0f + 2.0f) * scale;
        canvas.draw_list->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1),
                                  IM_COL32(255, 220, 50, 220), 0.0f, 0, 2.0f);

        if (IsMouseClicked(ImGuiMouseButton_Left)) {
          BeginScratchEdit();
          const auto status = scratch_workspace_.Stamp(*active_brush, x, y);
          FinishScratchEdit("Stamp scratch layout");
          if (!status.ok())
            report(status);
          else
            report(RebuildScratchBitmapFromTileData());
        }
      } else {
        if (IsMouseClicked(ImGuiMouseButton_Left)) {
          BeginScratchEdit();
          scratch_paint_gesture_owned_ = true;
        }
        if (scratch_paint_gesture_owned_ && IsMouseDown(ImGuiMouseButton_Left))
          DrawScratchSpaceEdits();
      }

      // Hover to View tooltip: show tile info, usage count, coordinates, zoomed preview
      if (!scratch_selecting_ && !IsMouseDown(ImGuiMouseButton_Left) &&
          !IsMouseDown(ImGuiMouseButton_Right) && page.in_use) {
        const int tile_id = page.tile_data[x][y];
        if (ImGui::BeginTooltip()) {
          ImGui::Text(tr("Tile %d (0x%03X)"), tile_id, tile_id);
          ImGui::TextDisabled("Scratch: (%d, %d)", x, y);
          const int usage = GetTile16UsageCount(tile_id);
          if (usage > 0) {
            ImGui::Text(tr("Placed %d times (%s)"), usage,
                        GetTile16ScopeLabel().c_str());
          }
          if (tile16_blockset_.atlas.is_active()) {
            const int tile_col = tile_id % 16;
            const int tile_row = tile_id / 16;
            const float atlas_w =
                static_cast<float>(tile16_blockset_.atlas.width());
            const float atlas_h =
                static_cast<float>(tile16_blockset_.atlas.height());
            if (atlas_w > 0 && atlas_h > 0) {
              const float uv0_x = (tile_col * 16.0f) / atlas_w;
              const float uv0_y = (tile_row * 16.0f) / atlas_h;
              const float uv1_x = ((tile_col + 1) * 16.0f) / atlas_w;
              const float uv1_y = ((tile_row + 1) * 16.0f) / atlas_h;
              ImGui::Image(
                  (ImTextureID)(intptr_t)tile16_blockset_.atlas.texture(),
                  ImVec2(64.0f, 64.0f), ImVec2(uv0_x, uv0_y),
                  ImVec2(uv1_x, uv1_y));
            }
          }
          ImGui::EndTooltip();
        }
      }
    }
  }
  gui::EndCanvas(scratch_canvas_, canvas, options);
  EndChild();
  if (!IsMouseDown(ImGuiMouseButton_Left)) {
    FinishScratchEdit("Paint scratch layout");
    if (scratch_save_status_.ok())
      (void)FlushScratchPadIfDirty();
  }
  if (!scratch_action_status_.ok())
    TextWrapped("Scratch action: %s",
                std::string(scratch_action_status_.message()).c_str());
  if (!scratch_save_status_.ok()) {
    TextWrapped("Unsaved scratch layouts: %s",
                std::string(scratch_save_status_.message()).c_str());
    if (Button(tr("Retry scratch save")))
      (void)FlushScratchPadIfDirty();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawScratchSpaceEdits() {
  const auto cursor = ScratchMousePosition(scratch_canvas_);
  const int x = static_cast<int>(std::floor(cursor.x / 32));
  const int y = static_cast<int>(std::floor(cursor.y / 32));
  auto& page = scratch_workspace_.current();
  if (x < 0 || y < 0 || x >= page.width || y >= page.height ||
      current_tile16_ < 0 || current_tile16_ > 0xFFFF)
    return;
  if (page.in_use && page.tile_data[x][y] == current_tile16_)
    return;
  if (scratch_workspace_.Stamp({1, 1, {current_tile16_}}, x, y).ok()) {
    MarkScratchSpaceDirty();
    UpdateScratchBitmapTile(x, y, current_tile16_);
  }
}

void OverworldEditor::DrawScratchSpacePattern() {
  if (!dependencies_.shared_clipboard ||
      !dependencies_.shared_clipboard->has_overworld_tile16)
    return;
  const auto& clipboard = *dependencies_.shared_clipboard;
  const auto cursor = ScratchMousePosition(scratch_canvas_);
  BeginScratchEdit();
  const auto status = scratch_workspace_.Stamp(
      {clipboard.overworld_width, clipboard.overworld_height,
       clipboard.overworld_tile16_ids},
      static_cast<int>(std::floor(cursor.x / 32)),
      static_cast<int>(std::floor(cursor.y / 32)));
  FinishScratchEdit("Stamp scratch layout");
  if (status.ok())
    (void)RebuildScratchBitmapFromTileData();
  else
    status_ = status;
}

void OverworldEditor::UpdateScratchBitmapTile(int tile_x, int tile_y,
                                              int tile_id) {
  const auto tile = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (tile.size() != 16 * 16)
    return;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      scratch_bitmap_.WriteToPixel(
          (tile_y * 32 + y) * scratch_bitmap_.width() + tile_x * 32 + x,
          tile[(y / 2) * 16 + x / 2]);
  scratch_bitmap_.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &scratch_bitmap_);
}

absl::Status OverworldEditor::SaveCurrentSelectionToScratch() {
  const auto* brush =
      tile_painting_ ? tile_painting_->selection_brush() : nullptr;
  if (!brush)
    return absl::FailedPreconditionError("Select an overworld rectangle first");
  BeginScratchEdit();
  const auto status = scratch_workspace_.Capture(*brush, current_map_);
  FinishScratchEdit("Capture map selection in scratch layout");
  RETURN_IF_ERROR(status);
  RETURN_IF_ERROR(RebuildScratchBitmapFromTileData());
  return FlushScratchPadIfDirty();
}

absl::Status OverworldEditor::LoadScratchToSelection() {
  const auto& page = scratch_workspace_.current();
  if (!page.in_use)
    return absl::FailedPreconditionError("Scratch layout is empty");
  const auto brush = page.AsBrush();
  if (overworld_.is_loaded() &&
      std::any_of(brush.tile_ids.begin(), brush.tile_ids.end(), [this](int id) {
        return id >= static_cast<int>(overworld_.tiles16().size());
      }))
    return absl::FailedPreconditionError(
        "Scratch layout references Tile16 IDs absent from this ROM");
  SetSelectionBrush(brush);
  return absl::OkStatus();
}

absl::Status OverworldEditor::ClearScratchSpace() {
  BeginScratchEdit();
  scratch_workspace_.Clear();
  FinishScratchEdit("Clear scratch layout");
  scratch_select_active_ = false;
  ClearSelectionBrush();
  RETURN_IF_ERROR(RebuildScratchBitmapFromTileData());
  return FlushScratchPadIfDirty();
}

}  // namespace yaze::editor
