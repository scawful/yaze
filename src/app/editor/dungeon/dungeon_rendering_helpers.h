#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERING_HELPERS_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERING_HELPERS_H

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "imgui/imgui.h"

#include <SDL.h>
#include <array>
#include <bitset>

namespace yaze::zelda3 {
class Room;
struct SpriteOamLayout;
}  // namespace yaze::zelda3

namespace yaze::editor {

enum class TrackDir : uint8_t { North, East, South, West };

constexpr uint8_t kTrackDirNorth = 1 << 0;
constexpr uint8_t kTrackDirEast = 1 << 1;
constexpr uint8_t kTrackDirSouth = 1 << 2;
constexpr uint8_t kTrackDirWest = 1 << 3;

struct TrackDirectionMasks {
  uint8_t primary = 0;
  uint8_t secondary = 0;
};

class DungeonRenderingHelpers {
 public:
  static TrackDirectionMasks GetTrackDirectionMasksForTrackIndex(size_t index);
  static TrackDirectionMasks GetTrackDirectionMasksForSwitchIndex(size_t index);
  static TrackDirectionMasks GetTrackDirectionMasksFromConfig(
      uint8_t tile, const std::vector<uint16_t>& track_tiles,
      const std::vector<uint16_t>& track_switches);

  static void DrawTrackArrowHead(ImDrawList* draw_list, const ImVec2& tip,
                                 TrackDir dir, float size, ImU32 color);
  static void DrawTrackDirectionMask(ImDrawList* draw_list, const ImVec2& min,
                                     float tile_size, uint8_t mask,
                                     ImU32 color);

  static std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y);

  // Draws a sprite preview into any ImGui draw list within the given bounding box.
  static bool DrawSpritePreview(ImDrawList* draw_list,
                                const std::vector<uint8_t>& preview,
                                const SDL_Rect& bounds, const ImVec2& box_min,
                                const ImVec2& box_max,
                                const std::array<SDL_Color, 256>& color_table);

  // Maps a pot item ID (0..27) to its sprite OAM layout and sprite ID, if one exists.
  static const zelda3::SpriteOamLayout* GetPotItemSpriteLayout(
      uint8_t item_id, uint8_t* out_sprite_id = nullptr);

  // Generates rendered sprite pixels and bounds for a pot item in the context of room gfx.
  static bool BuildPotItemPreview(uint8_t item_id, const zelda3::Room& room,
                                  std::vector<uint8_t>& out_pixels,
                                  SDL_Rect& out_bounds);

  // Overlay rendering structures
  struct TrackCollisionConfig {
    std::array<bool, 256> track_tiles{};
    std::array<bool, 256> stop_tiles{};
    std::array<bool, 256> switch_tiles{};

    bool IsEmpty() const {
      for (int i = 0; i < 256; ++i) {
        if (track_tiles[i] || stop_tiles[i] || switch_tiles[i])
          return false;
      }
      return true;
    }
  };

  struct CollisionOverlayEntry {
    uint8_t x, y, tile;
  };

  struct CollisionOverlayCache {
    bool has_data = false;
    std::vector<CollisionOverlayEntry> entries;
  };

  // Overlay rendering methods
  static void DrawTrackCollisionOverlay(
      ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
      const CollisionOverlayCache& cache, const TrackCollisionConfig& config,
      bool direction_map_enabled, const std::vector<uint16_t>& track_tile_order,
      const std::vector<uint16_t>& switch_tile_order, bool show_legend);

  static void DrawCustomCollisionOverlay(ImDrawList* draw_list,
                                         const ImVec2& canvas_pos, float scale,
                                         const zelda3::Room& room);

  static void DrawWaterFillOverlay(ImDrawList* draw_list,
                                   const ImVec2& canvas_pos, float scale,
                                   const zelda3::Room& room);

  static void DrawCameraQuadrantOverlay(ImDrawList* draw_list,
                                        const ImVec2& canvas_pos, float scale,
                                        const zelda3::Room& room);

  static void DrawMinecartSpriteOverlay(
      ImDrawList* draw_list, const ImVec2& canvas_pos, float scale,
      const zelda3::Room& room, const std::bitset<256>& minecart_sprite_ids,
      const TrackCollisionConfig& config);

  static void DrawTrackGapOverlay(ImDrawList* draw_list,
                                  const ImVec2& canvas_pos, float scale,
                                  const zelda3::Room& room,
                                  const CollisionOverlayCache& cache);

  static void DrawTrackRouteOverlay(ImDrawList* draw_list,
                                    const ImVec2& canvas_pos, float scale,
                                    const CollisionOverlayCache& cache);
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERING_HELPERS_H
