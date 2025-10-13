#include "canvas_utils.h"

#include <cmath>
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "util/log.h"

namespace yaze {
namespace gui {
namespace CanvasUtils {

ImVec2 AlignToGrid(ImVec2 pos, float grid_step) {
  return ImVec2(std::floor(pos.x / grid_step) * grid_step,
                std::floor(pos.y / grid_step) * grid_step);
}

float CalculateEffectiveScale(ImVec2 canvas_size, ImVec2 content_size,
                              float global_scale) {
  if (content_size.x <= 0 || content_size.y <= 0)
    return global_scale;

  float scale_x = (canvas_size.x * global_scale) / content_size.x;
  float scale_y = (canvas_size.y * global_scale) / content_size.y;
  return std::min(scale_x, scale_y);
}

int GetTileIdFromPosition(ImVec2 mouse_pos, float tile_size, float scale,
                          int tiles_per_row) {
  float scaled_tile_size = tile_size * scale;
  int tile_x = static_cast<int>(mouse_pos.x / scaled_tile_size);
  int tile_y = static_cast<int>(mouse_pos.y / scaled_tile_size);

  return tile_x + (tile_y * tiles_per_row);
}

bool LoadROMPaletteGroups(Rom* rom, CanvasPaletteManager& palette_manager) {
  if (!rom || palette_manager.palettes_loaded) {
    return palette_manager.palettes_loaded;
  }

  try {
    const auto& palette_groups = rom->palette_group();
    palette_manager.rom_palette_groups.clear();
    palette_manager.palette_group_names.clear();

    // Overworld palettes
    if (palette_groups.overworld_main.size() > 0) {
      palette_manager.rom_palette_groups.push_back(
          palette_groups.overworld_main[0]);
      palette_manager.palette_group_names.push_back("Overworld Main");
    }
    if (palette_groups.overworld_aux.size() > 0) {
      palette_manager.rom_palette_groups.push_back(
          palette_groups.overworld_aux[0]);
      palette_manager.palette_group_names.push_back("Overworld Aux");
    }
    if (palette_groups.overworld_animated.size() > 0) {
      palette_manager.rom_palette_groups.push_back(
          palette_groups.overworld_animated[0]);
      palette_manager.palette_group_names.push_back("Overworld Animated");
    }

    // Dungeon palettes
    if (palette_groups.dungeon_main.size() > 0) {
      palette_manager.rom_palette_groups.push_back(
          palette_groups.dungeon_main[0]);
      palette_manager.palette_group_names.push_back("Dungeon Main");
    }

    // Sprite palettes
    if (palette_groups.global_sprites.size() > 0) {
      palette_manager.rom_palette_groups.push_back(
          palette_groups.global_sprites[0]);
      palette_manager.palette_group_names.push_back("Global Sprites");
    }
    if (palette_groups.armors.size() > 0) {
      palette_manager.rom_palette_groups.push_back(palette_groups.armors[0]);
      palette_manager.palette_group_names.push_back("Armor");
    }
    if (palette_groups.swords.size() > 0) {
      palette_manager.rom_palette_groups.push_back(palette_groups.swords[0]);
      palette_manager.palette_group_names.push_back("Swords");
    }

    palette_manager.palettes_loaded = true;
    LOG_DEBUG("Canvas", "Loaded %zu ROM palette groups",
               palette_manager.rom_palette_groups.size());
    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Canvas", "Failed to load ROM palette groups");
    return false;
  }
}

bool ApplyPaletteGroup(gfx::IRenderer* renderer, gfx::Bitmap* bitmap, const CanvasPaletteManager& palette_manager, 
                       int group_index, int palette_index) {
  if (!bitmap) return false;

  if (group_index < 0 || group_index >= palette_manager.rom_palette_groups.size()) {
    return false;
  }

  const auto& palette = palette_manager.rom_palette_groups[group_index];
  
  // Apply the full palette or use SetPaletteWithTransparent if palette_index is specified
  if (palette_index == 0) {
    bitmap->SetPalette(palette);
  } else {
    bitmap->SetPaletteWithTransparent(palette, palette_index);
  }
  bitmap->set_modified(true);
  
  // Queue texture update via Arena's deferred system
  if (renderer) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, bitmap);
  }
  return true;
}

// Drawing utility functions
void DrawCanvasRect(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                    int x, int y, int w, int h, ImVec4 color,
                    float global_scale) {
  // Apply global scale to position and size
  float scaled_x = x * global_scale;
  float scaled_y = y * global_scale;
  float scaled_w = w * global_scale;
  float scaled_h = h * global_scale;

  ImVec2 origin(canvas_p0.x + scrolling.x + scaled_x,
                canvas_p0.y + scrolling.y + scaled_y);
  ImVec2 size(canvas_p0.x + scrolling.x + scaled_x + scaled_w,
              canvas_p0.y + scrolling.y + scaled_y + scaled_h);

  uint32_t color_u32 = IM_COL32(color.x * 255, color.y * 255, color.z * 255, color.w * 255);
  draw_list->AddRectFilled(origin, size, color_u32);

  // Add a black outline
  ImVec2 outline_origin(origin.x - 1, origin.y - 1);
  ImVec2 outline_size(size.x + 1, size.y + 1);
  draw_list->AddRect(outline_origin, outline_size, IM_COL32(0, 0, 0, 255));
}

void DrawCanvasText(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                    const std::string& text, int x, int y, float global_scale) {
  // Apply global scale to text position
  float scaled_x = x * global_scale;
  float scaled_y = y * global_scale;

  ImVec2 text_pos(canvas_p0.x + scrolling.x + scaled_x,
                  canvas_p0.y + scrolling.y + scaled_y);

  // Draw text with black shadow for better visibility
  draw_list->AddText(ImVec2(text_pos.x + 1, text_pos.y + 1),
                     IM_COL32(0, 0, 0, 255), text.c_str());
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), text.c_str());
}

