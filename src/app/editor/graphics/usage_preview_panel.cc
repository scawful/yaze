#include "app/editor/graphics/usage_preview_panel.h"

#include <algorithm>
#include <set>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "util/macro.h"
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

void UsagePreviewPanel::SetRom(Rom* rom) {
  if (rom != rom_) {
    rom_ = rom;
    InvalidateTables();
  }
}

void UsagePreviewPanel::SetGameData(zelda3::GameData* game_data) {
  if (game_data != game_data_) {
    game_data_ = game_data;
    InvalidateTables();
  }
}

void UsagePreviewPanel::InvalidateTables() {
  tables_ready_ = false;
  room_blocks_.clear();
  room_gfx_.clear();
  areas_.reset();
  room_preview_.Reset();
  configured_spriteset_ = -1;
  last_sheet_ = 0xFFFF;
  dirty_ = true;
}

void UsagePreviewPanel::EnsureTables() {
  if (tables_ready_ || rom_ == nullptr || !rom_->is_loaded() ||
      game_data_ == nullptr) {
    return;
  }
  room_gfx_ = zelda3::CollectRoomGfx(*rom_);
  room_blocks_ =
      usage_preview::CollectRoomSheetBlocks(rom_, game_data_, room_gfx_);
  tables_ready_ = true;
}

uint64_t UsagePreviewPanel::HashWatchedSheets() const {
  uint64_t hash = kFnvOffset;
  std::set<uint16_t> ids = state_->modified_sheets;
  ids.insert(state_->current_sheet_id);
  const auto sheets = usage_preview::SnapshotArenaSheets(ids, game_data_);
  for (const auto& [id, pixels] : sheets) {
    hash = (hash ^ id) * kFnvPrime;
    hash = HashBytes(hash, pixels);
  }
  return hash;
}

usage_preview::SheetPixels UsagePreviewPanel::Snapshot() const {
  std::set<uint16_t> ids = state_->modified_sheets;
  ids.insert(state_->current_sheet_id);
  return usage_preview::SnapshotArenaSheets(ids, game_data_);
}

int UsagePreviewPanel::SelectedTileIndex() const {
  const auto& selection = state_->selection;
  if (follow_selection_ && selection.is_active) {
    const int tile = (selection.y / 8) * 16 + selection.x / 8;
    return std::clamp(tile, 0, 63);
  }
  return tile_index_;
}

void UsagePreviewPanel::ShowImage(gfx::Bitmap& bitmap,
                                  const usage_preview::IndexedImage& image) {
  if (image.empty()) {
    return;
  }
  std::vector<SDL_Color> colors(image.colors.begin(), image.colors.end());
  if (image.index0_transparent) {
    colors[0].a = 0;
  }
  if (bitmap.width() != image.width || bitmap.height() != image.height ||
      !bitmap.is_active()) {
    bitmap.Create(image.width, image.height, 8, image.pixels);
  } else {
    bitmap.set_data(image.pixels);
  }
  bitmap.SetPalette(colors);
  QueueTexture(bitmap);
}

void UsagePreviewPanel::DrawBitmapFit(gfx::Bitmap& bitmap,
                                      const std::vector<SDL_Rect>& rects,
                                      float max_scale) {
  if (!bitmap.is_active() || bitmap.width() <= 0 || !bitmap.texture()) {
    ImGui::TextDisabled("%s", tr("Rendering..."));
    return;
  }
  const float avail = std::max(64.0f, ImGui::GetContentRegionAvail().x);
  const float scale =
      std::clamp(avail / static_cast<float>(bitmap.width()), 0.5f, max_scale);
  const ImVec2 size(bitmap.width() * scale, bitmap.height() * scale);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Image((ImTextureID)(intptr_t)bitmap.texture(), size);
  auto* draw_list = ImGui::GetWindowDrawList();
  const ImU32 color = ImGui::GetColorU32(gui::GetWarningColor());
  for (const auto& r : rects) {
    draw_list->AddRect(
        ImVec2(origin.x + r.x * scale, origin.y + r.y * scale),
        ImVec2(origin.x + (r.x + r.w) * scale, origin.y + (r.y + r.h) * scale),
        color, 0.0f, 0, 1.5f);
  }
}

