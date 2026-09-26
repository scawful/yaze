#include "app/editor/graphics/usage_preview.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/sprite_editor_internal.h"
#include "app/gfx/resource/arena.h"
#include "app/platform/sdl_compat.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/sprite/sprite_oam_tables.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor::usage_preview {

namespace {

constexpr int kTilesPerSheet = 64;

// Copies 4096 sheet bytes into an 8bpp buffer laid out like Room's and
// OverworldMap's (16 tiles per 128-pixel row, one sheet per 0x1000 bytes).
void CopySheet(const std::vector<uint8_t>& sheet, std::vector<uint8_t>& dst,
               size_t block) {
  if (sheet.size() == kSheetBytes && (block + 1) * kSheetBytes <= dst.size()) {
    std::copy(sheet.begin(), sheet.end(), dst.begin() + block * kSheetBytes);
  }
}

}  // namespace

SheetPixels SnapshotArenaSheets(const std::set<uint16_t>& ids,
                                const zelda3::GameData* game_data) {
  SheetPixels out;
  auto& arena = gfx::Arena::Get();
  if (game_data != nullptr && arena.gfx_sheets_owner() != nullptr &&
      arena.gfx_sheets_owner() != game_data) {
    return out;
  }
  const auto& sheets = arena.gfx_sheets();
  for (const uint16_t id : ids) {
    if (id >= sheets.size()) {
      continue;
    }
    const auto& sheet = sheets[id];
    if (sheet.width() == 128 && sheet.height() == 32 &&
        sheet.vector().size() == kSheetBytes) {
      out[id] = sheet.vector();
    }
  }
  return out;
}

std::vector<uint8_t> IndexedImage::ToRgba() const {
  std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4, 0);
  for (size_t i = 0; i < pixels.size() && i * 4 + 3 < rgba.size(); ++i) {
    const uint8_t index = pixels[i];
    if (index0_transparent && index == 0) {
      continue;
    }
    const SDL_Color& c = colors[index];
    rgba[i * 4 + 0] = c.r;
    rgba[i * 4 + 1] = c.g;
    rgba[i * 4 + 2] = c.b;
    rgba[i * 4 + 3] = 255;
  }
  return rgba;
}

int IndexedImage::CountDifferentPixels(const IndexedImage& other) const {
  if (width != other.width || height != other.height ||
      pixels.size() != other.pixels.size()) {
    return -1;
  }
  int count = 0;
  for (size_t i = 0; i < pixels.size(); ++i) {
    count += pixels[i] != other.pixels[i] ? 1 : 0;
  }
  return count;
}

IndexedImage SnapshotBitmap(const gfx::Bitmap& bitmap) {
  IndexedImage image;
  if (!bitmap.is_active() || bitmap.width() <= 0 || bitmap.height() <= 0) {
    return image;
  }
  image.width = bitmap.width();
  image.height = bitmap.height();
  image.pixels = bitmap.vector();
  image.pixels.resize(static_cast<size_t>(image.width) * image.height, 0);
  if (SDL_Palette* palette = platform::GetSurfacePalette(bitmap.surface())) {
    for (int i = 0; i < palette->ncolors && i < 256; ++i) {
      image.colors[i] = palette->colors[i];
    }
  }
  return image;
}

// ---------------------------------------------------------------------------
// Room context
// ---------------------------------------------------------------------------

std::map<int, std::array<uint8_t, 16>> CollectRoomSheetBlocks(
    Rom* rom, zelda3::GameData* game_data,
    const std::map<int, zelda3::RoomGfxInfo>& rooms) {
  std::map<int, std::array<uint8_t, 16>> out;
  if (rom == nullptr || game_data == nullptr) {
    return out;
  }
  for (const auto& [room_id, info] : rooms) {
    zelda3::Room room(room_id, rom, game_data);
    room.SetBlockset(info.blockset);
    room.SetSpriteset(info.spriteset);
    room.LoadRoomGraphics();
    std::array<uint8_t, 16> blocks{};
    const auto& room_blocks = room.blocks();
    std::copy_n(room_blocks.begin(), 16, blocks.begin());
    out[room_id] = blocks;
  }
  return out;
}

std::vector<int> RoomsUsingSheet(
    const std::map<int, std::array<uint8_t, 16>>& room_blocks, uint16_t sheet,
    bool include_sprites) {
  std::vector<int> rooms;
  for (const auto& [room_id, blocks] : room_blocks) {
    const int end = include_sprites ? 16 : 8;
    if (std::find(blocks.begin(), blocks.begin() + end, sheet) !=
        blocks.begin() + end) {
      rooms.push_back(room_id);
    }
  }
  return rooms;
}

