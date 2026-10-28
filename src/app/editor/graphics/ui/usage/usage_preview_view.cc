#include "app/editor/graphics/ui/usage/usage_preview_view.h"

#include <algorithm>
#include <set>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

uint64_t HashBytes(uint64_t hash, const std::vector<uint8_t>& bytes) {
  for (const uint8_t b : bytes) {
    hash = (hash ^ b) * kFnvPrime;
  }
  return hash;
}

void QueueTexture(gfx::Bitmap& bitmap) {
  gfx::Arena::Get().QueueTextureCommand(
      bitmap.texture() ? gfx::Arena::TextureCommandType::UPDATE
                       : gfx::Arena::TextureCommandType::CREATE,
      &bitmap);
}

}  // namespace

void UsagePreviewView::SetRom(Rom* rom) {
  rom_ = rom;
  // Explicit rebinding also covers replacement at the same Rom address.
  InvalidateTables();
}

void UsagePreviewView::SetGameData(zelda3::GameData* game_data) {
  game_data_ = game_data;
  InvalidateTables();
}

void UsagePreviewView::InvalidateTables() {
  shared_scan_ready_ = false;
  shared_scan_status_.clear();
  shared_targets_.clear();
  shared_uses_.clear();
  shared_preview_room_ = -1;
  shared_room_preview_.Reset();
  source_dirty_ = true;
  source_render_time_ = -1.0;
  source_model_revision_.reset();
  live_tile16_hash_ = 0;
  tile16_uses_live_source_ = false;
  tables_ready_ = false;
  room_blocks_.clear();
  fallback_room_blocks_.clear();
  live_rooms_.clear();
  live_room_revisions_.clear();
  rooms_for_sheet_.clear();
  object_rects_.clear();
  current_live_room_ = -1;
  group_hash_ = 0;
  last_render_time_ = -1.0;
  room_gfx_.clear();
  areas_.reset();
  tile16_preview_ = usage_preview::Tile16UsagePreview{};
  tile16s_.clear();
  room_preview_.Reset();
  configured_spriteset_ = -1;
  last_sheet_ = 0xFFFF;
  dirty_ = true;
}

void UsagePreviewView::EnsureTables() {
  if (!rom_ || !rom_->is_loaded() || !game_data_)
    return;
  uint64_t hash = kFnvOffset;
  auto groups = [&](const auto& table) {
    for (const auto& row : table)
      for (const auto id : row)
        hash = (hash ^ id) * kFnvPrime;
  };
  groups(game_data_->main_blockset_ids);
  groups(game_data_->room_blockset_ids);
  groups(game_data_->spriteset_ids);
  if (tables_ready_ && group_hash_ == hash)
    return;
  group_hash_ = hash;
  room_gfx_ = zelda3::CollectRoomGfx(*rom_);
  fallback_room_blocks_ =
      usage_preview::CollectRoomSheetBlocks(rom_, game_data_, room_gfx_);
  room_preview_.Reset();
  last_sheet_ = 0xFFFF;
  dirty_ = true;
  tables_ready_ = true;
}

void UsagePreviewView::RefreshRoomContexts() {
  auto contexts = live_rooms_provider_ ? live_rooms_provider_()
                                       : usage_preview::LiveRoomContexts{};
  auto blocks = fallback_room_blocks_;
  std::map<int, uint64_t> revisions;
  int current = -1;
  for (auto it = contexts.begin(); it != contexts.end();) {
    auto* room = it->second.room;
    if (!room || room->rom() != rom_ || room->game_data() != game_data_ ||
        !room->IsLoaded() || room->id() != it->first) {
      it = contexts.erase(it);
      continue;
    }
    std::copy_n(room->blocks().begin(), 16, blocks[it->first].begin());
    revisions[it->first] = room->composite_source_revision();
    if (it->second.current)
      current = it->first;
    ++it;
  }
  const bool changed = blocks != room_blocks_ ||
                       revisions != live_room_revisions_ ||
                       current != current_live_room_;
  if (changed || last_sheet_ != state_->current_sheet_id) {
    const int previous =
        room_choice_ >= 0 &&
                room_choice_ < static_cast<int>(rooms_for_sheet_.size())
            ? rooms_for_sheet_[room_choice_]
            : -1;
    rooms_for_sheet_ =
        usage_preview::RoomsUsingSheet(blocks, state_->current_sheet_id, true);
    // Keep the selected live room inspectable even if it stops loading the
    // edited sheet; that absence is useful feedback, not a reason to hide it.
    if (current >= 0 &&
        std::find(rooms_for_sheet_.begin(), rooms_for_sheet_.end(), current) ==
            rooms_for_sheet_.end())
      rooms_for_sheet_.push_back(current);
    const int preferred =
        current != current_live_room_ || last_sheet_ != state_->current_sheet_id
            ? current
            : previous;
    const auto selected =
        std::find(rooms_for_sheet_.begin(), rooms_for_sheet_.end(), preferred);
    room_choice_ = selected == rooms_for_sheet_.end()
                       ? 0
                       : static_cast<int>(selected - rooms_for_sheet_.begin());
    dirty_ = true;
    last_render_time_ = -1.0;
  }
  room_blocks_ = std::move(blocks);
  live_room_revisions_ = std::move(revisions);
  live_rooms_ = std::move(contexts);
  current_live_room_ = current;
}