void DrawCanvasOutline(ImDrawList* draw_list, ImVec2 canvas_p0,
                       ImVec2 scrolling, int x, int y, int w, int h,
                       uint32_t color) {
  ImVec2 origin(canvas_p0.x + scrolling.x + x, canvas_p0.y + scrolling.y + y);
  ImVec2 size(canvas_p0.x + scrolling.x + x + w,
              canvas_p0.y + scrolling.y + y + h);
  draw_list->AddRect(origin, size, color, 0, 0, 1.5f);
}

void DrawCanvasOutlineWithColor(ImDrawList* draw_list, ImVec2 canvas_p0,
                                ImVec2 scrolling, int x, int y, int w, int h,
                                ImVec4 color) {
  uint32_t color_u32 =
      IM_COL32(color.x * 255, color.y * 255, color.z * 255, color.w * 255);
  DrawCanvasOutline(draw_list, canvas_p0, scrolling, x, y, w, h, color_u32);
}

// Grid utility functions
void DrawCanvasGridLines(ImDrawList* draw_list, ImVec2 canvas_p0,
                         ImVec2 canvas_p1, ImVec2 scrolling, float grid_step,
                         float global_scale) {
  const uint32_t grid_color = IM_COL32(200, 200, 200, 50);
  const float grid_thickness = 0.5f;

  float scaled_grid_step = grid_step * global_scale;

  for (float x = fmodf(scrolling.x, scaled_grid_step);
       x < (canvas_p1.x - canvas_p0.x); x += scaled_grid_step) {
    draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                       ImVec2(canvas_p0.x + x, canvas_p1.y), grid_color,
                       grid_thickness);
  }

  for (float y = fmodf(scrolling.y, scaled_grid_step);
       y < (canvas_p1.y - canvas_p0.y); y += scaled_grid_step) {
    draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                       ImVec2(canvas_p1.x, canvas_p0.y + y), grid_color,
                       grid_thickness);
  }
}

void DrawCustomHighlight(ImDrawList* draw_list, ImVec2 canvas_p0,
                         ImVec2 scrolling, int highlight_tile_id,
                         float grid_step) {
  if (highlight_tile_id == -1)
    return;

  int tile_x = highlight_tile_id % 8;
  int tile_y = highlight_tile_id / 8;
  ImVec2 tile_pos(canvas_p0.x + scrolling.x + tile_x * grid_step,
                  canvas_p0.y + scrolling.y + tile_y * grid_step);
  ImVec2 tile_pos_end(tile_pos.x + grid_step, tile_pos.y + grid_step);

  draw_list->AddRectFilled(tile_pos, tile_pos_end, IM_COL32(255, 0, 255, 255));
}

