#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"

#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/graphics_scratch_storage.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/gfx/resource/arena.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/macro.h"
#include "util/platform_paths.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

void DefaultPalette(GraphicsScratchAsset& asset) {
  for (int i = 0; i < 16; ++i) {
    const int level = i * 31 / ((1 << asset.bpp) - 1);
    asset.palette[i] = std::min(level, 31) * 0x421;
  }
}

}  // namespace

class GraphicsScratchView::LibraryEdit : public UndoAction {
 public:
  LibraryEdit(GraphicsScratchView* owner, GraphicsScratchLibrary before,
              GraphicsScratchLibrary after)
      : owner_(owner), before_(std::move(before)), after_(std::move(after)) {}
  absl::Status Undo() override { return Restore(before_); }
  absl::Status Redo() override { return Restore(after_); }
  std::string Description() const override {
    return "Edit graphics scratch asset";
  }
  size_t MemoryUsage() const override {
    return (before_.assets.size() + after_.assets.size()) * 4096;
  }

 private:
  absl::Status Restore(const GraphicsScratchLibrary& library) {
    // IDs are never reused, including after undoing a creation.
    const uint64_t next_id = owner_->library_.next_id;
    owner_->library_ = library;
    owner_->library_.next_id = std::max(next_id, library.next_id);
    owner_->RefreshPublishedBaselines();
    owner_->dirty_ = true;
    return absl::OkStatus();
  }
  GraphicsScratchView* owner_;
  GraphicsScratchLibrary before_;
  GraphicsScratchLibrary after_;
};

GraphicsScratchView::GraphicsScratchView(GraphicsEditorState* state,
                                         UndoManager* history,
                                         std::string library_path)
    : state_(state), history_(history), path_(std::move(library_path)) {
  Load();
}

void GraphicsScratchView::Load() {
  if (path_.empty()) {
    auto config = util::PlatformPaths::GetConfigDirectory();
    if (!config.ok()) {
      SetStatus(config.status());
      return;
    }
    path_ = (*config / "GraphicsScratch.json").string();
  }
  auto loaded = graphics_scratch_storage::LoadLibrary(path_);
  if (!loaded.ok()) {
    SetStatus(loaded.status());
    return;
  }
  library_ = std::move(loaded->library);
  existed_ = loaded->bytes.has_value();
  loaded_bytes_ = loaded->bytes.value_or("");
  load_ok_ = true;
  if (!library_.assets.empty())
    selected_id_ = library_.assets.front().id;
}

void GraphicsScratchView::SetStatus(absl::Status status) {
  error_ = status.ok() ? "" : status.ToString();
}

absl::Status GraphicsScratchView::Flush() {
  if (!dirty_)
    return absl::OkStatus();
  if (!load_ok_)
    return absl::FailedPreconditionError(
        "Existing scratch library was not loaded; it will not be replaced");
  if (stroke_)
    return absl::FailedPreconditionError(
        "Finish the graphics scratch stroke before saving");
  const auto expected =
      existed_ ? std::optional<std::string>(loaded_bytes_) : std::nullopt;
  ASSIGN_OR_RETURN(loaded_bytes_, graphics_scratch_storage::SaveLibrary(
                                      path_, library_, expected));
  existed_ = true;
  dirty_ = false;
  return absl::OkStatus();
}

GraphicsScratchAsset* GraphicsScratchView::Selected() {
  for (auto& asset : library_.assets)
    if (asset.id == selected_id_)
      return &asset;
  return nullptr;
}

absl::Status GraphicsScratchView::Record(GraphicsScratchLibrary before) {
  dirty_ = true;
  history_->Push(
      std::make_unique<LibraryEdit>(this, std::move(before), library_));
  const auto status = Flush();
  SetStatus(status);
  return status;
}

void GraphicsScratchView::FinishGesture() {
  if (!stroke_ || ImGui::IsMouseDown(ImGuiMouseButton_Left))
    return;
  auto before = std::move(*stroke_);
  stroke_.reset();
  Record(std::move(before));
}

absl::Status GraphicsScratchView::PrepareCommand() {
  FinishGesture();
  if (stroke_)
    return absl::FailedPreconditionError(
        "Finish the scratch stroke before changing assets");
  return absl::OkStatus();
}

void GraphicsScratchView::RefreshPublishedBaselines() {
  for (auto& asset : library_.assets) {
    const auto found = published_chunks_.find(
        {asset.source_path, asset.source_offset, asset.bpp});
    if (found != published_chunks_.end())
      asset.original_chunk = found->second;
  }
}