uint64_t UsagePreviewView::HashWatchedSheets(bool include_room_context) const {
  uint64_t hash = kFnvOffset;
  // Other sheet editors can commit a sheet this room uses without adding it
  // to GraphicsEditor's modified_sheets. Watch only the chosen room's store
  // revisions here; membership refresh must not rasterize loaded rooms.
  if (include_room_context && room_choice_ >= 0 &&
      room_choice_ < static_cast<int>(rooms_for_sheet_.size())) {
    const auto found = room_blocks_.find(rooms_for_sheet_[room_choice_]);
    if (found != room_blocks_.end()) {
      for (const auto sheet : found->second) {
        hash = (hash ^ sheet) * kFnvPrime;
        hash = (hash ^ game_data_->sheet_store.Revision(sheet)) * kFnvPrime;
      }
    }
  }
  std::set<uint16_t> ids = state_->modified_sheets;
  ids.insert(state_->current_sheet_id);
  if (state_->source_context) {
    for (const auto& target : state_->source_context->targets) {
      ids.insert(target.sheet_id);
    }
    for (const auto& slot : state_->source_context->resolved.slots) {
      ids.insert(slot.sheet_id);
    }
  }
  const auto sheets = usage_preview::SnapshotSessionSheets(ids, game_data_);
  for (const auto& [id, pixels] : sheets) {
    hash = (hash ^ id) * kFnvPrime;
    hash = HashBytes(hash, pixels);
  }
  return hash;
}

usage_preview::SheetPixels UsagePreviewView::Snapshot() const {
  std::set<uint16_t> ids = state_->modified_sheets;
  ids.insert(state_->current_sheet_id);
  return usage_preview::SnapshotSessionSheets(ids, game_data_);
}

int UsagePreviewView::SelectedTileIndex() const {
  const auto& selection = state_->selection;
  if (follow_selection_ && selection.is_active) {
    const int tile = (selection.y / 8) * 16 + selection.x / 8;
    return std::clamp(tile, 0, 63);
  }
  return tile_index_;
}

void UsagePreviewView::ShowImage(gfx::Bitmap& bitmap,
                                 const usage_preview::PreviewImage& image) {
  if (image.empty()) {
    return;
  }
  const bool indexed =
      image.format == usage_preview::PreviewImage::Format::kIndexed;
  const int depth = indexed ? 8 : 32;
  std::vector<SDL_Color> colors(image.colors.begin(), image.colors.end());
  if (image.index0_transparent) {
    colors[0].a = 0;
  }
  if (bitmap.width() != image.width || bitmap.height() != image.height ||
      !bitmap.is_active() || bitmap.depth() != depth) {
    bitmap.Create(image.width, image.height, depth, image.pixels);
  } else {
    bitmap.set_data(image.pixels);
  }
  if (indexed)
    bitmap.SetPalette(colors);
  bitmap.metadata().purpose = gfx::Bitmap::BitmapPurpose::kPreview;
  QueueTexture(bitmap);
}

void UsagePreviewView::DrawBitmapFit(gfx::Bitmap& bitmap,
                                     const std::vector<SDL_Rect>& rects,
                                     float max_scale, bool fit_height) {
  if (!bitmap.is_active() || bitmap.width() <= 0 || !bitmap.texture()) {
    ImGui::TextDisabled("%s", tr("Rendering..."));
    return;
  }
  const auto available = ImGui::GetContentRegionAvail();
  float scale =
      std::min(max_scale, std::max(1.0f, available.x) / bitmap.width());
  if (fit_height) {
    scale = std::min(scale, std::max(1.0f, available.y) / bitmap.height());
  } else {
    scale = std::max(0.5f, scale);
  }
  const ImVec2 size(bitmap.width() * scale, bitmap.height() * scale);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Image((ImTextureID)(intptr_t)bitmap.texture(), size);
  auto* draw_list = ImGui::GetWindowDrawList();
  const ImU32 color = ImGui::GetColorU32(gui::GetWarningColor());
  for (const auto& r : rects) {
    draw_list->AddRect(
        ImVec2(origin.x + r.x * scale, origin.y + r.y * scale),
        ImVec2(origin.x + (r.x + r.w) * scale, origin.y + (r.y + r.h) * scale),
        color, 0.0f, 1.5f);
  }
}

