#include "app/gfx/atlas_renderer.h"

#include <algorithm>
#include <cmath>

namespace yaze {
namespace gfx {

AtlasRenderer& AtlasRenderer::Get() {
  static AtlasRenderer instance;
  return instance;
}

void AtlasRenderer::Initialize(SDL_Renderer* renderer, int initial_size) {
  renderer_ = renderer;
  next_atlas_id_ = 0;
  current_atlas_ = 0;
  
  // Create initial atlas
  atlases_.push_back(std::make_unique<Atlas>(initial_size));
  CreateNewAtlas();
}

int AtlasRenderer::AddBitmap(const Bitmap& bitmap) {
  if (!bitmap.is_active() || !bitmap.texture()) {
    return -1; // Invalid bitmap
  }
  
  ScopedTimer timer("atlas_add_bitmap");
  
  // Try to pack into current atlas
  SDL_Rect uv_rect;
  if (PackBitmap(*atlases_[current_atlas_], bitmap, uv_rect)) {
    int atlas_id = next_atlas_id_++;
    auto& atlas = *atlases_[current_atlas_];
    
    // Create atlas entry
    atlas.entries.emplace_back(atlas_id, uv_rect, bitmap.texture());
    atlas_lookup_[atlas_id] = &atlas.entries.back();
    
    return atlas_id;
  }
  
  // Current atlas is full, create new one
  CreateNewAtlas();
  if (PackBitmap(*atlases_[current_atlas_], bitmap, uv_rect)) {
    int atlas_id = next_atlas_id_++;
    auto& atlas = *atlases_[current_atlas_];
    
    atlas.entries.emplace_back(atlas_id, uv_rect, bitmap.texture());
    atlas_lookup_[atlas_id] = &atlas.entries.back();
    
    return atlas_id;
  }
  
  return -1; // Failed to add
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
  if (bitmap.width() != entry->uv_rect.w || bitmap.height() != entry->uv_rect.h) {
    // Remove old entry and add new one
    RemoveBitmap(atlas_id);
    AddBitmap(bitmap);
  }
}

void AtlasRenderer::RenderBatch(const std::vector<RenderCommand>& render_commands) {
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
    if (commands.empty()) continue;
    
    auto& atlas = *atlases_[atlas_index];
    
    // Set atlas texture
    SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);
    
    // Render all commands for this atlas
    for (const auto* cmd : commands) {
      auto it = atlas_lookup_.find(cmd->atlas_id);
      if (it == atlas_lookup_.end()) continue;
      
      AtlasEntry* entry = it->second;
      
      // Calculate destination rectangle
      SDL_Rect dest_rect = {
        static_cast<int>(cmd->x),
        static_cast<int>(cmd->y),
        static_cast<int>(entry->uv_rect.w * cmd->scale_x),
        static_cast<int>(entry->uv_rect.h * cmd->scale_y)
      };
      
      // Apply rotation if needed
      if (std::abs(cmd->rotation) > 0.001f) {
        // For rotation, we'd need to use SDL_RenderCopyEx
        // This is a simplified version
        SDL_RenderCopy(renderer_, atlas.texture, &entry->uv_rect, &dest_rect);
      } else {
        SDL_RenderCopy(renderer_, atlas.texture, &entry->uv_rect, &dest_rect);
      }
    }
  }
}

AtlasStats AtlasRenderer::GetStats() const {
  AtlasStats stats;
  
  stats.total_atlases = atlases_.size();
  
  for (const auto& atlas : atlases_) {
    stats.total_entries += atlas->entries.size();
    stats.used_entries += std::count_if(atlas->entries.begin(), atlas->entries.end(),
                                       [](const AtlasEntry& entry) { return entry.in_use; });
    
    // Calculate memory usage (simplified)
    stats.total_memory += atlas->size * atlas->size * 4; // RGBA8888
  }
  
  if (stats.total_entries > 0) {
    stats.utilization_percent = (static_cast<float>(stats.used_entries) / stats.total_entries) * 100.0f;
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
  atlases_.clear();
  atlas_lookup_.clear();
  next_atlas_id_ = 0;
  current_atlas_ = 0;
}

AtlasRenderer::~AtlasRenderer() {
  Clear();
}

bool AtlasRenderer::PackBitmap(Atlas& atlas, const Bitmap& bitmap, SDL_Rect& uv_rect) {
  int width = bitmap.width();
  int height = bitmap.height();
  
  // Find free region
  SDL_Rect free_rect = FindFreeRegion(atlas, width, height);
  if (free_rect.w == 0 || free_rect.h == 0) {
    return false; // No space available
  }
  
  // Mark region as used
  MarkRegionUsed(atlas, free_rect, true);
  
  // Set UV coordinates (normalized to 0-1 range)
  uv_rect = {
    free_rect.x,
    free_rect.y,
    width,
    height
  };
  
  return true;
}

void AtlasRenderer::CreateNewAtlas() {
  int size = 1024; // Default size
  if (!atlases_.empty()) {
    size = atlases_.back()->size * 2; // Double size for new atlas
  }
  
  atlases_.push_back(std::make_unique<Atlas>(size));
  current_atlas_ = atlases_.size() - 1;
  
  // Create SDL texture for the atlas
  auto& atlas = *atlases_[current_atlas_];
  atlas.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_TARGET, size, size);
  
  if (!atlas.texture) {
    SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
  }
}

void AtlasRenderer::RebuildAtlas(Atlas& atlas) {
  // Clear used regions
  std::fill(atlas.used_regions.begin(), atlas.used_regions.end(), false);
  
  // Rebuild atlas texture by copying from source textures
  SDL_SetRenderTarget(renderer_, atlas.texture);
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
  SDL_RenderClear(renderer_);
  
  for (auto& entry : atlas.entries) {
    if (entry.in_use && entry.texture) {
      SDL_RenderCopy(renderer_, entry.texture, nullptr, &entry.uv_rect);
      MarkRegionUsed(atlas, entry.uv_rect, true);
    }
  }
  
  SDL_SetRenderTarget(renderer_, nullptr);
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
          if (index >= static_cast<int>(atlas.used_regions.size()) || atlas.used_regions[index]) {
            can_fit = false;
          }
        }
      }
      
      if (can_fit) {
        return {x, y, width, height};
      }
    }
  }
  
  return {0, 0, 0, 0}; // No space found
}

void AtlasRenderer::MarkRegionUsed(Atlas& atlas, const SDL_Rect& rect, bool used) {
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
