#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "util/i18n/tr.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

namespace {

void MixFingerprint(uint64_t* fingerprint, uint64_t value) {
  constexpr uint64_t kFnvPrime = 1099511628211ULL;
  *fingerprint ^= value;
  *fingerprint *= kFnvPrime;
}

bool PathsReferToSameAsset(const std::filesystem::path& lhs,
                           const std::filesystem::path& rhs) {
  if (lhs == rhs) {
    return true;
  }
  std::error_code error;
  const bool equivalent = std::filesystem::equivalent(lhs, rhs, error);
  return !error && equivalent;
}

}  // namespace

ObjectTileEditorPanel::ObjectTileEditorPanel(gfx::IRenderer* renderer, Rom* rom)
    : renderer_(renderer), rom_(rom) {
  tile_editor_ = std::make_unique<zelda3::ObjectTileEditor>(rom);
}

ObjectTileEditorPanel::~ObjectTileEditorPanel() {
  ClearRenderedBitmaps();
}

void ObjectTileEditorPanel::ClearRenderedBitmaps() {
  gfx::Arena::Get().RetireBitmap(object_preview_bmp_);
  gfx::Arena::Get().RetireBitmap(tile8_atlas_bmp_);
  object_preview_bmp_ = gfx::Bitmap();
  tile8_atlas_bmp_ = gfx::Bitmap();
}

void ObjectTileEditorPanel::ClearActionStatus() {
  action_status_tone_ = ActionStatusTone::kNone;
  action_status_message_.clear();
}

void ObjectTileEditorPanel::SetActionStatus(ActionStatusTone tone,
                                            std::string message) {
  action_status_tone_ = tone;
  action_status_message_ = std::move(message);
}

void ObjectTileEditorPanel::ResetTransientState() {
  selected_cell_index_ = -1;
  selected_source_tile_ = -1;
  source_attributes_ = 0;
  preview_dirty_ = true;
  atlas_dirty_ = true;
  room_graphics_revision_ = 0;
  show_shared_confirm_ = false;
  shared_object_count_ = 0;
  pending_shared_confirmation_.reset();
  shared_tile_data_usage_override_ = -1;
  source_impact_display_cache_key_.reset();
  source_impact_display_cache_result_.reset();
  ClearActionStatus();
  ClearRenderedBitmaps();
}

absl::Status ObjectTileEditorPanel::OpenForObject(int16_t object_id,
                                                  int room_id,
                                                  DungeonRoomStore* rooms) {
  return OpenForObject(object_id, room_id, rooms, current_palette_group_);
}

absl::Status ObjectTileEditorPanel::OpenForObject(
    int16_t object_id, int room_id, DungeonRoomStore* rooms,
    const gfx::PaletteGroup& palette_group) {
  if (!zelda3::ObjectTileEditor::IsEditableStandardObject(object_id)) {
    return absl::UnimplementedError(
        "Standard object tile editing is supported only for audited objects "
        "0x11F and 0x120");
  }
  if (rooms == nullptr) {
    return absl::InvalidArgumentError(
        "A dungeon room store is required to edit object tiles");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms->size())) {
    return absl::OutOfRangeError("Dungeon room id is outside the room store");
  }
  if (is_open_ && current_layout_.HasModifications()) {
    return absl::FailedPreconditionError(
        "Apply, revert, or close the current Object Tile Editor session "
        "before opening another object");
  }

  auto& room = (*rooms)[room_id];
  auto layout_or =
      tile_editor_->CaptureEditableObjectLayout(object_id, room, palette_group);
  if (!layout_or.ok()) {
    return layout_or.status();
  }

  // Do not disturb a live editor session until every fail-closed capture check
  // has passed. In particular, a bad selection must not discard unsaved edits.
  current_object_id_ = object_id;
  current_room_id_ = room_id;
  rooms_ = rooms;
  current_palette_group_ = palette_group;
  current_layout_ = std::move(*layout_or);
  ResetTransientState();
  is_open_ = true;
  SelectFirstCellIfAvailable();
  return absl::OkStatus();
}

absl::Status ObjectTileEditorPanel::OpenForCustomObject(
    int16_t object_id, int subtype, int room_id, DungeonRoomStore* rooms) {
  return OpenForCustomObject(object_id, subtype, room_id, rooms,
                             current_palette_group_);
}

absl::Status ObjectTileEditorPanel::OpenForCustomObject(
    int16_t object_id, int subtype, int room_id, DungeonRoomStore* rooms,
    const gfx::PaletteGroup& palette_group) {
  if (rooms == nullptr) {
    return absl::InvalidArgumentError(
        "A dungeon room store is required to edit custom object tiles");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms->size())) {
    return absl::OutOfRangeError("Dungeon room id is outside the room store");
  }
  if (!core::FeatureFlags::get().kEnableCustomObjects) {
    return absl::FailedPreconditionError(
        "Custom Objects is disabled for the current project");
  }
  if (rooms->GetIfLoaded(room_id) == nullptr) {
    return absl::FailedPreconditionError(
        "Load the current dungeon room before editing custom object graphics");
  }
  if (is_open_ && current_layout_.HasModifications()) {
    return absl::FailedPreconditionError(
        "Apply, revert, or explicitly discard the current Object Tile Editor "
        "session before opening another custom object");
  }

  auto layout_or = tile_editor_->LoadCustomObjectLayout(object_id, subtype);
  if (!layout_or.ok()) {
    return layout_or.status();
  }

  current_object_id_ = object_id;
  current_room_id_ = room_id;
  rooms_ = rooms;
  current_palette_group_ = palette_group;
  current_layout_ = std::move(*layout_or);
  ResetTransientState();
  is_open_ = true;
  SelectFirstCellIfAvailable();
  return absl::OkStatus();
}

void ObjectTileEditorPanel::Close() {
  is_open_ = false;
  current_layout_ = {};
  current_room_id_ = -1;
  current_object_id_ = -1;
  rooms_ = nullptr;
  ResetTransientState();
}