void UsagePreviewPanel::Draw() {
  if (rom_ == nullptr || !rom_->is_loaded() || game_data_ == nullptr) {
    ImGui::TextDisabled("%s", tr("Load a ROM to preview sheet usage."));
    return;
  }
  EnsureTables();
  const uint16_t sheet = state_->current_sheet_id;
  if (sheet != last_sheet_) {
    last_sheet_ = sheet;
    rooms_for_sheet_ =
        usage_preview::RoomsUsingSheet(room_blocks_, sheet, true);
    room_choice_ = 0;
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
  }
  HOVER_HINT("Render now");

  // Throttle: re-render when the watched pixels changed, no more than every
  // kMinRenderInterval, and always once the mouse is released (stroke end).
  const uint64_t hash = live_ ? HashWatchedSheets() : last_hash_;
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

void UsagePreviewPanel::DrawRoomContext(bool render_now) {
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
    const auto status = room_preview_.Render(rom_, game_data_, room_id,
                                             Snapshot(), objects_only_);
    status_ = status.ok() ? std::string() : std::string(status.message());
    object_rects_ =
        room_preview_.ObjectRectsUsingSheet(state_->current_sheet_id);
    QueueTexture(room_preview_.mutable_bitmap());
  }
  const auto blocks = room_preview_.blocks();
  ImGui::TextDisabled(tr("BG sheets: %02X %02X %02X %02X %02X %02X %02X %02X"),
                      blocks[0], blocks[1], blocks[2], blocks[3], blocks[4],
                      blocks[5], blocks[6], blocks[7]);
  DrawBitmapFit(room_preview_.mutable_bitmap(),
                outline_objects_ ? object_rects_ : std::vector<SDL_Rect>{},
                2.0f);
}

void UsagePreviewPanel::DrawSpriteContext(bool render_now, float delta) {
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

  if (configured_spriteset_ != spriteset) {
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
    if (zsm_.has_value()) {
      sprite_preview_.SetZsm(*zsm_, zsm_animation_);
    } else {
      sprite_preview_.SetVanillaSprite(
          static_cast<uint8_t>(vanilla_sprite_id_));
    }
    configured_spriteset_ = spriteset;
    render_now = true;
  }

  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::InputInt(tr("Vanilla sprite"), &vanilla_sprite_id_, 1, 16,
                      ImGuiInputTextFlags_CharsHexadecimal)) {
    vanilla_sprite_id_ = std::clamp(vanilla_sprite_id_, 0, 0xFF);
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
    ImGui::TextDisabled(
        "%s", tr("Vanilla sprites have one catalog pose; load a .zsm to "
                 "play its animations."));
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

void UsagePreviewPanel::DrawTile16Context(bool render_now) {
  const uint16_t sheet = state_->current_sheet_id;
  if (!areas_.has_value()) {
    ImGui::TextDisabled(
        "%s", tr("Reads all 160 overworld areas' graphics (about a second)."));
    if (ImGui::Button(tr("Load overworld areas"))) {
      areas_ = zelda3::CollectOverworldAreaGfx(*rom_, game_data_);
      last_sheet_ = 0xFFFF;  // recompute the per-sheet lists
    }
    return;
  }
  if (areas_for_sheet_.empty()) {
    for (const auto& [area, info] : *areas_) {
      if (std::find(info.static_graphics.begin(),
                    info.static_graphics.begin() + 8,
                    sheet) != info.static_graphics.begin() + 8) {
        areas_for_sheet_.push_back(area);
      }
    }
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

  if (tile16_preview_.area() != area) {
    const auto status = tile16_preview_.Configure(rom_, game_data_, area);
    status_ = status.ok() ? std::string() : std::string(status.message());
    render_now = true;
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
