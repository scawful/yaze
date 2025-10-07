#include "app/gfx/arena.h"

#include <SDL.h>
#include <algorithm>

#include "app/gfx/backend/irenderer.h"
#include "util/sdl_deleter.h"

namespace yaze {
namespace gfx {

void Arena::Initialize(IRenderer* renderer) { renderer_ = renderer; }

Arena& Arena::Get() {
  static Arena instance;
  return instance;
}

Arena::Arena() {
  layer1_buffer_.fill(0);
  layer2_buffer_.fill(0);
}

Arena::~Arena() {
  // Use the safe shutdown method that handles cleanup properly
  Shutdown();
}



void Arena::QueueTextureCommand(TextureCommandType type, Bitmap* bitmap) {
  texture_command_queue_.push_back({type, bitmap});
}

void Arena::ProcessTextureQueue(IRenderer* renderer) {
  if (!renderer_) return;

  for (const auto& command : texture_command_queue_) {
    switch (command.type) {
      case TextureCommandType::CREATE: {
        // Create a new texture and update it with bitmap data
        if (command.bitmap && command.bitmap->surface() && 
            command.bitmap->surface()->format && 
            command.bitmap->is_active() && 
            command.bitmap->width() > 0 && command.bitmap->height() > 0) {
          auto texture = renderer_->CreateTexture(command.bitmap->width(), 
                                                  command.bitmap->height());
          if (texture) {
            command.bitmap->set_texture(texture);
            renderer_->UpdateTexture(texture, *command.bitmap);
          }
        }
        break;
      }
      case TextureCommandType::UPDATE: {
        // Update existing texture with current bitmap data
        if (command.bitmap && command.bitmap->texture() && 
            command.bitmap->surface() && command.bitmap->surface()->format &&
            command.bitmap->is_active()) {
          renderer_->UpdateTexture(command.bitmap->texture(), *command.bitmap);
        }
        break;
      }
      case TextureCommandType::DESTROY: {
        if (command.bitmap && command.bitmap->texture()) {
          renderer_->DestroyTexture(command.bitmap->texture());
          command.bitmap->set_texture(nullptr);
        }
        break;
      }
    }
  }
  texture_command_queue_.clear();
}

SDL_Surface* Arena::AllocateSurface(int width, int height, int depth, int format) {
  // Try to get a surface from the pool first
  for (auto it = surface_pool_.available_surfaces_.begin(); 
       it != surface_pool_.available_surfaces_.end(); ++it) {
    auto& info = surface_pool_.surface_info_[*it];
    if (std::get<0>(info) == width && std::get<1>(info) == height && 
        std::get<2>(info) == depth && std::get<3>(info) == format) {
      SDL_Surface* surface = *it;
      surface_pool_.available_surfaces_.erase(it);
      return surface;
    }
  }

  // Create new surface if none available in pool
  Uint32 sdl_format = GetSnesPixelFormat(format);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, depth, sdl_format);
  
  if (surface) {
    auto surface_ptr = std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>(surface);
    surfaces_[surface] = std::move(surface_ptr);
    surface_pool_.surface_info_[surface] = std::make_tuple(width, height, depth, format);
  }
  
  return surface;
}

void Arena::FreeSurface(SDL_Surface* surface) {
  if (!surface) return;
  
  // Return surface to pool if space available
  if (surface_pool_.available_surfaces_.size() < surface_pool_.MAX_POOL_SIZE) {
    surface_pool_.available_surfaces_.push_back(surface);
  } else {
    // Remove from tracking maps
    surface_pool_.surface_info_.erase(surface);
    surfaces_.erase(surface);
  }
}

void Arena::Shutdown() {
  // Process any remaining batch updates before shutdown
  ProcessTextureQueue(renderer_);
  
  // Clear pool references first to prevent reuse during shutdown
  surface_pool_.available_surfaces_.clear();
  surface_pool_.surface_info_.clear();
  texture_pool_.available_textures_.clear();
  texture_pool_.texture_sizes_.clear();
  
  // CRITICAL FIX: Clear containers in reverse order to prevent cleanup issues
  // This ensures that dependent resources are freed before their dependencies
  textures_.clear();
  surfaces_.clear();
  
  // Clear any remaining queue items
  texture_command_queue_.clear();
}


}  // namespace gfx
}  // namespace yaze