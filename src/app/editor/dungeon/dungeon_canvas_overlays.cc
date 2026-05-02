#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/screen_editor_internal.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/layout_helpers.h"
#include "dungeon_canvas_viewer.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

constexpr int kSpritePreviewSize = 64;
constexpr int kSpritePreviewAnchor = 16;
constexpr int kSpritePaletteRowsStart = 8;
constexpr int kPaletteRowSize = 16;

struct DungeonSpriteColorTable {
  std::array<ImU32, 256> colors{};
  bool has_sprite_colors = false;
};

ImU32 ToImU32(const gfx::SnesColor& color) {
  const auto rgb = color.rom_color();
  return IM_COL32(static_cast<int>(rgb.red), static_cast<int>(rgb.green),
                  static_cast<int>(rgb.blue), 255);
}

void CopyPaletteRange(std::array<ImU32, 256>& colors, bool& has_colors, int row,
                      int start_column, const gfx::SnesPalette& palette,
                      size_t max_colors) {
  const size_t count = std::min(max_colors, palette.size());
  for (size_t i = 0; i < count; ++i) {
    const int column = start_column + static_cast<int>(i);
    const int index = row * kPaletteRowSize + column;
    if (index < 0 || index >= static_cast<int>(colors.size())) {
      continue;
    }
    colors[static_cast<size_t>(index)] = ToImU32(palette[i]);
    has_colors = true;
  }
}

DungeonSpriteColorTable BuildDungeonSpriteColorTable(
    const zelda3::GameData* game_data) {
  DungeonSpriteColorTable table;
  if (!game_data) {
    return table;
  }

  const auto& groups = game_data->palette_groups;

  if (groups.sprites_aux1.size() > 1) {
    CopyPaletteRange(table.colors, table.has_sprite_colors,
                     kSpritePaletteRowsStart, 1,
                     groups.sprites_aux1.palette_ref(1), 7);
  }
  if (!groups.sprites_aux3.empty()) {
    CopyPaletteRange(table.colors, table.has_sprite_colors,
                     kSpritePaletteRowsStart, 9,
                     groups.sprites_aux3.palette_ref(0), 7);
  }

  if (!groups.global_sprites.empty()) {
    const auto& global = groups.global_sprites.palette_ref(0);
    size_t source_index = 0;
    for (int row = 9; row <= 12; ++row) {
      for (int column = 1; column < kPaletteRowSize; ++column) {
        if (source_index >= global.size()) {
          break;
        }
        table.colors[static_cast<size_t>(row * kPaletteRowSize + column)] =
            ToImU32(global[source_index++]);
        table.has_sprite_colors = true;
      }
    }
  }

  if (!groups.sprites_aux1.empty()) {
    CopyPaletteRange(table.colors, table.has_sprite_colors, 13, 1,
                     groups.sprites_aux1.palette_ref(0), 7);
  }
  if (!groups.sprites_aux2.empty()) {
    CopyPaletteRange(table.colors, table.has_sprite_colors, 14, 1,
                     groups.sprites_aux2.palette_ref(0), 7);
  }
  if (!groups.armors.empty()) {
    CopyPaletteRange(table.colors, table.has_sprite_colors, 15, 1,
                     groups.armors.palette_ref(0), 15);
  }

  return table;
}

bool DrawSpritePreviewPixels(const gui::CanvasRuntime& rt,
                             const std::vector<uint8_t>& preview, int room_x,
                             int room_y,
                             const DungeonSpriteColorTable& color_table) {
  if (!rt.draw_list ||
      preview.size() < kSpritePreviewSize * kSpritePreviewSize ||
      !color_table.has_sprite_colors) {
    return false;
  }

  const float scale = std::max(rt.scale, 0.01f);
  const ImVec2 origin(rt.canvas_p0.x + rt.scrolling.x,
                      rt.canvas_p0.y + rt.scrolling.y);
  const ImVec2 canvas_max(rt.canvas_p0.x + rt.canvas_sz.x,
                          rt.canvas_p0.y + rt.canvas_sz.y);
  bool drew_any_pixel = false;

  rt.draw_list->PushClipRect(rt.canvas_p0, canvas_max, true);
  for (int y = 0; y < kSpritePreviewSize; ++y) {
    int x = 0;
    while (x < kSpritePreviewSize) {
      const uint8_t palette_index =
          preview[static_cast<size_t>(y * kSpritePreviewSize + x)];
      const ImU32 color = color_table.colors[palette_index];
      if (palette_index == 0xFF || color == 0) {
        ++x;
        continue;
      }

      const int start_x = x;
      while (x < kSpritePreviewSize &&
             preview[static_cast<size_t>(y * kSpritePreviewSize + x)] ==
                 palette_index) {
        ++x;
      }

      const ImVec2 min(origin.x + (room_x + start_x) * scale,
                       origin.y + (room_y + y) * scale);
      const ImVec2 max(origin.x + (room_x + x) * scale,
                       origin.y + (room_y + y + 1) * scale);
      rt.draw_list->AddRectFilled(min, max, color);
      drew_any_pixel = true;
    }
  }
  rt.draw_list->PopClipRect();

  return drew_any_pixel;
}