absl::Status GraphicsScratchView::CaptureSheet() {
  RETURN_IF_ERROR(PrepareCommand());
  if (!load_ok_)
    return absl::FailedPreconditionError("Scratch library could not be loaded");
  auto* store = ActiveSheetStore(*state_);
  if (!store)
    return absl::FailedPreconditionError(
        "Load graphics in this ROM session first");
  GraphicsScratchAsset asset;
  asset.name = asset_name_;
  asset.bpp = bpp_;
  const auto pixels = store->Sheet(state_->current_sheet_id);
  asset.pixels.assign(pixels.begin(), pixels.end());
  asset.source = "Sheet " + std::to_string(state_->current_sheet_id);
  DefaultPalette(asset);
  const auto& bitmap = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  if (const auto* target = state_->SourceTarget()) {
    for (size_t i = 0; i < target->palette.size() && i < 16; ++i)
      asset.palette[i] = target->palette[i].snes();
  } else if (const auto* colors =
                 platform::GetSurfacePalette(bitmap.surface())) {
    for (int i = 0; i < colors->ncolors && i < 8; ++i) {
      const auto& c = colors->colors[i];
      asset.palette[i] = (c.r >> 3) | ((c.g >> 3) << 5) | ((c.b >> 3) << 10);
    }
  }
  auto before = library_;
  auto status = library_.Add(std::move(asset));
  if (!status.ok())
    return status;
  selected_id_ = library_.assets.back().id;
  return Record(std::move(before));
}

absl::Status GraphicsScratchView::ImportBin(const std::string& path, int bpp,
                                            size_t offset,
                                            const gfx::SnesPalette& palette) {
  RETURN_IF_ERROR(PrepareCommand());
  if (!load_ok_)
    return absl::FailedPreconditionError("Scratch library could not be loaded");
  ASSIGN_OR_RETURN(auto imported, graphics_scratch_storage::ImportBin(
                                      path, bpp, offset, asset_name_));
  auto* asset = &imported;
  DefaultPalette(*asset);
  for (size_t i = 0; i < palette.size() && i < 16; ++i)
    asset->palette[i] = palette[i].snes();
  auto before = library_;
  auto status = library_.Add(std::move(*asset));
  if (!status.ok())
    return status;
  selected_id_ = library_.assets.back().id;
  return Record(std::move(before));
}

absl::Status GraphicsScratchView::ApplyToSheet() {
  RETURN_IF_ERROR(PrepareCommand());
  const auto* asset = Selected();
  auto* store = ActiveSheetStore(*state_);
  if (!asset || !store)
    return absl::FailedPreconditionError(
        "Select a scratch asset and load session graphics first");
  if (zelda3::GetGfxSheetStorageKind(state_->current_sheet_id) ==
          zelda3::GfxSheetStorageKind::kCompressed2bpp ||
      std::any_of(asset->pixels.begin(), asset->pixels.end(),
                  [](uint8_t pixel) { return pixel > 7; }))
    return absl::FailedPreconditionError(
        "This ROM writer supports writable 3bpp sheets and indices 0-7; keep "
        "other assets as BIN files");
  if (apply_validator_) {
    const auto status = apply_validator_(state_->current_sheet_id);
    if (!status.ok())
      return status;
  }
  const auto current = store->Sheet(state_->current_sheet_id);
  auto diff =
      MakeSheetPixelDiff(state_->current_sheet_id, current, asset->pixels);
  if (diff.empty())
    return absl::OkStatus();
  auto status =
      CommitSheetPixels(*state_, state_->current_sheet_id, asset->pixels);
  if (!status.ok())
    return status;
  history_->Push(std::make_unique<GraphicsPixelEditAction>(
      state_, std::move(diff), "Apply graphics scratch asset"));
  return absl::OkStatus();
}

absl::Status GraphicsScratchView::ExportBin(const std::string& path) {
  RETURN_IF_ERROR(PrepareCommand());
  auto* asset = Selected();
  if (!asset)
    return absl::FailedPreconditionError("Select a scratch asset");
  ASSIGN_OR_RETURN(auto result, graphics_scratch_storage::ExportBin(
                                    *asset, path, path_, export_validator_));
  if (result.same_source) {
    published_chunks_[{asset->source_path, asset->source_offset, asset->bpp}] =
        std::move(result.published_chunk);
    RefreshPublishedBaselines();
    dirty_ = true;
    const auto saved = Flush();
    if (!saved.ok())
      return absl::Status(
          saved.code(),
          "BIN exported; updated scratch baseline remains unsaved: " +
              std::string(saved.message()));
  }
  return absl::OkStatus();
}

