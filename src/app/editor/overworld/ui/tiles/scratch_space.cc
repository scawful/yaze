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
  if (InputInt2("Size in tiles", dimensions,
                ImGuiInputTextFlags_EnterReturnsTrue)) {
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
  if (canvas.hovered && map_blockset_loaded_) {
    const auto cursor = ScratchMousePosition(scratch_canvas_);
    const int x = static_cast<int>(std::floor(cursor.x / 32));
    const int y = static_cast<int>(std::floor(cursor.y / 32));
    if (x >= 0 && y >= 0 && x < page.width && y < page.height) {
      if (IsMouseClicked(ImGuiMouseButton_Right)) {
        RequestTile16Selection(page.tile_data[x][y]);
      }
      const auto* brush =
          tile_painting_ ? tile_painting_->selection_brush() : nullptr;
      if (brush && IsMouseClicked(ImGuiMouseButton_Left)) {
        BeginScratchEdit();
        const auto status = scratch_workspace_.Stamp(*brush, x, y);
        FinishScratchEdit("Stamp scratch layout");
        if (!status.ok())
          report(status);
        else
          report(RebuildScratchBitmapFromTileData());
      } else if (!brush) {
        if (IsMouseClicked(ImGuiMouseButton_Left)) {
          BeginScratchEdit();
          scratch_paint_gesture_owned_ = true;
        }
        if (scratch_paint_gesture_owned_ && IsMouseDown(ImGuiMouseButton_Left))
          DrawScratchSpaceEdits();
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
  if (!dependencies_.shared_clipboard)
    return absl::FailedPreconditionError("Clipboard unavailable");
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
  dependencies_.shared_clipboard->overworld_tile16_ids = brush.tile_ids;
  dependencies_.shared_clipboard->overworld_width = brush.width;
  dependencies_.shared_clipboard->overworld_height = brush.height;
  dependencies_.shared_clipboard->has_overworld_tile16 = true;
  return absl::OkStatus();
}

absl::Status OverworldEditor::ClearScratchSpace() {
  BeginScratchEdit();
  scratch_workspace_.Clear();
  FinishScratchEdit("Clear scratch layout");
  RETURN_IF_ERROR(RebuildScratchBitmapFromTileData());
  return FlushScratchPadIfDirty();
}

}  // namespace yaze::editor