void UsagePreviewView::DrawSourcePreview(bool show_controls) {
  if (!state_ || !game_data_ || !state_->source_context ||
      state_->source_context->owner != game_data_ ||
      !state_->source_context->render_preview) {
    ImGui::TextDisabled("%s", tr("No source preview available."));
    return;
  }
  auto& source = *state_->source_context;
  if (show_controls) {
    ImGui::TextUnformatted(tr("Source preview"));
    ImGui::SameLine();
    ImGui::Checkbox(tr("Live"), &live_);
    gui::AutoRegisterLastItem("checkbox", "Live source preview");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_REFRESH "##SourceRefresh")) {
      source_dirty_ = true;
      source_render_time_ = -1.0;
    }
    gui::AutoRegisterLastItem("button", "Refresh source preview");
    HOVER_HINT("Render unsaved source pixels now");
  }
  if (!source.palette_error.empty()) {
    // Never present an old image as a successfully refreshed source.
    source_dirty_ = true;
    source_render_time_ = -1.0;
    ImGui::TextWrapped("%s", source.palette_error.c_str());
    return;
  }
  const auto revision = gfx::PaletteManager::Get().ColorRevision(game_data_);
  if (rendered_source_revision_ != state_->source_context_revision ||
      source_palette_revision_ != revision) {
    if (rendered_source_revision_ != state_->source_context_revision)
      source_model_revision_.reset();
    rendered_source_revision_ = state_->source_context_revision;
    source_palette_revision_ = revision;
    source_dirty_ = true;
    source_render_time_ = -1.0;
  }
  if (live_ && source.preview_revision) {
    const auto current = source.preview_revision();
    if (!current.ok()) {
      source_status_ = std::string(current.status().message());
      source_dirty_ = true;
      source_render_time_ = -1.0;
      ImGui::TextWrapped("%s", source_status_.c_str());
      return;
    }
    if (!source_model_revision_ || *source_model_revision_ != *current) {
      source_model_revision_ = *current;
      source_dirty_ = true;
    }
  }
  // The independent Room chooser must not invalidate the captured Source.
  const uint64_t hash =
      live_ ? HashWatchedSheets(/*include_room_context=*/false) : source_hash_;
  if (hash != source_hash_) {
    source_hash_ = hash;
    source_dirty_ = true;
  }
  const double now = ImGui::GetTime();
  if (source_dirty_ && (source_render_time_ < 0.0 ||
                        !ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                        now - source_render_time_ >= kMinRenderInterval)) {
    const auto rendered = source.render_preview();
    source_status_ = rendered.ok() ? std::string()
                                   : std::string(rendered.status().message());
    if (rendered.ok()) {
      if (rendered->empty())
        source_status_ = tr("Source preview is empty.");
      else
        ShowImage(source_bitmap_, *rendered);
    }
    source_dirty_ = false;
    // Rendering may refresh source caches. Sample after that work so a
    // renderer's own revision does not cause another render every frame.
    if (rendered.ok() && source.preview_revision) {
      const auto current = source.preview_revision();
      if (current.ok())
        source_model_revision_ = *current;
    }
    source_render_time_ = now;
  }
  if (!source_status_.empty()) {
    ImGui::TextWrapped("%s", source_status_.c_str());
    return;
  }
  DrawSharedUses();
  DrawBitmapFit(source_bitmap_, {}, 2.0f, true);
}

uint64_t UsagePreviewView::SharedUsesHash(
    const usage_preview::LiveRoomContexts& rooms) const {
  uint64_t hash = kFnvOffset;
  auto add = [&](uint64_t value) {
    hash = (hash ^ value) * kFnvPrime;
  };
  add(state_->source_context_revision);
  add(state_->source_context ? state_->source_context->selected_target : 0);
  add(state_->current_sheet_id);
  add(shared_whole_pattern_);
  add(gfx::PaletteManager::Get().ColorRevision(game_data_));
  add(zelda3::CustomObjectManager::Get().asset_generation());
  for (const auto& [id, context] : rooms) {
    add(id);
    add(reinterpret_cast<uintptr_t>(context.room));
    add(context.entrance_slot);
    if (context.room && context.room->rom() == rom_ &&
        context.room->game_data() == game_data_) {
      add(context.room->composite_source_revision());
      for (const auto sheet : context.room->blocks()) {
        add(sheet);
        add(game_data_->sheet_store.Revision(sheet));
      }
    }
  }
  return hash;
}

absl::Status UsagePreviewView::ScanSharedUses(
    const usage_preview::LiveRoomContexts& rooms) {
  shared_scan_ready_ = false;
  shared_uses_.clear();
  shared_targets_.clear();
  shared_preview_room_ = -1;
  if (!rom_ || !rom_->is_loaded() || !game_data_ || !state_->SourceTarget() ||
      !state_->source_context->resolve_live_source) {
    return absl::FailedPreconditionError(
        "Reopen Edit Graphics from a live source to find shared uses.");
  }
  const auto& source = *state_->source_context;
  GraphicsEditContext fresh;
  RETURN_IF_ERROR(source.resolve_live_source(fresh));
  if (fresh.owner != game_data_ ||
      fresh.resolved.surface != source.resolved.surface ||
      fresh.resolved.id != source.resolved.id ||
      fresh.resolved.parent_id != source.resolved.parent_id) {
    return absl::FailedPreconditionError(
        "Graphics source changed; reopen Edit Graphics from the source.");
  }
  // Validate against the captured source without redirecting pending edits.
  auto validated = source;
  RETURN_IF_ERROR(UpdateGraphicsContextPalettes(validated, std::move(fresh)));
  if (shared_whole_pattern_) {
    shared_targets_ = std::move(validated.targets);
  } else {
    shared_targets_.push_back(validated.targets.at(source.selected_target));
  }
  shared_uses_ = usage_preview::FindLoadedRoomSourceUses(
      rom_, game_data_, rooms, shared_targets_);
  shared_scan_hash_ = SharedUsesHash(rooms);
  shared_scan_ready_ = true;
  return absl::OkStatus();
}