absl::Status RoomUsagePreview::Render(Rom* rom, zelda3::GameData* game_data,
                                      int room_id, const SheetPixels& sheets,
                                      bool objects_only) {
  if (rom == nullptr || !rom->is_loaded() || game_data == nullptr) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room 0x%03X", room_id));
  }
  if (!room_ || room_id_ != room_id) {
    room_ =
        std::make_unique<zelda3::Room>(zelda3::LoadRoomFromRom(rom, room_id));
    room_->SetGameData(game_data);
    room_->LoadRoomGraphics();
    room_id_ = room_id;
  }
  // Only the sheets this room loads matter; others would just cost copies.
  SheetPixels used;
  for (const uint8_t block : room_->blocks()) {
    if (const auto it = sheets.find(block); it != sheets.end()) {
      used.emplace(block, it->second);
    }
  }
  room_->SetGraphicsSheetOverrides(std::move(used));
  room_->RenderRoomGraphics();

  zelda3::RoomLayerManager layers;
  layers.ApplyLayerMerging(room_->layer_merging());
  layers.ApplyRoomEffect(room_->effect());
  layers.ApplyGameLayerRegisters(room_->GameLayerRegisters());
  if (objects_only) {
    layers.SetLayerVisible(zelda3::LayerType::BG1_Layout, false);
    layers.SetLayerVisible(zelda3::LayerType::BG2_Layout, false);
  }
  room_->RenderComposite(layers, composite_);
  if (!composite_.is_active() || composite_.width() <= 0) {
    return absl::InternalError("Room composite is empty after render");
  }
  return absl::OkStatus();
}

std::array<uint8_t, 16> RoomUsagePreview::blocks() const {
  std::array<uint8_t, 16> out{};
  if (room_) {
    const auto& blocks = room_->blocks();
    std::copy_n(blocks.begin(), 16, out.begin());
  }
  return out;
}

std::vector<SDL_Rect> RoomUsagePreview::ObjectRectsUsingSheet(
    uint16_t sheet) const {
  std::vector<SDL_Rect> rects;
  if (!room_) {
    return rects;
  }
  const auto blocks = this->blocks();
  for (const auto& object : room_->GetTileObjects()) {
    auto tiles = object.GetTiles();
    if (!tiles.ok()) {
      continue;
    }
    const bool uses_sheet =
        std::any_of(tiles->begin(), tiles->end(), [&](const gfx::TileInfo& t) {
          const int block = t.id_ / kTilesPerSheet;
          return block < 8 && blocks[block] == sheet;
        });
    if (!uses_sheet) {
      continue;
    }
    // Same absolute pixel footprint the dungeon editor selects.
    const auto [x, y, w, h] =
        zelda3::DimensionService::Get().GetSelectionBoundsPixels(object);
    rects.push_back({x, y, w, h});
  }
  return rects;
}

void RoomUsagePreview::Reset() {
  room_.reset();
  room_id_ = -1;
}

// ---------------------------------------------------------------------------
// Sprite context
// ---------------------------------------------------------------------------

absl::Status SpriteUsagePreview::Configure(Rom* rom,
                                           zelda3::GameData* game_data,
                                           int spriteset, int palette_room,
                                           std::string hack_name) {
  if (rom == nullptr || !rom->is_loaded() || game_data == nullptr) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (spriteset < 0 ||
      spriteset >= static_cast<int>(game_data->spriteset_ids.size())) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid spriteset 0x%02X", spriteset));
  }
  rom_ = rom;
  game_data_ = game_data;
  hack_name_ = std::move(hack_name);
  slots_ = zelda3::SpriteSheetSlots(game_data->spriteset_ids[spriteset],
                                    zelda3::IsUnderworldSpriteset(spriteset));

  colors_ = {};
  if (palette_room >= 0) {
    const auto room = zelda3::LoadRoomHeaderFromRom(rom, palette_room);
    colors_ = zelda3::BuildDungeonSpriteRenderPalette(room, game_data);
  } else {
    const auto& groups = game_data->palette_groups;
    const auto palettes = internal::DefaultSpritePreviewPalettes(
        groups.global_sprites, groups.sprites_aux1, groups.sprites_aux2,
        groups.sprites_aux3);
    for (size_t row = 0; row < palettes.size() && row < 8; ++row) {
      const auto& palette = palettes.palette_ref(row);
      for (size_t i = 0; i < palette.size() && i < 16; ++i) {
        const auto rgb = palette[i].rgb();
        colors_[(8 + row) * 16 + i] =
            SDL_Color{static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                      static_cast<uint8_t>(rgb.z), 255};
      }
    }
  }
  return absl::OkStatus();
}

void SpriteUsagePreview::SetVanillaSprite(uint8_t sprite_id) {
  zsm_.reset();
  sprite_id_ = sprite_id;
  frame_ = 0;
  remainder_ = 0.0f;
}

