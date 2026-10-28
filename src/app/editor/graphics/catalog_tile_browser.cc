#include "app/editor/graphics/catalog_tile_browser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <utility>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/palette_controls_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/style.h"
#include "app/platform/asset_loader.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {
using Colors = std::array<ImU32, 16>;

bool SameRect(const CatalogArtRect& a, const CatalogArtRect& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

const CatalogSheetSource* SelectedSource(
    const CatalogTileBrowserState& browser) {
  return browser.source >= 0 && size_t(browser.source) < browser.sources.size()
             ? &browser.sources[browser.source]
             : nullptr;
}

absl::Status EnsureAssetPreview(CatalogTileBrowserState& browser) {
  const auto* source = SelectedSource(browser);
  if (!source)
    return absl::FailedPreconditionError("Select a catalog asset");
  const auto id = source->asset_id.empty() ? source->id : source->asset_id;
  if (browser.asset_preview && browser.asset_cache_id == id)
    return absl::OkStatus();
  ASSIGN_OR_RETURN(auto image, AssembleCatalogAsset(browser.sources, id));
  CancelCatalogArtPreview(browser);
  browser.asset_cache_id = id;
  browser.asset_preview = std::move(image);
  browser.art_selection = {0, 0, browser.asset_preview->width,
                           browser.asset_preview->height};
  browser.art_drag_x = browser.art_drag_y = -1;
  return absl::OkStatus();
}

void ResetSourceSelection(CatalogTileBrowserState& browser, int source) {
  browser.source = source;
  browser.selected.clear();
  browser.last_source_tile = browser.drag_start = -1;
  browser.pending.reset();
  browser.asset_cache_id.clear();
  browser.asset_preview.reset();
  CancelCatalogArtPreview(browser);
  if (SelectedSource(browser)) {
    const auto status = EnsureAssetPreview(browser);
    if (!status.ok())
      browser.status = std::string(status.message());
  }
}

bool SameSourcePattern(const GraphicsEditContext& a,
                       const GraphicsEditContext& b) {
  if (a.owner != b.owner || a.resolved.surface != b.resolved.surface ||
      a.resolved.id != b.resolved.id ||
      a.resolved.parent_id != b.resolved.parent_id ||
      a.pattern_width != b.pattern_width ||
      a.pattern_height != b.pattern_height ||
      a.targets.size() != b.targets.size() ||
      a.pattern_cells.size() != b.pattern_cells.size())
    return false;
  for (size_t i = 0; i < a.targets.size(); ++i)
    if (a.targets[i].sheet_id != b.targets[i].sheet_id ||
        a.targets[i].tile_index != b.targets[i].tile_index)
      return false;
  for (size_t i = 0; i < a.pattern_cells.size(); ++i) {
    const auto& x = a.pattern_cells[i];
    const auto& y = b.pattern_cells[i];
    if (x.x != y.x || x.y != y.y || x.target != y.target ||
        x.hflip != y.hflip || x.vflip != y.vflip)
      return false;
  }
  return true;
}

bool Matches(std::string value, std::string filter) {
  const auto lower = [](unsigned char c) {
    return std::tolower(c);
  };
  std::transform(value.begin(), value.end(), value.begin(), lower);
  std::transform(filter.begin(), filter.end(), filter.begin(), lower);
  return value.find(filter) != std::string::npos;
}

// Draw from the immutable source or owning store; never from the global Arena.
int DrawSheet(const char* id, const std::vector<uint8_t>& pixels,
              const Colors& colors, const std::set<int>& selected = {},
              int* hovered_tile = nullptr) {
  if (pixels.size() != 4096)
    return -1;
  const float scale =
      std::clamp(ImGui::GetContentRegionAvail().x / 128.f, 2.f, 5.f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(128 * scale, 32 * scale));
  gui::AutoRegisterLastItem("canvas", id);
  auto* draw = ImGui::GetWindowDrawList();
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      const uint8_t index = pixels[y * 128 + x];
      const ImU32 color =
          index == 0 ? (((x / 4 + y / 4) % 2) ? IM_COL32(60, 60, 60, 255)
                                              : IM_COL32(38, 38, 38, 255))
                     : colors[index & 15];
      draw->AddRectFilled(
          ImVec2(origin.x + x * scale, origin.y + y * scale),
          ImVec2(origin.x + (x + 1) * scale, origin.y + (y + 1) * scale),
          color);
    }
  }
  for (int tile : selected) {
    const ImVec2 a(origin.x + (tile % 16) * 8 * scale,
                   origin.y + (tile / 16) * 8 * scale);
    draw->AddRect(a, ImVec2(a.x + 8 * scale, a.y + 8 * scale),
                  IM_COL32(255, 210, 55, 255), 0.f, 0, 2.f);
  }
  if (ImGui::IsItemHovered()) {
    const auto mouse = ImGui::GetIO().MousePos;
    const int x = std::clamp(int((mouse.x - origin.x) / (8 * scale)), 0, 15);
    const int y = std::clamp(int((mouse.y - origin.y) / (8 * scale)), 0, 3);
    if (hovered_tile)
      *hovered_tile = y * 16 + x;
    ImGui::SetTooltip("Tile $%02X", y * 16 + x);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      return y * 16 + x;
  }
  return -1;
}
void DrawLogicalAsset(CatalogTileBrowserState& browser, const Colors& colors) {
  if (!browser.asset_preview)
    return;
  const auto& image = *browser.asset_preview;
  ImGui::Text("Complete asset: %d x %d pixels; %d bpp", image.width,
              image.height, image.bpp);
  ImGui::TextUnformatted(
      "Drag an 8-pixel rectangle, or select the whole asset.");
  const float scale =
      std::clamp(ImGui::GetContentRegionAvail().x / image.width, 1.f, 4.f);
  ImGui::BeginChild("CatalogLogicalAsset",
                    ImVec2(0, std::min(330.f, image.height * scale + 16)),
                    ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const auto origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("Logical asset pixels",
                         ImVec2(image.width * scale, image.height * scale));
  gui::AutoRegisterLastItem("canvas", "Logical asset pixels");
  auto* draw = ImGui::GetWindowDrawList();
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width;) {
      const auto index = image.indexed_pixels[y * image.width + x];
      const auto color_at = [&](int px) {
        return index == 0 ? ((px / 4 + y / 4) % 2 ? IM_COL32(60, 60, 60, 255)
                                                  : IM_COL32(38, 38, 38, 255))
                          : colors[index & 15];
      };
      const auto color = color_at(x);
      int end = x + 1;
      while (end < image.width &&
             image.indexed_pixels[y * image.width + end] == index &&
             color_at(end) == color)
        ++end;
      draw->PrimReserve(6, 4);
      draw->PrimRect(ImVec2(origin.x + x * scale, origin.y + y * scale),
                     ImVec2(origin.x + end * scale, origin.y + (y + 1) * scale),
                     color);
      x = end;
    }
  }
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const auto mouse = ImGui::GetMousePos();
    browser.art_drag_x = std::clamp(int((mouse.x - origin.x) / (8 * scale)), 0,
                                    image.width / 8 - 1);
    browser.art_drag_y = std::clamp(int((mouse.y - origin.y) / (8 * scale)), 0,
                                    image.height / 8 - 1);
  }
  if (browser.art_drag_x >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    const auto mouse = ImGui::GetMousePos();
    const int x = std::clamp(int((mouse.x - origin.x) / (8 * scale)), 0,
                             image.width / 8 - 1);
    const int y = std::clamp(int((mouse.y - origin.y) / (8 * scale)), 0,
                             image.height / 8 - 1);
    const CatalogArtRect rectangle{std::min(x, browser.art_drag_x) * 8,
                                   std::min(y, browser.art_drag_y) * 8,
                                   (std::abs(x - browser.art_drag_x) + 1) * 8,
                                   (std::abs(y - browser.art_drag_y) + 1) * 8};
    if (!SameRect(rectangle, browser.art_selection)) {
      browser.art_selection = rectangle;
      CancelCatalogArtPreview(browser);
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    browser.art_drag_x = browser.art_drag_y = -1;
  const auto& selected = browser.art_selection;
  draw->AddRect(
      ImVec2(origin.x + selected.x * scale, origin.y + selected.y * scale),
      ImVec2(origin.x + (selected.x + selected.width) * scale,
             origin.y + (selected.y + selected.height) * scale),
      IM_COL32(255, 210, 55, 255), 0.0f, 2.0f, ImDrawFlags_None);
  ImGui::EndChild();
  int crop_x = selected.x / 8, crop_y = selected.y / 8;
  int crop_width = selected.width / 8, crop_height = selected.height / 8;
  const auto edit_crop = [](const char* label, int& value) {
    ImGui::SetNextItemWidth(140);
    // A scalar input retains its own item ID for keyboard focus and widget
    // discovery. Step buttons wrap InputInt in a group whose final ID is zero.
    const bool changed = ImGui::InputInt(label, &value, 0, 0,
                                         ImGuiInputTextFlags_EnterReturnsTrue);
    gui::AutoRegisterLastItem("input", label);
    return changed;
  };
  ImGui::TextUnformatted("Crop in 8-pixel tiles; press Enter to commit.");
  bool crop_changed = edit_crop("Crop X (tiles)", crop_x);
  crop_changed |= edit_crop("Crop Y (tiles)", crop_y);
  crop_changed |= edit_crop("Crop width (tiles)", crop_width);
  crop_changed |= edit_crop("Crop height (tiles)", crop_height);
  if (crop_changed) {
    crop_x = std::clamp(crop_x, 0, image.width / 8 - 1);
    crop_y = std::clamp(crop_y, 0, image.height / 8 - 1);
    crop_width = std::clamp(crop_width, 1, image.width / 8 - crop_x);
    crop_height = std::clamp(crop_height, 1, image.height / 8 - crop_y);
    const CatalogArtRect rectangle{crop_x * 8, crop_y * 8, crop_width * 8,
                                   crop_height * 8};
    if (!SameRect(rectangle, browser.art_selection)) {
      browser.art_selection = rectangle;
      CancelCatalogArtPreview(browser);
    }
  }
  if (gui::AutoButton("Select whole asset")) {
    browser.art_selection = {0, 0, image.width, image.height};
    CancelCatalogArtPreview(browser);
  }
  ImGui::SameLine();
  ImGui::Text("Crop %d,%d: %d x %d pixels", selected.x, selected.y,
              selected.width, selected.height);
}

void DrawStagedPreview(CatalogTileBrowserState& browser) {
  if (!browser.staged_bitmap.is_active())
    return;
  const float scale = std::clamp(
      ImGui::GetContentRegionAvail().x / browser.staged_bitmap.width(), 0.25f,
      2.f);
  gui::CanvasFrameOptions options;
  options.canvas_size = ImVec2(browser.staged_bitmap.width() * scale + 4,
                               browser.staged_bitmap.height() * scale + 4);
  options.draw_grid = options.draw_context_menu = options.draw_overlay =
      options.render_popups = options.use_child_window = false;
  // The scrollbar helper takes content size and fills the remaining viewport.
  // Bound the preview viewport so Apply/Cancel remain reachable below it.
  const float preview_height =
      std::clamp(options.canvas_size.y + ImGui::GetStyle().WindowPadding.y * 2,
                 64.f, 330.f);
  ImGui::BeginChild("CatalogDetachedPreview", ImVec2(0, preview_height),
                    ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
  auto canvas = gui::BeginCanvas(browser.staged_canvas, options);
  browser.staged_canvas.DrawBitmap(browser.staged_bitmap, 2, 2, scale);
  gui::EndCanvas(browser.staged_canvas, canvas, options);
  ImGui::EndChild();
}

bool DrawMetadataFilter(const char* label, std::string& selected,
                        const std::set<std::string>& values) {
  const bool open =
      ImGui::BeginCombo(label, selected.empty() ? "All" : selected.c_str());
  gui::AutoRegisterLastItem("combo", label);
  bool changed = false;
  if (open) {
    if (ImGui::Selectable("All", selected.empty())) {
      selected.clear();
      changed = true;
    }
    for (const auto& value : values) {
      if (ImGui::Selectable(value.c_str(), value == selected)) {
        selected = value;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}
}  // namespace

CatalogTileBrowserState::~CatalogTileBrowserState() {
  gfx::Arena::Get().RetireBitmap(staged_bitmap);
}

void CancelCatalogArtPreview(CatalogTileBrowserState& browser) {
  browser.object_pending.reset();
  browser.candidate_source_revision.reset();
  browser.candidate_source_id.clear();
  gfx::Arena::Get().RetireBitmap(browser.staged_bitmap);
}

absl::Status LoadCatalogTileBrowser(CatalogTileBrowserState& browser,
                                    const std::string& filename) {
  auto sources = LoadCatalogSheetSources(filename);
  if (!sources.ok()) {
    browser.status = "Catalog load failed; previous library retained: " +
                     std::string(sources.status().message());
    return sources.status();
  }
  std::vector<CatalogPreviewPalette> palettes;
  auto loaded_palettes = LoadCatalogPreviewPalettes(filename);
  if (loaded_palettes.ok())
    palettes = std::move(*loaded_palettes);
  browser.sources = std::move(*sources);
  browser.palettes = std::move(palettes);
  browser.path = browser.loaded_path = filename;
  browser.pending.reset();
  browser.palette =
      browser.sources.empty() || browser.sources[0].palette.empty() ? -1 : -3;
  ResetSourceSelection(browser, browser.sources.empty() ? -1 : 0);
  browser.status = "Loaded " + std::to_string(browser.sources.size()) +
                   " verified source pages.";
  if (!loaded_palettes.ok() &&
      std::none_of(browser.sources.begin(), browser.sources.end(),
                   [](const auto& source) { return !source.palette.empty(); }))
    browser.status += " Palette preview unavailable: " +
                      std::string(loaded_palettes.status().message());
  return absl::OkStatus();
}

namespace {
absl::Status ValidateCatalogArtPreviewInputs(
    CatalogTileBrowserState& browser, const GraphicsEditorState& destination,
    bool resolve_pattern) {
  if (!browser.object_pending)
    return absl::OkStatus();
  auto refuse = [&](const std::string& message) {
    CancelCatalogArtPreview(browser);
    return absl::FailedPreconditionError(message);
  };
  const auto& plan = *browser.object_pending;
  const auto* source = SelectedSource(browser);
  if (plan.owner != destination.game_data ||
      plan.source_context_revision != destination.source_context_revision ||
      !destination.source_context || !source ||
      source->id != browser.candidate_source_id ||
      !SameRect(browser.art_selection, browser.candidate_selection))
    return refuse("Source or destination context changed; try the art again.");
  const auto* store = ActiveSheetStore(destination);
  if (!store)
    return refuse("Destination graphics session is inactive.");
  for (const auto& [sheet, before] : plan.before_sheets) {
    const auto revision = plan.sheet_revisions.find(sheet);
    if (!store->HasSheet(sheet) || revision == plan.sheet_revisions.end() ||
        store->Revision(sheet) != revision->second)
      return refuse("Destination pixels changed; try the art again.");
    const auto current = store->Sheet(sheet);
    if (current.size() != before.size() ||
        !std::equal(current.begin(), current.end(), before.begin()))
      return refuse("Destination pixels changed; try the art again.");
  }
  const auto& context = *destination.source_context;
  if (!plan.source || !context.resolve_live_source)
    return refuse("Selected object cannot resolve its live source.");
  // A room-aware source trace replays the preceding object stream. Keep that
  // work at Try/Apply boundaries when the source supplies a semantic stamp.
  // Callers without a stamp retain full validation on each draw.
  if (resolve_pattern || !context.preview_revision) {
    GraphicsEditContext fresh;
    const auto resolved = context.resolve_live_source(fresh);
    if (!resolved.ok() || !SameSourcePattern(*plan.source, fresh))
      return refuse("Selected object pattern changed; try the art again.");
  }
  if (context.preview_revision) {
    const auto revision = context.preview_revision();
    if (!revision.ok() || !browser.candidate_source_revision ||
        *revision != *browser.candidate_source_revision)
      return refuse("Selected object preview changed; try the art again.");
  } else if (browser.candidate_source_revision) {
    return refuse("Selected object preview context changed.");
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status ValidateCatalogArtPreview(CatalogTileBrowserState& browser,
                                       const GraphicsEditorState& destination) {
  return ValidateCatalogArtPreviewInputs(browser, destination, true);
}

absl::Status TryCatalogArtOnObject(CatalogTileBrowserState& browser,
                                   GraphicsEditorState& destination) {
  CancelCatalogArtPreview(browser);
  RETURN_IF_ERROR(EnsureAssetPreview(browser));
  if (!destination.source_context ||
      !destination.source_context->render_staged_preview)
    return absl::FailedPreconditionError(
        "Open an object's graphics source to preview imported art.");
  ASSIGN_OR_RETURN(
      auto plan, PlanCatalogObjectTransfer(destination, *browser.asset_preview,
                                           browser.art_selection));
  auto overrides = plan.before_sheets;
  for (const auto& diff : plan.sheets) {
    auto& pixels = overrides.at(diff.sheet);
    for (size_t i = 0; i < diff.offsets.size(); ++i)
      pixels.at(diff.offsets[i]) = diff.after.at(i);
  }
  ASSIGN_OR_RETURN(
      auto image, destination.source_context->render_staged_preview(overrides));
  const bool indexed =
      image.format == usage_preview::PreviewImage::Format::kIndexed;
  if (image.width <= 0 || image.height <= 0 || image.width > 1024 ||
      image.height > 1024 ||
      image.pixels.size() !=
          static_cast<size_t>(image.width) * image.height * (indexed ? 1 : 4))
    return absl::InvalidArgumentError(
        "Detached object renderer returned invalid image dimensions.");
  if (!destination.source_context ||
      destination.source_context->owner != plan.owner ||
      destination.source_context_revision != plan.source_context_revision)
    return absl::FailedPreconditionError(
        "Selected object context changed during preview.");
  std::optional<uint64_t> revision;
  if (destination.source_context->preview_revision) {
    ASSIGN_OR_RETURN(revision, destination.source_context->preview_revision());
  }
  browser.staged_bitmap.Create(image.width, image.height, indexed ? 8 : 32,
                               image.pixels);
  if (indexed) {
    std::vector<SDL_Color> colors(image.colors.begin(), image.colors.end());
    if (image.index0_transparent)
      colors[0].a = 0;
    browser.staged_bitmap.SetPalette(colors);
  }
  browser.staged_bitmap.metadata().purpose =
      gfx::Bitmap::BitmapPurpose::kPreview;
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &browser.staged_bitmap);
  browser.candidate_source_revision = revision;
  browser.candidate_source_id = SelectedSource(browser)->id;
  browser.candidate_selection = browser.art_selection;
  browser.object_pending = std::move(plan);
  browser.destination_owner = destination.game_data;
  browser.destination_sheet = destination.current_sheet_id;
  return ValidateCatalogArtPreview(browser, destination);
}

absl::Status ApplyCatalogArtToObject(CatalogTileBrowserState& browser,
                                     GraphicsEditorState& destination,
                                     UndoManager& history) {
  RETURN_IF_ERROR(ValidateCatalogArtPreview(browser, destination));
  if (!browser.object_pending)
    return absl::FailedPreconditionError("Try the art before applying it.");
  RETURN_IF_ERROR(
      ApplyPixelIndexPlan(destination, history, *browser.object_pending));
  CancelCatalogArtPreview(browser);
  return absl::OkStatus();
}

void DrawCatalogTileBrowser(CatalogTileBrowserState& browser,
                            GraphicsEditorState& destination,
                            UndoManager* undo_manager,
                            const std::string& destination_usage) {
  gui::AutoWidgetScope widgets("Graphics/Catalog");
  if (!browser.default_attempted) {
    browser.default_attempted = true;
    if (browser.sources.empty() && browser.path.empty()) {
      const auto bundled =
          AssetLoader::FindAssetFile("graphics/catalog/minish/catalog.json");
      if (bundled.ok()) {
        (void)LoadCatalogTileBrowser(browser, bundled->string());
      } else {
        browser.status =
            "Bundled art catalog is unavailable; open a catalog JSON.";
      }
    }
  }
  if (browser.destination_owner != destination.game_data ||
      browser.destination_sheet != destination.current_sheet_id) {
    browser.pending.reset();
    CancelCatalogArtPreview(browser);
    browser.destination_owner = destination.game_data;
    browser.destination_sheet = destination.current_sheet_id;
  }
  const auto valid_candidate =
      ValidateCatalogArtPreviewInputs(browser, destination, false);
  if (!valid_candidate.ok())
    browser.status = std::string(valid_candidate.message());
  ImGui::TextWrapped(
      "Browse complete art assets or source sheets. Try an art rectangle on "
      "the "
      "selected object's existing graphics before applying pixel changes.");
  if (ImGui::InputTextWithHint("Catalog JSON", "references.catalog.json",
                               &browser.path))
    browser.pending.reset();
  gui::AutoRegisterLastItem("input", "Catalog JSON");
  if (gui::AutoButton("Browse catalog")) {
    util::FileDialogOptions options;
    options.filters.push_back({"Graphics catalog", "json"});
    const auto path = util::FileDialogWrapper::ShowOpenFileDialog(options);
    if (!path.empty()) {
      browser.path = path;
      browser.pending.reset();
    }
  }
  ImGui::SameLine();
  if (gui::AutoButton("Load catalog"))
    (void)LoadCatalogTileBrowser(browser, browser.path);
  if (!browser.status.empty())
    ImGui::TextWrapped("%s", browser.status.c_str());
  if (browser.sources.empty())
    return;
  ImGui::TextWrapped("Loaded source: %s", browser.loaded_path.c_str());
  ImGui::InputTextWithHint("Filter sources", "Project or sheet name",
                           &browser.search);
  gui::AutoRegisterLastItem("input", "Filter sources");
  std::set<std::string> groups, categories;
  for (const auto& source : browser.sources) {
    if (!source.group.empty())
      groups.insert(source.group);
    if (!source.category.empty())
      categories.insert(source.category);
  }
  DrawMetadataFilter("Group", browser.group_filter, groups);
  DrawMetadataFilter("Category", browser.category_filter, categories);
  if (ImGui::BeginListBox("##CatalogSources", ImVec2(-1, 130))) {
    for (size_t i = 0; i < browser.sources.size(); ++i) {
      const auto& source = browser.sources[i];
      if (source.page != 0 ||
          (!browser.group_filter.empty() &&
           source.group != browser.group_filter) ||
          (!browser.category_filter.empty() &&
           source.category != browser.category_filter) ||
          !Matches(source.title + " " + source.id + " " + source.material +
                       " " + source.game + " " + source.group,
                   browser.search))
        continue;
      ImGui::PushID(static_cast<int>(i));
      const auto* current_source = SelectedSource(browser);
      const bool selected =
          current_source && (source.asset_id.empty()
                                 ? current_source->id == source.id
                                 : current_source->asset_id == source.asset_id);
      const auto& label = source.name.empty() ? source.title : source.name;
      if (ImGui::Selectable(label.c_str(), selected)) {
        ResetSourceSelection(browser, static_cast<int>(i));
        browser.palette = source.palette.empty() ? -1 : -3;
      }
      gui::AutoRegisterLastItem(
          "selectable", source.name.empty() ? source.title : source.name);
      ImGui::PopID();
    }
    ImGui::EndListBox();
  }
  if (!SelectedSource(browser))
    return;
  const auto assembled = EnsureAssetPreview(browser);
  if (!assembled.ok()) {
    ImGui::TextWrapped("Asset preview unavailable: %s",
                       std::string(assembled.message()).c_str());
    return;
  }
  Colors colors;
  for (int i = 0; i < 16; ++i) {
    const int level =
        std::min(255, i * 255 / ((1 << browser.asset_preview->bpp) - 1));
    colors[i] = IM_COL32(level, level, level, 255);
  }
  if (browser.palette >= int(browser.palettes.size()))
    browser.palette = -3;
  const bool source_colors_available = !browser.asset_preview->palette.empty();
  const bool eight_color_preview = browser.asset_preview->bpp <= 3;
  if ((browser.palette == -3 && !source_colors_available) ||
      (!eight_color_preview && (browser.palette == -2 || browser.palette >= 0)))
    browser.palette = source_colors_available ? -3 : -1;
  ImGui::InputTextWithHint("Filter palettes", "ROM, walls, sprites, overworld",
                           &browser.palette_search);
  gui::AutoRegisterLastItem("input", "Filter palettes");
  const char* palette_label =
      browser.palette == -3   ? "Source asset palette"
      : browser.palette == -1 ? "Grayscale"
      : browser.palette == -2 ? "Destination palette"
                              : browser.palettes[browser.palette].label.c_str();
  const bool palette_open = ImGui::BeginCombo("Preview colors", palette_label);
  gui::AutoRegisterLastItem("combo", "Preview colors");
  if (palette_open) {
    ImGui::BeginDisabled(!source_colors_available);
    if (ImGui::Selectable("Source asset palette", browser.palette == -3))
      browser.palette = -3;
    ImGui::EndDisabled();
    if (ImGui::Selectable("Grayscale", browser.palette == -1))
      browser.palette = -1;
    ImGui::BeginDisabled(!eight_color_preview);
    if (ImGui::Selectable("Destination palette", browser.palette == -2))
      browser.palette = -2;
    for (size_t i = 0; i < browser.palettes.size(); ++i) {
      if (!Matches(
              browser.palettes[i].label + " " + browser.palettes[i].source_path,
              browser.palette_search))
        continue;
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(browser.palettes[i].label.c_str(),
                            browser.palette == int(i)))
        browser.palette = static_cast<int>(i);
      ImGui::PopID();
    }
    ImGui::EndDisabled();
    ImGui::EndCombo();
  }
  if (!eight_color_preview)
    ImGui::TextWrapped(
        "This 4bpp asset uses source colors or grayscale. Destination and "
        "catalog preview palettes contain only 8 colors.");
  if (browser.palette == -3 && browser.asset_preview) {
    for (size_t i = 0;
         i < browser.asset_preview->palette.size() && i < colors.size(); ++i) {
      const auto c = browser.asset_preview->palette[i];
      colors[i] = IM_COL32((c & 31) * 255 / 31, ((c >> 5) & 31) * 255 / 31,
                           ((c >> 10) & 31) * 255 / 31, 255);
    }
  }
  if (browser.palette >= 0) {
    const auto& palette = browser.palettes[browser.palette];
    ImGui::TextWrapped("Palette source: %s", palette.source_path.c_str());
    for (int i = 0; i < 8; ++i)
      colors[i] = IM_COL32(palette.colors[i][0], palette.colors[i][1],
                           palette.colors[i][2], 255);
  }
  ImGui::TextUnformatted("Destination sheet colors");
  DrawGraphicsPreviewPaletteSelector(destination);
  Colors destination_colors = colors;
  bool destination_colors_available = false;
  if (destination.game_data) {
    if (const auto* choice =
            destination.PreviewPalette(destination.current_sheet_id)) {
      const auto resolved =
          ResolveGraphicsPreviewPalette(*destination.game_data, *choice);
      if (resolved.ok()) {
        for (int i = 0; i < 8; ++i) {
          const auto rgb = resolved->colors[i].rgb();
          destination_colors[i] =
              IM_COL32(int(rgb.x), int(rgb.y), int(rgb.z), 255);
        }
        destination_colors_available = true;
      }
    } else {
      const auto sheet = destination.current_sheet_id;
      const auto* loaded =
          sheet < destination.game_data->gfx_bitmaps.size()
              ? platform::GetSurfacePalette(
                    destination.game_data->gfx_bitmaps[sheet].surface())
              : nullptr;
      if (loaded && loaded->ncolors >= 8) {
        for (int i = 0; i < 8; ++i) {
          const auto color = loaded->colors[i];
          destination_colors[i] = IM_COL32(color.r, color.g, color.b, 255);
        }
        destination_colors_available = true;
      }
    }
  }
  if (!destination_colors_available) {
    ImGui::TextWrapped(
        "Destination colors unavailable; select a loaded sheet "
        "or a destination preview palette.");
    browser.pending.reset();
  }
  if (browser.palette == -2 && destination_colors_available)
    colors = destination_colors;
  ImGui::TextWrapped(
      "Preview colors only. Paste preserves pixel indices and "
      "does not replace destination palette colors.");
  const auto& source = browser.sources[browser.source];
  ImGui::TextWrapped("%s | %s | %s", source.group.c_str(),
                     source.category.c_str(), source.material.c_str());
  if (!source.game.empty())
    ImGui::TextWrapped("Game: %s", source.game.c_str());
  if (!source.credit.empty())
    ImGui::TextWrapped("Credit: %s", source.credit.c_str());
  ImGui::TextWrapped("Source: %s", source.source_path.c_str());
  ImGui::TextWrapped(
      "Apply replaces the existing source tiles. Objects sharing those tiles "
      "will also show the new artwork.");
  DrawLogicalAsset(browser, colors);
  if (destination.source_context) {
    const auto& context = *destination.source_context;
    ImGui::TextWrapped("Selected object: %s", context.label.c_str());
    ImGui::Text("Target pattern: %d x %d pixels; %zu source targets",
                context.pattern_width, context.pattern_height,
                context.targets.size());
  } else {
    ImGui::TextWrapped(
        "Open an object's graphics source to try this art on its existing "
        "pattern.");
  }
  ImGui::BeginDisabled(!destination.source_context ||
                       !destination.source_context->render_staged_preview);
  if (gui::AutoButton("Try on selected object")) {
    const auto status = TryCatalogArtOnObject(browser, destination);
    browser.status =
        status.ok()
            ? "Detached candidate ready. Apply changes pixel indices only."
            : std::string(status.message());
  }
  ImGui::EndDisabled();
  if (browser.object_pending) {
    DrawStagedPreview(browser);
    ImGui::BeginDisabled(!undo_manager || destination.selection.is_floating);
    if (gui::AutoButton("Apply imported art")) {
      const bool changed = !browser.object_pending->sheets.empty();
      const auto status =
          ApplyCatalogArtToObject(browser, destination, *undo_manager);
      browser.status = !status.ok() ? std::string(status.message())
                       : changed
                           ? "Applied imported art. Undo restores prior pixels."
                           : "No pixels changed; no Undo step was added.";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (gui::AutoButton("Cancel imported art")) {
      CancelCatalogArtPreview(browser);
      browser.status =
          "Detached candidate canceled; destination pixels were unchanged.";
    }
  }
  if (!destination_colors_available)
    return;
  const bool show_page_copy = ImGui::CollapsingHeader(
      "Page-local tile copy", source.source_kind == "indexed-png"
                                  ? ImGuiTreeNodeFlags_None
                                  : ImGuiTreeNodeFlags_DefaultOpen);
  gui::AutoRegisterLastItem("header", "Page-local tile copy");
  if (!show_page_copy)
    return;
  if (source.page_count > 1) {
    const bool open = ImGui::BeginCombo("Source page", source.title.c_str());
    gui::AutoRegisterLastItem("combo", "Source page");
    if (open) {
      for (size_t i = 0; i < browser.sources.size(); ++i) {
        const auto& page = browser.sources[i];
        if (page.asset_id == source.asset_id &&
            ImGui::Selectable(page.title.c_str(), browser.source == int(i)))
          ResetSourceSelection(browser, static_cast<int>(i));
      }
      ImGui::EndCombo();
    }
  }
  ImGui::TextUnformatted(
      "Click tiles, drag a rectangle, or Shift-click a range.");
  int hovered_source = -1;
  const int clicked = DrawSheet("##CatalogSourcePixels",
                                SelectedSource(browser)->indexed_pixels, colors,
                                browser.selected, &hovered_source);
  if (clicked >= 0) {
    browser.drag_start = clicked;
    if (ImGui::GetIO().KeyShift && browser.last_source_tile >= 0) {
      for (int i = std::min(clicked, browser.last_source_tile);
           i <= std::max(clicked, browser.last_source_tile); ++i)
        browser.selected.insert(i);
    } else if (!browser.selected.erase(clicked)) {
      browser.selected.insert(clicked);
    }
    browser.last_source_tile = clicked;
    browser.pending.reset();
  }
  if (browser.drag_start >= 0 && hovered_source >= 0 &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    std::set<int> rectangle;
    const int x0 = std::min(browser.drag_start % 16, hovered_source % 16);
    const int x1 = std::max(browser.drag_start % 16, hovered_source % 16);
    const int y0 = std::min(browser.drag_start / 16, hovered_source / 16);
    const int y1 = std::max(browser.drag_start / 16, hovered_source / 16);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        rectangle.insert(y * 16 + x);
    if (rectangle != browser.selected) {
      browser.selected = std::move(rectangle);
      browser.pending.reset();
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    browser.drag_start = -1;
  if (gui::AutoCheckbox("Keep source tile layout", &browser.preserve_layout))
    browser.pending.reset();
  if (gui::AutoButton("Select whole sheet")) {
    for (int i = 0; i < 64; ++i)
      browser.selected.insert(i);
    browser.pending.reset();
  }
  ImGui::SameLine();
  if (gui::AutoButton("Clear selection")) {
    browser.selected.clear();
    browser.pending.reset();
  }
  if (gui::AutoInputInt("First destination tile",
                        &browser.first_destination_tile)) {
    browser.first_destination_tile =
        std::clamp(browser.first_destination_tile, 0, 63);
    browser.pending.reset();
  }
  const auto* store = ActiveSheetStore(destination);
  if (!store || !store->HasSheet(destination.current_sheet_id)) {
    ImGui::TextWrapped(
        "Select a loaded destination sheet in this ROM session.");
    browser.pending.reset();
    return;
  }
  const auto current = store->Sheet(destination.current_sheet_id);
  const std::vector<uint8_t> before(current.begin(), current.end());
  ImGui::Text("Destination sheet $%02X — click its first tile",
              destination.current_sheet_id);
  std::set<int> target_tiles;
  if (!browser.selected.empty()) {
    int min_x = 15, min_y = 3;
    for (int tile : browser.selected) {
      min_x = std::min(min_x, tile % 16);
      min_y = std::min(min_y, tile / 16);
    }
    int i = 0;
    for (int tile : browser.selected) {
      const int offset = browser.preserve_layout
                             ? (tile / 16 - min_y) * 16 + tile % 16 - min_x
                             : i;
      const int target = browser.first_destination_tile + offset;
      if (target < 64 &&
          (!browser.preserve_layout ||
           browser.first_destination_tile % 16 + tile % 16 - min_x < 16))
        target_tiles.insert(target);
      ++i;
    }
  }
  const int target = DrawSheet("##CatalogDestinationPixels", before,
                               destination_colors, target_tiles);
  if (target >= 0) {
    browser.first_destination_tile = target;
    browser.pending.reset();
  }
  if (!destination_usage.empty())
    ImGui::TextWrapped("Used by: %s", destination_usage.c_str());
  ImGui::Text("%zu selected tiles; %s", browser.selected.size(),
              browser.preserve_layout ? "source layout preserved"
                                      : "packed in row-major order");
  ImGui::BeginDisabled(browser.selected.empty() || !undo_manager ||
                       destination.selection.is_floating);
  if (gui::AutoButton("Preview tile copy")) {
    const std::vector<int> selected(browser.selected.begin(),
                                    browser.selected.end());
    auto plan = PreviewCatalogTileTransfer(
        destination.current_sheet_id, before,
        SelectedSource(browser)->indexed_pixels, selected,
        browser.first_destination_tile, browser.preserve_layout);
    browser.pending.reset();
    if (!plan.ok())
      browser.status = std::string(plan.status().message());
    else {
      plan->destination_owner = destination.game_data;
      plan->destination_revision =
          store->Revision(destination.current_sheet_id);
      browser.pending = std::move(*plan);
      browser.status = "Ready to apply one undoable tile copy.";
    }
  }
  ImGui::EndDisabled();
  if (browser.pending) {
    if (browser.pending->destination_revision !=
            store->Revision(destination.current_sheet_id) ||
        browser.pending->before_pixels != before) {
      browser.pending.reset();
      browser.status = "Destination changed; preview the copy again.";
      return;
    }
    ImGui::Text("After copy: %zu changed tiles",
                browser.pending->changed_tiles.size());
    DrawSheet("##CatalogAfterPixels", browser.pending->after_pixels,
              destination_colors, target_tiles);
    if (gui::AutoButton("Apply tile copy")) {
      const auto result =
          ApplyCatalogTileTransfer(*browser.pending, destination, undo_manager);
      browser.status = !result.ok() ? std::string(result.message())
                       : browser.pending->changed_tiles.empty()
                           ? "No pixels changed; no undo step was added."
                           : "Copied tiles. Undo restores prior pixels.";
      browser.pending.reset();
    }
    ImGui::SameLine();
    if (gui::AutoButton("Cancel copy"))
      browser.pending.reset();
  }
}
}  // namespace yaze::editor