void UsagePreviewView::DrawSharedUses() {
  const bool expanded = ImGui::CollapsingHeader(
      tr("Shared uses in loaded rooms"), ImGuiTreeNodeFlags_NoAutoOpenOnLog);
  gui::AutoRegisterLastItem("header", "Shared uses in loaded rooms");
  if (!expanded)
    return;
  const auto rooms = live_rooms_provider_ ? live_rooms_provider_()
                                          : usage_preview::LiveRoomContexts{};
  if (shared_scan_ready_ && shared_scan_hash_ != SharedUsesHash(rooms)) {
    shared_scan_ready_ = false;
    shared_uses_.clear();
    shared_preview_room_ = -1;
    shared_scan_status_ =
        tr("Source or loaded rooms changed. Find shared uses again.");
  }
  if (ImGui::RadioButton(tr("Current tile##SharedScope"),
                         !shared_whole_pattern_)) {
    shared_whole_pattern_ = false;
    shared_scan_ready_ = false;
    shared_preview_room_ = -1;
  }
  gui::AutoRegisterLastItem("radio", "Shared uses current tile");
  if (ImGui::RadioButton(tr("Source pattern##SharedScope"),
                         shared_whole_pattern_)) {
    shared_whole_pattern_ = true;
    shared_scan_ready_ = false;
    shared_preview_room_ = -1;
  }
  gui::AutoRegisterLastItem("radio", "Shared uses source pattern");
  if (ImGui::Button(tr("Find shared uses"))) {
    const auto status = ScanSharedUses(rooms);
    shared_scan_status_ =
        status.ok() ? std::string() : std::string(status.message());
  }
  gui::AutoRegisterLastItem("button", "Find shared uses");
  ImGui::TextWrapped(
      "%s", tr("Loaded dungeon rooms only. Doors, layout, sprites, unloaded "
               "rooms and untraced drawing are not covered. Matches are "
               "possible uses, not a prediction of visible changes."));
  if (!shared_scan_status_.empty())
    ImGui::TextWrapped("%s", shared_scan_status_.c_str());
  if (!shared_scan_ready_)
    return;
  size_t matches = 0, unknown = 0, failed = 0;
  for (const auto& room : shared_uses_) {
    matches += room.objects.size();
    unknown += room.unknown_objects;
    failed += !room.error.empty();
  }
  ImGui::Text(tr("%zu objects / %zu loaded rooms"), matches,
              shared_uses_.size());
  ImGui::Text(tr("%zu unknown objects; %zu rooms unavailable"), unknown,
              failed);
  ImGui::TextWrapped("%s", tr("Pixels = same sheet and tile. Palette = shared "
                              "source color addresses; unmapped colors are "
                              "not counted."));
  if (ImGui::TreeNodeEx(tr("Source tiles and palettes"),
                        ImGuiTreeNodeFlags_NoAutoOpenOnLog)) {
    for (const auto& target : shared_targets_) {
      ImGui::Text("Sheet $%02X / tile $%02X", target.sheet_id,
                  target.tile_index);
      if (target.writable_palette) {
        const auto& mapping = *target.writable_palette;
        ImGui::Text("Row %u, %s half", mapping.row,
                    mapping.half_offset == 8 ? "right" : "left");
        for (size_t i = 1; i < mapping.colors.size(); ++i) {
          if (const auto& address = mapping.colors[i])
            ImGui::Text("%zu: %s / %d / %d", i,
                        zelda3::PaletteAddressGroupName(address->group),
                        address->palette_index, address->color_index);
        }
      } else {
        ImGui::TextWrapped("%s", target.palette_address_limitation.c_str());
      }
    }
    ImGui::TreePop();
  }
  if (matches == 0) {
    ImGui::TextWrapped("%s", tr("No traced matches in these loaded rooms. "
                                "This does not establish exclusive use."));
  }
  ImGui::BeginChild("##SharedUseResults", ImVec2(0, 8 * ImGui::GetFontSize()),
                    true);
  for (const auto& room : shared_uses_) {
    if (!room.error.empty())
      ImGui::TextWrapped("$%03X: %s", room.room_id, room.error.c_str());
    for (const auto& object : room.objects) {
      const auto label = absl::StrFormat(
          "$%03X / object %03X #%zu: %s%s%s", room.room_id, object.object_id,
          object.object_index, object.shares_pixels ? "Pixels" : "",
          object.shares_pixels && object.shares_palette ? " + " : "",
          object.shares_palette ? "Palette" : "");
      if (ImGui::Selectable(
              label.c_str(),
              shared_preview_room_ == room.room_id &&
                  shared_preview_object_ == object.object_index)) {
        const auto live = rooms.find(room.room_id);
        const auto status =
            live == rooms.end()
                ? absl::FailedPreconditionError("Room is no longer loaded")
                : shared_room_preview_.Render(rom_, game_data_, room.room_id,
                                              {}, false, live->second.room);
        shared_scan_status_ =
            status.ok() ? std::string() : std::string(status.message());
        if (status.ok()) {
          shared_preview_room_ = room.room_id;
          shared_preview_object_ = object.object_index;
          QueueTexture(shared_room_preview_.mutable_bitmap());
          shared_scan_hash_ = SharedUsesHash(rooms);
        }
      }
      gui::AutoRegisterLastItem("selectable", label);
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        for (const auto& reference : object.references)
          ImGui::Text("Sheet $%02X / tile $%02X / row %d / slot %d",
                      reference.sheet, reference.tile_word & 0x3F,
                      (reference.tile_word >> 10) & 7, reference.slot);
        ImGui::EndTooltip();
      }
    }
  }
  ImGui::EndChild();
  if (shared_preview_room_ >= 0) {
    ImGui::Text("Room $%03X / object #%zu", shared_preview_room_,
                shared_preview_object_);
    DrawBitmapFit(
        shared_room_preview_.mutable_bitmap(),
        shared_room_preview_.ObjectRectsForObject(shared_preview_object_),
        0.5f);
  }
}