void SpriteUsagePreview::SetZsm(const zsprite::ZSprite& zsm, int animation) {
  zsm_ = zsm;
  animation_ = std::clamp<int>(
      animation, 0,
      std::max<int>(0, static_cast<int>(zsm.animations.size()) - 1));
  frame_ = zsm.animations.empty() ? 0 : zsm.animations[animation_].frame_start;
  remainder_ = 0.0f;
}

int SpriteUsagePreview::frame_count() const {
  if (!zsm_.has_value()) {
    return 1;
  }
  return static_cast<int>(zsm_->editor.Frames.size());
}

void SpriteUsagePreview::SetFrame(int frame) {
  frame_ = std::clamp(frame, 0, std::max(0, frame_count() - 1));
  remainder_ = 0.0f;
}

bool SpriteUsagePreview::Advance(float seconds) {
  if (!zsm_.has_value() || zsm_->animations.empty()) {
    return false;
  }
  return sprite_authoring::Advance(zsm_->animations[animation_],
                                   zsm_->editor.Frames.size(), seconds, frame_,
                                   remainder_);
}

bool SpriteUsagePreview::UsesSheet(uint16_t sheet) const {
  return std::find(slots_.begin(), slots_.end(), sheet) != slots_.end();
}

std::vector<uint8_t> SpriteUsagePreview::SheetOrRom(const SheetPixels& sheets,
                                                    uint16_t sheet) const {
  if (const auto it = sheets.find(sheet); it != sheets.end()) {
    return it->second;
  }
  if (game_data_ != nullptr) {
    const auto& buffer = game_data_->graphics_buffer;
    const size_t offset = static_cast<size_t>(sheet) * kSheetBytes;
    if (offset + kSheetBytes <= buffer.size()) {
      return {buffer.begin() + offset, buffer.begin() + offset + kSheetBytes};
    }
  }
  return {};
}

IndexedImage SpriteUsagePreview::RenderFrame(const SheetPixels& sheets) const {
  IndexedImage image;
  image.colors = colors_;
  image.index0_transparent = true;
  if (game_data_ == nullptr) {
    return image;
  }

  if (zsm_.has_value()) {
    // SpriteDrawer reads OAM tiles 0x000-0x1FF from eight sheets in a row,
    // the same buffer SpriteEditor::LoadSpriteGraphicsBuffer builds.
    std::vector<uint8_t> buffer(0x10000, 0);
    for (size_t slot = 0; slot < slots_.size(); ++slot) {
      CopySheet(SheetOrRom(sheets, slots_[slot]), buffer, slot);
    }
    constexpr int kCanvas = 128;
    gfx::Bitmap canvas(kCanvas, kCanvas, 8,
                       std::vector<uint8_t>(kCanvas * kCanvas, 0));
    SpriteDrawer drawer(buffer.data());
    if (frame_ >= 0 && frame_ < frame_count()) {
      drawer.DrawFrame(canvas, zsm_->editor.Frames[frame_], kCanvas / 2,
                       kCanvas / 2);
    }
    image.width = kCanvas;
    image.height = kCanvas;
    image.pixels = canvas.vector();
    // SpriteDrawer writes OAM palette rows 0-7; CGRAM holds them at 8-15.
    for (auto& pixel : image.pixels) {
      if (pixel != 0) {
        pixel = static_cast<uint8_t>(pixel | 0x80);
      }
    }
    return image;
  }

  // Sprite::RenderPreviewGraphics reads a room-style buffer: sprite sheets
  // in blocks 8-15 (8x8 tile 512 onward), the order Room::LoadRoomGraphics
  // and SpriteSheetSlots share.
  std::vector<uint8_t> buffer(0x10000, 0);
  for (size_t slot = 0; slot < slots_.size(); ++slot) {
    CopySheet(SheetOrRom(sheets, slots_[slot]), buffer, 8 + slot);
  }
  zelda3::Sprite sprite(sprite_id_, 0, 0, 0, 0);
  const auto* layout =
      zelda3::SpriteOamRegistry::GetPreviewOverride(sprite_id_, hack_name_);
  sprite.RenderPreviewGraphics(buffer, layout);
  const SDL_Rect bounds = sprite.preview_bounds();
  const auto* pixels = sprite.preview_graphics();
  if (pixels == nullptr || bounds.w <= 0 || bounds.h <= 0 ||
      pixels->size() < static_cast<size_t>(bounds.w * bounds.h)) {
    return image;
  }
  image.width = bounds.w;
  image.height = bounds.h;
  image.pixels.assign(pixels->begin(), pixels->begin() + bounds.w * bounds.h);
  return image;
}

// ---------------------------------------------------------------------------
// Tile16 context
// ---------------------------------------------------------------------------