const char* GetObjectStreamLabel(int layer_value) {
  switch (layer_value) {
    case 0:
      return "L1 Primary";
    case 1:
      return "L2 BG2";
    case 2:
      return "L3 BG1";
    default:
      return "Unknown";
  }
}

}  // namespace

void DungeonCanvasViewer::RenderSprites(const gui::CanvasRuntime& rt,
                                        const zelda3::Room& room) {
  if (!entity_visibility_.show_sprites) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
  const int entity_size = is_touch ? 24 : 16;
  const DungeonSpriteColorTable sprite_colors =
      BuildDungeonSpriteColorTable(game_data_);
  const auto& room_gfx = room.get_gfx_buffer();
  const std::span<const uint8_t> room_gfx_span(room_gfx.data(),
                                               room_gfx.size());

  for (const auto& sprite : room.GetSprites()) {
    const int canvas_x = sprite.x() * 16;
    const int canvas_y = sprite.y() * 16;

    if (canvas_x >= -entity_size && canvas_y >= -entity_size &&
        canvas_x < 512 + entity_size && canvas_y < 512 + entity_size) {
      ImVec4 sprite_color = sprite.layer() == 0 ? theme.dungeon_sprite_layer0
                                                : theme.dungeon_sprite_layer1;

      zelda3::Sprite preview_sprite = sprite;
      preview_sprite.RenderPreviewGraphics(room_gfx_span);
      const auto* preview = preview_sprite.preview_graphics();
      const bool drew_preview =
          preview && DrawSpritePreviewPixels(
                         rt, *preview, canvas_x - kSpritePreviewAnchor,
                         canvas_y - kSpritePreviewAnchor, sprite_colors);

      if (drew_preview) {
        gui::DrawOutline(rt, canvas_x, canvas_y, entity_size, entity_size,
                         ImGui::GetColorU32(sprite_color));
      } else {
        gui::DrawRect(rt, canvas_x, canvas_y, entity_size, entity_size,
                      sprite_color);
      }

      std::string full_name = zelda3::GetSpriteLabel(sprite.id());
      std::string sprite_text;
      if (full_name.length() > 12) {
        sprite_text = absl::StrFormat("%02X %s..", sprite.id(),
                                      full_name.substr(0, 8).c_str());
      } else {
        sprite_text =
            absl::StrFormat("%02X %s", sprite.id(), full_name.c_str());
      }

      gui::DrawText(rt, sprite_text, canvas_x, canvas_y);
    }
  }
}

