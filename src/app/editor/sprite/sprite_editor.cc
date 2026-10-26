#include "sprite_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/sprite_editor_internal.h"
#include "app/editor/sprite/sprite_graphics_context.h"
#include "app/editor/sprite/sprite_graphics_source.h"
#include "app/editor/sprite/sprite_undo_actions.h"
#include "app/editor/sprite/zsprite.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/project.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndTable;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

namespace {
template <size_t N>
void CopyStringToBuffer(const std::string& src, char (&dest)[N]) {
  std::strncpy(dest, src.c_str(), N - 1);
  dest[N - 1] = '\0';
}

int ParseIntOrDefault(const std::string& text, int fallback = 0) {
  if (text.empty()) {
    return fallback;
  }
  errno = 0;
  char* end = nullptr;
  long value = std::strtol(text.c_str(), &end, 0);
  if (end == text.c_str() || errno == ERANGE) {
    return fallback;
  }
  return static_cast<int>(value);
}
}  // namespace

void SpriteEditor::Initialize() {
  if (!dependencies_.window_manager)
    return;
  auto* window_manager = dependencies_.window_manager;

  // Register WindowContent implementations with callbacks
  // EditorPanels provide both metadata (icon, name, priority) and drawing logic
  window_manager->RegisterWindowContent(
      std::make_unique<VanillaSpriteEditorPanel>([this]() {
        if (rom_ && rom_->is_loaded()) {
          DrawVanillaSpriteEditor();
        } else {
          ImGui::TextDisabled(tr("Load a ROM to view vanilla sprites"));
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<CustomSpriteEditorPanel>(
          [this]() { DrawCustomSprites(); }));
  window_manager->RegisterWindowContent(
      std::make_unique<SpriteCatalogPanel>([this]() { DrawSpriteCatalog(); }));
}

absl::Status SpriteEditor::Load() {
  gfx::ScopedTimer timer("SpriteEditor::Load");
  sheets_loaded_ = rom_ && rom_->is_loaded() && game_data();
  gfx_buffer_loaded_ = false;
  vanilla_preview_needs_update_ = true;
  preview_needs_update_ = true;
  room_palette_cache_.reset();
  if (sheets_loaded_) {
    const auto slots = zelda3::SpriteSheetSlots(
        game_data()->spriteset_ids[preview_spriteset_],
        zelda3::IsUnderworldSpriteset(preview_spriteset_));
    std::copy(slots.begin(), slots.end(), vanilla_sheets_);
    if (!current_custom_sprite())
      std::copy(slots.begin(), slots.end(), current_sheets_);
  }
  SelectVanillaSprite(current_sprite_id_);
  (void)ReloadSpriteCatalog();
  asset_load_status_ = ReloadProjectSpriteAssets();
  return absl::OkStatus();
}

absl::Status SpriteEditor::Update() {
  if (rom() && rom()->is_loaded() && !sheets_loaded_) {
    sheets_loaded_ = true;
  }

  // Update animation playback for custom sprites
  float current_time = ImGui::GetTime();
  float delta_time = current_time - last_frame_time_;
  last_frame_time_ = current_time;
  UpdateAnimationPlayback(delta_time);

  // Handle editor-level shortcuts
  HandleEditorShortcuts();

  // Panel drawing is handled by WorkspaceWindowManager via registered EditorPanels
  // Each panel's Draw() callback invokes the appropriate draw method

  // Commit any pending undo transaction at the end of the frame
  CommitUndoTransaction();

  return status_.ok() ? absl::OkStatus() : status_;
}

void SpriteEditor::HandleEditorShortcuts() {
  // Animation playback shortcuts (when custom sprite panel is active)
  if (ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
      !ImGui::GetIO().WantTextInput) {
    animation_playing_ = !animation_playing_;
  }

  // Frame navigation
  if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) {
    if (current_frame_ > 0) {
      current_frame_--;
      preview_needs_update_ = true;
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
    current_frame_++;
    preview_needs_update_ = true;
  }

  // Sprite navigation
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    if (current_sprite_id_ > 0) {
      SelectVanillaSprite(current_sprite_id_ - 1);
    }
  }
  if (ImGui::GetIO().KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    SelectVanillaSprite(current_sprite_id_ + 1);
  }
}

absl::Status SpriteEditor::Save() {
  if (!dirty_sprite_indices_.empty()) {
    CommitUndoTransaction();
    const int selected = current_custom_sprite_index_;
    const int frame = current_frame_;
    absl::Cleanup restore = [this, selected, frame] {
      current_custom_sprite_index_ = selected;
      current_frame_ = frame;
      ApplyCurrentSpriteBinding();
    };
    const auto pending = dirty_sprite_indices_;
    for (const int index : pending) {
      current_custom_sprite_index_ = index;
      current_frame_ = 0;
      const auto path = GetCurrentZsmPath();
      if (path.empty())
        SaveZsmFileAs();
      else
        SaveZsmFile(path);
      if (!status_.ok())
        return status_;
      if (dirty_sprite_indices_.count(index))
        return absl::CancelledError("Sprite Save As was cancelled");
    }
    return absl::OkStatus();
  }
  return absl::OkStatus();
}

void SpriteEditor::DrawToolset() {
  // Sidebar handled by EditorManager for card-based editors
}

// ============================================================
// Vanilla Sprite Editor
// ============================================================

void SpriteEditor::SelectVanillaSprite(int sprite_id) {
  if (sprite_id < 0 || sprite_id >= 256)
    return;
  current_sprite_id_ = sprite_id;
  vanilla_preview_needs_update_ = true;
  pending_sprite_tab_ = sprite_id;
  if (!active_sprites_.contains(sprite_id))
    active_sprites_.push_back(sprite_id);
}

const zelda3::SpriteOamLayout* SpriteEditor::CurrentPreviewLayout() const {
  const auto id = static_cast<uint8_t>(current_sprite_id_);
  const auto* override = zelda3::SpriteOamRegistry::GetPreviewOverride(
      id, project() ? project()->hack_manifest.hack_name() : "");
  return override ? override : zelda3::SpriteOamRegistry::GetLayout(id);
}

void SpriteEditor::DrawVanillaSpriteEditor() {
  gui::AutoWidgetScope sprite_scope("Sprite/Vanilla");
  if (ImGui::BeginTable("##SpriteCanvasTable", 3, ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    TableSetupColumn("Sprites List", ImGuiTableColumnFlags_WidthFixed, 256);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    DrawSpritesList();

    TableNextColumn();
    if (active_sprites_.empty()) {
      ImGui::TextWrapped("Select a sprite from the list to open its preview.");
    }
    if (gui::BeginThemedTabBar("SpriteTabBar", kSpriteTabBarFlags)) {
      if (ImGui::TabItemButton(ICON_MD_ADD, kSpriteTabFlags)) {
        SelectVanillaSprite((current_sprite_id_ + 1) % 256);
      }

      for (int n = 0; n < active_sprites_.Size;) {
        bool open = true;

        const int sprite_id = active_sprites_[n];
        if (sprite_id < 0 || sprite_id >= 256) {
          active_sprites_.erase(active_sprites_.Data + n);
          continue;
        }

        const auto label =
            absl::StrFormat("%02X %s###SpriteTab%02X", sprite_id,
                            zelda3::kSpriteDefaultNames[sprite_id], sprite_id);
        const auto flags = pending_sprite_tab_ == sprite_id
                               ? ImGuiTabItemFlags_SetSelected
                               : ImGuiTabItemFlags_None;
        const bool selected = ImGui::BeginTabItem(label.c_str(), &open, flags);
        gui::AutoRegisterLastItem("tab",
                                  absl::StrFormat("SpriteTab%02X", sprite_id));
        if (selected) {
          if (current_sprite_id_ != sprite_id) {
            current_sprite_id_ = sprite_id;
            vanilla_preview_needs_update_ = true;
          }
          DrawSpriteCanvas();
          ImGui::EndTabItem();
        }

        if (!open)
          active_sprites_.erase(active_sprites_.Data + n);
        else
          n++;
      }

      gui::EndThemedTabBar();
      pending_sprite_tab_ = -1;
    }

    TableNextColumn();
    if (sheets_loaded_) {
      DrawCurrentSheets(false);
    }
    ImGui::EndTable();
  }
}

void SpriteEditor::DrawSpriteCanvas() {
  if (ImGui::BeginChild(gui::GetID("##SpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();

    // Static layouts are preview references, not a complete game renderer.
    const auto* layout = CurrentPreviewLayout();
    ImGui::SetCursorPos(ImVec2(10, 10));
    ImGui::TextColored(gui::GetWarningColor(), "Experimental static preview");
    if (!layout) {
      ImGui::TextWrapped(
          "No static OAM layout for sprite $%02X. Open or import a ZSM in "
          "Custom Sprites to preview authored frames.",
          current_sprite_id_);
    } else if (layout->graphics_resource) {
      ImGui::TextWrapped(
          "%s ($%02X) requires external graphics (%s). This editor does not "
          "yet preview that graphics source.",
          layout->name, layout->sprite_id, layout->graphics_resource);
    } else {
      RenderVanillaSprite(*layout);
      if (!graphics_binding_status_.ok() || !palette_binding_status_.ok()) {
        const auto& error = !graphics_binding_status_.ok()
                                ? graphics_binding_status_
                                : palette_binding_status_;
        ImGui::TextWrapped("Preview unavailable: %s",
                           std::string(error.message()).c_str());
      } else {
        if (vanilla_preview_bitmap_.is_active())
          sprite_canvas_.DrawBitmap(vanilla_preview_bitmap_, 64, 64, 2.0f);
        Text(tr("Sprite: %s (0x%02X)"), layout->name, layout->sprite_id);
        Text(tr("Tiles: %zu"), layout->tiles.size());
        if (std::none_of(vanilla_preview_bitmap_.vector().begin(),
                         vanilla_preview_bitmap_.vector().end(),
                         [](uint8_t pixel) { return (pixel & 0x0F) != 0; })) {
          ImGui::TextWrapped(
              "The selected sheets contain no visible pixels for this layout. "
              "Choose the sprite's spriteset or graphics sheets.");
        }
      }
      ImGui::TextWrapped(
          "Uses the selected sheets and palette. Animation, behavior, and "
          "runtime graphics changes are not simulated.");
    }

    if (ImGui::Button(layout ? tr("Edit preview copy")
                             : tr("Create empty sprite template")))
      status_ = CreateSpriteAssetFromVanilla(current_sprite_id_);
    ImGui::TextWrapped(
        layout
            ? "Creates a detached static pose. Bind any external graphics "
              "in Custom Sprites > Graphics sources."
            : "This ID has no decoded static layout. The template starts "
              "with an empty frame for authoring; game OAM is not imported.");

    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
}

void SpriteEditor::ApplySpritesetToSheets(int spriteset,
                                          bool use_asset_binding) {
  if (game_data() == nullptr || spriteset < 0 ||
      spriteset >= static_cast<int>(zelda3::kNumSpritesets)) {
    return;
  }
  const auto slots =
      zelda3::SpriteSheetSlots(game_data()->spriteset_ids[spriteset],
                               zelda3::IsUnderworldSpriteset(spriteset));
  if (use_asset_binding && current_custom_sprite())
    BeginUndoTransaction();
  std::copy(slots.begin(), slots.end(),
            use_asset_binding ? current_sheets_ : vanilla_sheets_);
  gfx_buffer_loaded_ = false;
  preview_needs_update_ = true;
  if (use_asset_binding && current_custom_sprite()) {
    auto& binding = custom_sprite_bindings_[current_custom_sprite_index_];
    std::copy(slots.begin(), slots.end(), binding.sheets.begin());
    MarkSpriteMutated();
  }
  CommitUndoTransaction();
}

absl::Status SpriteEditor::OpenSelectedOamGraphics() {
  const auto* sprite = current_custom_sprite();
  if (!sprite || current_frame_ < 0 ||
      current_frame_ >= static_cast<int>(sprite->editor.Frames.size())) {
    return absl::FailedPreconditionError("Select a sprite frame");
  }
  if (!rom_ || !rom_->is_loaded() || !game_data()) {
    return absl::FailedPreconditionError(
        "Load a ROM before editing sprite pixels");
  }
  if (const auto* binding = current_sprite_binding();
      binding &&
      std::any_of(binding->graphics_sources.begin(),
                  binding->graphics_sources.end(),
                  [](const auto& source) { return source.kind != "sheet"; }))
    return absl::FailedPreconditionError(
        "This asset uses BIN or raw ROM graphics. Edit BIN pixels in Graphics "
        "Scratch, export the BIN, then Reload graphics sources here. Raw ROM "
        "bindings are read-only preview ranges.");
  if (!gfx_buffer_loaded_)
    LoadSpriteGraphicsBuffer();
  LoadSpritePalettes();
  if (!graphics_binding_status_.ok())
    return graphics_binding_status_;
  if (!palette_binding_status_.ok())
    return palette_binding_status_;
  std::array<uint8_t, 8> slots;
  std::copy_n(current_sheets_, 8, slots.begin());
  const auto frame = sprite->editor.Frames[current_frame_];
  const auto palettes = sprite_palettes_;
  const std::string source =
      preview_use_room_palette_
          ? absl::StrFormat("room %03X CGRAM rows 8-15; sprite slots %02X",
                            preview_room_, preview_spriteset_)
          : (current_sprite_binding()
                 ? absl::StrFormat(
                       "sprite asset binding; preview spriteset %02X",
                       preview_spriteset_)
                 : absl::StrFormat("preview spriteset %02X",
                                   preview_spriteset_));
  auto context = sprite_graphics_context::Make(
      frame, current_frame_, selected_tile_index_, slots, palettes, game_data(),
      sprite->sprName, source);
  if (!context.ok())
    return context.status();
  const auto* owner = game_data();
  context->render_preview = [frame, slots, palettes, owner]() {
    return sprite_graphics_context::RenderCapturedFrame(frame, slots, palettes,
                                                        owner);
  };
  auto* manager = static_cast<EditorManager*>(dependencies_.custom_data);
  if (!manager)
    return absl::FailedPreconditionError("Editor manager unavailable");
  const int sprite_index = current_custom_sprite_index_;
  const int frame_index = current_frame_;
  const int tile_index = selected_tile_index_;
  context->return_to_source = [this, manager, sprite_index, frame_index,
                               tile_index]() {
    if (sprite_index >= 0 &&
        sprite_index < static_cast<int>(custom_sprites_.size())) {
      current_custom_sprite_index_ = sprite_index;
      if (frame_index <
          static_cast<int>(
              custom_sprites_[sprite_index].editor.Frames.size())) {
        current_frame_ = frame_index;
        selected_tile_index_ = tile_index;
      }
      preview_needs_update_ = true;
      gfx_buffer_loaded_ = false;
    }
    manager->SwitchToEditor(EditorType::kSprite, true);
  };
  auto* editor_set = manager->GetCurrentEditorSet();
  if (!editor_set || !editor_set->GetGraphicsEditor()) {
    return absl::FailedPreconditionError("Graphics editor unavailable");
  }
  auto status = manager->EnsureEditorAssetsLoaded(EditorType::kGraphics);
  if (!status.ok())
    return status;
  status = editor_set->GetGraphicsEditor()->OpenInContext(std::move(*context));
  if (!status.ok())
    return status;
  manager->SwitchToEditor(EditorType::kGraphics, true);
  return absl::OkStatus();
}

void SpriteEditor::DrawSpritesetPicker(bool use_asset_binding) {
  if (!ImGui::CollapsingHeader(tr("Spriteset preview"),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (game_data() == nullptr || rom_ == nullptr || !rom_->is_loaded()) {
    ImGui::TextDisabled(tr("Load a ROM to preview against spritesets."));
    return;
  }
  if (usage_rom_ != rom_ || !usage_areas_.has_value()) {
    usage_areas_ = zelda3::CollectOverworldAreaGfx(*rom_, game_data());
    usage_rooms_ = zelda3::CollectRoomGfx(*rom_);
    usage_rom_ = rom_;
  }

  uint8_t set = static_cast<uint8_t>(preview_spriteset_);
  if (gui::InputHexByte("Spriteset", &set,
                        static_cast<uint8_t>(zelda3::kNumSpritesets - 1))) {
    preview_spriteset_ = std::min<int>(set, zelda3::kNumSpritesets - 1);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DONE " Apply to sheets")) {
    ApplySpritesetToSheets(preview_spriteset_, use_asset_binding);
  }
  HOVER_HINT(
      "Slots 0-3 take the static sprite sheets, slots 4-7 this spriteset's "
      "values + 0x73 (OAM tiles 0x100-0x1FF).");

  const auto& values = game_data()->spriteset_ids[preview_spriteset_];
  const auto slots = zelda3::SpriteSheetSlots(
      values, zelda3::IsUnderworldSpriteset(preview_spriteset_));
  ImGui::TextDisabled(
      "Values %02X %02X %02X %02X -> sheets %02X %02X %02X %02X", values[0],
      values[1], values[2], values[3], slots[4], slots[5], slots[6], slots[7]);

  const auto usage = zelda3::FindSpritesetUsage(preview_spriteset_,
                                                *usage_areas_, *usage_rooms_);
  auto hex_list = [](const std::vector<int>& ids) {
    if (ids.empty()) {
      return std::string("-");
    }
    std::string out;
    for (size_t i = 0; i < ids.size() && i < 24; ++i) {
      out += absl::StrFormat("%s%02X", i == 0 ? "" : " ", ids[i]);
    }
    if (ids.size() > 24) {
      out += absl::StrFormat(" (+%zu)", ids.size() - 24);
    }
    return out;
  };
  for (int state = 0; state < 3; ++state) {
    ImGui::TextWrapped("OW areas, state %d: %s", state,
                       hex_list(usage.ow_areas_by_state[state]).c_str());
  }
  ImGui::TextWrapped("Rooms: %s", hex_list(usage.rooms).c_str());

  // Room context: its header picks the spriteset and the CGRAM palette.
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetHexInputWidth() * 1.5f);
  ImGui::InputScalar("Room", ImGuiDataType_U16, &preview_room_, nullptr,
                     nullptr, "%03X", ImGuiInputTextFlags_CharsHexadecimal);
  preview_room_ =
      std::min<uint16_t>(preview_room_, zelda3::kNumDungeonRooms - 1);
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_MEETING_ROOM " Use room")) {
    if (auto room = usage_rooms_->find(preview_room_);
        room != usage_rooms_->end()) {
      preview_spriteset_ =
          std::min<int>(room->second.spriteset + zelda3::kDungeonSpritesetBase,
                        zelda3::kNumSpritesets - 1);
      ApplySpritesetToSheets(preview_spriteset_, use_asset_binding);
      preview_use_room_palette_ = true;
      room_palette_cache_.reset();
      gfx_buffer_loaded_ = false;
    }
  }
  HOVER_HINT("Use the room's header spriteset (+0x40) and its sprite palette");
  if (ImGui::Checkbox(tr("Room palette (CGRAM rows 8-15)"),
                      &preview_use_room_palette_)) {
    room_palette_cache_.reset();
    gfx_buffer_loaded_ = false;
    preview_needs_update_ = true;
  }
  HOVER_HINT(
      "Off: palette rows from the sprite binding (Sprite Asset panel). On: "
      "the rows the game loads for the room above.");

  if (use_asset_binding)
    DrawFrameTileWarnings();
  ImGui::Separator();
}

void SpriteEditor::DrawFrameTileWarnings() {
  const auto* sprite = current_custom_sprite();
  if (sprite == nullptr || current_frame_ < 0 ||
      current_frame_ >= static_cast<int>(sprite->editor.Frames.size())) {
    return;
  }
  auto tiles = internal::FrameTiles8x8(sprite->editor.Frames[current_frame_]);
  if (const auto* binding = current_sprite_binding()) {
    const auto before = tiles.size();
    tiles.erase(
        std::remove_if(tiles.begin(), tiles.end(),
                       [&](int tile) {
                         const int slot = zelda3::SpriteSlotForTile(tile);
                         return slot >= 0 && slot < 8 &&
                                binding->graphics_sources[slot].kind != "sheet";
                       }),
        tiles.end());
    if (tiles.size() != before)
      ImGui::TextWrapped(
          "%zu tiles use BIN/raw ROM sources; ROM sheet usage checks apply "
          "only to the remaining tiles.",
          before - tiles.size());
  }
  if (tiles.empty())
    return;
  std::array<uint8_t, 8> slots;
  std::copy_n(current_sheets_, 8, slots.begin());
  const auto options = core::BuildGfxSheetInventoryOptions(project());
  std::string key =
      absl::StrFormat("%d:%d:", current_custom_sprite_index_, current_frame_);
  for (uint8_t sheet : slots) {
    key += absl::StrFormat("%02X", sheet);
  }
  for (int tile : tiles) {
    key += absl::StrFormat(",%X", tile);
  }
  for (uint16_t sheet : options.reserved_sheets) {
    key += absl::StrFormat("r%X", sheet);
  }
  if (key != tile_check_key_) {
    tile_check_key_ = key;
    tile_issues_ =
        zelda3::CheckSpriteTiles(*rom_, tiles, slots, options.reserved_sheets);
    tile_check_count_ = tiles.size();
  }

  if (tile_issues_.empty()) {
    ImGui::TextColored(gui::GetSuccessColor(),
                       ICON_MD_CHECK " Frame %d: all %zu 8x8 tiles have art",
                       current_frame_, tile_check_count_);
    return;
  }
  ImGui::TextColored(gui::GetWarningColor(),
                     ICON_MD_WARNING
                     " Frame %d: %zu of %zu 8x8 tiles need "
                     "attention",
                     current_frame_, tile_issues_.size(), tile_check_count_);
  for (size_t i = 0; i < tile_issues_.size() && i < 8; ++i) {
    const auto& issue = tile_issues_[i];
    const char* what =
        issue.kind == zelda3::SpriteTileIssue::Kind::kBlank
            ? "blank"
            : (issue.kind == zelda3::SpriteTileIssue::Kind::kReservedSheet
                   ? "reserved sheet"
                   : "sheet unreadable");
    ImGui::BulletText("tile 0x%03X, slot %d (sheet 0x%02X): %s", issue.tile,
                      issue.slot, issue.sheet, what);
  }
  if (tile_issues_.size() > 8) {
    ImGui::TextDisabled("... %zu more", tile_issues_.size() - 8);
  }
}

void SpriteEditor::DrawCurrentSheets(bool use_asset_binding) {
  if (ImGui::BeginChild(gui::GetID("sheet_label"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    DrawSpritesetPicker(use_asset_binding);
    auto* sheets = use_asset_binding ? current_sheets_ : vanilla_sheets_;
    // Track previous sheet values for change detection
    bool sheets_changed = false;
    if (use_asset_binding && current_custom_sprite())
      BeginUndoTransaction();

    for (int i = 0; i < 8; i++) {
      std::string sheet_label = absl::StrFormat("Sheet %d", i);
      if (gui::InputHexByte(sheet_label.c_str(), &sheets[i])) {
        sheets_changed = true;
      }
      if (i % 2 == 0)
        ImGui::SameLine();
    }

    // Reload graphics buffer if sheets changed
    if (sheets_changed) {
      gfx_buffer_loaded_ = false;
      preview_needs_update_ = true;
      if (use_asset_binding && current_custom_sprite()) {
        auto& binding = custom_sprite_bindings_[current_custom_sprite_index_];
        std::copy_n(current_sheets_, 8, binding.sheets.begin());
        MarkSpriteMutated();
      }
    }
    CommitUndoTransaction();

    graphics_sheet_canvas_.GetConfig().role = gui::CanvasRole::kSelectionSource;
    graphics_sheet_canvas_.DrawBackground();
    graphics_sheet_canvas_.DrawContextMenu();
    graphics_sheet_canvas_.DrawTileSelector(32);
    for (int i = 0; i < 8; i++) {
      const auto* binding =
          use_asset_binding ? current_sprite_binding() : nullptr;
      if (binding && binding->graphics_sources[i].kind != "sheet") {
        RefreshSpritePreviewGraphics(true);
        auto& sheet = external_sheet_previews_[i];
        const std::vector<uint8_t> pixels(
            sprite_gfx_buffer_.begin() + i * 4096,
            sprite_gfx_buffer_.begin() + (i + 1) * 4096);
        const bool create = !sheet.is_active();
        bool palette_changed = false;
        if (!sprite_palettes_.empty()) {
          const auto& palette = sprite_palettes_.palette_ref(0);
          palette_changed = sheet.palette().size() != palette.size();
          if (!palette_changed)
            for (size_t color = 0; color < palette.size(); ++color)
              palette_changed |=
                  sheet.palette()[color].snes() != palette[color].snes();
        }
        if (create || sheet.vector() != pixels || palette_changed) {
          if (create)
            sheet.Create(128, 32, 8, pixels);
          else
            sheet.set_data(pixels);
          if (!sprite_palettes_.empty())
            sheet.SetPalette(sprite_palettes_.palette_ref(0));
          gfx::Arena::Get().QueueTextureCommand(
              create ? gfx::Arena::TextureCommandType::CREATE
                     : gfx::Arena::TextureCommandType::UPDATE,
              &sheet);
        }
        graphics_sheet_canvas_.DrawBitmap(sheet, 1, (i * 0x40) + 1, 2);
        continue;
      }
      if (sheets[i] >= gfx::Arena::Get().gfx_sheets().size())
        continue;
      auto& sheet = gfx::Arena::Get().gfx_sheets().at(sheets[i]);
      if (sheet.is_active() && !sheet.texture())
        sheet.CreateTexture();
      graphics_sheet_canvas_.DrawBitmap(sheet, 1, (i * 0x40) + 1, 2);
    }
    graphics_sheet_canvas_.DrawGrid();
    graphics_sheet_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
}

void SpriteEditor::DrawSpritesList() {
  if (ImGui::BeginChild(gui::GetID("##SpritesList"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    int i = 0;
    for (const auto each_sprite_name : zelda3::kSpriteDefaultNames) {
      rom()->resource_label()->SelectableLabelWithNameEdit(
          current_sprite_id_ == i, "Sprite Names", util::HexByte(i),
          zelda3::kSpriteDefaultNames[i].data());
      if (ImGui::IsItemClicked()) {
        SelectVanillaSprite(i);
      }
      i++;
    }
  }
  ImGui::EndChild();
}

// ============================================================
// Custom ZSM Sprite Editor
// ============================================================

void SpriteEditor::DrawCustomSprites() {
  if (BeginTable("##CustomSpritesTable", 3,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders,
                 ImVec2(0, 0))) {
    TableSetupColumn("Sprite Data", ImGuiTableColumnFlags_WidthFixed, 300);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Tilesheets", ImGuiTableColumnFlags_WidthFixed, 280);

    TableHeadersRow();
    TableNextRow();
    TableNextColumn();

    // Keep the canvas visible while scrolling the longer authoring controls.
    if (ImGui::BeginChild("##SpriteMetadata", ImVec2(0, 0)))
      DrawCustomSpritesMetadata();
    ImGui::EndChild();

    TableNextColumn();
    DrawZSpriteOnCanvas();

    TableNextColumn();
    DrawCurrentSheets(true);

    EndTable();
  }
}

void SpriteEditor::DrawCustomSpritesMetadata() {
  if (!asset_load_status_.ok())
    ImGui::TextWrapped("%s", asset_load_status_.ToString().c_str());
  // File operations toolbar
  if (ImGui::Button(ICON_MD_ADD " New")) {
    CreateNewZSprite();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open")) {
    std::string file_path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!file_path.empty()) {
      status_ = OpenSpriteAsset(file_path);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    if (current_custom_sprite_index_ >= 0) {
      const std::string& zsm_path = GetCurrentZsmPath();
      if (zsm_path.empty()) {
        SaveZsmFileAs();
      } else {
        SaveZsmFile(zsm_path);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE_AS " Save As")) {
    SaveZsmFileAs();
  }

  Separator();

  // Sprite list
  Text(tr("Loaded Sprites:"));
  const float avail_y = std::max(0.0f, ImGui::GetContentRegionAvail().y);
  const float metadata_reserve = std::min(160.0f, avail_y * 0.45f);
  const float list_budget = std::max(0.0f, avail_y - metadata_reserve);
  const float preferred_list = std::clamp(avail_y * 0.35f, 180.0f, 280.0f);
  const float list_height = std::min(preferred_list, list_budget);
  if (ImGui::BeginChild("SpriteList", ImVec2(0, list_height), true)) {
    for (size_t i = 0; i < custom_sprites_.size(); i++) {
      std::string label = custom_sprites_[i].sprName.empty()
                              ? "Unnamed Sprite"
                              : custom_sprites_[i].sprName;
      ImGui::PushID(static_cast<int>(i));
      if (Selectable(label.c_str(), current_custom_sprite_index_ == (int)i)) {
        current_custom_sprite_index_ = static_cast<int>(i);
        current_frame_ = custom_sprites_[i].editor.Frames.empty() ? -1 : 0;
        current_animation_index_ =
            custom_sprites_[i].animations.empty() ? -1 : 0;
        selected_tile_index_ = -1;
        selected_routine_index_ = -1;
        animation_playing_ = false;
        frame_timer_ = 0.0f;
        preview_needs_update_ = true;
        ApplyCurrentSpriteBinding();
        (void)CheckCurrentSpriteSource();
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  Separator();

  // Show properties for selected sprite
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    BeginUndoTransaction();
    DrawSpriteAssetBindings();
    if (gui::BeginThemedTabBar("SpriteDataTabs")) {
      if (ImGui::BeginTabItem(tr("Properties"))) {
        DrawSpritePropertiesPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Animations"))) {
        DrawAnimationPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Behavior"))) {
        DrawSpriteBehaviorPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Routines"))) {
        DrawUserRoutinesPanel();
        ImGui::EndTabItem();
      }
      gui::EndThemedTabBar();
    }
  } else {
    Text(tr("No sprite selected"));
  }
  CommitUndoTransaction();
}

absl::Status SpriteEditor::Copy() {
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= static_cast<int>(custom_sprites_.size()))
    return absl::FailedPreconditionError(
        "Select a custom sprite frame to copy");
  const auto& frames =
      custom_sprites_[current_custom_sprite_index_].editor.Frames;
  if (current_frame_ < 0 || current_frame_ >= static_cast<int>(frames.size()))
    return absl::FailedPreconditionError("Select a frame to copy");
  frame_clipboard_ = frames[current_frame_];
  return absl::OkStatus();
}

absl::Status SpriteEditor::Cut() {
  RETURN_IF_ERROR(Copy());
  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (sprite.editor.Frames.size() <= 1)
    return absl::FailedPreconditionError(
        "Cannot cut the last frame; copied it instead");
  BeginUndoTransaction();
  sprite_authoring::DeleteFrame(sprite, current_frame_);
  current_frame_ = std::min(current_frame_,
                            static_cast<int>(sprite.editor.Frames.size()) - 1);
  selected_tile_index_ = -1;
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

absl::Status SpriteEditor::Paste() {
  if (!frame_clipboard_ || current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= static_cast<int>(custom_sprites_.size()))
    return absl::FailedPreconditionError(
        "Copy a frame and select a destination sprite");
  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (sprite.editor.Frames.size() >= sprite_authoring::kMaxFrames)
    return absl::OutOfRangeError("ZSM animations address at most 256 frames");
  BeginUndoTransaction();
  sprite.editor.Frames.push_back(*frame_clipboard_);
  current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
  selected_tile_index_ = -1;
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

void SpriteEditor::CreateNewZSprite() {
  CommitUndoTransaction();
  zsprite::ZSprite new_sprite;
  new_sprite.Reset();
  new_sprite.sprName = "New Sprite";

  // Add default frame
  new_sprite.editor.Frames.emplace_back();

  // Add default animation
  new_sprite.animations.emplace_back(0, 0, 1, "Idle");

  custom_sprites_.push_back(std::move(new_sprite));
  custom_sprite_paths_.push_back(std::string());
  project::SpriteAssetBinding binding;
  std::copy_n(current_sheets_, 8, binding.sheets.begin());
  custom_sprite_bindings_.push_back(std::move(binding));
  source_checked_ = false;
  current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
  current_frame_ = 0;
  current_animation_index_ = 0;
  selected_tile_index_ = -1;
  selected_routine_index_ = -1;
  animation_playing_ = false;
  frame_timer_ = 0.0f;
  zsm_dirty_ = true;
  dirty_sprite_indices_.insert(current_custom_sprite_index_);
  preview_needs_update_ = true;
}

void SpriteEditor::LoadZsmFile(const std::string& path) {
  zsprite::ZSprite sprite;
  status_ = sprite.Load(path);
  if (status_.ok()) {
    custom_sprites_.push_back(std::move(sprite));
    custom_sprite_paths_.push_back(path);
    project::SpriteAssetBinding binding;
    binding.zsm_path = path;
    std::copy_n(current_sheets_, 8, binding.sheets.begin());
    if (project()) {
      for (const auto& saved : project()->sprite_assets) {
        if (project()->GetAbsolutePath(saved.zsm_path) == path) {
          binding = saved;
          break;
        }
      }
    }
    custom_sprite_bindings_.push_back(std::move(binding));
    current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
    current_frame_ = custom_sprites_.back().editor.Frames.empty() ? -1 : 0;
    current_animation_index_ =
        custom_sprites_.back().animations.empty() ? -1 : 0;
    selected_tile_index_ = -1;
    selected_routine_index_ = -1;
    animation_playing_ = false;
    frame_timer_ = 0.0f;
    zsm_dirty_ = !dirty_sprite_indices_.empty();
    preview_needs_update_ = true;
    ApplyCurrentSpriteBinding();
    (void)CheckCurrentSpriteSource();
  }
}

void SpriteEditor::SaveZsmFile(const std::string& path) {
  CommitUndoTransaction();
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    status_ = custom_sprites_[current_custom_sprite_index_].Save(path);
    if (status_.ok()) {
      SetCurrentZsmPath(path);
      auto& binding = custom_sprite_bindings_[current_custom_sprite_index_];
      binding.zsm_path = path;
      if (project()) {
        auto& assets = project()->sprite_assets;
        auto found =
            std::find_if(assets.begin(), assets.end(), [&](const auto& asset) {
              return project()->GetAbsolutePath(asset.zsm_path) == path;
            });
        if (found == assets.end())
          assets.push_back(binding);
        else
          *found = binding;
        sprite_bindings_pending_ = true;
      }
      dirty_sprite_indices_.erase(current_custom_sprite_index_);
      zsm_dirty_ = !dirty_sprite_indices_.empty();
    }
  }
}

void SpriteEditor::SaveZsmFileAs() {
  if (current_custom_sprite_index_ >= 0) {
    std::string path =
        util::FileDialogWrapper::ShowSaveFileDialog("sprite.zsm", "zsm");
    if (!path.empty()) {
      SaveZsmFile(path);
    }
  }
}

// ============================================================
// Properties Panel
// ============================================================

void SpriteEditor::DrawSpritePropertiesPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  // Basic info
  Text(tr("Sprite Info"));
  Separator();

  static char name_buf[256];
  CopyStringToBuffer(sprite.sprName, name_buf);
  if (ImGui::InputText(tr("Name"), name_buf, sizeof(name_buf))) {
    sprite.sprName = name_buf;
    sprite.property_sprname.Text = name_buf;
    MarkSpriteMutated();
  }

  static char id_buf[32];
  CopyStringToBuffer(sprite.property_sprid.Text, id_buf);
  if (ImGui::InputText(tr("Sprite ID"), id_buf, sizeof(id_buf))) {
    sprite.property_sprid.Text = id_buf;
    MarkSpriteMutated();
  }

  Separator();
  DrawStatProperties();

  Separator();
  DrawBooleanProperties();
}

void SpriteEditor::DrawStatProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Stats"));

  // Use InputInt for numeric values
  int prize = ParseIntOrDefault(sprite.property_prize.Text);
  if (ImGui::InputInt(tr("Prize"), &prize)) {
    sprite.property_prize.Text = std::to_string(std::clamp(prize, 0, 255));
    MarkSpriteMutated();
  }

  int palette = ParseIntOrDefault(sprite.property_palette.Text);
  if (ImGui::InputInt(tr("Palette"), &palette)) {
    sprite.property_palette.Text = std::to_string(std::clamp(palette, 0, 7));
    MarkSpriteMutated();
  }

  int oamnbr = ParseIntOrDefault(sprite.property_oamnbr.Text);
  if (ImGui::InputInt(tr("OAM Count"), &oamnbr)) {
    sprite.property_oamnbr.Text = std::to_string(std::clamp(oamnbr, 0, 255));
    MarkSpriteMutated();
  }

  int hitbox = ParseIntOrDefault(sprite.property_hitbox.Text);
  if (ImGui::InputInt(tr("Hitbox"), &hitbox)) {
    sprite.property_hitbox.Text = std::to_string(std::clamp(hitbox, 0, 255));
    MarkSpriteMutated();
  }

  int health = ParseIntOrDefault(sprite.property_health.Text);
  if (ImGui::InputInt(tr("Health"), &health)) {
    sprite.property_health.Text = std::to_string(std::clamp(health, 0, 255));
    MarkSpriteMutated();
  }

  int damage = ParseIntOrDefault(sprite.property_damage.Text);
  if (ImGui::InputInt(tr("Damage"), &damage)) {
    sprite.property_damage.Text = std::to_string(std::clamp(damage, 0, 255));
    MarkSpriteMutated();
  }
}

void SpriteEditor::DrawBooleanProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Behavior Flags"));

  // Two columns for boolean properties
  if (ImGui::BeginTable("BoolProps", 2, ImGuiTableFlags_None)) {
    // Column 1
    ImGui::TableNextColumn();
    if (ImGui::Checkbox(tr("Blockable"), &sprite.property_blockable.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Can Fall"), &sprite.property_canfall.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Collision Layer"),
                        &sprite.property_collisionlayer.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Custom Death"),
                        &sprite.property_customdeath.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Damage Sound"),
                        &sprite.property_damagesound.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Deflect Arrows"),
                        &sprite.property_deflectarrows.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Deflect Projectiles"),
                        &sprite.property_deflectprojectiles.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Fast"), &sprite.property_fast.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Harmless"), &sprite.property_harmless.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Impervious"),
                        &sprite.property_impervious.IsChecked))
      MarkSpriteMutated();

    // Column 2
    ImGui::TableNextColumn();
    if (ImGui::Checkbox(tr("Impervious Arrow"),
                        &sprite.property_imperviousarrow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Impervious Melee"),
                        &sprite.property_imperviousmelee.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Interaction"),
                        &sprite.property_interaction.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Is Boss"), &sprite.property_isboss.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Persist"), &sprite.property_persist.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Shadow"), &sprite.property_shadow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Small Shadow"),
                        &sprite.property_smallshadow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Stasis"), &sprite.property_statis.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Statue"), &sprite.property_statue.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Water Sprite"),
                        &sprite.property_watersprite.IsChecked))
      MarkSpriteMutated();

    ImGui::EndTable();
  }
}

// ============================================================
// Animation Panel
// ============================================================

void SpriteEditor::DrawAnimationPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  if (ImGui::Button(tr("Copy draw tables (ASM)"))) {
    auto tables = ExportCurrentSpriteDraw();
    draw_export_status_ = tables.status();
    if (tables.ok())
      ImGui::SetClipboardText(tables->c_str());
  }
  HOVER_HINT(
      "Copy a draw-table candidate. Does not register or patch a sprite.");
  if (!draw_export_status_.ok())
    ImGui::TextWrapped("%s", draw_export_status_.ToString().c_str());

  // Playback controls
  if (animation_playing_) {
    if (ImGui::Button(ICON_MD_STOP " Stop")) {
      animation_playing_ = false;
    }
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play")) {
      animation_playing_ = true;
      frame_timer_ = 0.0f;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_PREVIOUS)) {
    if (current_frame_ > 0)
      current_frame_--;
    preview_needs_update_ = true;
  }
  HOVER_HINT("Previous Frame");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_NEXT)) {
    if (current_frame_ < (int)sprite.editor.Frames.size() - 1)
      current_frame_++;
    preview_needs_update_ = true;
  }
  HOVER_HINT("Next Frame");
  ImGui::SameLine();
  Text(tr("Frame: %d / %d"), current_frame_,
       (int)sprite.editor.Frames.size() - 1);

  Separator();

  // Animation list
  Text(tr("Animations"));
  if (ImGui::Button(ICON_MD_ADD " Add Animation")) {
    int frame_count = static_cast<int>(sprite.editor.Frames.size());
    sprite.animations.emplace_back(0, std::clamp(frame_count - 1, 0, 255), 1,
                                   "New Animation");
    MarkSpriteMutated();
  }
  HOVER_HINT("Add a new animation sequence");

  if (ImGui::BeginChild("AnimList", ImVec2(0, 120), true)) {
    for (size_t i = 0; i < sprite.animations.size(); i++) {
      auto& anim = sprite.animations[i];
      std::string label = anim.frame_name.empty() ? "Unnamed" : anim.frame_name;
      ImGui::PushID(static_cast<int>(i));
      if (Selectable(label.c_str(), current_animation_index_ == (int)i)) {
        current_animation_index_ = static_cast<int>(i);
        current_frame_ = anim.frame_start;
        frame_timer_ = 0;
        preview_needs_update_ = true;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  // Edit selected animation
  if (current_animation_index_ >= 0 &&
      current_animation_index_ < (int)sprite.animations.size()) {
    auto& anim = sprite.animations[current_animation_index_];

    Separator();
    Text(tr("Animation Properties"));

    static char anim_name[128];
    CopyStringToBuffer(anim.frame_name, anim_name);
    if (ImGui::InputText(tr("Name##Anim"), anim_name, sizeof(anim_name))) {
      anim.frame_name = anim_name;
      MarkSpriteMutated();
    }

    int start = anim.frame_start;
    int end = anim.frame_end;
    int speed = anim.frame_speed;

    if (ImGui::SliderInt(
            tr("Start Frame"), &start, 0,
            std::clamp((int)sprite.editor.Frames.size() - 1, 0, 255))) {
      anim.frame_start = static_cast<uint8_t>(start);
      anim.frame_end = std::max(anim.frame_start, anim.frame_end);
      end = anim.frame_end;
      MarkSpriteMutated();
    }
    if (ImGui::SliderInt(
            tr("End Frame"), &end, 0,
            std::clamp((int)sprite.editor.Frames.size() - 1, 0, 255))) {
      anim.frame_end = static_cast<uint8_t>(end);
      anim.frame_start = std::min(anim.frame_start, anim.frame_end);
      MarkSpriteMutated();
    }
    if (ImGui::SliderInt(tr("Ticks per frame (60 Hz)"), &speed, 1, 255)) {
      anim.frame_speed = static_cast<uint8_t>(speed);
      MarkSpriteMutated();
    }

    auto& actions =
        custom_sprite_bindings_[current_custom_sprite_index_].behavior.actions;
    bool used_by_action =
        std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
          return action.animation == current_animation_index_;
        });
    ImGui::BeginDisabled(used_by_action);
    if (ImGui::Button(tr("Delete Animation")) && sprite.animations.size() > 1) {
      for (auto& action : actions)
        if (action.animation > current_animation_index_)
          --action.animation;
      sprite.animations.erase(sprite.animations.begin() +
                              current_animation_index_);
      current_animation_index_ =
          std::min(current_animation_index_, (int)sprite.animations.size() - 1);
      MarkSpriteMutated();
    }
    ImGui::EndDisabled();
    if (used_by_action)
      ImGui::TextDisabled(
          "Assign another animation to referencing actions before deleting.");
    HOVER_HINT("Delete the selected animation");
  }

  Separator();
  DrawFrameEditor();
}

void SpriteEditor::DrawFrameEditor() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Frames"));
  if (ImGui::Button(ICON_MD_ADD " Add Frame")) {
    if (sprite_authoring::AppendFrame(sprite)) {
      current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
      selected_tile_index_ = -1;
      MarkSpriteMutated();
    }
  }
  HOVER_HINT("Add a new animation frame");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Delete Frame") &&
      sprite.editor.Frames.size() > 1 && current_frame_ >= 0) {
    if (sprite_authoring::DeleteFrame(sprite, current_frame_)) {
      current_frame_ =
          std::min(current_frame_, (int)sprite.editor.Frames.size() - 1);
      selected_tile_index_ = -1;
      frame_timer_ = 0;
      MarkSpriteMutated();
    }
  }
  HOVER_HINT("Delete the current frame and repair animation ranges");
  if (ImGui::Button(tr("Duplicate Frame")) && current_frame_ >= 0 &&
      sprite_authoring::AppendFrame(sprite, current_frame_)) {
    current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
    selected_tile_index_ = -1;
    MarkSpriteMutated();
  }
  ImGui::TextDisabled("Up to 256 frames; duplicate appends a frame.");

  // Frame selector
  if (ImGui::BeginChild("FrameList", ImVec2(0, 80), true,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    for (size_t i = 0; i < sprite.editor.Frames.size(); i++) {
      ImGui::PushID(static_cast<int>(i));
      std::string label = absl::StrFormat("F%d", i);
      if (Selectable(label.c_str(), current_frame_ == (int)i,
                     ImGuiSelectableFlags_None, ImVec2(40, 40))) {
        current_frame_ = static_cast<int>(i);
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  // Edit tiles in current frame
  if (current_frame_ >= 0 &&
      current_frame_ < (int)sprite.editor.Frames.size()) {
    auto& frame = sprite.editor.Frames[current_frame_];

    Separator();
    Text(tr("Tiles in Frame %d"), current_frame_);

    if (ImGui::Button(ICON_MD_ADD " Add Tile")) {
      frame.Tiles.emplace_back();
      frame.Tiles.back().x = sprite_authoring::kOriginX;
      frame.Tiles.back().y = sprite_authoring::kOriginY;
      selected_tile_index_ = static_cast<int>(frame.Tiles.size()) - 1;
      MarkSpriteMutated();
      preview_needs_update_ = true;
    }
    HOVER_HINT("Add a new tile to this frame");

    if (ImGui::BeginChild("TileList", ImVec2(0, 100), true)) {
      for (size_t i = 0; i < frame.Tiles.size(); i++) {
        auto& tile = frame.Tiles[i];
        std::string label = absl::StrFormat("Tile %d (ID: %d)", i, tile.id);
        if (Selectable(label.c_str(), selected_tile_index_ == (int)i)) {
          selected_tile_index_ = static_cast<int>(i);
        }
      }
    }
    ImGui::EndChild();

    // Edit selected tile
    if (selected_tile_index_ >= 0 &&
        selected_tile_index_ < (int)frame.Tiles.size()) {
      auto& tile = frame.Tiles[selected_tile_index_];

      int tile_id = tile.id;
      if (ImGui::InputInt(tr("Tile ID"), &tile_id)) {
        tile.id = static_cast<uint16_t>(std::clamp(tile_id, 0, 511));
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int x = sprite_authoring::OffsetX(tile),
          y = sprite_authoring::OffsetY(tile);
      if (ImGui::InputInt(tr("X offset"), &x)) {
        tile.x = static_cast<uint8_t>(std::clamp(x, -128, 127) +
                                      sprite_authoring::kOriginX);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      if (ImGui::InputInt(tr("Y offset"), &y)) {
        tile.y = static_cast<uint8_t>(std::clamp(y, -112, 143) +
                                      sprite_authoring::kOriginY);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int pal = tile.palette;
      if (ImGui::SliderInt(tr("Palette##Tile"), &pal, 0, 7)) {
        tile.palette = static_cast<uint8_t>(pal);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int priority = tile.priority;
      if (ImGui::SliderInt(tr("BG priority"), &priority, 0, 3)) {
        tile.priority = static_cast<uint8_t>(priority);
        MarkSpriteMutated();
      }
      ImGui::TextDisabled(
          "BG priority is stored; preview has no background layers.");
      if (ImGui::Checkbox(tr("16x16"), &tile.size)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox(tr("Flip X"), &tile.mirror_x)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox(tr("Flip Y"), &tile.mirror_y)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      if (ImGui::Button(tr("Edit selected OAM pixels"))) {
        graphics_context_status_ = OpenSelectedOamGraphics();
      }
      if (!graphics_context_status_.ok()) {
        ImGui::TextWrapped(
            "%s", std::string(graphics_context_status_.message()).c_str());
      }

      if (ImGui::Button(tr("Duplicate Tile"))) {
        const auto copy = tile;
        frame.Tiles.push_back(copy);
        selected_tile_index_ = static_cast<int>(frame.Tiles.size()) - 1;
        MarkSpriteMutated();
      }
      if (ImGui::Button(tr("Delete Tile"))) {
        frame.Tiles.erase(frame.Tiles.begin() + selected_tile_index_);
        selected_tile_index_ = -1;
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      HOVER_HINT("Delete the selected tile");
    }
  }
}

void SpriteEditor::UpdateAnimationPlayback(float delta_time) {
  if (!animation_playing_ || current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= (int)custom_sprites_.size()) {
    return;
  }

  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (current_animation_index_ < 0 ||
      current_animation_index_ >= (int)sprite.animations.size()) {
    return;
  }

  auto& anim = sprite.animations[current_animation_index_];

  if (sprite_authoring::Advance(anim, sprite.editor.Frames.size(), delta_time,
                                current_frame_, frame_timer_)) {
    preview_needs_update_ = true;
  }
}

// ============================================================
// User Routines Panel
// ============================================================

void SpriteEditor::DrawUserRoutinesPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  if (ImGui::Button(ICON_MD_ADD " Add Routine")) {
    sprite.userRoutines.emplace_back("New Routine", "; ASM code here\n");
    MarkSpriteMutated();
  }
  HOVER_HINT("Add a new ASM routine");

  // Routine list
  if (ImGui::BeginChild("RoutineList", ImVec2(0, 100), true)) {
    for (size_t i = 0; i < sprite.userRoutines.size(); i++) {
      auto& routine = sprite.userRoutines[i];
      if (Selectable(routine.name.c_str(), selected_routine_index_ == (int)i)) {
        selected_routine_index_ = static_cast<int>(i);
      }
    }
  }
  ImGui::EndChild();

  // Edit selected routine
  if (selected_routine_index_ >= 0 &&
      selected_routine_index_ < (int)sprite.userRoutines.size()) {
    auto& routine = sprite.userRoutines[selected_routine_index_];

    Separator();

    static char routine_name[128];
    CopyStringToBuffer(routine.name, routine_name);
    if (ImGui::InputText(tr("Routine Name"), routine_name,
                         sizeof(routine_name))) {
      routine.name = routine_name;
      MarkSpriteMutated();
    }

    Text(tr("ASM Code:"));

    // Multiline text input for code
    static char code_buffer[16384];
    CopyStringToBuffer(routine.code, code_buffer);
    if (ImGui::InputTextMultiline("##RoutineCode", code_buffer,
                                  sizeof(code_buffer), ImVec2(-1, 200))) {
      routine.code = code_buffer;
      MarkSpriteMutated();
    }

    if (ImGui::Button(tr("Delete Routine"))) {
      sprite.userRoutines.erase(sprite.userRoutines.begin() +
                                selected_routine_index_);
      selected_routine_index_ = -1;
      MarkSpriteMutated();
    }
    HOVER_HINT("Delete the selected routine");
  }
}

// ============================================================
// Graphics Pipeline
// ============================================================

void SpriteEditor::RefreshSpritePreviewGraphics(bool use_asset_binding) {
  const bool owner_changed = preview_graphics_owner_ != game_data();
  const auto palette_revision =
      gfx::PaletteManager::Get().ColorRevision(game_data());
  if (owner_changed || preview_palette_revision_ != palette_revision) {
    room_palette_cache_.reset();
    preview_palette_revision_ = palette_revision;
    vanilla_preview_needs_update_ = true;
    preview_needs_update_ = true;
  }
  bool changed = !gfx_buffer_loaded_ || owner_changed ||
                 preview_uses_asset_graphics_ != use_asset_binding;
  const auto* sheets = use_asset_binding ? current_sheets_ : vanilla_sheets_;
  for (int slot = 0; slot < 8; ++slot) {
    const auto revision =
        game_data() ? game_data()->sheet_store.Revision(sheets[slot]) : 0;
    changed |= loaded_preview_sheets_[slot] != sheets[slot] ||
               loaded_preview_revisions_[slot] != revision;
  }
  if (changed) {
    LoadSpriteGraphicsBuffer(use_asset_binding);
    preview_uses_asset_graphics_ = use_asset_binding;
    preview_graphics_owner_ = game_data();
    for (int slot = 0; slot < 8; ++slot) {
      loaded_preview_sheets_[slot] = sheets[slot];
      loaded_preview_revisions_[slot] =
          game_data() ? game_data()->sheet_store.Revision(sheets[slot]) : 0;
    }
    vanilla_preview_needs_update_ = true;
    preview_needs_update_ = true;
  }
  LoadSpritePalettes(use_asset_binding);
}

void SpriteEditor::LoadSpriteGraphicsBuffer(bool use_asset_binding) {
  // Combine selected sheets (current_sheets_[0-7]) into single 8BPP buffer
  // Layout: 16 tiles per row, 8 rows per sheet, 8 sheets total = 64 tile rows
  // Buffer size: 0x10000 bytes (65536)

  sprite_gfx_buffer_.assign(0x10000, 0);
  graphics_binding_status_ = absl::OkStatus();

  // Each sheet is 128x32 pixels (128 bytes per row, 32 rows) = 4096 bytes
  // We combine 8 sheets vertically: 128x256 pixels total
  const auto* binding = use_asset_binding ? current_sprite_binding() : nullptr;
  const auto* sheets = use_asset_binding ? current_sheets_ : vanilla_sheets_;
  const auto directory =
      project() && !project()->filepath.empty()
          ? std::filesystem::path(project()->filepath).parent_path()
          : std::filesystem::path{};
  for (int sheet_idx = 0; sheet_idx < 8; sheet_idx++) {
    const auto source = binding ? binding->graphics_sources[sheet_idx]
                                : project::SpriteGraphicsSource{};
    auto pixels = sprite_graphics_source::Resolve(source, sheets[sheet_idx],
                                                  game_data(), rom_, directory);
    if (!pixels.ok()) {
      if (graphics_binding_status_.ok())
        graphics_binding_status_ =
            absl::Status(pixels.status().code(),
                         absl::StrFormat("Graphics sheet/source in slot %d: %s",
                                         sheet_idx, pixels.status().message()));
      continue;
    }
    std::copy(pixels->begin(), pixels->end(),
              sprite_gfx_buffer_.begin() + sheet_idx * 4096);
  }

  // Update drawer with new buffer
  sprite_drawer_.SetGraphicsBuffer(sprite_gfx_buffer_.data());
  gfx_buffer_loaded_ = true;
}

void SpriteEditor::LoadSpritePalettes(bool use_asset_binding) {
  // Load sprite palettes from ROM palette groups
  // ALTTP sprites use a combination of palette groups:
  // - Rows 0-1: Global sprite palettes (shared by all sprites)
  // - Rows 2-7: Aux palettes (vary by sprite type)
  //
  // For simplicity, we load global_sprites which contains the main
  // sprite palettes. More accurate rendering would require looking up
  // which aux palette group each sprite type uses.

  if (!rom_ || !rom_->is_loaded() || !game_data()) {
    sprite_palettes_.clear();
    palette_binding_status_ = absl::FailedPreconditionError(
        "Load ROM graphics and palettes before previewing sprites");
    return;
  }

  // Build combined sprite palette from global + aux groups
  sprite_palettes_.clear();

  // Add global sprite palettes (typically 2 palettes, 16 colors each)
  const auto& global = game_data()->palette_groups.global_sprites;
  const auto& aux1 = game_data()->palette_groups.sprites_aux1;
  const auto& aux2 = game_data()->palette_groups.sprites_aux2;
  const auto& aux3 = game_data()->palette_groups.sprites_aux3;
  sprite_palettes_ =
      internal::DefaultSpritePreviewPalettes(global, aux1, aux2, aux3);

  palette_binding_status_ = absl::OkStatus();
  if (global.empty() && aux1.empty() && aux2.empty() && aux3.empty() &&
      !preview_use_room_palette_) {
    palette_binding_status_ = absl::FailedPreconditionError(
        "Sprite palette colors are unavailable in this ROM session");
  }
  if (const auto* binding =
          use_asset_binding ? current_sprite_binding() : nullptr) {
    auto palettes = sprite_authoring::BindPaletteRows(
        *binding, sprite_palettes_, global, aux1, aux2, aux3);
    palette_binding_status_ = palettes.status();
    if (palettes.ok())
      sprite_palettes_ = std::move(*palettes);
    else
      sprite_palettes_.clear();
  }
  if (preview_use_room_palette_ && game_data() != nullptr) {
    if (!room_palette_cache_.has_value() ||
        room_palette_cache_room_ != preview_room_) {
      const auto room = zelda3::LoadRoomHeaderFromRom(rom_, preview_room_);
      room_palette_cache_ = internal::SpritePalettesFromCgram(
          zelda3::BuildDungeonSpriteRenderPalette(room, game_data()));
      room_palette_cache_room_ = preview_room_;
    }
    sprite_palettes_ = *room_palette_cache_;
    palette_binding_status_ = absl::OkStatus();
  }
  sprite_drawer_.SetPalettes(&sprite_palettes_);
}

void SpriteEditor::RenderVanillaSprite(const zelda3::SpriteOamLayout& layout) {
  // Ensure graphics buffer is loaded
  RefreshSpritePreviewGraphics(false);

  // Initialize vanilla preview bitmap if needed. The helper also queues a
  // CREATE texture command so canvas_rendering doesn't silently skip the
  // draw, and stamps the bitmap as kCompositeOutput.
  internal::EnsureSpritePreviewBitmapReady(vanilla_preview_bitmap_, 128, 128, 8,
                                           sprite_gfx_buffer_);

  if (!sprite_drawer_.IsReady() || !vanilla_preview_needs_update_) {
    return;
  }

  // Clear and render
  sprite_drawer_.ClearBitmap(vanilla_preview_bitmap_);

  // Origin is center of bitmap
  int origin_x = 64;
  int origin_y = 64;

  if (graphics_binding_status_.ok() && palette_binding_status_.ok()) {
    sprite_drawer_.DrawFrame(vanilla_preview_bitmap_,
                             sprite_authoring::CopyVanillaLayout(layout),
                             origin_x, origin_y);
  }

  // Build combined 128-color palette (8 sub-palettes × 16 colors)
  // and apply to bitmap for proper color rendering
  if (sprite_palettes_.size() > 0) {
    gfx::SnesPalette combined_palette;
    for (size_t pal_idx = 0; pal_idx < 8 && pal_idx < sprite_palettes_.size();
         pal_idx++) {
      const auto& sub_pal = sprite_palettes_.palette(pal_idx);
      for (size_t col_idx = 0; col_idx < 16 && col_idx < sub_pal.size();
           col_idx++) {
        combined_palette.AddColor(sub_pal[col_idx]);
      }
      // Pad to 16 if sub-palette is smaller
      while (combined_palette.size() < (pal_idx + 1) * 16) {
        combined_palette.AddColor(gfx::SnesColor(0));
      }
    }
    vanilla_preview_bitmap_.SetPalette(combined_palette);
  }

  // Surface pixels and palette were just mutated above; queue an UPDATE so
  // the GPU texture reflects the new state on the next frame. Without this,
  // the texture stays frozen at first-CREATE state forever.
  internal::PublishSpritePreviewPixels(vanilla_preview_bitmap_);

  vanilla_preview_needs_update_ = false;
}

// ============================================================
// Canvas Rendering
// ============================================================

void SpriteEditor::RenderZSpriteFrame(int frame_index) {
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= (int)custom_sprites_.size()) {
    return;
  }

  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (frame_index < 0 || frame_index >= (int)sprite.editor.Frames.size()) {
    return;
  }

  auto& frame = sprite.editor.Frames[frame_index];

  // Ensure graphics buffer is loaded
  RefreshSpritePreviewGraphics(true);

  // The sprite_canvas_ displays a post-render composite (pixels come from
  // SpriteDrawer, not direct user paint). Declaring the role lets future
  // canvas surfaces (cursor hints, context-menu items) default appropriately.
  sprite_canvas_.GetConfig().role = gui::CanvasRole::kCompositeOutput;

  // Initialize preview bitmap if needed. The helper also queues a CREATE
  // texture command so canvas_rendering doesn't silently skip the draw, and
  // stamps the bitmap as kCompositeOutput.
  internal::EnsureSpritePreviewBitmapReady(sprite_preview_bitmap_, 256, 256, 8,
                                           sprite_gfx_buffer_);

  // Only render if drawer is ready
  if (sprite_drawer_.IsReady() && preview_needs_update_) {
    // Clear and render to preview bitmap
    sprite_drawer_.ClearBitmap(sprite_preview_bitmap_);

    // Origin is center of canvas (128, 128 for 256x256 bitmap).
    if (palette_binding_status_.ok() && graphics_binding_status_.ok())
      sprite_drawer_.DrawFrame(sprite_preview_bitmap_, frame, 128, 128);

    // Build combined 128-color palette and apply to bitmap
    if (sprite_palettes_.size() > 0) {
      gfx::SnesPalette combined_palette;
      for (size_t pal_idx = 0; pal_idx < 8 && pal_idx < sprite_palettes_.size();
           pal_idx++) {
        const auto& sub_pal = sprite_palettes_.palette(pal_idx);
        for (size_t col_idx = 0; col_idx < 16 && col_idx < sub_pal.size();
             col_idx++) {
          combined_palette.AddColor(sub_pal[col_idx]);
        }
        // Pad to 16 if sub-palette is smaller
        while (combined_palette.size() < (pal_idx + 1) * 16) {
          combined_palette.AddColor(gfx::SnesColor(0));
        }
      }
      sprite_preview_bitmap_.SetPalette(combined_palette);
    }

    // Surface pixels and palette were just mutated above; queue an UPDATE so
    // the GPU texture reflects the new frame on the next paint.
    internal::PublishSpritePreviewPixels(sprite_preview_bitmap_);

    // Mark as updated
    preview_needs_update_ = false;
  }

  // Draw the preview bitmap on canvas
  if (sprite_preview_bitmap_.is_active()) {
    sprite_canvas_.DrawBitmap(sprite_preview_bitmap_, 0, 0, 2.0f);
  }

  // Draw tile outlines for selection (over the bitmap)
  if (show_tile_grid_) {
    for (size_t i = 0; i < frame.Tiles.size(); i++) {
      const auto& tile = frame.Tiles[i];
      int tile_size = tile.size ? 16 : 8;

      // Convert signed tile position to canvas position
      int signed_x = sprite_authoring::OffsetX(tile);
      int signed_y = sprite_authoring::OffsetY(tile);

      int canvas_x = 128 + signed_x;
      int canvas_y = 128 + signed_y;

      // Highlight selected tile
      ImVec4 color = (selected_tile_index_ == static_cast<int>(i))
                         ? ImVec4(0.0f, 1.0f, 0.0f, 0.8f)  // Green for selected
                         : ImVec4(1.0f, 1.0f, 0.0f, 0.3f);  // Yellow for others

      // Match the 2x bitmap scale used above.
      sprite_canvas_.DrawRect(canvas_x * 2, canvas_y * 2, tile_size * 2,
                              tile_size * 2, color);
    }
  }
}

void SpriteEditor::DrawZSpriteOnCanvas() {
  if (ImGui::BeginChild(gui::GetID("##ZSpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();

    // Render current frame if we have a sprite selected
    if (current_custom_sprite_index_ >= 0 &&
        current_custom_sprite_index_ < (int)custom_sprites_.size()) {
      RenderZSpriteFrame(current_frame_);
      if (!graphics_binding_status_.ok() || !palette_binding_status_.ok()) {
        ImGui::SetCursorPos(ImVec2(10, 34));
        const auto& error = !graphics_binding_status_.ok()
                                ? graphics_binding_status_
                                : palette_binding_status_;
        ImGui::TextWrapped("Preview unavailable: %s",
                           std::string(error.message()).c_str());
      }
    } else {
      ImGui::SetCursorPos(ImVec2(10, 10));
      ImGui::TextWrapped(
          "Open a ZSM or import a draw copy to preview its frames.");
    }

    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();

    // Display current frame info
    if (current_custom_sprite_index_ >= 0) {
      auto& sprite = custom_sprites_[current_custom_sprite_index_];
      ImGui::SetCursorPos(ImVec2(10, 10));
      char label[256];
      std::snprintf(label, sizeof(label), tr("Frame: %d | Tiles: %d"),
                    current_frame_,
                    current_frame_ >= 0 &&
                            current_frame_ < (int)sprite.editor.Frames.size()
                        ? (int)sprite.editor.Frames[current_frame_].Tiles.size()
                        : 0);
      const ImVec2 position = ImGui::GetCursorScreenPos();
      const ImVec2 size = ImGui::CalcTextSize(label);
      // Preview colors come from the ROM; keep status legible on any palette.
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(position.x - 3, position.y - 2),
          ImVec2(position.x + size.x + 3, position.y + size.y + 2),
          IM_COL32(24, 24, 24, 255), 3.0f);
      ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", label);
    }
  }
  ImGui::EndChild();
}

// ============================================================
// Undo/Redo Helpers
// ============================================================

SpriteSnapshot SpriteEditor::CaptureCurrentSpriteSnapshot() const {
  SpriteSnapshot snapshot;
  snapshot.sprite_index = current_custom_sprite_index_;
  snapshot.current_frame = current_frame_;
  snapshot.current_animation_index = current_animation_index_;
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < static_cast<int>(custom_sprites_.size())) {
    snapshot.sprite_data = custom_sprites_[current_custom_sprite_index_];
    snapshot.binding = custom_sprite_bindings_[current_custom_sprite_index_];
  }
  return snapshot;
}

void SpriteEditor::RestoreFromSnapshot(const SpriteSnapshot& snapshot) {
  if (snapshot.sprite_index < 0 ||
      snapshot.sprite_index >= static_cast<int>(custom_sprites_.size())) {
    return;
  }
  current_custom_sprite_index_ = snapshot.sprite_index;
  current_frame_ = snapshot.current_frame;
  current_animation_index_ = snapshot.current_animation_index;
  custom_sprites_[snapshot.sprite_index] = snapshot.sprite_data;
  custom_sprite_bindings_[snapshot.sprite_index] = snapshot.binding;
  ApplyCurrentSpriteBinding();
  preview_needs_update_ = true;
  zsm_dirty_ = true;
  dirty_sprite_indices_.insert(current_custom_sprite_index_);
}

void SpriteEditor::BeginUndoTransaction() {
  if (undo_snapshot_pending_) {
    return;  // Already in a transaction
  }
  undo_before_snapshot_ = CaptureCurrentSpriteSnapshot();
  undo_snapshot_pending_ = true;
}

void SpriteEditor::CommitUndoTransaction() {
  if (!undo_snapshot_pending_) {
    return;
  }

  if (!sprite_mutated_this_frame_) {
    // No mutation happened; discard the pending snapshot so we can
    // capture a fresh one next frame.
    undo_snapshot_pending_ = false;
    return;
  }

  undo_snapshot_pending_ = false;
  sprite_mutated_this_frame_ = false;

  auto after = CaptureCurrentSpriteSnapshot();
  auto restore_fn = [this](const SpriteSnapshot& snapshot) {
    RestoreFromSnapshot(snapshot);
  };
  undo_manager_.Push(std::make_unique<SpriteEditAction>(
      std::move(undo_before_snapshot_), std::move(after),
      std::move(restore_fn)));
}

void SpriteEditor::MarkSpriteMutated() {
  behavior_candidate_.clear();
  preview_needs_update_ = true;
  zsm_dirty_ = true;
  dirty_sprite_indices_.insert(current_custom_sprite_index_);
  sprite_mutated_this_frame_ = true;
}

void SpriteEditor::EnsureCustomSpritePaths() {
  if (custom_sprite_paths_.size() < custom_sprites_.size()) {
    custom_sprite_paths_.resize(custom_sprites_.size());
  }
}

const std::string& SpriteEditor::GetCurrentZsmPath() const {
  static const std::string kEmptyPath;
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >=
          static_cast<int>(custom_sprite_paths_.size())) {
    return kEmptyPath;
  }
  return custom_sprite_paths_[current_custom_sprite_index_];
}

void SpriteEditor::SetCurrentZsmPath(const std::string& path) {
  EnsureCustomSpritePaths();
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >=
          static_cast<int>(custom_sprite_paths_.size())) {
    return;
  }
  custom_sprite_paths_[current_custom_sprite_index_] = path;
}

}  // namespace editor
}  // namespace yaze
