#ifndef YAZE_APP_EDITOR_GRAPHICS_UI_SCRATCH_GRAPHICS_SCRATCH_VIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_UI_SCRATCH_GRAPHICS_SCRATCH_VIEW_H_

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_preview_image.h"
#include "app/editor/graphics/graphics_scratch_asset.h"
#include "app/editor/registry/undo_manager.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

using ScratchPreviewCallback =
    std::function<absl::StatusOr<usage_preview::PreviewImage>(
        const GraphicsScratchAsset&)>;

class GraphicsScratchView : public WindowContent {
  friend struct GraphicsScratchBinTestPeer;
  friend struct ScratchSpritePreviewTestPeer;

 public:
  GraphicsScratchView(GraphicsEditorState* state, UndoManager* history,
                      std::string library_path = "");
  ~GraphicsScratchView() override;
  std::string GetId() const override { return "graphics.scratch"; }
  std::string GetDisplayName() const override { return "Graphics Scratch"; }
  std::string GetIcon() const override { return ICON_MD_BRUSH; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::OptionalPopOut();
  }
  int GetPriority() const override { return 28; }
  float GetPreferredWidth() const override { return 650; }
  void Draw(bool* p_open) override;
  bool HasPendingChanges() const { return dirty_ || stroke_.has_value(); }
  bool HasActiveStroke() const { return stroke_.has_value(); }
  absl::Status Flush();
  void FinishGesture();
  // These commands are shared by the toolbar and future source navigation.
  absl::Status CaptureSheet();
  absl::Status ImportBin(const std::string& path, int bpp, size_t offset,
                         const gfx::SnesPalette& palette = {});
  absl::Status ApplyToSheet();
  absl::Status ExportBin(const std::string& path);
  const GraphicsScratchLibrary& library() const { return library_; }
  // Binds a captured source pose to the currently imported asset, never another
  // library selection. The callback must own its captured input values.
  void SetAssociatedPreview(std::string label, ScratchPreviewCallback callback);
  void SetApplyValidator(std::function<absl::Status(uint16_t)> validator) {
    apply_validator_ = std::move(validator);
  }
  void SetExportValidator(
      std::function<absl::Status(const std::string&)> validator) {
    export_validator_ = std::move(validator);
  }

 private:
  class LibraryEdit;
  void Load();
  absl::Status Record(GraphicsScratchLibrary before);
  absl::Status PrepareCommand();
  void RefreshPublishedBaselines();
  void SetStatus(absl::Status status);
  void DrawPixels(GraphicsScratchAsset& asset);
  void DrawAssociatedPreview();
  GraphicsScratchAsset* Selected();
  GraphicsEditorState* state_;
  UndoManager* history_;
  GraphicsScratchLibrary library_;
  std::optional<GraphicsScratchLibrary> stroke_;
  uint64_t selected_id_ = 0;
  std::string path_;
  std::string loaded_bytes_;
  bool existed_ = false;
  bool load_ok_ = false;
  bool dirty_ = false;
  std::string error_;
  std::string asset_name_ = "Untitled sheet";
  std::string bin_path_;
  int bpp_ = 4;
  int bin_offset_ = 0;
  int color_ = 1;
  float zoom_ = 4;
  std::function<absl::Status(uint16_t)> apply_validator_;
  std::function<absl::Status(const std::string&)> export_validator_;
  ScratchPreviewCallback preview_callback_;
  std::string preview_label_;
  std::string preview_error_;
  uint64_t preview_asset_id_ = 0;
  std::optional<uint64_t> preview_hash_;
  gfx::Bitmap preview_bitmap_;
  gui::Canvas preview_canvas_{"ScratchSourcePreview", ImVec2(256, 256),
                              gui::CanvasGridSize::k8x8};
  // Export is an external write, not an Undoable pixel edit. Keep successful
  // CAS baselines current when an older pixel/history snapshot is restored.
  std::map<std::tuple<std::string, uint64_t, int>, std::vector<uint8_t>>
      published_chunks_;
};

// The editor owns scratch state and history targets across Initialize calls.
// Workspace replacement only replaces this callback, never the document.
class GraphicsScratchWindow : public WindowContent {
 public:
  explicit GraphicsScratchWindow(std::function<void()> draw)
      : draw_(std::move(draw)) {}
  std::string GetId() const override { return "graphics.scratch"; }
  std::string GetDisplayName() const override { return "Graphics Scratch"; }
  std::string GetIcon() const override { return ICON_MD_BRUSH; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::OptionalPopOut();
  }
  int GetPriority() const override { return 28; }
  float GetPreferredWidth() const override { return 650; }
  void Draw(bool*) override { draw_(); }

 private:
  std::function<void()> draw_;
};

}  // namespace yaze::editor
#endif  // YAZE_APP_EDITOR_GRAPHICS_UI_SCRATCH_GRAPHICS_SCRATCH_VIEW_H_
