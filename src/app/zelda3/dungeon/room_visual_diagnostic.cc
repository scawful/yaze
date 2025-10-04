#include "app/zelda3/dungeon/room_visual_diagnostic.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_tile.h"
#include "imgui/imgui.h"

namespace yaze {
namespace zelda3 {
namespace dungeon {

void RoomVisualDiagnostic::DrawDiagnosticWindow(
    bool* p_open,
    gfx::BackgroundBuffer& bg1_buffer,
    gfx::BackgroundBuffer& bg2_buffer,
    const gfx::SnesPalette& palette,
    const std::vector<uint8_t>& gfx16_data) {
  
  if (!ImGui::Begin("Room Rendering Diagnostic", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  if (ImGui::CollapsingHeader("Texture Previews", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawTexturePreview(bg1_buffer.bitmap(), "BG1 Texture");
    ImGui::Separator();
    DrawTexturePreview(bg2_buffer.bitmap(), "BG2 Texture");
  }

  if (ImGui::CollapsingHeader("Palette Inspector")) {
    DrawPaletteInspector(palette);
  }

  if (ImGui::CollapsingHeader("Tile Buffer Inspector")) {
    if (ImGui::BeginTabBar("BufferTabs")) {
      if (ImGui::BeginTabItem("BG1 Buffer")) {
        DrawTileBufferInspector(bg1_buffer, palette);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("BG2 Buffer")) {
        DrawTileBufferInspector(bg2_buffer, palette);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

  if (ImGui::CollapsingHeader("Pixel Inspector")) {
    if (ImGui::BeginTabBar("PixelTabs")) {
      if (ImGui::BeginTabItem("BG1 Pixels")) {
        DrawPixelInspector(bg1_buffer.bitmap(), palette);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("BG2 Pixels")) {
        DrawPixelInspector(bg2_buffer.bitmap(), palette);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

  if (ImGui::CollapsingHeader("Tile Decoder")) {
    static int test_tile_id = 0xEE;
    ImGui::InputInt("Tile ID (hex)", &test_tile_id, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    test_tile_id = std::clamp(test_tile_id, 0, 0x1FF);
    DrawTileDecoder(gfx16_data, test_tile_id);
  }

  ImGui::End();
}

void RoomVisualDiagnostic::DrawTexturePreview(const gfx::Bitmap& bitmap, const char* label) {
  ImGui::Text("%s", label);
  
  if (!bitmap.is_active()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Bitmap not active");
    return;
  }

  ImGui::Text("Size: %dx%d, Data: %zu bytes", 
              bitmap.width(), bitmap.height(), bitmap.vector().size());
  
  // Show texture if available
  if (bitmap.texture()) {
    ImVec2 preview_size(256, 256);  // Quarter size preview
    ImGui::Image(bitmap.texture(), preview_size);
    
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      // Zoomed preview on hover
      ImVec2 zoom_size(512, 512);
      ImGui::Image(bitmap.texture(), zoom_size);
      ImGui::EndTooltip();
    }
  } else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "No texture available");
  }
}

void RoomVisualDiagnostic::DrawPaletteInspector(const gfx::SnesPalette& palette) {
  ImGui::Text("Palette size: %zu colors", palette.size());
  
  int cols = 16;
  for (size_t i = 0; i < palette.size(); i++) {
    if (i % cols != 0) ImGui::SameLine();
    
    auto color = palette[i];
    auto rgb = color.rgb();
    ImVec4 imcolor(rgb.x, rgb.y, rgb.z, 1.0f);
    
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::ColorButton("##color", imcolor, 
                           ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
                           ImVec2(20, 20))) {
      // Clicked - could add inspection
    }
    ImGui::PopID();
    
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("[%zu] SNES:0x%04X RGB:(%.0f,%.0f,%.0f)", 
                        i, color.snes(), 
                        rgb.x * 255, rgb.y * 255, rgb.z * 255);
    }
  }
}

void RoomVisualDiagnostic::DrawTileBufferInspector(gfx::BackgroundBuffer& buffer,
                                                     const gfx::SnesPalette& palette) {
  const auto& tile_buffer = buffer.buffer();
  const auto& bitmap = buffer.bitmap();
  int tiles_w = bitmap.width() / 8;
  int tiles_h = bitmap.height() / 8;
  
  ImGui::Text("Buffer size: %zu tiles (%d x %d)", tile_buffer.size(), tiles_w, tiles_h);
  
  // Count non-empty tiles
  int non_empty = 0;
  std::set<uint16_t> unique_tiles;
  for (auto word : tile_buffer) {
    if (word != 0xFFFF && word != 0) {
      non_empty++;
      unique_tiles.insert(word);
    }
  }
  
  ImGui::Text("Non-empty tiles: %d / %zu", non_empty, tile_buffer.size());
  ImGui::Text("Unique tile words: %zu", unique_tiles.size());
  
  // Sample tiles
  ImGui::Separator();
  ImGui::Text("First 20 tiles:");
  
  static int selected_tile = -1;
  for (int i = 0; i < std::min<int>(20, tile_buffer.size()); i++) {
    uint16_t word = tile_buffer[i];
    auto tile = gfx::WordToTileInfo(word);
    
    bool is_selected = (selected_tile == i);
    if (ImGui::Selectable(absl::StrFormat("[%d] Word:0x%04X ID:%d Pal:%d H:%d V:%d P:%d",
                                          i, word, tile.id_, tile.palette_,
                                          tile.horizontal_mirror_, tile.vertical_mirror_,
                                          tile.over_).c_str(),
                          is_selected)) {
      selected_tile = i;
    }
  }
  
  if (selected_tile >= 0 && selected_tile < tile_buffer.size()) {
    ImGui::Separator();
    ImGui::Text("Selected Tile %d:", selected_tile);
    uint16_t word = tile_buffer[selected_tile];
    auto tile = gfx::WordToTileInfo(word);
    
    ImGui::BulletText("Word: 0x%04X", word);
    ImGui::BulletText("Tile ID: 0x%03X (%d)", tile.id_, tile.id_);
    ImGui::BulletText("Palette: %d", tile.palette_);
    ImGui::BulletText("H-Mirror: %d, V-Mirror: %d", 
                      tile.horizontal_mirror_, tile.vertical_mirror_);
    ImGui::BulletText("Priority: %d", tile.over_);
    
    // Calculate palette color range for this tile
    int pal_start = tile.palette_ * 16;
    int pal_end = pal_start + 15;
    ImGui::BulletText("Palette range: colors %d-%d", pal_start, pal_end);
    
    if (pal_end >= palette.size()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), 
                         "WARNING: Palette range exceeds palette size (%zu)!", 
                         palette.size());
    }
  }
}

void RoomVisualDiagnostic::DrawPixelInspector(const gfx::Bitmap& bitmap, 
                                               const gfx::SnesPalette& palette) {
  if (!bitmap.is_active() || bitmap.vector().empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Bitmap not active or empty");
    return;
  }
  
  const auto& pixels = bitmap.vector();
  
  // Analyze pixels
  std::map<uint8_t, int> color_histogram;
  for (size_t i = 0; i < std::min<size_t>(1000, pixels.size()); i++) {
    color_histogram[pixels[i]]++;
  }
  
  ImGui::Text("Pixel analysis (first 1000 pixels):");
  ImGui::Text("Unique colors: %zu", color_histogram.size());
  
  ImGui::Separator();
  ImGui::Text("Color distribution:");
  
  // Show top 10 most common colors
  std::vector<std::pair<uint8_t, int>> sorted_colors(color_histogram.begin(), color_histogram.end());
  std::sort(sorted_colors.begin(), sorted_colors.end(), 
            [](const auto& a, const auto& b) { return a.second > b.second; });
  
  for (size_t i = 0; i < std::min<size_t>(10, sorted_colors.size()); i++) {
    uint8_t idx = sorted_colors[i].first;
    int count = sorted_colors[i].second;
    
    ImGui::Text("[%3d] Count: %4d (%.1f%%)", idx, count, (count * 100.0f) / 1000.0f);
    
    if (idx < palette.size()) {
      ImGui::SameLine();
      auto color = palette[idx];
      auto rgb = color.rgb();
      ImVec4 imcolor(rgb.x, rgb.y, rgb.z, 1.0f);
      ImGui::ColorButton("##pal", imcolor, ImGuiColorEditFlags_NoAlpha, ImVec2(16, 16));
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("SNES:0x%04X RGB:(%.0f,%.0f,%.0f)", 
                          color.snes(), rgb.x * 255, rgb.y * 255, rgb.z * 255);
      }
    } else {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "OUT OF RANGE!");
    }
  }
  
  // Interactive pixel inspector
  ImGui::Separator();
  static int inspect_x = 0;
  static int inspect_y = 0;
  
  ImGui::Text("Pixel Inspector:");
  ImGui::SliderInt("X", &inspect_x, 0, bitmap.width() - 1);
  ImGui::SliderInt("Y", &inspect_y, 0, bitmap.height() - 1);
  
  int pixel_idx = inspect_y * bitmap.width() + inspect_x;
  if (pixel_idx >= 0 && pixel_idx < pixels.size()) {
    uint8_t pal_idx = pixels[pixel_idx];
    ImGui::Text("Pixel (%d, %d) = Palette Index %d", inspect_x, inspect_y, pal_idx);
    
    if (pal_idx < palette.size()) {
      auto color = palette[pal_idx];
      auto rgb = color.rgb();
      ImVec4 imcolor(rgb.x, rgb.y, rgb.z, 1.0f);
      ImGui::ColorButton("##pixel_color", imcolor, ImGuiColorEditFlags_NoAlpha, ImVec2(40, 40));
      ImGui::SameLine();
      ImGui::Text("SNES:0x%04X RGB:(%.0f,%.0f,%.0f)", 
                  color.snes(), rgb.x * 255, rgb.y * 255, rgb.z * 255);
    } else {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Palette index %d out of range (max: %zu)!", 
                         pal_idx, palette.size() - 1);
    }
  }
}

void RoomVisualDiagnostic::DrawTileDecoder(const std::vector<uint8_t>& gfx16_data, int tile_id) {
  if (gfx16_data.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No graphics data");
    return;
  }
  
  // Calculate tile offset in graphics data
  int tx = (tile_id / 16 * 512) + ((tile_id & 0xF) * 4);
  
  ImGui::Text("Decoding tile 0x%03X", tile_id);
  ImGui::Text("Offset in gfx16_data: 0x%X (%d)", tx, tx);
  
  if (tx < 0 || tx + 32 > gfx16_data.size()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Tile offset out of range!");
    return;
  }
  
  // Show raw tile data
  ImGui::Text("Raw 4bpp data (32 bytes):");
  for (int i = 0; i < 32; i += 8) {
    ImGui::Text("%02X %02X %02X %02X %02X %02X %02X %02X",
                gfx16_data[tx + i], gfx16_data[tx + i + 1],
                gfx16_data[tx + i + 2], gfx16_data[tx + i + 3],
                gfx16_data[tx + i + 4], gfx16_data[tx + i + 5],
                gfx16_data[tx + i + 6], gfx16_data[tx + i + 7]);
  }
  
  // Decode and visualize 8x8 tile
  ImGui::Separator();
  ImGui::Text("Decoded 8x8 pixel values (palette 0):");
  
  // Decode the tile manually to show what pixels are produced
  for (int y = 0; y < 8; y++) {
    std::string line;
    for (int x = 0; x < 8; x++) {
      int yl = (y / 8) * 64 + (y % 8) * 8;
      int xl = x / 2;
      
      if (tx + yl + xl < gfx16_data.size()) {
        uint8_t pixel_byte = gfx16_data[tx + yl + xl];
        uint8_t pixel_val = (x % 2 == 0) ? (pixel_byte >> 4) : (pixel_byte & 0x0F);
        line += absl::StrFormat("%X", pixel_val);
      } else {
        line += "?";
      }
    }
    ImGui::Text("%s", line.c_str());
  }
}

}  // namespace dungeon
}  // namespace zelda3
}  // namespace yaze