void DungeonCanvasViewer::RenderPotItems(const gui::CanvasRuntime& rt,
                                         const zelda3::Room& room) {
  if (!entity_visibility_.show_pot_items) {
    return;
  }

  const auto& pot_items = room.GetPotItems();
  if (pot_items.empty()) {
    return;
  }

  static const char* kPotItemNames[] = {
      "Nothing",       "Green Rupee", "Rock",         "Bee",        "Health",
      "Bomb",          "Heart",       "Blue Rupee",   "Key",        "Arrow",
      "Bomb",          "Heart",       "Magic",        "Full Magic", "Cucco",
      "Green Soldier", "Bush Stal",   "Blue Soldier", "Landmine",   "Heart",
      "Fairy",         "Heart",       "Nothing",      "Hole",       "Warp",
      "Staircase",     "Bombable",    "Switch"};
  constexpr size_t kPotItemNameCount =
      sizeof(kPotItemNames) / sizeof(kPotItemNames[0]);

  for (const auto& pot_item : pot_items) {
    const int pixel_x = pot_item.GetPixelX();
    const int pixel_y = pot_item.GetPixelY();
    auto [canvas_x, canvas_y] =
        DungeonRenderingHelpers::RoomToCanvasCoordinates(pixel_x / 8,
                                                         pixel_y / 8);

    const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
    const int entity_size = is_touch ? 24 : 16;

    if (canvas_x >= -entity_size && canvas_y >= -entity_size &&
        canvas_x < 512 + entity_size && canvas_y < 512 + entity_size) {
      const auto& theme = AgentUI::GetTheme();
      ImVec4 pot_item_color;
      if (pot_item.item == 0) {
        pot_item_color = theme.status_inactive;
        pot_item_color.w = 0.4f;
      } else {
        pot_item_color = theme.item_color;
        pot_item_color.w = 0.75f;
      }

      gui::DrawRect(rt, canvas_x, canvas_y, entity_size, entity_size,
                    pot_item_color);

      std::string item_name;
      if (pot_item.item < kPotItemNameCount) {
        item_name = kPotItemNames[pot_item.item];
      } else {
        item_name = absl::StrFormat("Unk%02X", pot_item.item);
      }

      std::string item_text =
          absl::StrFormat("%02X %s", pot_item.item, item_name.c_str());
      gui::DrawText(rt, item_text, canvas_x, canvas_y - 10);
    }
  }
}

void DungeonCanvasViewer::RenderEntityOverlay(const gui::CanvasRuntime& rt,
                                              const zelda3::Room& room) {
  RenderSprites(rt, room);
  RenderPotItems(rt, room);
}

void DungeonCanvasViewer::HandleTouchLongPressContextMenu(
    const gui::CanvasRuntime& rt, const zelda3::Room& room) {
  constexpr const char* kPopupId = "##TouchEntityContextMenu";
  const ImGuiIO& io = ImGui::GetIO();
  const bool touch_context_click =
      rt.hovered && io.MouseSource == ImGuiMouseSource_TouchScreen &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  const bool gesture_long_press = touch_handler_.WasLongPressed();
  const bool should_open_context = gesture_long_press || touch_context_click;

  if (should_open_context) {
    const ImVec2 gesture_pos = gesture_long_press
                                   ? touch_handler_.GetGesturePosition()
                                   : ImGui::GetMousePos();
    const float scale = rt.scale > 0.0f ? rt.scale : 1.0f;
    const float rel_x = (gesture_pos.x - rt.canvas_p0.x) / scale;
    const float rel_y = (gesture_pos.y - rt.canvas_p0.y) / scale;
    const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
    const int hit_size = is_touch ? 24 : 16;

    const auto& sprites = room.GetSprites();
    for (size_t idx = 0; idx < sprites.size(); ++idx) {
      const int sprite_px = sprites[idx].x() * 16;
      const int sprite_py = sprites[idx].y() * 16;
      if (rel_x >= sprite_px && rel_x < sprite_px + hit_size &&
          rel_y >= sprite_py && rel_y < sprite_py + hit_size) {
        object_interaction_.SelectEntity(EntityType::Sprite, idx);
        ImGui::OpenPopup(kPopupId);
        break;
      }
    }

    if (!ImGui::IsPopupOpen(kPopupId)) {
      const auto& pot_items = room.GetPotItems();
      for (size_t idx = 0; idx < pot_items.size(); ++idx) {
        const int item_px = pot_items[idx].GetPixelX();
        const int item_py = pot_items[idx].GetPixelY();
        if (rel_x >= item_px && rel_x < item_px + hit_size &&
            rel_y >= item_py && rel_y < item_py + hit_size) {
          object_interaction_.SelectEntity(EntityType::Item, idx);
          ImGui::OpenPopup(kPopupId);
          break;
        }
      }
    }

    if (!ImGui::IsPopupOpen(kPopupId)) {
      const auto& objects = room.GetTileObjects();
      for (size_t idx = 0; idx < objects.size(); ++idx) {
        const auto& obj = objects[idx];
        const int obj_px = obj.x() * 8;
        const int obj_py = obj.y() * 8;
        auto [obj_w, obj_h] =
            zelda3::DimensionService::Get().GetPixelDimensions(obj);
        obj_w = std::max(obj_w, 8);
        obj_h = std::max(obj_h, 8);
        if (rel_x >= obj_px && rel_x < obj_px + obj_w && rel_y >= obj_py &&
            rel_y < obj_py + obj_h) {
          object_interaction_.SetSelectedObjects({idx});
          ImGui::OpenPopup(kPopupId);
          break;
        }
      }
    }
  }

  if (ImGui::BeginPopup(kPopupId)) {
    if (object_interaction_.HasEntitySelection()) {
      auto sel = object_interaction_.GetSelectedEntity();
      if (sel.type == EntityType::Sprite) {
        const auto& sprites = room.GetSprites();
        if (sel.index < sprites.size()) {
          std::string label = zelda3::GetSpriteLabel(sprites[sel.index].id());
          ImGui::TextDisabled("Sprite: %02X %s", sprites[sel.index].id(),
                              label.c_str());
          ImGui::Separator();
        }
        if (ImGui::MenuItem("Delete Sprite")) {
          object_interaction_.entity_coordinator().DeleteSelectedEntity();
        }
      } else if (sel.type == EntityType::Item) {
        ImGui::TextDisabled("Pot Item");
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Item")) {
          object_interaction_.entity_coordinator().DeleteSelectedEntity();
        }
      }
    } else if (object_interaction_.GetSelectionCount() > 0) {
      const auto indices = object_interaction_.GetSelectedObjectIndices();
      if (indices.size() == 1) {
        const auto& objects = room.GetTileObjects();
        if (indices[0] < objects.size()) {
          std::string name = zelda3::GetObjectName(objects[indices[0]].id_);
          ImGui::TextDisabled("Object: %03X %s", objects[indices[0]].id_,
                              name.c_str());
          ImGui::Separator();
        }
      } else {
        ImGui::TextDisabled("%zu objects selected",
                            object_interaction_.GetSelectionCount());
        ImGui::Separator();
      }
      if (ImGui::MenuItem("Delete")) {
        object_interaction_.HandleDeleteSelected();
      }
      if (ImGui::MenuItem("Copy")) {
        object_interaction_.HandleCopySelected();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Send to Front")) {
        object_interaction_.SendSelectedToFront();
      }
      if (ImGui::MenuItem("Send to Back")) {
        object_interaction_.SendSelectedToBack();
      }
    }
    ImGui::EndPopup();
  }
}