void UsagePreviewView::Draw() {
  if (rom_ == nullptr || !rom_->is_loaded() || game_data_ == nullptr) {
    ImGui::TextDisabled("%s", tr("Load a ROM to preview sheet usage."));
    return;
  }
  EnsureTables();
  RefreshRoomContexts();
  const auto palette_revision =
      gfx::PaletteManager::Get().ColorRevision(game_data_);
  if (palette_revision != palette_revision_) {
    palette_revision_ = palette_revision;
    dirty_ = true;
    // Cached generic renderers must reload colors too, even without a stroke.
    room_preview_.Reset();
    tile16_preview_ = usage_preview::Tile16UsagePreview{};
  }
  const bool source_changed =
      source_context_revision_ != state_->source_context_revision;
  if (source_changed) {
    source_context_revision_ = state_->source_context_revision;
    dirty_ = true;
    last_render_time_ = -1.0;
  }
  const uint16_t sheet = state_->current_sheet_id;
  if (sheet != last_sheet_) {
    last_sheet_ = sheet;
    spritesets_for_sheet_.clear();
    for (int ss = 0; ss < static_cast<int>(game_data_->spriteset_ids.size());
         ++ss) {
      const auto slots = zelda3::SpriteSheetSlots(
          game_data_->spriteset_ids[ss], zelda3::IsUnderworldSpriteset(ss));
      // Slots 4-7 are the spriteset's own sheets; 0-3 are shared by all.
      if (std::find(slots.begin() + 4, slots.end(), sheet) != slots.end()) {
        spritesets_for_sheet_.push_back(ss);
      }
    }
    // Dungeon spritesets that a room uses come first: they have a room
    // sprite palette to preview with.
    std::stable_partition(
        spritesets_for_sheet_.begin(), spritesets_for_sheet_.end(),
        [this](int ss) {
          return std::any_of(
              room_gfx_.begin(), room_gfx_.end(), [ss](const auto& room) {
                return room.second.spriteset + zelda3::kDungeonSpritesetBase ==
                       ss;
              });
        });
    spriteset_choice_ = 0;
    configured_spriteset_ = -1;
    areas_for_sheet_.clear();
    area_choice_ = 0;
    dirty_ = true;
  }

  ImGui::Text(tr("Sheet 0x%02X"), sheet);
  ImGui::SameLine();
  ImGui::Checkbox(tr("Live"), &live_);
  HOVER_HINT(
      "Re-render while you draw (at most 10 times a second, and when the "
      "stroke ends). Unsaved edits only; the ROM is not written.");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    dirty_ = true;
    last_render_time_ = -1.0;
    source_dirty_ = true;
    source_render_time_ = -1.0;
  }
  HOVER_HINT("Render now");

  // Throttle: re-render when the watched pixels changed, no more than every
  // kMinRenderInterval, and always once the mouse is released (stroke end).
  const uint64_t hash =
      live_ ? HashWatchedSheets(/*include_room_context=*/mode_ == Mode::kRoom)
            : last_hash_;
  if (hash != last_hash_) {
    last_hash_ = hash;
    dirty_ = true;
  }
  const double now = ImGui::GetTime();
  const bool stroke_active = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool render_now =
      dirty_ && (last_render_time_ < 0.0 || !stroke_active ||
                 now - last_render_time_ >= kMinRenderInterval);
  if (render_now) {
    last_render_time_ = now;
    dirty_ = false;
  }

  if (ImGui::BeginTabBar("##UsagePreviewTabs")) {
    if (state_->source_context && state_->source_context->render_preview &&
        state_->source_context->owner == game_data_) {
      if (ImGui::BeginTabItem(
              tr("Source"), nullptr,
              source_changed ? ImGuiTabItemFlags_SetSelected : 0)) {
        DrawSourcePreview(false);
        ImGui::EndTabItem();
      }
    }
    if (ImGui::BeginTabItem(tr("Room"))) {
      if (mode_ != Mode::kRoom) {
        mode_ = Mode::kRoom;
        dirty_ = true;
      }
      DrawRoomContext(render_now);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tr("Sprite"))) {
      if (mode_ != Mode::kSprite) {
        mode_ = Mode::kSprite;
        dirty_ = true;
      }
      DrawSpriteContext(render_now, ImGui::GetIO().DeltaTime);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tr("Tile16"))) {
      if (mode_ != Mode::kTile16) {
        mode_ = Mode::kTile16;
        dirty_ = true;
      }
      DrawTile16Context(render_now);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  if (!status_.empty()) {
    ImGui::TextColored(gui::GetWarningColor(), "%s", status_.c_str());
  }
}

