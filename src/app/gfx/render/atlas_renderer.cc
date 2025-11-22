#include "app/gfx/render/atlas_renderer.h"

#include <algorithm>
#include <cmath>

#include "app/gfx/util/bpp_format_manager.h"

namespace yaze {
namespace gfx {

AtlasRenderer& AtlasRenderer::Get() {
  static AtlasRenderer instance;
  return instance;
}

void AtlasRenderer::Initialize(IRenderer* renderer, int initial_size) {
  renderer_ = renderer;
  next_atlas_id_ = 0;
  current_atlas_ = 0;

  // Clear any existing atlases
  Clear();

  // Create initial atlas
  CreateNewAtlas();
}

int AtlasRenderer::AddBitmap(const Bitmap& bitmap) {
  if (!bitmap.is_active() || !bitmap.texture()) {
    return -1;  // Invalid bitmap
  }

  ScopedTimer timer("atlas_add_bitmap");

  // Try to pack into current atlas
  SDL_Rect uv_rect;
  if (PackBitmap(*atlases_[current_atlas_], bitmap, uv_rect)) {
    int atlas_id = next_atlas_id_++;
    auto& atlas = *atlases_[current_atlas_];

    // Copy bitmap data to atlas texture
    renderer_->SetRenderTarget(atlas.texture);
    renderer_->RenderCopy(bitmap.texture(), nullptr, &uv_rect);
    renderer_->SetRenderTarget(nullptr);

    return atlas_id;
  }

  // Current atlas is full, create new one
  CreateNewAtlas();
  if (PackBitmap(*atlases_[current_atlas_], bitmap, uv_rect)) {
    int atlas_id = next_atlas_id_++;
    auto& atlas = *atlases_[current_atlas_];

    BppFormat bpp_format = BppFormatManager::Get().DetectFormat(
        bitmap.vector(), bitmap.width(), bitmap.height());
    atlas.entries.emplace_back(atlas_id, uv_rect, bitmap.texture(), bpp_format,
                               bitmap.width(), bitmap.height());
    atlas_lookup_[atlas_id] = &atlas.entries.back();

    // Copy bitmap data to atlas texture
    renderer_->SetRenderTarget(atlas.texture);
    renderer_->RenderCopy(bitmap.texture(), nullptr, &uv_rect);
    renderer_->SetRenderTarget(nullptr);

    return atlas_id;
  }

  return -1;  // Failed to add
}

int AtlasRenderer::AddBitmapWithBppOptimization(const Bitmap& bitmap,
                                                BppFormat target_bpp) {
  if (!bitmap.is_active() || !bitmap.texture()) {
    return -1;  // Invalid bitmap
  }

  ScopedTimer timer("atlas_add_bitmap_bpp_optimized");

  // Detect current BPP format
  BppFormat current_bpp = BppFormatManager::Get().DetectFormat(
      bitmap.vector(), bitmap.width(), bitmap.height());

  // If formats match, use standard addition
  if (current_bpp == target_bpp) {
    return AddBitmap(bitmap);
  }

  // Convert bitmap to target BPP format
  auto converted_data = BppFormatManager::Get().ConvertFormat(
      bitmap.vector(), current_bpp, target_bpp, bitmap.width(),
      bitmap.height());

  // Create temporary bitmap with converted data
  Bitmap converted_bitmap(bitmap.width(), bitmap.height(), bitmap.depth(),
                          converted_data, bitmap.palette());
  converted_bitmap.CreateTexture();

  // Add converted bitmap to atlas
  return AddBitmap(converted_bitmap);
}

void AtlasRenderer::RemoveBitmap(int atlas_id) {
  auto it = atlas_lookup_.find(atlas_id);
  if (it == atlas_lookup_.end()) {
    return;
  }

  AtlasEntry* entry = it->second;
  entry->in_use = false;

  // Mark region as free
  for (auto& atlas : atlases_) {
    for (auto& atlas_entry : atlas->entries) {
      if (atlas_entry.atlas_id == atlas_id) {
        MarkRegionUsed(*atlas, atlas_entry.uv_rect, false);
        break;
      }
    }
  }

  atlas_lookup_.erase(it);
}

void AtlasRenderer::UpdateBitmap(int atlas_id, const Bitmap& bitmap) {
  auto it = atlas_lookup_.find(atlas_id);
  if (it == atlas_lookup_.end()) {
    return;
  }

  AtlasEntry* entry = it->second;
  entry->texture = bitmap.texture();

  // Update UV coordinates if size changed
  if (bitmap.width() != entry->uv_rect.w ||
      bitmap.height() != entry->uv_rect.h) {
    // Remove old entry and add new one
    RemoveBitmap(atlas_id);
    AddBitmap(bitmap);
  }
}

void AtlasRenderer::RenderBatch(
    const std::vector<RenderCommand>& render_commands) {
  if (render_commands.empty()) {
    return;
  }

  ScopedTimer timer("atlas_batch_render");

  // Group commands by atlas for efficient rendering
  std::unordered_map<int, std::vector<const RenderCommand*>> atlas_groups;

  for (const auto& cmd : render_commands) {
    auto it = atlas_lookup_.find(cmd.atlas_id);
    if (it != atlas_lookup_.end() && it->second->in_use) {
      // Find which atlas contains this entry
      for (size_t i = 0; i < atlases_.size(); ++i) {
        for (const auto& entry : atlases_[i]->entries) {
          if (entry.atlas_id == cmd.atlas_id) {
            atlas_groups[i].push_back(&cmd);
            break;
          }
        }
      }
    }
  }

  // Render each atlas group
  for (const auto& [atlas_index, commands] : atlas_groups) {
    if (commands.empty())
      continue;

    auto& atlas = *atlases_[atlas_index];

    // Set atlas texture
    // SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);

    // Render all commands for this atlas
    for (const auto* cmd : commands) {
      auto it = atlas_lookup_.find(cmd->atlas_id);
      if (it == atlas_lookup_.end())
        continue;

      AtlasEntry* entry = it->second;

      // Calculate destination rectangle
      SDL_Rect dest_rect = {static_cast<int>(cmd->x), static_cast<int>(cmd->y),
                            static_cast<int>(entry->uv_rect.w * cmd->scale_x),
                            static_cast<int>(entry->uv_rect.h * cmd->scale_y)};

      // Apply rotation if needed
      if (std::abs(cmd->rotation) > 0.001F) {
        // For rotation, we'd need to use SDL_RenderCopyEx
        // This is a simplified version
        renderer_->RenderCopy(atlas.texture, &entry->uv_rect, &dest_rect);
      } else {
        renderer_->RenderCopy(atlas.texture, &entry->uv_rect, &dest_rect);
      }
    }
  }
}

void AtlasRenderer::RenderBatchWithBppOptimization(
    const std::vector<RenderCommand>& render_commands,
    const std::unordered_map<BppFormat, std::vector<int>>& bpp_groups) {
  if (render_commands.empty()) {
    return;
  }

  ScopedTimer timer("atlas_batch_render_bpp_optimized");

  // Render each BPP group separately for optimal performance
  for (const auto& [bpp_format, command_indices] : bpp_groups) {
    if (command_indices.empty())
      continue;

    // Group commands by atlas for this BPP format
    std::unordered_map<int, std::vector<const RenderCommand*>> atlas_groups;

    for (int cmd_index : command_indices) {
      if (cmd_index >= 0 &&
          cmd_index < static_cast<int>(render_commands.size())) {
        const auto& cmd = render_commands[cmd_index];
        auto it = atlas_lookup_.find(cmd.atlas_id);
        if (it != atlas_lookup_.end() && it->second->in_use &&
            it->second->bpp_format == bpp_format) {
          // Find which atlas contains this entry
          for (size_t i = 0; i < atlases_.size(); ++i) {
            for (const auto& entry : atlases_[i]->entries) {
              if (entry.atlas_id == cmd.atlas_id) {
                atlas_groups[i].push_back(&cmd);
                break;
              }
            }
          }
        }
      }
    }

    // Render each atlas group for this BPP format
    for (const auto& [atlas_index, commands] : atlas_groups) {
      if (commands.empty())
        continue;

      auto& atlas = *atlases_[atlas_index];

      // Set atlas texture with BPP-specific blend mode
      // SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);

      // Render all commands for this atlas and BPP format
      for (const auto* cmd : commands) {
        auto it = atlas_lookup_.find(cmd->atlas_id);
        if (it == atlas_lookup_.end())
          continue;

        AtlasEntry* entry = it->second;

        // Calculate destination rectangle
        SDL_Rect dest_rect = {
            static_cast<int>(cmd->x), static_cast<int>(cmd->y),
            static_cast<int>(entry->uv_rect.w * cmd->scale_x),
            static_cast<int>(entry->uv_rect.h * cmd->scale_y)};

        // Apply rotation if needed
        if (std::abs(cmd->rotation) > 0.001F) {
          renderer_->RenderCopy(atlas.texture, &entry->uv_rect, &dest_rect);
        } else {
          renderer_->RenderCopy(atlas.texture, &entry->uv_rect, &dest_rect);
        }
      }
    }
  }
}

AtlasStats AtlasRenderer::GetStats() const {
  AtlasStats stats;

  stats.total_atlases = atlases_.size();

  for (const auto& atlas : atlases_) {
    stats.total_entries += atlas->entries.size();
    stats.used_entries +=
        std::count_if(atlas->entries.begin(), atlas->entries.end(),
                      [](const AtlasEntry& entry) { return entry.in_use; });

    // Calculate memory usage (simplified)
    stats.total_memory += atlas->size * atlas->size * 4;  // RGBA8888
  }

  if (stats.total_entries > 0) {
    stats.utilization_percent =
        (static_cast<float>(stats.used_entries) / stats.total_entries) * 100.0F;
  }

  return stats;
}

void AtlasRenderer::Defragment() {
  ScopedTimer timer("atlas_defragment");

  for (auto& atlas : atlases_) {
    // Remove unused entries
    atlas->entries.erase(
        std::remove_if(atlas->entries.begin(), atlas->entries.end(),
                       [](const AtlasEntry& entry) { return !entry.in_use; }),
        atlas->entries.end());

    // Rebuild atlas texture
    RebuildAtlas(*atlas);
  }
}

void AtlasRenderer::Clear() {
  // Clean up SDL textures
  for (auto& atlas : atlases_) {
    if (atlas->texture) {
      renderer_->DestroyTexture(atlas->texture);
    }
  }

  atlases_.clear();
  atlas_lookup_.clear();
  next_atlas_id_ = 0;
  current_atlas_ = 0;
}

AtlasRenderer::~AtlasRenderer() {
  Clear();
}

void AtlasRenderer::RenderBitmap(int atlas_id, float x, float y, float scale_x,
                                 float scale_y) {
  auto it = atlas_lookup_.find(atlas_id);
  if (it == atlas_lookup_.end() || !it->second->in_use) {
    return;
  }

  AtlasEntry* entry = it->second;

  // Find which atlas contains this entry
  for (auto& atlas : atlases_) {
    for (const auto& atlas_entry : atlas->entries) {
      if (atlas_entry.atlas_id == atlas_id) {
        // Calculate destination rectangle
        SDL_Rect dest_rect = {static_cast<int>(x), static_cast<int>(y),
                              static_cast<int>(entry->uv_rect.w * scale_x),
                              static_cast<int>(entry->uv_rect.h * scale_y)};

        // Render using atlas texture
        // SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
        renderer_->RenderCopy(atlas->texture, &entry->uv_rect, &dest_rect);
        return;
      }
    }
  }
}

SDL_Rect AtlasRenderer::GetUVCoordinates(int atlas_id) const {
  auto it = atlas_lookup_.find(atlas_id);
  if (it == atlas_lookup_.end() || !it->second->in_use) {
    return {0, 0, 0, 0};
  }

  return it->second->uv_rect;
}

bool AtlasRenderer::PackBitmap(Atlas& atlas, const Bitmap& bitmap,
                               SDL_Rect& uv_rect) {
  int width = bitmap.width();
  int height = bitmap.height();

  // Find free region
  SDL_Rect free_rect = FindFreeRegion(atlas, width, height);
  if (free_rect.w == 0 || free_rect.h == 0) {
    return false;  // No space available
  }

  // Mark region as used
  MarkRegionUsed(atlas, free_rect, true);

  // Set UV coordinates (normalized to 0-1 range)
  uv_rect = {free_rect.x, free_rect.y, width, height};

  return true;
}

void AtlasRenderer::CreateNewAtlas() {
  int size = 1024;  // Default size
  if (!atlases_.empty()) {
    size = atlases_.back()->size * 2;  // Double size for new atlas
  }

  atlases_.push_back(std::make_unique<Atlas>(size));
  current_atlas_ = atlases_.size() - 1;

  // Create SDL texture for the atlas
  auto& atlas = *atlases_[current_atlas_];
  atlas.texture = renderer_->CreateTexture(size, size);

  if (!atlas.texture) {
    SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
  }
}

void AtlasRenderer::RebuildAtlas(Atlas& atlas) {
  // Clear used regions
  std::fill(atlas.used_regions.begin(), atlas.used_regions.end(), false);

  // Rebuild atlas texture by copying from source textures
  renderer_->SetRenderTarget(atlas.texture);
  renderer_->SetDrawColor({0, 0, 0, 0});
  renderer_->Clear();

  for (auto& entry : atlas.entries) {
    if (entry.in_use && entry.texture) {
      renderer_->RenderCopy(entry.texture, nullptr, &entry.uv_rect);
      MarkRegionUsed(atlas, entry.uv_rect, true);
    }
  }

  renderer_->SetRenderTarget(nullptr);
}

SDL_Rect AtlasRenderer::FindFreeRegion(Atlas& atlas, int width, int height) {
  // Simple first-fit algorithm
  for (int y = 0; y <= atlas.size - height; ++y) {
    for (int x = 0; x <= atlas.size - width; ++x) {
      bool can_fit = true;

      // Check if region is free
      for (int dy = 0; dy < height && can_fit; ++dy) {
        for (int dx = 0; dx < width && can_fit; ++dx) {
          int index = (y + dy) * atlas.size + (x + dx);
          if (index >= static_cast<int>(atlas.used_regions.size()) ||
              atlas.used_regions[index]) {
            can_fit = false;
          }
        }
      }

      if (can_fit) {
        return {x, y, width, height};
      }
    }
  }

  return {0, 0, 0, 0};  // No space found
}

void AtlasRenderer::MarkRegionUsed(Atlas& atlas, const SDL_Rect& rect,
                                   bool used) {
  for (int y = rect.y; y < rect.y + rect.h; ++y) {
    for (int x = rect.x; x < rect.x + rect.w; ++x) {
      int index = y * atlas.size + x;
      if (index >= 0 && index < static_cast<int>(atlas.used_regions.size())) {
        atlas.used_regions[index] = used;
      }
    }
  }
}

}  // namespace gfx
}  // namespace yaze
