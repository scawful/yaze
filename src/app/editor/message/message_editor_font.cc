#include "message_editor.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "imgui.h"
#include "util/log.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

void MessageEditor::ResolveFontPalette() {
  if (game_data() && !game_data()->palette_groups.hud.empty()) {
    font_preview_colors_ = game_data()->palette_groups.hud.palette(0);
  }

  if (font_preview_colors_.empty()) {
    font_preview_colors_ = BuildFallbackFontPalette();
  }
}

gfx::SnesPalette MessageEditor::BuildFallbackFontPalette() const {
  std::vector<gfx::SnesColor> colors;
  colors.reserve(16);
  for (int i = 0; i < 16; ++i) {
    const float value = static_cast<float>(i) / 15.0f;
    colors.emplace_back(ImVec4(value, value, value, 1.0f));
  }

  if (!colors.empty()) {
    colors[0].set_transparent(true);
  }

  return gfx::SnesPalette(colors);
}

void MessageEditor::LoadFontGraphics() {
  ResolveFontPalette();
  if (!rom() || !rom()->is_loaded()) {
    LOG_WARN("MessageEditor", "ROM not loaded - skipping font graphics load");
    return;
  }

  std::fill(raw_font_gfx_data_.begin(), raw_font_gfx_data_.end(), 0);

  const size_t rom_size = rom()->size();
  if (rom_size > static_cast<size_t>(kGfxFont)) {
    const size_t available = std::min(raw_font_gfx_data_.size(),
                                      rom_size - static_cast<size_t>(kGfxFont));
    std::copy_n(rom()->data() + kGfxFont, available,
                raw_font_gfx_data_.begin());
    if (available < raw_font_gfx_data_.size()) {
      LOG_WARN("MessageEditor",
               "Font graphics truncated (ROM size %zu, read %zu bytes)",
               rom_size, available);
    }
  } else {
    LOG_WARN("MessageEditor",
             "ROM size %zu too small for font graphics offset 0x%X", rom_size,
             kGfxFont);
  }

  message_preview_.font_gfx16_data_ =
      gfx::SnesTo8bppSheet(raw_font_gfx_data_, /*bpp=*/2, /*num_sheets=*/2);

  auto load_font = zelda3::LoadFontGraphics(*rom());
  if (load_font.ok()) {
    message_preview_.font_gfx16_data_2_ = load_font.value().vector();
  } else {
    const std::string error_message(load_font.status().message());
    LOG_WARN("MessageEditor", "LoadFontGraphics failed: %s",
             error_message.c_str());
  }

  const auto& font_data = !message_preview_.font_gfx16_data_.empty()
                              ? message_preview_.font_gfx16_data_
                              : message_preview_.font_gfx16_data_2_;
  RefreshFontAtlasBitmap(font_data);
  font_graphics_loaded_ = true;
}

void MessageEditor::RefreshFontAtlasBitmap(
    const std::vector<uint8_t>& font_data) {
  if (font_data.empty()) {
    LOG_WARN("MessageEditor", "Font graphics data missing - atlas stays empty");
    return;
  }

  const int atlas_width = kFontGfxMessageSize;
  const size_t row_count = (font_data.size() + atlas_width - 1) / atlas_width;
  const int atlas_height = static_cast<int>(std::max<size_t>(1, row_count));

  const size_t expected_size = static_cast<size_t>(atlas_width) * atlas_height;
  std::vector<uint8_t> padded(font_data.begin(), font_data.end());
  if (padded.size() < expected_size) {
    padded.resize(expected_size, 0);
  } else if (padded.size() > expected_size) {
    padded.resize(expected_size);
  }

  font_gfx_bitmap_.Create(atlas_width, atlas_height, kFontGfxMessageDepth,
                          padded);
  if (!font_preview_colors_.empty()) {
    font_gfx_bitmap_.SetPalette(font_preview_colors_);
  }
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &font_gfx_bitmap_);
}

void MessageEditor::ApplyFontPalette() {
  ResolveFontPalette();

  if (font_preview_colors_.empty()) {
    return;
  }

  auto queue_refresh = [](gfx::Bitmap& bitmap) {
    if (!bitmap.is_active()) {
      return;
    }
    const auto command = bitmap.texture()
                             ? gfx::Arena::TextureCommandType::UPDATE
                             : gfx::Arena::TextureCommandType::CREATE;
    gfx::Arena::Get().QueueTextureCommand(command, &bitmap);
  };

  font_gfx_bitmap_.SetPalette(font_preview_colors_);
  queue_refresh(font_gfx_bitmap_);

  if (current_font_gfx16_bitmap_.is_active()) {
    current_font_gfx16_bitmap_.SetPalette(font_preview_colors_);
    queue_refresh(current_font_gfx16_bitmap_);
  }
}

void MessageEditor::EnsureFontTexturesReady() {
  if (!font_graphics_loaded_) {
    LoadFontGraphics();
  }
  if (font_gfx_bitmap_.is_active() && !font_gfx_bitmap_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &font_gfx_bitmap_);
  }
  if (!current_font_gfx16_bitmap_.is_active() &&
      !current_message_.Data.empty()) {
    DrawMessagePreview();
  }
  if (current_font_gfx16_bitmap_.is_active() &&
      !current_font_gfx16_bitmap_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_font_gfx16_bitmap_);
  }
}

}  // namespace yaze::editor