void UsagePreviewView::DrawRoomContext(bool render_now) {
  if (rooms_for_sheet_.empty()) {
    ImGui::TextDisabled("%s", tr("No dungeon room loads this sheet."));
    return;
  }
  room_choice_ = std::clamp(room_choice_, 0,
                            static_cast<int>(rooms_for_sheet_.size()) - 1);
  const int room_id = rooms_for_sheet_[room_choice_];
  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::BeginCombo(tr("Room"),
                        absl::StrFormat("0x%03X", room_id).c_str())) {
    for (int i = 0; i < static_cast<int>(rooms_for_sheet_.size()); ++i) {
      const bool selected = i == room_choice_;
      if (ImGui::Selectable(
              absl::StrFormat("0x%03X", rooms_for_sheet_[i]).c_str(),
              selected)) {
        room_choice_ = i;
        dirty_ = true;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextDisabled(tr("%zu rooms"), rooms_for_sheet_.size());
  if (ImGui::Checkbox(tr("Objects only"), &objects_only_)) {
    dirty_ = true;
  }
  HOVER_HINT("Hide the floor and layout layers; show object layers only.");
  ImGui::SameLine();
  ImGui::Checkbox(tr("Outline objects using this sheet"), &outline_objects_);

  if (render_now || room_preview_.room_id() != room_id) {
    const auto live = live_rooms_.find(room_id);
    auto* room = live == live_rooms_.end() ? nullptr : live->second.room;
    const auto status = room_preview_.Render(rom_, game_data_, room_id,
                                             Snapshot(), objects_only_, room);
    if (room)
      live_room_revisions_[room_id] = room->composite_source_revision();
    status_ = status.ok() ? std::string() : std::string(status.message());
    if (!status.ok()) {
      object_rects_.clear();
      return;
    }
    object_rects_ =
        room_preview_.ObjectRectsUsingSheet(state_->current_sheet_id);
    QueueTexture(room_preview_.mutable_bitmap());
  }
  const auto live = live_rooms_.find(room_id);
  const auto* context = live == live_rooms_.end() ? nullptr : &live->second;
  const auto blocks = room_preview_.blocks();
  const bool loads_sheet = std::find(blocks.begin(), blocks.end(),
                                     state_->current_sheet_id) != blocks.end();
  ImGui::TextDisabled("%s%s", usage_preview::RoomContextLabel(context).c_str(),
                      loads_sheet ? "" : " · Sheet not loaded");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(
        context ? context->provenance.c_str()
                : "ROM room header / default entrance (not live room edits)");
    ImGui::TextUnformatted(
        "Session sheet pixels and graphics-group assignments");
    ImGui::Text("%zu objects with traced references; %zu unknown/unattributed",
                object_rects_.size(), room_preview_.unknown_objects());
    ImGui::Text("BG sheets: %02X %02X %02X %02X %02X %02X %02X %02X", blocks[0],
                blocks[1], blocks[2], blocks[3], blocks[4], blocks[5],
                blocks[6], blocks[7]);
    ImGui::TextUnformatted(
        "Object traces only; doors, layout and sprites are unscanned.");
    ImGui::TextUnformatted(
        "Loaded sheets and traced references do not prove a visible pixel "
        "change.");
    ImGui::TextUnformatted(
        "Coverage can be partial. Unknown includes covered, custom or untraced "
        "drawing.");
    int shown = 0;
    for (const auto& use : room_preview_.tile_references()) {
      if (use.sheet != state_->current_sheet_id)
        continue;
      if (++shown > 12) {
        ImGui::TextDisabled("Additional traced references omitted here.");
        break;
      }
      ImGui::Text("Object %03X: slot %d, tile %03X, palette row %d, H:%d V:%d",
                  use.object_id, use.slot, use.tile_word & 0x3FF,
                  (use.tile_word >> 10) & 7, (use.tile_word >> 14) & 1,
                  (use.tile_word >> 15) & 1);
    }
    ImGui::EndTooltip();
  }
  DrawBitmapFit(room_preview_.mutable_bitmap(),
                outline_objects_ ? object_rects_ : std::vector<SDL_Rect>{},
                2.0f);
}

void UsagePreviewView::DrawSpriteContext(bool render_now, float delta) {
  if (spritesets_for_sheet_.empty()) {
    ImGui::TextDisabled("%s",
                        tr("No spriteset loads this sheet in slots 4-7."));
    return;
  }
  spriteset_choice_ = std::clamp(
      spriteset_choice_, 0, static_cast<int>(spritesets_for_sheet_.size()) - 1);
  const int spriteset = spritesets_for_sheet_[spriteset_choice_];
  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::BeginCombo(tr("Spriteset"),
                        absl::StrFormat("0x%02X", spriteset).c_str())) {
    for (int i = 0; i < static_cast<int>(spritesets_for_sheet_.size()); ++i) {
      if (ImGui::Selectable(
              absl::StrFormat("0x%02X", spritesets_for_sheet_[i]).c_str(),
              i == spriteset_choice_)) {
        spriteset_choice_ = i;
        configured_spriteset_ = -1;
      }
    }
    ImGui::EndCombo();
  }

  const bool spriteset_changed = configured_spriteset_ != spriteset;
  if (spriteset_changed || sprite_palette_revision_ != palette_revision_) {
    // Palette: the first dungeon room using this spriteset (header value +
    // 0x40), else the default sprite preview rows.
    int palette_room = -1;
    for (const auto& [room_id, info] : room_gfx_) {
      if (info.spriteset + zelda3::kDungeonSpritesetBase == spriteset) {
        palette_room = room_id;
        break;
      }
    }
    const auto status = sprite_preview_.Configure(rom_, game_data_, spriteset,
                                                  palette_room, hack_name_);
    status_ = status.ok() ? std::string() : std::string(status.message());
    if (spriteset_changed) {
      if (zsm_.has_value()) {
        sprite_preview_.SetZsm(*zsm_, zsm_animation_);
      } else {
        sprite_preview_.SetVanillaSprite(
            static_cast<uint8_t>(vanilla_sprite_id_));
      }
    }
    sprite_palette_revision_ = palette_revision_;
    configured_spriteset_ = spriteset;
    render_now = true;
  }

  ImGui::SetNextItemWidth(120.0f);
  uint8_t sprite_id = static_cast<uint8_t>(vanilla_sprite_id_);
  constexpr uint8_t kStep = 1;
  if (ImGui::InputScalar(tr("Vanilla sprite"), ImGuiDataType_U8, &sprite_id,
                         &kStep, nullptr, "%02X",
                         ImGuiInputTextFlags_CharsHexadecimal)) {
    vanilla_sprite_id_ = sprite_id;
    zsm_.reset();
    zsm_loaded_path_.clear();
    sprite_preview_.SetVanillaSprite(static_cast<uint8_t>(vanilla_sprite_id_));
    render_now = true;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%s", zelda3::ResolveSpriteName(
                                static_cast<uint16_t>(vanilla_sprite_id_)));

  ImGui::SetNextItemWidth(-80.0f);
  ImGui::InputTextWithHint("##ZsmPath", tr(".zsm path"), zsm_path_,
                           sizeof(zsm_path_));
  ImGui::SameLine();
  if (ImGui::Button(tr("Load"))) {
    zsprite::ZSprite zsm;
    const auto status = zsm.Load(zsm_path_);
    if (status.ok()) {
      zsm_ = std::move(zsm);
      zsm_loaded_path_ = zsm_path_;
      zsm_animation_ = 0;
      sprite_preview_.SetZsm(*zsm_, zsm_animation_);
      status_.clear();
    } else {
      status_ = std::string(status.message());
    }
    render_now = true;
  }
  if (zsm_.has_value() && !zsm_->animations.empty()) {
    zsm_animation_ = std::clamp(zsm_animation_, 0,
                                static_cast<int>(zsm_->animations.size()) - 1);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo(
            tr("Animation"),
            zsm_->animations[zsm_animation_].frame_name.c_str())) {
      for (int i = 0; i < static_cast<int>(zsm_->animations.size()); ++i) {
        if (ImGui::Selectable(zsm_->animations[i].frame_name.c_str(),
                              i == zsm_animation_)) {
          zsm_animation_ = i;
          sprite_preview_.SetZsm(*zsm_, zsm_animation_);
          render_now = true;
        }
      }
      ImGui::EndCombo();
    }
    if (ImGui::SmallButton(playing_ ? ICON_MD_PAUSE : ICON_MD_PLAY_ARROW)) {
      playing_ = !playing_;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_SKIP_PREVIOUS)) {
      playing_ = false;
      sprite_preview_.SetFrame(sprite_preview_.frame() - 1);
      render_now = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_SKIP_NEXT)) {
      playing_ = false;
      sprite_preview_.SetFrame(sprite_preview_.frame() + 1);
      render_now = true;
    }
    ImGui::SameLine();
    ImGui::Text(tr("Frame %d / %d"), sprite_preview_.frame(),
                sprite_preview_.frame_count());
    // Game speed: frame_speed ticks at 60 per second. Frame changes render
    // immediately; the pixel data comes from the live Arena sheets, so edits
    // show up without restarting playback.
    if (playing_ && sprite_preview_.Advance(delta)) {
      render_now = true;
    }
  } else {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled(
        "%s", tr("Vanilla sprites have one catalog pose; load a .zsm to "
                 "play its animations."));
    ImGui::PopTextWrapPos();
  }

  if (render_now || !sprite_bitmap_.is_active()) {
    ShowImage(sprite_bitmap_, sprite_preview_.RenderFrame(Snapshot()));
  }
  const auto& slots = sprite_preview_.slots();
  ImGui::TextDisabled(
      tr("OAM slots: %02X %02X %02X %02X | %02X %02X %02X %02X"), slots[0],
      slots[1], slots[2], slots[3], slots[4], slots[5], slots[6], slots[7]);
  DrawBitmapFit(sprite_bitmap_, {}, 4.0f);
}

