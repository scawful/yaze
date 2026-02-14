#include "app/editor/dungeon/dungeon_rendering_helpers.h"

#include <cmath>
#include <algorithm>
#include <array>

#include "absl/strings/str_format.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/agent_theme.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze::editor {

TrackDirectionMasks DungeonRenderingHelpers::GetTrackDirectionMasksForTrackIndex(size_t index) {
  switch (index) {
    case 0:
      return TrackDirectionMasks{kTrackDirEast | kTrackDirWest, 0};
    case 1:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirSouth, 0};
    case 2:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirEast, 0};
    case 3:
      return TrackDirectionMasks{kTrackDirSouth | kTrackDirEast, 0};
    case 4:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirWest, 0};
    case 5:
      return TrackDirectionMasks{kTrackDirSouth | kTrackDirWest, 0};
    case 6:
      return TrackDirectionMasks{
          kTrackDirNorth | kTrackDirEast | kTrackDirSouth | kTrackDirWest, 0};
    case 7:
      return TrackDirectionMasks{kTrackDirSouth, 0};
    case 8:
      return TrackDirectionMasks{kTrackDirNorth, 0};
    case 9:
      return TrackDirectionMasks{kTrackDirEast, 0};
    case 10:
      return TrackDirectionMasks{kTrackDirWest, 0};
    case 11:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirEast | kTrackDirWest, 0};
    case 12:
      return TrackDirectionMasks{kTrackDirSouth | kTrackDirEast | kTrackDirWest, 0};
    case 13:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirSouth | kTrackDirEast, 0};
    case 14:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirSouth | kTrackDirWest, 0};
    default:
      return {};
  }
}

TrackDirectionMasks DungeonRenderingHelpers::GetTrackDirectionMasksForSwitchIndex(size_t index) {
  switch (index) {
    case 0:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirEast,
                                 kTrackDirNorth | kTrackDirWest};
    case 1:
      return TrackDirectionMasks{kTrackDirSouth | kTrackDirEast,
                                 kTrackDirNorth | kTrackDirEast};
    case 2:
      return TrackDirectionMasks{kTrackDirNorth | kTrackDirWest,
                                 kTrackDirSouth | kTrackDirWest};
    case 3:
      return TrackDirectionMasks{kTrackDirSouth | kTrackDirWest,
                                 kTrackDirSouth | kTrackDirEast};
    default:
      return {};
  }
}

TrackDirectionMasks DungeonRenderingHelpers::GetTrackDirectionMasksFromConfig(
    uint8_t tile, const std::vector<uint16_t>& track_tiles,
    const std::vector<uint16_t>& switch_tiles) {
  auto track_it = std::find(track_tiles.begin(), track_tiles.end(), tile);
  if (track_it != track_tiles.end()) {
    return GetTrackDirectionMasksForTrackIndex(
        static_cast<size_t>(track_it - track_tiles.begin()));
  }

  auto switch_it = std::find(switch_tiles.begin(), switch_tiles.end(), tile);
  if (switch_it != switch_tiles.end()) {
    return GetTrackDirectionMasksForSwitchIndex(
        static_cast<size_t>(switch_it - switch_tiles.begin()));
  }

  return {};
}

void DungeonRenderingHelpers::DrawTrackArrowHead(ImDrawList* draw_list, const ImVec2& tip,
                                               TrackDir dir, float size, ImU32 color) {
  ImVec2 a, b;
  switch (dir) {
    case TrackDir::North:
      a = ImVec2(tip.x - size, tip.y + size);
      b = ImVec2(tip.x + size, tip.y + size);
      break;
    case TrackDir::East:
      a = ImVec2(tip.x - size, tip.y - size);
      b = ImVec2(tip.x - size, tip.y + size);
      break;
    case TrackDir::South:
      a = ImVec2(tip.x - size, tip.y - size);
      b = ImVec2(tip.x + size, tip.y - size);
      break;
    case TrackDir::West:
      a = ImVec2(tip.x + size, tip.y - size);
      b = ImVec2(tip.x + size, tip.y + size);
      break;
  }
  draw_list->AddTriangleFilled(tip, a, b, color);
}

void DungeonRenderingHelpers::DrawTrackDirectionMask(ImDrawList* draw_list, const ImVec2& min,
                                                   float tile_size, uint8_t mask, ImU32 color) {
  if (!mask) {
    return;
  }
  const ImVec2 center(min.x + tile_size * 0.5f, min.y + tile_size * 0.5f);
  const float line_len = tile_size * 0.32f;
  const float head_size = std::max(2.0f, tile_size * 0.18f);
  const float thickness = std::max(1.0f, tile_size * 0.08f);

  auto draw_dir = [&](TrackDir dir, float dx, float dy) {
    ImVec2 tip(center.x + dx * line_len, center.y + dy * line_len);
    draw_list->AddLine(center, tip, color, thickness);
    DrawTrackArrowHead(draw_list, tip, dir, head_size, color);
  };

  if (mask & kTrackDirNorth) {
    draw_dir(TrackDir::North, 0.0f, -1.0f);
  }
  if (mask & kTrackDirEast) {
    draw_dir(TrackDir::East, 1.0f, 0.0f);
  }
  if (mask & kTrackDirSouth) {
    draw_dir(TrackDir::South, 0.0f, 1.0f);
  }
  if (mask & kTrackDirWest) {
    draw_dir(TrackDir::West, -1.0f, 0.0f);
  }
}