void DrawHexTileLabels(ImDrawList* draw_list, ImVec2 canvas_p0,
                       ImVec2 scrolling, ImVec2 canvas_sz, float grid_step,
                       float global_scale) {
  float scaled_grid_step = grid_step * global_scale;

  for (float x = fmodf(scrolling.x, scaled_grid_step);
       x < canvas_sz.x * global_scale; x += scaled_grid_step) {
    for (float y = fmodf(scrolling.y, scaled_grid_step);
         y < canvas_sz.y * global_scale; y += scaled_grid_step) {
      int tile_x = (x - scrolling.x) / scaled_grid_step;
      int tile_y = (y - scrolling.y) / scaled_grid_step;
      int tile_id = tile_x + (tile_y * 16);

      char hex_id[8];
      snprintf(hex_id, sizeof(hex_id), "%02X", tile_id);

      draw_list->AddText(ImVec2(canvas_p0.x + x + (scaled_grid_step / 2) - 4,
                                canvas_p0.y + y + (scaled_grid_step / 2) - 4),
                         IM_COL32(255, 255, 255, 255), hex_id);
    }
  }
}

// Layout and interaction utilities
ImVec2 CalculateCanvasSize(ImVec2 content_region, ImVec2 custom_size,
                           bool use_custom) {
  return use_custom ? custom_size : content_region;
}

ImVec2 CalculateScaledCanvasSize(ImVec2 canvas_size, float global_scale) {
  return ImVec2(canvas_size.x * global_scale, canvas_size.y * global_scale);
}

bool IsPointInCanvas(ImVec2 point, ImVec2 canvas_p0, ImVec2 canvas_p1) {
  return point.x >= canvas_p0.x && point.x <= canvas_p1.x &&
         point.y >= canvas_p0.y && point.y <= canvas_p1.y;
}

// Size reporting for ImGui table integration
ImVec2 CalculateMinimumCanvasSize(ImVec2 content_size, float global_scale,
                                  float padding) {
  // Calculate minimum size needed to display content with padding
  ImVec2 min_size = ImVec2(content_size.x * global_scale + padding * 2,
                           content_size.y * global_scale + padding * 2);

  // Ensure minimum practical size
  min_size.x = std::max(min_size.x, 64.0f);
  min_size.y = std::max(min_size.y, 64.0f);

  return min_size;
}

ImVec2 CalculatePreferredCanvasSize(ImVec2 content_size, float global_scale,
                                    float min_scale) {
  // Calculate preferred size with minimum scale constraint
  float effective_scale = std::max(global_scale, min_scale);
  ImVec2 preferred_size = ImVec2(content_size.x * effective_scale + 8.0f,
                                 content_size.y * effective_scale + 8.0f);

  // Cap to reasonable maximum sizes for table integration
  preferred_size.x = std::min(preferred_size.x, 800.0f);
  preferred_size.y = std::min(preferred_size.y, 600.0f);

  return preferred_size;
}

void ReserveCanvasSpace(ImVec2 canvas_size, const std::string& label) {
  // Reserve space in ImGui layout so tables know the size
  if (!label.empty()) {
    ImGui::Text("%s", label.c_str());
  }
  ImGui::Dummy(canvas_size);
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() -
                       canvas_size.x);  // Move back to start
}

void SetNextCanvasSize(ImVec2 size, bool auto_resize) {
  if (auto_resize) {
    // Use auto-sizing child window for table integration
    ImGui::SetNextWindowContentSize(size);
  } else {
    // Fixed size
    ImGui::SetNextWindowSize(size);
  }
}