void UsagePreviewView::DrawTile16Context(bool render_now) {
  const uint16_t sheet = state_->current_sheet_id;
  auto* live = live_overworld_provider_ ? live_overworld_provider_() : nullptr;
  if (live && (!live->is_loaded() || live->rom() != rom_))
    live = nullptr;
  std::vector<int> choices;
  if (live) {
    for (const auto& map : live->overworld_maps()) {
      if (!map.is_built() || map.game_data() != game_data_)
        continue;
      for (int slot = 0; slot < 8; ++slot) {
        if (map.static_graphics(slot) == sheet) {
          choices.push_back(map.index());
          break;
        }
      }
    }
    const int current = live->current_map_id();
    const auto preferred = std::find(choices.begin(), choices.end(), current);
    if (preferred != choices.end())
      std::rotate(choices.begin(), preferred, preferred + 1);
  }
  if (!areas_.has_value() && choices.empty()) {
    ImGui::TextDisabled(
        "%s", tr("Open an overworld area or load ROM area assignments."));
    if (ImGui::Button(tr("Load overworld areas"))) {
      areas_ = zelda3::CollectOverworldAreaGfx(*rom_, game_data_);
      last_sheet_ = 0xFFFF;
    }
    return;
  }
  if (!areas_ && !choices.empty() &&
      ImGui::SmallButton(tr("Load other ROM areas"))) {
    areas_ = zelda3::CollectOverworldAreaGfx(*rom_, game_data_);
  }
  if (areas_) {
    for (const auto& [area, info] : *areas_) {
      if (std::find(info.static_graphics.begin(),
                    info.static_graphics.begin() + 8,
                    sheet) != info.static_graphics.begin() + 8 &&
          std::find(choices.begin(), choices.end(), area) == choices.end()) {
        choices.push_back(area);
      }
    }
  }
  if (choices != areas_for_sheet_) {
    const int previous =
        area_choice_ >= 0 &&
                area_choice_ < static_cast<int>(areas_for_sheet_.size())
            ? areas_for_sheet_[area_choice_]
            : -1;
    areas_for_sheet_ = std::move(choices);
    const auto selected =
        std::find(areas_for_sheet_.begin(), areas_for_sheet_.end(), previous);
    area_choice_ = selected == areas_for_sheet_.end()
                       ? 0
                       : static_cast<int>(selected - areas_for_sheet_.begin());
    render_now = true;
  }
  if (areas_for_sheet_.empty()) {
    ImGui::TextDisabled("%s", tr("No overworld area loads this sheet."));
    return;
  }
  area_choice_ = std::clamp(area_choice_, 0,
                            static_cast<int>(areas_for_sheet_.size()) - 1);
  const int area = areas_for_sheet_[area_choice_];
  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::BeginCombo(tr("Area"), absl::StrFormat("0x%02X", area).c_str())) {
    for (int i = 0; i < static_cast<int>(areas_for_sheet_.size()); ++i) {
      if (ImGui::Selectable(
              absl::StrFormat("0x%02X", areas_for_sheet_[i]).c_str(),
              i == area_choice_)) {
        area_choice_ = i;
        render_now = true;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::Checkbox(tr("Follow selection"), &follow_selection_);
  HOVER_HINT("Use the 8x8 tile at the pixel editor's selection origin.");
  if (!follow_selection_ || !state_->selection.is_active) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt(tr("8x8 tile"), &tile_index_)) {
      tile_index_ = std::clamp(tile_index_, 0, 63);
      render_now = true;
    }
  }
  const int tile_index = SelectedTileIndex();

  const auto* live_map = live ? live->overworld_map(area) : nullptr;
  if (live_map &&
      (!live_map->is_built() || live_map->game_data() != game_data_))
    live_map = nullptr;
  if (live_map) {
    uint64_t hash = kFnvOffset;
    auto add = [&](uint64_t value) {
      hash = (hash ^ value) * kFnvPrime;
    };
    add(area);
    for (const auto& tile : live->tiles16())
      for (const auto& quadrant : tile.tiles_info)
        add(gfx::TileInfoToWord(quadrant));
    for (const auto id : live_map->tileset_key()) {
      add(id);
      add(game_data_->sheet_store.Revision(id));
    }
    for (const auto& color : live_map->current_palette())
      add(color.snes());
    if (!tile16_uses_live_source_ || tile16_preview_.area() != area ||
        live_tile16_hash_ != hash) {
      const auto status = tile16_preview_.ConfigureFromLive(
          *live_map, live->tiles16(), game_data_);
      status_ = status.ok() ? std::string() : std::string(status.message());
      if (!status.ok())
        return;
      tile16_uses_live_source_ = true;
      live_tile16_hash_ = hash;
      render_now = true;
    }
    ImGui::TextDisabled("%s", tr("Live overworld: unsaved Tile16 definitions"));
  } else {
    if (tile16_uses_live_source_ || tile16_preview_.area() != area) {
      const auto status = tile16_preview_.Configure(rom_, game_data_, area);
      status_ = status.ok() ? std::string() : std::string(status.message());
      if (!status.ok())
        return;
      tile16_uses_live_source_ = false;
      render_now = true;
    }
    ImGui::TextDisabled("%s",
                        tr("ROM definitions: open this area for live edits"));
  }
  const auto tile16s = tile16_preview_.Tile16sUsingTile(sheet, tile_index);
  if (tile16s != tile16s_) {
    tile16s_ = tile16s;
    render_now = true;
  }
  ImGui::Text(tr("8x8 tile 0x%02X: %zu tile16s"), tile_index, tile16s_.size());
  if (tile16s_.empty()) {
    return;
  }
  if (render_now || !tile16_bitmap_.is_active()) {
    tile16_highlights_.clear();
    ShowImage(tile16_bitmap_,
              tile16_preview_.Render(Snapshot(), tile16s_, sheet, tile_index,
                                     &tile16_highlights_));
  }
  DrawBitmapFit(tile16_bitmap_, tile16_highlights_, 4.0f);
}

}  // namespace yaze::editor