std::pair<int, int> DungeonRenderingHelpers::RoomToCanvasCoordinates(int room_x, int room_y) {
  // Return unscaled relative coordinates (8 pixels per tile)
  return {room_x * 8, room_y * 8};
}

std::pair<int, int> DungeonRenderingHelpers::ScreenToRoomCoordinates(
    const ImVec2& screen_pos, const ImVec2& zero_point, float scale) {
  if (scale <= 0.0f) return {0, 0};
  float rel_x = (screen_pos.x - zero_point.x) / scale;
  float rel_y = (screen_pos.y - zero_point.y) / scale;
  return {static_cast<int>(rel_x / 8.0f), static_cast<int>(rel_y / 8.0f)};
}

void DungeonRenderingHelpers::DrawTrackCollisionOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const CollisionOverlayCache& cache,
    const TrackCollisionConfig& config,
    bool direction_map_enabled,
    const std::vector<uint16_t>& track_tile_order,
    const std::vector<uint16_t>& switch_tile_order,
    bool show_legend) {
  
  const ImU32 track_color = ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.4f, 0.35f));
  const ImU32 stop_color = ImGui::GetColorU32(ImVec4(0.9f, 0.3f, 0.2f, 0.45f));
  const ImU32 switch_color = ImGui::GetColorU32(ImVec4(0.95f, 0.8f, 0.2f, 0.45f));
  const ImU32 outline_color = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.4f));
  const ImU32 arrow_color = ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 0.75f));
  const ImU32 arrow_color_dim = ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 0.4f));

  for (const auto& entry : cache.entries) {
    const float px = static_cast<float>(entry.x * 8) * scale;
    const float py = static_cast<float>(entry.y * 8) * scale;
    ImVec2 min(canvas_pos.x + px, canvas_pos.y + py);
    const float tile_size = 8.0f * scale;
    ImVec2 max(min.x + tile_size, min.y + tile_size);

    ImU32 color = track_color;
    if (config.stop_tiles[entry.tile]) {
      color = stop_color;
    } else if (config.switch_tiles[entry.tile]) {
      color = switch_color;
    }

    draw_list->AddRectFilled(min, max, color);
    draw_list->AddRect(min, max, outline_color);

    if (config.stop_tiles[entry.tile]) {
      const char* dir_label = nullptr;
      if (entry.tile == 0xB7) dir_label = "U";
      else if (entry.tile == 0xB8) dir_label = "D";
      else if (entry.tile == 0xB9) dir_label = "L";
      else if (entry.tile == 0xBA) dir_label = "R";
      
      if (dir_label) {
        const ImU32 label_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        ImVec2 text_size = ImGui::CalcTextSize(dir_label);
        ImVec2 text_pos(min.x + (tile_size - text_size.x) * 0.5f,
                        min.y + (tile_size - text_size.y) * 0.5f);
        draw_list->AddText(text_pos, label_color, dir_label);
      }
    }

    if (direction_map_enabled) {
      const auto masks = GetTrackDirectionMasksFromConfig(entry.tile, track_tile_order, switch_tile_order);
      if (masks.primary != 0) {
        DrawTrackDirectionMask(draw_list, min, tile_size, masks.primary, arrow_color);
        if (masks.secondary != 0) {
          DrawTrackDirectionMask(draw_list, min, tile_size, masks.secondary, arrow_color_dim);
        }
      }
    }
  }

  if (show_legend) {
    ImVec2 legend_pos(canvas_pos.x + 8.0f, canvas_pos.y + 8.0f);
    const float swatch = 10.0f;
    const float pad = 6.0f;
    const ImU32 text_color = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.85f));

    struct LegendItem {
      const char* label;
      ImU32 color;
    };
    const LegendItem items[] = {
        {"Track", track_color},
        {"Stop", stop_color},
        {"Switch", switch_color},
    };

    float y = legend_pos.y;
    for (const auto& item : items) {
      ImVec2 swatch_min(legend_pos.x, y);
      ImVec2 swatch_max(legend_pos.x + swatch, y + swatch);
      draw_list->AddRectFilled(swatch_min, swatch_max, item.color);
      draw_list->AddRect(swatch_min, swatch_max, outline_color);
      draw_list->AddText(ImVec2(legend_pos.x + swatch + pad, y - 1.0f), text_color, item.label);
      y += swatch + 4.0f;
    }
    const char* arrow_label = direction_map_enabled ? "Arrows: track directions" : "Arrows: disabled";
    draw_list->AddText(ImVec2(legend_pos.x, y + 2.0f), text_color, arrow_label);
  }
}