void GraphicsScratchView::DrawPixels(GraphicsScratchAsset& asset) {
  ImGui::SliderFloat("Zoom", &zoom_, 2, 8, "%.0fx");
  color_ = std::clamp(color_, 0, (1 << asset.bpp) - 1);
  ImGui::SliderInt("Color", &color_, 0, (1 << asset.bpp) - 1);
  ImGui::BeginChild("ScratchPixels", ImVec2(0, 32 * zoom_ + 24), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const auto origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(
      "Pixels", ImVec2(128 * zoom_, 32 * zoom_),
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  auto* draw = ImGui::GetWindowDrawList();
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      const uint16_t c = asset.palette[asset.pixels[y * 128 + x]];
      const auto color =
          IM_COL32((c & 31) * 255 / 31, ((c >> 5) & 31) * 255 / 31,
                   ((c >> 10) & 31) * 255 / 31, 255);
      draw->AddRectFilled(
          ImVec2(origin.x + x * zoom_, origin.y + y * zoom_),
          ImVec2(origin.x + (x + 1) * zoom_, origin.y + (y + 1) * zoom_),
          color);
    }
  }
  if (ImGui::IsItemHovered()) {
    const auto mouse = ImGui::GetMousePos();
    const int x = static_cast<int>((mouse.x - origin.x) / zoom_);
    const int y = static_cast<int>((mouse.y - origin.y) / zoom_);
    if (x >= 0 && x < 128 && y >= 0 && y < 32) {
      auto& pixel = asset.pixels[y * 128 + x];
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        color_ = pixel;
      if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && pixel != color_) {
        if (!stroke_)
          stroke_ = library_;
        pixel = color_;
      }
    }
  }
  ImGui::EndChild();
}

void GraphicsScratchView::Draw(bool* p_open) {
  (void)p_open;
  FinishGesture();
  ImGui::TextWrapped(
      "Keep named sheets here until you choose their destination. Left drag "
      "paints; right click picks a color. Scratch assets retain their own "
      "pixels and palette.");
  ImGui::TextWrapped("Library: %s", path_.c_str());
  if (!error_.empty())
    ImGui::TextWrapped("%s", error_.c_str());
  ImGui::BeginDisabled(!load_ok_ || stroke_.has_value());
  ImGui::InputText("Asset name", &asset_name_);
  ImGui::SliderInt("SNES bpp", &bpp_, 2, 4);
  if (ImGui::Button("New blank")) {
    auto before = library_;
    GraphicsScratchAsset asset;
    asset.name = asset_name_;
    asset.bpp = bpp_;
    DefaultPalette(asset);
    const auto status = library_.Add(std::move(asset));
    if (status.ok()) {
      selected_id_ = library_.assets.back().id;
      Record(std::move(before));
    } else {
      SetStatus(status);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Keep current sheet"))
    SetStatus(CaptureSheet());
  ImGui::InputText("BIN path", &bin_path_);
  ImGui::InputInt("BIN byte offset", &bin_offset_);
  bin_offset_ = std::max(0, bin_offset_);
  if (ImGui::Button("Import 64 tiles from BIN")) {
    auto path = bin_path_;
    if (path.empty())
      path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty())
      SetStatus(ImportBin(path, bpp_, bin_offset_));
  }
  if (ImGui::BeginCombo("Saved assets", Selected() ? Selected()->name.c_str()
                                                   : "Choose asset")) {
    for (const auto& asset : library_.assets) {
      ImGui::PushID(static_cast<int>(asset.id));
      if (ImGui::Selectable(asset.name.c_str(), asset.id == selected_id_))
        selected_id_ = asset.id;
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  auto* asset = Selected();
  if (!asset)
    return;
  ImGui::Text("%s (%d bpp) | %s", asset->name.c_str(), asset->bpp,
              asset->source.c_str());
  ImGui::BeginDisabled(stroke_.has_value());
  if (ImGui::Button("Rename to asset name")) {
    auto candidate = *asset;
    candidate.name = asset_name_;
    if (auto status = candidate.Validate(); status.ok()) {
      auto before = library_;
      asset->name = asset_name_;
      Record(std::move(before));
    } else {
      SetStatus(status);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Duplicate")) {
    auto before = library_;
    auto copy = *asset;
    copy.name = asset_name_;
    const auto status = library_.Add(std::move(copy));
    if (status.ok()) {
      selected_id_ = library_.assets.back().id;
      Record(std::move(before));
    } else {
      SetStatus(status);
    }
  }
  // Add may reallocate the vector.
  asset = Selected();
  ImGui::SameLine();
  if (ImGui::Button("Remove asset")) {
    auto before = library_;
    const auto id = selected_id_;
    library_.assets.erase(
        std::remove_if(library_.assets.begin(), library_.assets.end(),
                       [id](const auto& entry) { return entry.id == id; }),
        library_.assets.end());
    selected_id_ = library_.assets.empty() ? 0 : library_.assets.front().id;
    Record(std::move(before));
    ImGui::EndDisabled();
    return;
  }
  if (ImGui::Button("Apply pixels to current sheet"))
    SetStatus(ApplyToSheet());
  ImGui::SameLine();
  if (ImGui::Button("Export BIN")) {
    const auto path =
        util::FileDialogWrapper::ShowSaveFileDialog(asset->name, "bin");
    if (!path.empty())
      SetStatus(ExportBin(path));
  }
  ImGui::SameLine();
  if (ImGui::Button("Save scratch library"))
    SetStatus(Flush());
  ImGui::EndDisabled();
  DrawPixels(*asset);
}

}  // namespace yaze::editor