void DungeonCanvasViewer::DrawObjectPositionOutlines(
    const gui::CanvasRuntime& rt, const zelda3::Room& room) {
  const auto& theme = AgentUI::GetTheme();
  const auto& objects = room.GetTileObjects();

  for (const auto& obj : objects) {
    bool show_this_type = true;
    if (obj.id_ < 0x100) {
      show_this_type = object_outline_toggles_.show_type1_objects;
    } else if (obj.id_ >= 0x100 && obj.id_ < 0x200) {
      show_this_type = object_outline_toggles_.show_type2_objects;
    } else if (obj.id_ >= 0xF00) {
      show_this_type = object_outline_toggles_.show_type3_objects;
    }

    bool show_this_layer = true;
    if (obj.GetLayerValue() == 0) {
      show_this_layer = object_outline_toggles_.show_layer0_objects;
    } else if (obj.GetLayerValue() == 1) {
      show_this_layer = object_outline_toggles_.show_layer1_objects;
    } else if (obj.GetLayerValue() == 2) {
      show_this_layer = object_outline_toggles_.show_layer2_objects;
    }

    if (!show_this_type || !show_this_layer) {
      continue;
    }

    auto [canvas_x, canvas_y, width, height] =
        zelda3::DimensionService::Get().GetSelectionBoundsPixels(obj);
    width = std::min(width, 512);
    height = std::min(height, 512);

    ImVec4 outline_color;
    if (obj.GetLayerValue() == 0) {
      outline_color = theme.dungeon_outline_layer0;
    } else if (obj.GetLayerValue() == 1) {
      outline_color = theme.dungeon_outline_layer1;
    } else {
      outline_color = theme.dungeon_outline_layer2;
    }

    gui::DrawRect(rt, canvas_x, canvas_y, width, height, outline_color);

    std::string name = zelda3::GetObjectName(obj.id_);
    if (name.length() > 12) {
      name = name.substr(0, 10) + "..";
    }
    std::string label;
    if (obj.id_ >= 0x100) {
      label = absl::StrFormat("0x%03X\n%s\n%s  [%dx%d]", obj.id_, name.c_str(),
                              GetObjectStreamLabel(obj.GetLayerValue()), width,
                              height);
    } else {
      label = absl::StrFormat("0x%02X\n%s\n%s  [%dx%d]", obj.id_, name.c_str(),
                              GetObjectStreamLabel(obj.GetLayerValue()), width,
                              height);
    }
    gui::DrawText(rt, label, canvas_x + 1, canvas_y + 1);
  }
}