void DungeonRenderingHelpers::DrawCustomCollisionOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const zelda3::Room& room) {
  const auto& theme = AgentUI::GetTheme();
  const auto& custom = room.custom_collision();
  if (!custom.has_data) return;

  const float tile_size = 8.0f * scale;
  auto apply_alpha = [](ImVec4 c, float a) { c.w = a; return c; };

  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      uint8_t tile = custom.tiles[static_cast<size_t>(y * 64 + x)];
      if (tile == 0) continue;

      const float px = static_cast<float>(x * 8) * scale;
      const float py = static_cast<float>(y * 8) * scale;
      ImVec2 min(canvas_pos.x + px, canvas_pos.y + py);
      ImVec2 max(min.x + tile_size, min.y + tile_size);

      ImVec4 fill = apply_alpha(theme.panel_border_color, 0.25f);
      switch (tile) {
        case 0x08: fill = apply_alpha(theme.status_active, 0.35f); break;
        case 0x09: fill = apply_alpha(theme.status_active, 0.25f); break;
        case 0x1B: fill = apply_alpha(theme.editor_background, 0.50f); break;
        case 0x0E:
        case 0x3C: fill = apply_alpha(theme.status_error, 0.30f); break;
        case 0x1C: fill = apply_alpha(theme.status_warning, 0.30f); break;
        case 0x21:
        case 0x22:
        case 0x23: fill = apply_alpha(theme.accent_color, 0.30f); break;
      }

      draw_list->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(fill));
      draw_list->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(apply_alpha(theme.editor_grid, 0.50f)));
      
      if (scale >= 4.0f) {
        std::string buf = absl::StrFormat("%02X", tile);
        ImVec2 text_size = ImGui::CalcTextSize(buf.c_str());
        if (text_size.x < tile_size * 0.9f) {
          ImVec2 text_pos(min.x + (tile_size - text_size.x) * 0.5f, min.y + (tile_size - text_size.y) * 0.5f);
          draw_list->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(apply_alpha(theme.text_primary, 0.85f)), buf.c_str());
        }
      }
    }
  }
}

void DungeonRenderingHelpers::DrawWaterFillOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const zelda3::Room& room) {
  const auto& theme = AgentUI::GetTheme();
  const auto& zone = room.water_fill_zone();
  if (!zone.has_data) return;

  const float tile_size = 8.0f * scale;
  auto apply_alpha = [](ImVec4 c, float a) { c.w = a; return c; };
  const ImU32 fill_u32 = ImGui::ColorConvertFloat4ToU32(apply_alpha(theme.status_active, 0.30f));
  const ImU32 border_u32 = ImGui::ColorConvertFloat4ToU32(apply_alpha(theme.editor_grid, 0.40f));

  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      if (zone.tiles[static_cast<size_t>(y * 64 + x)] == 0) continue;
      const float px = static_cast<float>(x * 8) * scale;
      const float py = static_cast<float>(y * 8) * scale;
      ImVec2 min(canvas_pos.x + px, canvas_pos.y + py);
      ImVec2 max(min.x + tile_size, min.y + tile_size);
      draw_list->AddRectFilled(min, max, fill_u32);
      draw_list->AddRect(min, max, border_u32);
    }
  }
}

void DungeonRenderingHelpers::DrawCameraQuadrantOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const zelda3::Room& room) {
  const auto& theme = AgentUI::GetTheme();
  const float room_size_px = 512.0f * scale;
  const float mid_px = 256.0f * scale;
  const float thickness = std::max(1.0f, 1.0f * scale);
  const ImU32 line_color = ImGui::GetColorU32(theme.editor_grid);

  draw_list->AddLine(ImVec2(canvas_pos.x + mid_px, canvas_pos.y), ImVec2(canvas_pos.x + mid_px, canvas_pos.y + room_size_px), line_color, thickness);
  draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + mid_px), ImVec2(canvas_pos.x + room_size_px, canvas_pos.y + mid_px), line_color, thickness);

  std::string label = absl::StrFormat("Layout %d", room.layout);
  draw_list->AddText(ImVec2(canvas_pos.x + 6.0f, canvas_pos.y + 6.0f), ImGui::GetColorU32(theme.text_secondary_color), label.c_str());
}