absl::Status Tile16UsagePreview::Configure(Rom* rom,
                                           zelda3::GameData* game_data,
                                           int area) {
  if (rom == nullptr || !rom->is_loaded() || game_data == nullptr) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (area < 0 || area >= zelda3::kNumOverworldAreas) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid overworld area 0x%02X", area));
  }
  if (tiles16_.empty() || rom_ != rom) {
    auto tiles16 = zelda3::ReadMap16Tiles(*rom);
    if (!tiles16.ok()) {
      return tiles16.status();
    }
    tiles16_ = std::move(*tiles16);
  }
  rom_ = rom;
  game_data_ = game_data;
  if (!map_ || area_ != area) {
    map_ = std::make_unique<zelda3::OverworldMap>(area, rom, game_data);
    map_->set_game_state(0);
    map_->LoadAreaGraphics();
    if (auto status = map_->LoadPalette(); !status.ok()) {
      map_.reset();
      return status;
    }
    area_ = area;
  }
  return absl::OkStatus();
}

std::vector<int> Tile16UsagePreview::TileIdsFor(uint16_t sheet,
                                                int tile_index) const {
  std::vector<int> ids;
  if (!map_ || tile_index < 0 || tile_index >= kTilesPerSheet) {
    return ids;
  }
  // BuildTileset fills slots 0-7 (0x1000 bytes, 64 tiles each) from
  // static_graphics; tile16s address that buffer with 10-bit tile ids.
  for (int slot = 0; slot < 8; ++slot) {
    if (map_->static_graphics(slot) == sheet) {
      ids.push_back(slot * kTilesPerSheet + tile_index);
    }
  }
  return ids;
}

std::vector<int> Tile16UsagePreview::Tile16sUsingTile(uint16_t sheet,
                                                      int tile_index) const {
  std::vector<int> result;
  const auto ids = TileIdsFor(sheet, tile_index);
  if (ids.empty()) {
    return result;
  }
  for (size_t i = 0; i < tiles16_.size(); ++i) {
    const auto& info = tiles16_[i].tiles_info;
    if (std::any_of(info.begin(), info.end(), [&](const gfx::TileInfo& t) {
          return std::find(ids.begin(), ids.end(), t.id_) != ids.end();
        })) {
      result.push_back(static_cast<int>(i));
    }
  }
  return result;
}

IndexedImage Tile16UsagePreview::Render(const SheetPixels& sheets,
                                        const std::vector<int>& tile16s,
                                        uint16_t sheet, int tile_index,
                                        std::vector<SDL_Rect>* highlights) {
  IndexedImage image;
  image.index0_transparent = false;
  if (!map_ || tile16s.empty()) {
    return image;
  }
  map_->SetGraphicsSheetOverrides(sheets);
  if (!map_->BuildTileset().ok() ||
      !map_->BuildTiles16Gfx(tiles16_, static_cast<int>(tiles16_.size()))
           .ok()) {
    return image;
  }
  const auto& blockset = map_->current_tile16_blockset();
  const auto& palette = map_->current_palette();
  for (size_t i = 0; i < palette.size() && i < 256; ++i) {
    const auto rgb = palette[i].rgb();
    image.colors[i] =
        SDL_Color{static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                  static_cast<uint8_t>(rgb.z), 255};
  }

  constexpr int kPerRow = 8;
  const int rows = (static_cast<int>(tile16s.size()) + kPerRow - 1) / kPerRow;
  image.width = kPerRow * 16;
  image.height = rows * 16;
  image.pixels.assign(static_cast<size_t>(image.width) * image.height, 0);
  const auto ids = TileIdsFor(sheet, tile_index);
  constexpr int kQuadrantX[] = {0, 8, 0, 8};
  constexpr int kQuadrantY[] = {0, 0, 8, 8};
  for (size_t n = 0; n < tile16s.size(); ++n) {
    const int tile16 = tile16s[n];
    // BuildTiles16Gfx lays tile16s out 8 per 128-pixel row.
    const int src_x = (tile16 % kPerRow) * 16;
    const int src_y = (tile16 / kPerRow) * 16;
    const int dst_x = static_cast<int>(n % kPerRow) * 16;
    const int dst_y = static_cast<int>(n / kPerRow) * 16;
    for (int y = 0; y < 16; ++y) {
      const size_t src = static_cast<size_t>(src_y + y) * 128 + src_x;
      if (src + 16 > blockset.size()) {
        break;
      }
      std::copy_n(blockset.begin() + src, 16,
                  image.pixels.begin() + (dst_y + y) * image.width + dst_x);
    }
    if (highlights != nullptr && tile16 >= 0 &&
        tile16 < static_cast<int>(tiles16_.size())) {
      for (int q = 0; q < 4; ++q) {
        const int id = tiles16_[tile16].tiles_info[q].id_;
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
          highlights->push_back(
              {dst_x + kQuadrantX[q], dst_y + kQuadrantY[q], 8, 8});
        }
      }
    }
  }
  return image;
}

}  // namespace yaze::editor::usage_preview
