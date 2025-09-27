#include "app/gfx/arena.h"

#include <SDL.h>

#include "app/core/platform/sdl_deleter.h"

namespace yaze {
namespace gfx {

Arena& Arena::Get() {
  static Arena instance;
  return instance;
}

Arena::Arena() {
  layer1_buffer_.fill(0);
  layer2_buffer_.fill(0);
}

Arena::~Arena() {
  // Safely clear all resources with proper error checking
  for (auto& [key, texture] : textures_) {
    // Don't rely on unique_ptr deleter during shutdown - manually manage
    if (texture && key) {
      [[maybe_unused]] auto* released = texture.release(); // Release ownership to prevent double deletion
    }
  }
  textures_.clear();
  
  for (auto& [key, surface] : surfaces_) {
    // Don't rely on unique_ptr deleter during shutdown - manually manage
    if (surface && key) {
      [[maybe_unused]] auto* released = surface.release(); // Release ownership to prevent double deletion
    }
  }
  surfaces_.clear();
}

SDL_Texture* Arena::AllocateTexture(SDL_Renderer* renderer, int width,
                                    int height) {
  if (!renderer) {
    SDL_Log("Invalid renderer passed to AllocateTexture");
    return nullptr;
  }

  if (width <= 0 || height <= 0) {
    SDL_Log("Invalid texture dimensions: width=%d, height=%d", width, height);
    return nullptr;
  }

  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
    return nullptr;
  }

  textures_[texture] =
      std::unique_ptr<SDL_Texture, core::SDL_Texture_Deleter>(texture);
  return texture;
}

void Arena::FreeTexture(SDL_Texture* texture) {
  if (!texture) return;

  auto it = textures_.find(texture);
  if (it != textures_.end()) {
    textures_.erase(it);
  }
}

void Arena::Shutdown() {
  // Clear all resources safely - let the unique_ptr deleters handle the cleanup
  // while SDL context is still available
  
  // Just clear the containers - the unique_ptr destructors will handle SDL cleanup
  // This avoids double-free issues from manual destruction
  textures_.clear();
  surfaces_.clear();
}

void Arena::UpdateTexture(SDL_Texture* texture, SDL_Surface* surface) {
  if (!texture || !surface) {
    SDL_Log("Invalid texture or surface passed to UpdateTexture");
    return;
  }

  if (surface->pixels == nullptr) {
    SDL_Log("Surface pixels are nullptr");
    return;
  }

  auto converted_surface =
      std::unique_ptr<SDL_Surface, core::SDL_Surface_Deleter>(
          SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0),
          core::SDL_Surface_Deleter());

  if (!converted_surface) {
    SDL_Log("SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
    return;
  }

  void* pixels;
  int pitch;
  if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) {
    SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
    return;
  }

  memcpy(pixels, converted_surface->pixels,
         converted_surface->h * converted_surface->pitch);

  SDL_UnlockTexture(texture);
}

SDL_Surface* Arena::AllocateSurface(int width, int height, int depth,
                                    int format) {
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth, format);
  if (!surface) {
    SDL_Log("Failed to create surface: %s", SDL_GetError());
    return nullptr;
  }

  surfaces_[surface] =
      std::unique_ptr<SDL_Surface, core::SDL_Surface_Deleter>(surface);
  return surface;
}


void Arena::FreeSurface(SDL_Surface* surface) {
  if (!surface) return;

  auto it = surfaces_.find(surface);
  if (it != surfaces_.end()) {
    surfaces_.erase(it);
  }
}

}  // namespace gfx
}  // namespace yaze