void DungeonRenderingHelpers::DrawMinecartSpriteOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const zelda3::Room& room, const std::bitset<256>& minecart_sprite_ids,
    const TrackCollisionConfig& config) {
  const auto& theme = AgentUI::GetTheme();
  auto map_or = zelda3::LoadCustomCollisionMap(room.rom(), room.id());
  const bool has_collision = map_or.ok() && map_or.value().has_data;

  const ImU32 ok_color = ImGui::GetColorU32(theme.status_success);
  const ImU32 warn_color = ImGui::GetColorU32(theme.status_warning);
  const ImU32 unknown_color = ImGui::GetColorU32(theme.status_inactive);

  for (const auto& sprite : room.GetSprites()) {
    if (sprite.id() >= 256 || !minecart_sprite_ids[sprite.id()]) continue;

    bool on_stop_tile = false;
    bool has_tile = false;
    if (has_collision) {
      int tile_x = sprite.x() * 2;
      int tile_y = sprite.y() * 2;
      if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
        uint8_t tile = map_or.value().tiles[static_cast<size_t>(tile_y * 64 + tile_x)];
        has_tile = true;
        on_stop_tile = config.stop_tiles[tile];
      }
    }

    ImU32 color = has_tile ? (on_stop_tile ? ok_color : warn_color) : unknown_color;
    const float px = static_cast<float>(sprite.x() * 16) * scale;
    const float py = static_cast<float>(sprite.y() * 16) * scale;
    ImVec2 min(canvas_pos.x + px, canvas_pos.y + py);
    ImVec2 max(min.x + (16.0f * scale), min.y + (16.0f * scale));
    draw_list->AddRect(min, max, color, 0.0f, 0, 2.0f);
  }
}

void DungeonRenderingHelpers::DrawTrackGapOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const zelda3::Room& room, const CollisionOverlayCache& cache) {
  
  std::array<bool, 64 * 64> object_grid{};
  for (const auto& obj : room.GetTileObjects()) {
    if (obj.id_ != 0x31) continue;
    auto dim = zelda3::DimensionService::Get().GetDimensions(obj);
    int base_x = obj.x() * 2;
    int base_y = obj.y() * 2;
    for (int dy = 0; dy < dim.height_tiles && (base_y + dy) < 64; ++dy) {
      for (int dx = 0; dx < dim.width_tiles && (base_x + dx) < 64; ++dx) {
        object_grid[static_cast<size_t>((base_y + dy) * 64 + (base_x + dx))] = true;
      }
    }
  }

  std::array<bool, 64 * 64> collision_grid{};
  for (const auto& entry : cache.entries) {
    collision_grid[static_cast<size_t>(entry.y * 64 + entry.x)] = true;
  }

  const auto& theme = AgentUI::GetTheme();
  const ImU32 gap_color = ImGui::ColorConvertFloat4ToU32(theme.status_warning);
  const ImU32 orphan_color = ImGui::ColorConvertFloat4ToU32(theme.status_inactive);
  const float tile_size = 8.0f * scale;

  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      size_t idx = static_cast<size_t>(y * 64 + x);
      if (object_grid[idx] == collision_grid[idx]) continue;

      const float px = static_cast<float>(x * 8) * scale;
      const float py = static_cast<float>(y * 8) * scale;
      ImVec2 min_pt(canvas_pos.x + px, canvas_pos.y + py);
      ImVec2 max_pt(min_pt.x + tile_size, min_pt.y + tile_size);
      draw_list->AddRectFilled(min_pt, max_pt, object_grid[idx] ? gap_color : orphan_color);
    }
  }
}

void DungeonRenderingHelpers::DrawTrackRouteOverlay(
    ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
    const CollisionOverlayCache& cache) {
  const auto& theme = AgentUI::GetTheme();
  std::array<bool, 64 * 64> occupied{};
  for (const auto& entry : cache.entries) {
    occupied[static_cast<size_t>(entry.y * 64 + entry.x)] = true;
  }

  const float tile_size = 8.0f * scale;
  const float half_tile = tile_size * 0.5f;
  const ImU32 route_color = ImGui::ColorConvertFloat4ToU32(theme.status_active);
  const float thickness = std::max(1.0f, 1.5f * scale);

  for (const auto& entry : cache.entries) {
    float cx = canvas_pos.x + static_cast<float>(entry.x * 8) * scale + half_tile;
    float cy = canvas_pos.y + static_cast<float>(entry.y * 8) * scale + half_tile;

    if (entry.x + 1 < 64 && occupied[static_cast<size_t>(entry.y * 64 + (entry.x + 1))]) {
      draw_list->AddLine(ImVec2(cx, cy), ImVec2(cx + tile_size, cy), route_color, thickness);
    }
    if (entry.y + 1 < 64 && occupied[static_cast<size_t>((entry.y + 1) * 64 + entry.x)]) {
      draw_list->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + tile_size), route_color, thickness);
    }
  }
}

} // namespace yaze::editor