const DungeonRenderingHelpers::CollisionOverlayCache&
DungeonCanvasViewer::GetCollisionOverlayCache(int room_id) {
  auto it = collision_overlay_cache_.find(room_id);
  if (it != collision_overlay_cache_.end()) {
    return it->second;
  }

  DungeonRenderingHelpers::CollisionOverlayCache cache;
  cache.entries.clear();

  if (!rom_ || !rom_->is_loaded()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  auto map_or = zelda3::LoadCustomCollisionMap(rom_, room_id);
  if (!map_or.ok()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  const auto& map = map_or.value();
  cache.has_data = map.has_data;
  if (cache.has_data && !track_collision_config_.IsEmpty()) {
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        const uint8_t tile = map.tiles[static_cast<size_t>(y * 64 + x)];
        if (tile < 256 && (track_collision_config_.track_tiles[tile] ||
                           track_collision_config_.stop_tiles[tile] ||
                           track_collision_config_.switch_tiles[tile])) {
          cache.entries.push_back(
              DungeonRenderingHelpers::CollisionOverlayEntry{
                  static_cast<uint8_t>(x), static_cast<uint8_t>(y), tile});
        }
      }
    }
  }

  collision_overlay_cache_.emplace(room_id, std::move(cache));
  return collision_overlay_cache_.at(room_id);
}

gfx::Bitmap* DungeonCanvasViewer::PrepareRoomCompositeBitmap(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms || !rooms_ || !rom_ ||
      !rom_->is_loaded()) {
    return nullptr;
  }

  auto* room_ptr = rooms_->TryEnsureRoom(room_id);
  if (!room_ptr) {
    return nullptr;
  }
  auto& room = *room_ptr;
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  if (!bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
    (void)LoadAndRenderRoomGraphics(room_id);
  }

  auto& layer_mgr = GetRoomLayerManager(room_id);
  layer_mgr.ApplyLayerMerging(room.layer_merging());
  layer_mgr.ApplyRoomEffect(room.effect());

  auto& composite = room.GetCompositeBitmap(layer_mgr);
  if (!composite.is_active() || composite.width() <= 0) {
    return nullptr;
  }

  // Closes the A3 first-frame race for the dungeon room composite, mirroring
  // the title-screen path (A4). The helper queues CREATE on first sight,
  // queues UPDATE when RoomLayerManager::CompositeToOutput marked the bitmap
  // modified, and stamps metadata().purpose so canvas_rendering's diagnostic
  // log can identify the bitmap.
  internal::EnsureCompositeBitmapTextureQueued(composite);
  return &composite;
}

bool DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms || !rooms_) {
    return false;
  }

  const float scale = canvas_.global_scale();
  gfx::Bitmap* composite = PrepareRoomCompositeBitmap(room_id);
  if (!composite) {
    return false;
  }

  // Single-room canvas rendering prepares the composite after the normal room
  // state upload pass. Flush here too so workbench mode can draw the freshly
  // queued room texture in the same frame.
  gfx::Arena::Get().ProcessTextureQueue(renderer_);

  if (composite->texture()) {
    canvas_.DrawBitmap(*composite, 0, 0, scale, 255);
    return true;
  }

  return false;
}

void DungeonCanvasViewer::DrawMaskHighlights(const gui::CanvasRuntime& rt,
                                             const zelda3::Room& room) {
  const auto& objects = room.GetTileObjects();
  const ImVec4 mask_color(0.2f, 0.6f, 1.0f, 0.4f);

  for (const auto& obj : objects) {
    if (obj.GetLayerValue() != 1) {
      continue;
    }

    auto [canvas_x, canvas_y] =
        DungeonRenderingHelpers::RoomToCanvasCoordinates(obj.x(), obj.y());
    auto [width, height] =
        zelda3::DimensionService::Get().GetPixelDimensions(obj);
    width = std::min(width, 512);
    height = std::min(height, 512);
    gui::DrawRect(rt, canvas_x, canvas_y, width, height, mask_color);
  }
}

}  // namespace yaze::editor