void ObjectTileEditorPanel::OnClose() {
  // Hiding a modified editor window must not silently throw away tile edits.
  // The session remains available when the workspace window is reopened; the
  // explicit action-bar close remains the user's discard action.
  if (!current_layout_.HasModifications()) {
    Close();
  }
}

void ObjectTileEditorPanel::SetCurrentPaletteGroup(
    const gfx::PaletteGroup& group) {
  current_palette_group_ = group;
  preview_dirty_ = true;
  atlas_dirty_ = true;
}

void ObjectTileEditorPanel::SetCurrentPaletteGroupForRoom(
    int room_id, const gfx::PaletteGroup& group) {
  if (is_open_ && current_room_id_ != room_id) {
    return;
  }
  SetCurrentPaletteGroup(group);
}

std::string ObjectTileEditorPanel::BuildWindowTitle() const {
  if (current_layout_.is_custom && !current_layout_.custom_filename.empty()) {
    return absl::StrFormat(ICON_MD_GRID_ON " Custom 0x%03X:%02X - %s",
                           current_object_id_, current_layout_.custom_subtype,
                           current_layout_.custom_filename.c_str());
  }

  return absl::StrFormat(ICON_MD_GRID_ON " Object 0x%03X - %s",
                         current_object_id_,
                         zelda3::GetObjectName(current_object_id_).c_str());
}

void ObjectTileEditorPanel::SelectFirstCellIfAvailable() {
  if (current_layout_.cells.empty()) {
    selected_cell_index_ = -1;
    selected_source_tile_ = -1;
    SyncSourceAttributesFromSelectedCell();
    return;
  }

  selected_cell_index_ = 0;
  SyncSourceSelectionFromSelectedCell();
}

absl::Status ObjectTileEditorPanel::AddFirstTileToEmptyCustomLayout() {
  if (!is_open_ || !current_layout_.is_custom) {
    return absl::FailedPreconditionError(
        "Only an open custom-object session can add its first tile");
  }
  if (!current_layout_.cells.empty()) {
    return absl::FailedPreconditionError(
        "The custom-object layout already contains tiles");
  }

  zelda3::ObjectTileLayout::Cell cell;
  cell.rel_x = 0;
  cell.rel_y = 0;
  cell.tile_info = gfx::TileInfo(/*id=*/0, /*palette=*/2, false, false, false);
  cell.original_word = 0;
  cell.write_index = 0;
  cell.modified = true;
  current_layout_.cells.push_back(cell);
  current_layout_.bounds_width = 1;
  current_layout_.bounds_height = 1;
  SelectFirstCellIfAvailable();
  preview_dirty_ = true;
  atlas_dirty_ = true;
  ClearActionStatus();
  return absl::OkStatus();
}

void ObjectTileEditorPanel::RevertCurrentLayout() {
  const bool added_first_tile_to_empty_asset =
      current_layout_.is_custom &&
      current_layout_.custom_source_bytes == std::vector<uint8_t>({0, 0}) &&
      current_layout_.cells.size() == 1 &&
      current_layout_.cells.front().original_word == 0;
  if (added_first_tile_to_empty_asset) {
    current_layout_.cells.clear();
    current_layout_.bounds_width = 1;
    current_layout_.bounds_height = 1;
    SelectFirstCellIfAvailable();
  } else {
    current_layout_.RevertAll();
    SyncSourceSelectionFromSelectedCell();
  }
  preview_dirty_ = true;
  ClearActionStatus();
}

absl::StatusOr<ObjectTileEditorPanel::SourceImpactSnapshot>
ObjectTileEditorPanel::AnalyzeSourceImpactSnapshot() const {
  if (shared_tile_data_usage_override_ >= 0) {
    uint64_t fingerprint = 1469598103934665603ULL;
    MixFingerprint(&fingerprint,
                   static_cast<uint64_t>(shared_tile_data_usage_override_));
    return SourceImpactSnapshot{shared_tile_data_usage_override_, fingerprint};
  }

  if (current_layout_.is_custom) {
    if (current_layout_.custom_resolved_path.empty()) {
      return absl::FailedPreconditionError(
          "Custom object source-impact analysis requires its resolved asset "
          "path");
    }

    auto& manager = zelda3::CustomObjectManager::Get();
    uint64_t fingerprint = 1469598103934665603ULL;
    MixFingerprint(&fingerprint, manager.asset_generation());
    int consumer_count = 0;
    std::vector<int> mapped_object_ids(
        zelda3::CustomObjectManager::RuntimeObjectIds().begin(),
        zelda3::CustomObjectManager::RuntimeObjectIds().end());
    const auto manager_state = manager.SnapshotState();
    for (const auto& mapping : manager_state.custom_file_map) {
      const int object_id = mapping.first;
      if (std::find(mapped_object_ids.begin(), mapped_object_ids.end(),
                    object_id) == mapped_object_ids.end()) {
        mapped_object_ids.push_back(object_id);
      }
    }
    std::sort(mapped_object_ids.begin(), mapped_object_ids.end());

    for (const int object_id : mapped_object_ids) {
      const int subtype_count = manager.GetSubtypeCount(object_id);
      for (int subtype = 0; subtype < subtype_count; ++subtype) {
        const std::string filename =
            manager.ResolveFilename(object_id, subtype);
        if (filename.empty()) {
          continue;
        }
        auto resolved_or = zelda3::ResolveCustomObjectAssetPath(
            manager.GetBasePath(), filename);
        if (!resolved_or.ok()) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Could not resolve custom runtime slot 0x%02X:%02X: %s",
              object_id, subtype, resolved_or.status().message()));
        }
        if (!PathsReferToSameAsset(*resolved_or,
                                   current_layout_.custom_resolved_path)) {
          continue;
        }
        ++consumer_count;
        MixFingerprint(&fingerprint, static_cast<uint16_t>(object_id));
        MixFingerprint(&fingerprint, static_cast<uint16_t>(subtype));
      }
    }
    if (consumer_count == 0) {
      return absl::AbortedError(
          "Custom object mapping or project folder changed after this asset "
          "was opened; edits were kept");
    }
    return SourceImpactSnapshot{consumer_count, fingerprint};
  }
  if (current_object_id_ < 0 ||
      current_layout_.object_id != current_object_id_) {
    return absl::FailedPreconditionError(
        "No matching standard object is selected for source-impact analysis");
  }
  if (rom_ == nullptr || !rom_->is_loaded()) {
    return absl::FailedPreconditionError(
        "A loaded ROM is required for source-impact analysis");
  }

  auto impact_or =
      tile_editor_->AnalyzeStandardTileSourceImpact(current_layout_);
  if (!impact_or.ok()) {
    return impact_or.status();
  }

  uint64_t fingerprint = 1469598103934665603ULL;
  MixFingerprint(&fingerprint, impact_or->affected_objects.size());
  for (const auto& entry : impact_or->affected_objects) {
    MixFingerprint(&fingerprint, 0xA11EC7EDULL);
    MixFingerprint(&fingerprint, static_cast<uint16_t>(entry.object_id));
    MixFingerprint(&fingerprint, entry.overlapping_ranges.size());
    for (const auto& range : entry.overlapping_ranges) {
      MixFingerprint(&fingerprint, range.begin);
      MixFingerprint(&fingerprint, range.end);
    }
  }
  MixFingerprint(&fingerprint, impact_or->runtime_consumers.size());
  for (const auto& entry : impact_or->runtime_consumers) {
    MixFingerprint(&fingerprint, 0x52754E71ULL);
    MixFingerprint(&fingerprint, static_cast<uint64_t>(entry.consumer));
    MixFingerprint(&fingerprint, entry.overlapping_ranges.size());
    for (const auto& range : entry.overlapping_ranges) {
      MixFingerprint(&fingerprint, range.begin);
      MixFingerprint(&fingerprint, range.end);
    }
  }
  return SourceImpactSnapshot{static_cast<int>(impact_or->consumer_count()),
                              fingerprint};
}