// High-level composite operations
void DrawCanvasGrid(const CanvasRenderContext& ctx, int highlight_tile_id) {
  if (!ctx.enable_grid)
    return;

  ctx.draw_list->PushClipRect(ctx.canvas_p0, ctx.canvas_p1, true);

  // Draw grid lines
  DrawCanvasGridLines(ctx.draw_list, ctx.canvas_p0, ctx.canvas_p1,
                      ctx.scrolling, ctx.grid_step, ctx.global_scale);

  // Draw highlight if specified
  if (highlight_tile_id != -1) {
    DrawCustomHighlight(ctx.draw_list, ctx.canvas_p0, ctx.scrolling,
                        highlight_tile_id, ctx.grid_step * ctx.global_scale);
  }

  // Draw hex labels if enabled
  if (ctx.enable_hex_labels) {
    DrawHexTileLabels(ctx.draw_list, ctx.canvas_p0, ctx.scrolling,
                      ImVec2(ctx.canvas_p1.x - ctx.canvas_p0.x,
                             ctx.canvas_p1.y - ctx.canvas_p0.y),
                      ctx.grid_step, ctx.global_scale);
  }

  ctx.draw_list->PopClipRect();
}

void DrawCanvasOverlay(const CanvasRenderContext& ctx,
                       const ImVector<ImVec2>& points,
                       const ImVector<ImVec2>& selected_points) {
  const ImVec2 origin(ctx.canvas_p0.x + ctx.scrolling.x,
                      ctx.canvas_p0.y + ctx.scrolling.y);

  // Draw hover points
  for (int n = 0; n < points.Size; n += 2) {
    ctx.draw_list->AddRect(
        ImVec2(origin.x + points[n].x, origin.y + points[n].y),
        ImVec2(origin.x + points[n + 1].x, origin.y + points[n + 1].y),
        IM_COL32(255, 255, 255, 255), 1.0f);
  }

  // Draw selection rectangles
  if (!selected_points.empty()) {
    for (int n = 0; n < selected_points.size(); n += 2) {
      ctx.draw_list->AddRect(ImVec2(origin.x + selected_points[n].x,
                                    origin.y + selected_points[n].y),
                             ImVec2(origin.x + selected_points[n + 1].x + 0x10,
                                    origin.y + selected_points[n + 1].y + 0x10),
                             IM_COL32(255, 255, 255, 255), 1.0f);
    }
  }
}

void DrawCanvasLabels(const CanvasRenderContext& ctx,
                      const ImVector<ImVector<std::string>>& labels,
                      int current_labels, int tile_id_offset) {
  if (current_labels >= labels.size())
    return;

  float scaled_grid_step = ctx.grid_step * ctx.global_scale;

  for (float x = fmodf(ctx.scrolling.x, scaled_grid_step);
       x < (ctx.canvas_p1.x - ctx.canvas_p0.x); x += scaled_grid_step) {
    for (float y = fmodf(ctx.scrolling.y, scaled_grid_step);
         y < (ctx.canvas_p1.y - ctx.canvas_p0.y); y += scaled_grid_step) {
      int tile_x = (x - ctx.scrolling.x) / scaled_grid_step;
      int tile_y = (y - ctx.scrolling.y) / scaled_grid_step;
      int tile_id = tile_x + (tile_y * tile_id_offset);

      if (tile_id >= labels[current_labels].size()) {
        break;
      }

      const std::string& label = labels[current_labels][tile_id];
      ctx.draw_list->AddText(
          ImVec2(ctx.canvas_p0.x + x + (scaled_grid_step / 2) - tile_id_offset,
                 ctx.canvas_p0.y + y + (scaled_grid_step / 2) - tile_id_offset),
          IM_COL32(255, 255, 255, 255), label.c_str());
    }
  }
}

}  // namespace CanvasUtils

// CanvasConfig theme-aware methods implementation
float CanvasConfig::GetToolbarHeight() const {
  if (!use_theme_sizing) {
    return 32.0f;  // Legacy fixed height
  }

  // Use layout helpers for theme-aware sizing
  // We need to include layout_helpers.h in the implementation file
  // For now, return a reasonable default that respects ImGui font size
  return ImGui::GetFontSize() * 0.75f;  // Will be replaced with LayoutHelpers call
}

float CanvasConfig::GetGridSpacing() const {
  if (!use_theme_sizing) {
    return grid_step;  // Use configured grid_step as-is
  }

  // Apply minimal theme-aware adjustment based on font size
  // Grid should stay consistent, but scale slightly with UI density
  float base_spacing = ImGui::GetFontSize() * 0.5f;
  return std::max(grid_step, base_spacing);
}

}  // namespace gui
}  // namespace yaze
