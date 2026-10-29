#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"

#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/graphics_scratch_storage.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/style.h"
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

GraphicsScratchView::~GraphicsScratchView() {
  gfx::Arena::Get().RetireBitmap(preview_bitmap_);
}

void GraphicsScratchView::SetAssociatedPreview(
    std::string label, ScratchPreviewCallback callback) {
  const auto* asset = Selected();
  preview_asset_id_ = asset ? asset->id : 0;
  preview_label_ = std::move(label);
  preview_callback_ = std::move(callback);
  preview_hash_.reset();
  preview_error_.clear();
  gfx::Arena::Get().RetireBitmap(preview_bitmap_);
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

absl::Status GraphicsScratchView::SwapColorIds(
    const std::vector<ScratchColorIdTarget>& targets,
    const ColorIdPermutation& permutation) {
  RETURN_IF_ERROR(PrepareCommand());
  if (!load_ok_)
    return absl::FailedPreconditionError("Scratch library could not be loaded");
  ASSIGN_OR_RETURN(const auto plan, PlanScratchColorIdPermutation(
                                        library_, targets, permutation));
  if (plan.edit.ranges.empty())
    return absl::OkStatus();
  auto next = library_;
  RETURN_IF_ERROR(ApplyScratchColorIdPlan(next, plan));
  auto before = std::move(library_);
  library_ = std::move(next);
  return Record(std::move(before));
}

void GraphicsScratchView::DrawColorIdSwap(
    const GraphicsScratchAsset& selected) {
  gui::AutoWidgetScope scratch_scope("Graphics/Scratch");
  // SwapColorIds replaces the library that owns `selected`.
  const uint64_t asset_id = selected.id;
  const int bpp = selected.bpp;
  const int last_id = (1 << bpp) - 1;
  ImGui::TextUnformatted("Swap color IDs");
  ImGui::SetNextItemWidth(120);
  ImGui::InputInt("From ID##scratch_swap", &swap_from_id_, 0, 0);
  gui::AutoRegisterLastItem("input", "Scratch swap from color ID");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  ImGui::InputInt("To ID##scratch_swap", &swap_to_id_, 0, 0);
  gui::AutoRegisterLastItem("input", "Scratch swap to color ID");
  swap_from_id_ = std::clamp(swap_from_id_, 1, last_id);
  swap_to_id_ = std::clamp(swap_to_id_, 1, last_id);
  ImGui::Checkbox("Every asset at this depth", &swap_all_at_depth_);
  gui::AutoRegisterLastItem("checkbox", "Scratch swap every asset at depth");
  ImGui::TextDisabled("ID 0 stays transparent; IDs 1-%d at %dbpp.", last_id,
                      bpp);
  ImGui::BeginDisabled(stroke_.has_value() || swap_from_id_ == swap_to_id_);
  if (ImGui::Button("Swap color IDs##scratch")) {
    std::vector<ScratchColorIdTarget> targets;
    for (const auto& candidate : library_.assets) {
      if (candidate.id == asset_id ||
          (swap_all_at_depth_ && candidate.bpp == bpp)) {
        const auto whole = WholeScratchAssetSelection(candidate.id);
        targets.insert(targets.end(), whole.begin(), whole.end());
      }
    }
    const auto permutation = MakeColorIdSwap(bpp, swap_from_id_, swap_to_id_);
    SetStatus(permutation.ok() ? SwapColorIds(targets, *permutation)
                               : permutation.status());
  }
  gui::AutoRegisterLastItem("button", "Scratch swap color IDs");
  ImGui::EndDisabled();
}

void GraphicsScratchView::DrawPixels(GraphicsScratchAsset& asset) {
  gui::AutoWidgetScope scratch_scope("Graphics/Scratch");
  ImGui::SliderFloat("Zoom", &zoom_, 2, 8, "%.0fx");
  color_ = std::clamp(color_, 0, (1 << asset.bpp) - 1);
  ImGui::SliderInt("Color", &color_, 0, (1 << asset.bpp) - 1);
  ImGui::BeginChild("ScratchPixels", ImVec2(0, 32 * zoom_ + 24), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const auto origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(
      "Pixels", ImVec2(128 * zoom_, 32 * zoom_),
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  gui::AutoRegisterLastItem("canvas", "ScratchPixels");
  auto* draw = ImGui::GetWindowDrawList();
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      const uint16_t c = asset.palette[asset.pixels[y * 128 + x]];
      const auto color =
          IM_COL32((c & 31) * 255 / 31, ((c >> 5) & 31) * 255 / 31,
                   ((c >> 10) & 31) * 255 / 31, 255);
      // Pixel cells must share hard edges at fractional zoom. ImGui 1.93
      // antialiases AddRectFilled when its bounds are fractional.
      draw->PrimReserve(6, 4);
      draw->PrimRect(
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

void GraphicsScratchView::DrawAssociatedPreview() {
  const auto* asset = Selected();
  if (!asset || asset->id != preview_asset_id_ || !preview_callback_)
    return;
  uint64_t hash = 1469598103934665603ULL;
  for (const auto pixel : asset->pixels)
    hash = (hash ^ pixel) * 1099511628211ULL;
  for (const auto color : asset->palette)
    hash = (hash ^ color) * 1099511628211ULL;
  if (!preview_hash_ || *preview_hash_ != hash) {
    preview_hash_ = hash;
    auto image = preview_callback_(*asset);
    if (!image.ok()) {
      preview_error_ = std::string(image.status().message());
    } else {
      const bool indexed =
          image->format == usage_preview::PreviewImage::Format::kIndexed;
      const size_t bytes_per_pixel = indexed ? 1 : 4;
      if (image->width <= 0 || image->height <= 0 || image->width > 1024 ||
          image->height > 1024 ||
          image->pixels.size() != static_cast<size_t>(image->width) *
                                      image->height * bytes_per_pixel) {
        preview_error_ = "Associated preview returned invalid image dimensions";
      } else {
        preview_error_.clear();
        const int depth = indexed ? 8 : 32;
        const bool create = !preview_bitmap_.is_active() ||
                            preview_bitmap_.width() != image->width ||
                            preview_bitmap_.height() != image->height ||
                            preview_bitmap_.depth() != depth;
        if (create)
          preview_bitmap_.Create(image->width, image->height, depth,
                                 image->pixels);
        else
          preview_bitmap_.set_data(image->pixels);
        if (indexed) {
          std::vector<SDL_Color> colors(image->colors.begin(),
                                        image->colors.end());
          if (image->index0_transparent)
            colors[0].a = 0;
          preview_bitmap_.SetPalette(colors);
        }
        preview_bitmap_.metadata().purpose =
            gfx::Bitmap::BitmapPurpose::kPreview;
        gfx::Arena::Get().QueueTextureCommand(
            create ? gfx::Arena::TextureCommandType::CREATE
                   : gfx::Arena::TextureCommandType::UPDATE,
            &preview_bitmap_);
      }
    }
  }
  ImGui::Separator();
  ImGui::TextWrapped("Associated sprite preview: %s", preview_label_.c_str());
  ImGui::TextDisabled(
      "Captured pose; scratch pixels update before BIN export.");
  if (!preview_error_.empty()) {
    ImGui::TextWrapped("Preview unavailable: %s", preview_error_.c_str());
    return;
  }
  if (!preview_bitmap_.is_active())
    return;
  const float scale = std::clamp(
      ImGui::GetContentRegionAvail().x / preview_bitmap_.width(), 0.25f, 2.0f);
  gui::CanvasFrameOptions options;
  options.canvas_size = ImVec2(preview_bitmap_.width() * scale + 4,
                               preview_bitmap_.height() * scale + 4);
  options.draw_grid = false;
  options.draw_context_menu = false;
  options.draw_overlay = false;
  options.render_popups = false;
  options.use_child_window = false;
  gui::BeginChildWithScrollbar("ScratchAssociatedPreview", options.canvas_size);
  auto canvas = gui::BeginCanvas(preview_canvas_, options);
  preview_canvas_.DrawBitmap(preview_bitmap_, 2, 2, scale);
  gui::EndCanvas(preview_canvas_, canvas, options);
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
  DrawColorIdSwap(*asset);
  // Swapping replaces the library; reacquire the selection before painting.
  asset = Selected();
  if (!asset)
    return;
  DrawPixels(*asset);
  DrawAssociatedPreview();
}

}  // namespace yaze::editor