absl::StatusOr<int> ObjectTileEditorPanel::GetSharedTileDataUsageCount() const {
  auto snapshot_or = AnalyzeSourceImpactSnapshot();
  if (!snapshot_or.ok()) {
    return snapshot_or.status();
  }
  return snapshot_or->consumer_count;
}

uint64_t ObjectTileEditorPanel::BuildSourceImpactProvenanceFingerprint() const {
  uint64_t fingerprint = 1469598103934665603ULL;
  MixFingerprint(&fingerprint,
                 static_cast<uint16_t>(current_layout_.object_id));
  MixFingerprint(&fingerprint,
                 static_cast<uint32_t>(current_layout_.tile_data_address));
  MixFingerprint(&fingerprint, current_layout_.is_custom ? 1 : 0);
  if (current_layout_.is_custom) {
    MixFingerprint(&fingerprint,
                   static_cast<uint32_t>(current_layout_.custom_subtype));
    MixFingerprint(&fingerprint,
                   zelda3::CustomObjectManager::Get().asset_generation());
    for (const unsigned char character :
         current_layout_.custom_resolved_path.generic_string()) {
      MixFingerprint(&fingerprint, character);
    }
  }
  MixFingerprint(&fingerprint,
                 static_cast<uint32_t>(current_layout_.bounds_width));
  MixFingerprint(&fingerprint,
                 static_cast<uint32_t>(current_layout_.bounds_height));
  MixFingerprint(&fingerprint, current_layout_.cells.size());
  for (const auto& cell : current_layout_.cells) {
    MixFingerprint(&fingerprint, static_cast<uint32_t>(cell.rel_x));
    MixFingerprint(&fingerprint, static_cast<uint32_t>(cell.rel_y));
    MixFingerprint(&fingerprint, cell.original_word);
    MixFingerprint(&fingerprint, cell.source_ref.has_value() ? 1 : 0);
    if (cell.source_ref.has_value()) {
      MixFingerprint(&fingerprint, cell.source_ref->span_index);
      MixFingerprint(&fingerprint, cell.source_ref->word_index);
    }
  }
  MixFingerprint(&fingerprint,
                 current_layout_.source_provenance.has_value() ? 1 : 0);
  if (current_layout_.source_provenance.has_value()) {
    const auto& provenance = *current_layout_.source_provenance;
    MixFingerprint(&fingerprint, static_cast<uint16_t>(provenance.object_id));
    MixFingerprint(&fingerprint, provenance.descriptor_pc_address);
    MixFingerprint(&fingerprint, provenance.expected_descriptor_word);
    MixFingerprint(&fingerprint, provenance.spans.size());
    for (const auto& span : provenance.spans) {
      MixFingerprint(&fingerprint, span.pc_address);
      MixFingerprint(&fingerprint, span.expected_words.size());
      for (const uint16_t word : span.expected_words) {
        MixFingerprint(&fingerprint, word);
      }
    }
  }
  return fingerprint;
}

absl::StatusOr<int>
ObjectTileEditorPanel::GetDisplayedSharedTileDataUsageCount() {
  const SourceImpactDisplayCacheKey key{
      current_object_id_, BuildSourceImpactProvenanceFingerprint(),
      rom_ != nullptr ? rom_->object_tile_revision() : 0};
  if (source_impact_display_cache_key_ == key &&
      source_impact_display_cache_result_.has_value()) {
    return *source_impact_display_cache_result_;
  }

  source_impact_display_cache_key_ = key;
  source_impact_display_cache_result_ = GetSharedTileDataUsageCount();
  return *source_impact_display_cache_result_;
}

absl::StatusOr<bool> ObjectTileEditorPanel::HasSharedTileDataConflict() const {
  auto usage_count_or = GetSharedTileDataUsageCount();
  if (!usage_count_or.ok()) {
    return usage_count_or.status();
  }
  return *usage_count_or > 1;
}

bool ObjectTileEditorPanel::HasRenderableRoomContext() const {
  return rooms_ != nullptr &&
         rooms_->GetIfLoaded(current_room_id_) != nullptr &&
         !current_layout_.cells.empty();
}

void ObjectTileEditorPanel::RefreshRenderedViewsFromCurrentRoom() {
  preview_dirty_ = true;
  atlas_dirty_ = true;

  if (!HasRenderableRoomContext()) {
    return;
  }

  RenderObjectPreview();
  RenderTile8Atlas();
}

void ObjectTileEditorPanel::Draw(bool* p_open) {
  if (!is_open_) {
    if (p_open != nullptr) {
      *p_open = false;
    }
    return;
  }
  if (p_open != nullptr && !*p_open) {
    return;
  }

  const auto* room =
      rooms_ != nullptr ? rooms_->GetIfLoaded(current_room_id_) : nullptr;
  const uint64_t graphics_revision =
      room != nullptr ? room->graphics_revision() : 0;
  if (graphics_revision != room_graphics_revision_) {
    room_graphics_revision_ = graphics_revision;
    preview_dirty_ = true;
    atlas_dirty_ = true;
  }

  const std::string session_title = BuildWindowTitle();
  ImGui::TextUnformatted(session_title.c_str());
  ImGui::TextDisabled(tr("Room 0x%03X"), current_room_id_);
  ImGui::Separator();

  if (current_layout_.cells.empty()) {
    if (!current_layout_.is_custom) {
      if (p_open != nullptr) {
        *p_open = false;
      }
      return;
    }
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning),
                       ICON_MD_VISIBILITY_OFF " Empty runtime asset");
    ImGui::TextWrapped(
        "%s", tr("This fixed slot intentionally draws nothing. Add its first "
                 "tile to begin a graphics override."));
    if (ImGui::Button(ICON_MD_ADD " Add First Tile")) {
      const absl::Status status = AddFirstTileToEmptyCustomLayout();
      if (!status.ok()) {
        SetActionStatus(ActionStatusTone::kError,
                        std::string(status.message()));
      }
    }
    ImGui::Separator();
    DrawActionBar(p_open);
    return;
  }

  // Two-column layout: tile grid + source sheet
  if (ImGui::BeginTable(
          "##TileEditorLayout", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Tile Grid", ImGuiTableColumnFlags_WidthFixed,
                            280.0f);
    ImGui::TableSetupColumn("Source Sheet", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawTileGrid();

    ImGui::TableNextColumn();
    DrawSourceSheet();

    ImGui::EndTable();
  }

  ImGui::Separator();
  DrawTileProperties();
  ImGui::Separator();
  DrawActionBar(p_open);

  HandleKeyboardShortcuts(p_open);

  // Shared ROM data or project source-asset confirmation modal.
  const char* shared_popup_title =
      current_layout_.is_custom
          ? "Shared Custom Asset###SharedTileDataGlobalEdit"
          : "Shared Tile Data (Global Edit)###SharedTileDataGlobalEdit";
  if (show_shared_confirm_) {
    ImGui::OpenPopup(shared_popup_title);
    show_shared_confirm_ = false;
  }
  if (ImGui::BeginPopupModal(shared_popup_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (current_layout_.is_custom) {
      ImGui::Text(tr("This source asset is used by %d runtime slots."),
                  shared_object_count_);
      ImGui::Text(tr("Publishing will update all of those slots."));
      ImGui::TextWrapped(
          tr("This project source-asset edit is not covered by Dungeon Editor "
             "Ctrl+Z."));
    } else {
      ImGui::Text(tr("This tile data is shared by %d consumers."),
                  shared_object_count_);
      ImGui::Text(tr("Changes will affect all of them."));
      ImGui::TextWrapped(
          tr("This global ROM tile edit is not covered by Dungeon Editor "
             "Ctrl+Z."));
    }
    ImGui::Spacing();
    const char* apply_label = current_layout_.is_custom
                                  ? tr("Publish Shared Asset")
                                  : tr("Apply Global Edit");
    if (ImGui::Button(apply_label, ImVec2(170, 0))) {
      ApplyChanges(/*confirm_shared=*/false);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"), ImVec2(120, 0))) {
      pending_shared_confirmation_.reset();
      shared_object_count_ = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void ObjectTileEditorPanel::RenderObjectPreview() {
  auto* room =
      rooms_ != nullptr ? rooms_->GetIfLoaded(current_room_id_) : nullptr;
  if (room == nullptr) {
    gfx::Arena::Get().RetireBitmap(object_preview_bmp_);
    object_preview_bmp_ = gfx::Bitmap();
    return;
  }

  auto status = tile_editor_->RenderLayoutToBitmap(
      current_layout_, object_preview_bmp_, room->get_gfx_buffer().data(),
      current_palette_group_);
  if (status.ok()) {
    object_preview_bmp_.UpdateTexture();
    preview_dirty_ = false;
  } else {
    gfx::Arena::Get().RetireBitmap(object_preview_bmp_);
    object_preview_bmp_ = gfx::Bitmap();
  }
}

void ObjectTileEditorPanel::RenderTile8Atlas() {
  auto* room =
      rooms_ != nullptr ? rooms_->GetIfLoaded(current_room_id_) : nullptr;
  if (room == nullptr) {
    gfx::Arena::Get().RetireBitmap(tile8_atlas_bmp_);
    tile8_atlas_bmp_ = gfx::Bitmap();
    return;
  }

  auto status = tile_editor_->BuildTile8Atlas(
      tile8_atlas_bmp_, room->get_gfx_buffer().data(), current_palette_group_,
      source_palette_,
      current_layout_.is_custom
          ? std::optional<int16_t>(current_layout_.object_id)
          : std::nullopt,
      source_attributes_);
  if (status.ok()) {
    tile8_atlas_bmp_.UpdateTexture();
    atlas_dirty_ = false;
  } else {
    gfx::Arena::Get().RetireBitmap(tile8_atlas_bmp_);
    tile8_atlas_bmp_ = gfx::Bitmap();
  }
}

void ObjectTileEditorPanel::SyncSourceSelectionFromSelectedCell() {
  if (selected_cell_index_ < 0 ||
      selected_cell_index_ >= static_cast<int>(current_layout_.cells.size())) {
    SyncSourceAttributesFromSelectedCell();
    return;
  }

  const auto& cell = current_layout_.cells[selected_cell_index_];
  selected_source_tile_ = static_cast<int>(cell.tile_info.id_);

  const int cell_palette = static_cast<int>(cell.tile_info.palette_);
  if (source_palette_ != cell_palette) {
    source_palette_ = cell_palette;
    atlas_dirty_ = true;
  }
  SyncSourceAttributesFromSelectedCell();
}

void ObjectTileEditorPanel::SyncSourceAttributesFromSelectedCell() {
  uint16_t attributes = 0;
  if (current_layout_.is_custom && selected_cell_index_ >= 0 &&
      selected_cell_index_ < static_cast<int>(current_layout_.cells.size())) {
    attributes = gfx::TileInfoToWord(
                     current_layout_.cells[selected_cell_index_].tile_info) &
                 0xE000;
  }
  if (source_attributes_ != attributes) {
    source_attributes_ = attributes;
    atlas_dirty_ = true;
  }
}

void ObjectTileEditorPanel::DrawTileGrid() {
  if (preview_dirty_) {
    RenderObjectPreview();
  }

  ImGui::Text(tr("Object Tiles (%dx%d)"), current_layout_.bounds_width,
              current_layout_.bounds_height);

  constexpr float kScale = 4.0f;
  float grid_width = current_layout_.bounds_width * 8 * kScale;
  float grid_height = current_layout_.bounds_height * 8 * kScale;

  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size(grid_width, grid_height);

  // Draw the preview bitmap as background
  if (object_preview_bmp_.is_active() && object_preview_bmp_.texture()) {
    ImGui::Image((ImTextureID)(intptr_t)object_preview_bmp_.texture(),
                 canvas_size);
  } else {
    ImGui::Dummy(canvas_size);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw 8px grid overlay
  for (int gx = 0; gx <= current_layout_.bounds_width; ++gx) {
    float line_x = canvas_pos.x + gx * 8 * kScale;
    draw_list->AddLine(ImVec2(line_x, canvas_pos.y),
                       ImVec2(line_x, canvas_pos.y + grid_height),
                       IM_COL32(128, 128, 128, 80));
  }
  for (int gy = 0; gy <= current_layout_.bounds_height; ++gy) {
    float line_y = canvas_pos.y + gy * 8 * kScale;
    draw_list->AddLine(ImVec2(canvas_pos.x, line_y),
                       ImVec2(canvas_pos.x + grid_width, line_y),
                       IM_COL32(128, 128, 128, 80));
  }

  // Highlight selected cell
  if (selected_cell_index_ >= 0 &&
      selected_cell_index_ < static_cast<int>(current_layout_.cells.size())) {
    const auto& cell = current_layout_.cells[selected_cell_index_];
    ImVec2 cell_min(canvas_pos.x + cell.rel_x * 8 * kScale,
                    canvas_pos.y + cell.rel_y * 8 * kScale);
    ImVec2 cell_max(cell_min.x + 8 * kScale, cell_min.y + 8 * kScale);
    draw_list->AddRect(cell_min, cell_max, IM_COL32(255, 255, 0, 255), 0, 0,
                       2.0f);
  }

  // Handle clicks on the grid
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
    ImVec2 mouse = ImGui::GetMousePos();
    int click_tile_x =
        static_cast<int>((mouse.x - canvas_pos.x) / (8 * kScale));
    int click_tile_y =
        static_cast<int>((mouse.y - canvas_pos.y) / (8 * kScale));

    // Find the cell at this position
    for (int idx = 0; idx < static_cast<int>(current_layout_.cells.size());
         ++idx) {
      if (current_layout_.cells[idx].rel_x == click_tile_x &&
          current_layout_.cells[idx].rel_y == click_tile_y) {
        selected_cell_index_ = idx;
        SyncSourceSelectionFromSelectedCell();
        break;
      }
    }
  }

  // Show cell count
  int modified_count = 0;
  for (const auto& cell : current_layout_.cells) {
    if (cell.modified)
      ++modified_count;
  }
  ImGui::Text(tr("%zu tiles, %d modified"), current_layout_.cells.size(),
              modified_count);
}

void ObjectTileEditorPanel::DrawSourceSheet() {
  if (atlas_dirty_) {
    RenderTile8Atlas();
  }

  ImGui::Text(tr("Source Tiles (Palette %d)"), source_palette_);

  // Palette selector
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  if (ImGui::SliderInt("##SrcPal", &source_palette_, 0, 7)) {
    atlas_dirty_ = true;
  }

  constexpr float kAtlasScale = 2.0f;
  float display_width = zelda3::ObjectTileEditor::kAtlasWidthPx * kAtlasScale;
  float display_height = zelda3::ObjectTileEditor::kAtlasHeightPx * kAtlasScale;

  ImVec2 atlas_pos = ImGui::GetCursorScreenPos();
  ImVec2 atlas_size(display_width, display_height);

  // Scrollable child for the atlas
  ImGui::BeginChild("##AtlasScroll", ImVec2(display_width + 16, 300), true,
                    ImGuiWindowFlags_HorizontalScrollbar);

  atlas_pos = ImGui::GetCursorScreenPos();

  if (tile8_atlas_bmp_.is_active() && tile8_atlas_bmp_.texture()) {
    ImGui::Image((ImTextureID)(intptr_t)tile8_atlas_bmp_.texture(), atlas_size);
  } else {
    ImGui::Dummy(atlas_size);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw 8px grid
  for (int gx = 0; gx <= zelda3::ObjectTileEditor::kAtlasTilesPerRow; ++gx) {
    float line_x = atlas_pos.x + gx * 8 * kAtlasScale;
    draw_list->AddLine(ImVec2(line_x, atlas_pos.y),
                       ImVec2(line_x, atlas_pos.y + display_height),
                       IM_COL32(64, 64, 64, 60));
  }
  for (int gy = 0; gy <= zelda3::ObjectTileEditor::kAtlasTileRows; ++gy) {
    float line_y = atlas_pos.y + gy * 8 * kAtlasScale;
    draw_list->AddLine(ImVec2(atlas_pos.x, line_y),
                       ImVec2(atlas_pos.x + display_width, line_y),
                       IM_COL32(64, 64, 64, 60));
  }

  // Highlight selected source tile
  if (selected_source_tile_ >= 0) {
    int src_col =
        selected_source_tile_ % zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    int src_row =
        selected_source_tile_ / zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    ImVec2 sel_min(atlas_pos.x + src_col * 8 * kAtlasScale,
                   atlas_pos.y + src_row * 8 * kAtlasScale);
    ImVec2 sel_max(sel_min.x + 8 * kAtlasScale, sel_min.y + 8 * kAtlasScale);
    draw_list->AddRect(sel_min, sel_max, IM_COL32(0, 255, 255, 255), 0, 0,
                       2.0f);
  }

  // Handle clicks on the atlas
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
    ImVec2 mouse = ImGui::GetMousePos();
    int click_col =
        static_cast<int>((mouse.x - atlas_pos.x) / (8 * kAtlasScale));
    int click_row =
        static_cast<int>((mouse.y - atlas_pos.y) / (8 * kAtlasScale));

    if (click_col >= 0 &&
        click_col < zelda3::ObjectTileEditor::kAtlasTilesPerRow &&
        click_row >= 0 &&
        click_row < zelda3::ObjectTileEditor::kAtlasTileRows) {
      int tile_id =
          click_row * zelda3::ObjectTileEditor::kAtlasTilesPerRow + click_col;
      selected_source_tile_ = tile_id;

      // If a cell is selected, replace its tile
      if (selected_cell_index_ >= 0 &&
          selected_cell_index_ <
              static_cast<int>(current_layout_.cells.size())) {
        auto& cell = current_layout_.cells[selected_cell_index_];
        cell.tile_info.id_ = static_cast<uint16_t>(tile_id);
        cell.tile_info.palette_ = static_cast<uint8_t>(source_palette_);
        cell.modified = true;
        preview_dirty_ = true;
        ClearActionStatus();
      }
    }
  }

  ImGui::EndChild();

  if (selected_source_tile_ >= 0) {
    ImGui::Text(tr("Tile: 0x%03X"), selected_source_tile_);
  }
}

void ObjectTileEditorPanel::DrawTileProperties() {
  if (selected_cell_index_ < 0 ||
      selected_cell_index_ >= static_cast<int>(current_layout_.cells.size())) {
    ImGui::TextDisabled(tr("Select a tile cell to edit properties"));
    return;
  }

  auto& cell = current_layout_.cells[selected_cell_index_];
  ImGui::Text(tr("Cell (%d, %d)"), cell.rel_x, cell.rel_y);
  ImGui::SameLine();

  // Tile ID
  int tile_id = cell.tile_info.id_;
  ImGui::SetNextItemWidth(80);
  if (ImGui::InputInt(tr("ID"), &tile_id, 1, 16)) {
    cell.tile_info.id_ = static_cast<uint16_t>(tile_id & 0x3FF);
    cell.modified = true;
    preview_dirty_ = true;
    ClearActionStatus();
    SyncSourceSelectionFromSelectedCell();
  }
  ImGui::SameLine();

  // Palette
  int pal = cell.tile_info.palette_;
  ImGui::SetNextItemWidth(60);
  if (ImGui::SliderInt(tr("Pal"), &pal, 0, 7)) {
    cell.tile_info.palette_ = static_cast<uint8_t>(pal);
    cell.modified = true;
    preview_dirty_ = true;
    ClearActionStatus();
    SyncSourceSelectionFromSelectedCell();
  }
  ImGui::SameLine();

  // Flip flags
  if (ImGui::Checkbox(tr("H"), &cell.tile_info.horizontal_mirror_)) {
    cell.modified = true;
    preview_dirty_ = true;
    ClearActionStatus();
    SyncSourceAttributesFromSelectedCell();
  }
  ImGui::SameLine();
  if (ImGui::Checkbox(tr("V"), &cell.tile_info.vertical_mirror_)) {
    cell.modified = true;
    preview_dirty_ = true;
    ClearActionStatus();
    SyncSourceAttributesFromSelectedCell();
  }
  ImGui::SameLine();
  if (ImGui::Checkbox(tr("Pri"), &cell.tile_info.over_)) {
    cell.modified = true;
    preview_dirty_ = true;
    ClearActionStatus();
    SyncSourceAttributesFromSelectedCell();
  }
}

absl::Status ObjectTileEditorPanel::WriteBackCurrentLayout() {
  if (current_layout_.is_custom) {
    return tile_editor_->WriteBack(current_layout_);
  }

  auto plan_or = tile_editor_->BuildStandardWritePlan(current_layout_);
  if (!plan_or.ok()) {
    return plan_or.status();
  }

  const auto& plan = *plan_or;
  if (standard_write_preflight_ && !plan.write_ranges().empty()) {
    const absl::Status preflight =
        standard_write_preflight_(plan.write_ranges());
    if (!preflight.ok()) {
      return preflight;
    }
  }

  return tile_editor_->ApplyStandardWritePlan(plan);
}

void ObjectTileEditorPanel::ApplyChanges(bool confirm_shared) {
  // Resolve the complete source impact before every write, including
  // the second call after the user accepts the shared-data confirmation. A
  // malformed or unresolved object family must block the write rather than be
  // interpreted as an unshared source.
  const auto impact_snapshot_or = AnalyzeSourceImpactSnapshot();
  if (!impact_snapshot_or.ok()) {
    show_shared_confirm_ = false;
    shared_object_count_ = 0;
    pending_shared_confirmation_.reset();
    SetActionStatus(
        ActionStatusTone::kError,
        absl::StrFormat(ICON_MD_ERROR
                        " Apply blocked: source impact could not be resolved: "
                        "%s",
                        impact_snapshot_or.status().message()));
    return;
  }
  const SourceImpactSnapshot impact_snapshot = *impact_snapshot_or;
  const int shared_count = impact_snapshot.consumer_count;
  if (confirm_shared && shared_count > 1) {
    shared_object_count_ = shared_count;
    pending_shared_confirmation_ = impact_snapshot;
    show_shared_confirm_ = true;
    if (current_layout_.is_custom) {
      SetActionStatus(
          ActionStatusTone::kWarning,
          absl::StrFormat(
              ICON_MD_WARNING
              " Confirm shared asset publish: %d runtime slots use this "
              "file. This project-source edit is not covered by Ctrl+Z.",
              shared_count));
    } else {
      SetActionStatus(
          ActionStatusTone::kWarning,
          absl::StrFormat(ICON_MD_WARNING
                          " Confirm global apply: %d consumers use this tile "
                          "data. This edit is not covered by Ctrl+Z.",
                          shared_count));
    }
    return;
  }
  const bool missing_required_confirmation =
      shared_count > 1 && !pending_shared_confirmation_.has_value();
  const bool confirmed_impact_changed =
      pending_shared_confirmation_.has_value() &&
      *pending_shared_confirmation_ != impact_snapshot;
  if (!confirm_shared &&
      (missing_required_confirmation || confirmed_impact_changed)) {
    shared_object_count_ = shared_count;
    pending_shared_confirmation_ = impact_snapshot;
    show_shared_confirm_ = true;
    if (current_layout_.is_custom) {
      SetActionStatus(
          ActionStatusTone::kWarning,
          absl::StrFormat(
              ICON_MD_WARNING
              " Source impact changed; review and confirm the updated "
              "publish for %d runtime slots. This project-source edit is not "
              "covered by Ctrl+Z.",
              shared_count));
    } else {
      SetActionStatus(
          ActionStatusTone::kWarning,
          absl::StrFormat(
              ICON_MD_WARNING
              " Source impact changed; review and confirm the updated global "
              "apply for %d consumers. This edit is not covered by Ctrl+Z.",
              shared_count));
    }
    return;
  }

  show_shared_confirm_ = false;
  shared_object_count_ = 0;
  pending_shared_confirmation_.reset();

  auto status = WriteBackCurrentLayout();
  if (status.ok()) {
    if (rooms_ != nullptr) {
      rooms_->ForEachMaterialized(
          [](int, zelda3::Room& room) { room.MarkObjectsDirty(); });
    }
    if (on_tiles_applied_) {
      on_tiles_applied_();
    }

    // Re-render the current room immediately. Other materialized rooms were
    // marked dirty above and refresh lazily when their canvases draw.
    if (HasRenderableRoomContext()) {
      auto& room = (*rooms_)[current_room_id_];
      room.MarkObjectsDirty();
      room.RenderRoomGraphics();
    }

    // Update both the cells and the authoritative source snapshot only after
    // the write succeeds. The next impact analysis reuses BuildStandardWritePlan
    // as its CAS gate, so leaving expected_words stale here would make every
    // later apply fail until the panel was reopened.
    for (auto& cell : current_layout_.cells) {
      if (cell.modified) {
        const uint16_t applied_word = gfx::TileInfoToWord(cell.tile_info);
        cell.original_word = applied_word;
        if (!current_layout_.is_custom &&
            current_layout_.source_provenance.has_value() &&
            cell.source_ref.has_value()) {
          const auto& source_ref = *cell.source_ref;
          auto& spans = current_layout_.source_provenance->spans;
          if (source_ref.span_index < spans.size() &&
              source_ref.word_index <
                  spans[source_ref.span_index].expected_words.size()) {
            spans[source_ref.span_index].expected_words[source_ref.word_index] =
                applied_word;
          }
        }
        cell.modified = false;
      }
    }

    RefreshRenderedViewsFromCurrentRoom();
    if (shared_count > 1) {
      if (current_layout_.is_custom) {
        SetActionStatus(
            ActionStatusTone::kSuccess,
            absl::StrFormat(
                ICON_MD_CHECK_CIRCLE
                " Published shared custom asset used by %d runtime slots.",
                shared_count));
      } else {
        SetActionStatus(
            ActionStatusTone::kSuccess,
            absl::StrFormat(
                ICON_MD_CHECK_CIRCLE
                " Applied changes to shared tile data used by %d consumers.",
                shared_count));
      }
    } else {
      ClearActionStatus();
    }
    return;
  }

  SetActionStatus(
      ActionStatusTone::kError,
      absl::StrFormat(ICON_MD_ERROR " Apply failed: %s", status.message()));
}

void ObjectTileEditorPanel::DrawActionBar(bool* p_open) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  int modified_count = 0;
  for (const auto& cell : current_layout_.cells) {
    if (cell.modified)
      ++modified_count;
  }

  bool has_mods = modified_count > 0;

  if (has_mods) {
    ImGui::Text(tr("%d tile(s) modified"), modified_count);
    ImGui::SameLine();
  }

  // Shared tile data warning. Keep analysis errors visible here, but enforce
  // fail-closed behavior in ApplyChanges so the disabled-by-default standard
  // editor cannot silently turn an unknown impact into a write.
  const auto shared_count_or = GetDisplayedSharedTileDataUsageCount();
  if (!shared_count_or.ok()) {
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.error),
                       ICON_MD_ERROR " Source impact unavailable");
    if (ImGui::IsItemHovered()) {
      ImGui::SetItemTooltip("%s", shared_count_or.status().message().data());
    }
    ImGui::SameLine();
  } else if (*shared_count_or > 1) {
    const int shared_count = *shared_count_or;
    if (current_layout_.is_custom) {
      ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning),
                         ICON_MD_WARNING " Shared by %d runtime slots",
                         shared_count);
    } else {
      ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning),
                         ICON_MD_WARNING " Shared by %d consumers",
                         shared_count);
    }
    if (ImGui::IsItemHovered()) {
      if (current_layout_.is_custom) {
        ImGui::SetItemTooltip(
            tr("%d fixed runtime slots resolve to this source file.\n"
               "Publishing changes updates all of them.\nThis project "
               "source-asset edit is not covered by Ctrl+Z."),
            shared_count);
      } else {
        ImGui::SetItemTooltip(
            tr("This object reuses tile data with %d consumers.\nApplying "
               "changes will update every object or runtime consumer in that "
               "shared data group.\nThis global ROM edit is not covered by "
               "Ctrl+Z."),
            shared_count);
      }
    }
    ImGui::SameLine();
  }

  // Apply button
  if (!has_mods)
    ImGui::BeginDisabled();
  const char* primary_action_label = current_layout_.is_custom
                                         ? ICON_MD_SAVE " Publish Asset"
                                         : ICON_MD_SAVE " Apply";
  if (ImGui::Button(primary_action_label)) {
    ApplyChanges();
  }
  if (!has_mods)
    ImGui::EndDisabled();

  ImGui::SameLine();

  // Revert button
  if (!has_mods)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_MD_UNDO " Revert")) {
    RevertCurrentLayout();
  }
  if (!has_mods)
    ImGui::EndDisabled();

  ImGui::SameLine();

  const char* close_label =
      has_mods ? ICON_MD_CLOSE " Discard & Close" : ICON_MD_CLOSE " Close";
  if (ImGui::Button(close_label)) {
    if (p_open != nullptr) {
      *p_open = false;
    }
    Close();
    return;
  }
  if (has_mods && ImGui::IsItemHovered()) {
    ImGui::SetItemTooltip(
        "%s", tr("Close the panel and discard all unapplied tile changes."));
  }

  ImGui::Spacing();
  ImGui::TextDisabled("%s",
                      current_layout_.is_custom ? ICON_MD_INFO
                          " Project source-asset edit; not covered by Ctrl+Z."
                                                : ICON_MD_INFO
                          " Global ROM tile edit; not covered by Ctrl+Z.");

  if (action_status_tone_ != ActionStatusTone::kNone &&
      !action_status_message_.empty()) {
    ImVec4 status_color = gui::ConvertColorToImVec4(theme.text_secondary);
    switch (action_status_tone_) {
      case ActionStatusTone::kWarning:
        status_color = gui::ConvertColorToImVec4(theme.warning);
        break;
      case ActionStatusTone::kSuccess:
        status_color = gui::ConvertColorToImVec4(theme.success);
        break;
      case ActionStatusTone::kError:
        status_color = gui::ConvertColorToImVec4(theme.error);
        break;
      case ActionStatusTone::kNone:
        break;
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, status_color);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(action_status_message_.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
}

void ObjectTileEditorPanel::RequestSafeWindowClose(bool* p_open) {
  if (current_layout_.HasModifications()) {
    if (p_open != nullptr) {
      *p_open = false;
    }
    SetActionStatus(
        ActionStatusTone::kWarning, ICON_MD_WARNING
        " Unapplied tile changes were kept. Reopen the panel to continue, or "
        "use Discard & Close to throw them away.");
    return;
  }

  if (p_open != nullptr) {
    *p_open = false;
  }
  Close();
}

void ObjectTileEditorPanel::HandleKeyboardShortcuts(bool* p_open) {
  // Only handle shortcuts when this window is focused
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    return;
  if (current_layout_.cells.empty())
    return;

  int cell_count = static_cast<int>(current_layout_.cells.size());

  // Arrow keys: navigate selected cell by spatial position
  auto find_neighbor = [&](int dx, int dy) -> int {
    if (selected_cell_index_ < 0)
      return 0;
    const auto& cur = current_layout_.cells[selected_cell_index_];
    int target_x = cur.rel_x + dx;
    int target_y = cur.rel_y + dy;
    for (int i = 0; i < cell_count; ++i) {
      if (current_layout_.cells[i].rel_x == target_x &&
          current_layout_.cells[i].rel_y == target_y) {
        return i;
      }
    }
    return selected_cell_index_;
  };

  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
    selected_cell_index_ = find_neighbor(-1, 0);
    SyncSourceSelectionFromSelectedCell();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
    selected_cell_index_ = find_neighbor(1, 0);
    SyncSourceSelectionFromSelectedCell();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    selected_cell_index_ = find_neighbor(0, -1);
    SyncSourceSelectionFromSelectedCell();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    selected_cell_index_ = find_neighbor(0, 1);
    SyncSourceSelectionFromSelectedCell();
  }

  // Number keys 0-7: set palette on selected cell
  if (selected_cell_index_ >= 0 && selected_cell_index_ < cell_count) {
    auto& cell = current_layout_.cells[selected_cell_index_];
    for (int key = 0; key <= 7; ++key) {
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + key), false)) {
        cell.tile_info.palette_ = static_cast<uint8_t>(key);
        cell.modified = true;
        preview_dirty_ = true;
        ClearActionStatus();
        SyncSourceSelectionFromSelectedCell();
      }
    }

    // H: toggle horizontal flip
    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
      cell.tile_info.horizontal_mirror_ = !cell.tile_info.horizontal_mirror_;
      cell.modified = true;
      preview_dirty_ = true;
      ClearActionStatus();
      SyncSourceAttributesFromSelectedCell();
    }

    // V: toggle vertical flip
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
      cell.tile_info.vertical_mirror_ = !cell.tile_info.vertical_mirror_;
      cell.modified = true;
      preview_dirty_ = true;
      ClearActionStatus();
      SyncSourceAttributesFromSelectedCell();
    }

    // P: toggle priority
    if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
      cell.tile_info.over_ = !cell.tile_info.over_;
      cell.modified = true;
      preview_dirty_ = true;
      ClearActionStatus();
      SyncSourceAttributesFromSelectedCell();
    }
  }

  // Escape: deselect or close
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    if (selected_cell_index_ >= 0) {
      selected_cell_index_ = -1;
      SyncSourceAttributesFromSelectedCell();
    } else {
      RequestSafeWindowClose(p_open);
    }
    return;
  }

  // Tab: cycle to next cell
  if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    if (cell_count > 0) {
      selected_cell_index_ = (selected_cell_index_ + 1) % cell_count;
      SyncSourceSelectionFromSelectedCell();
    }
  }
}

}  // namespace editor
}  // namespace yaze
