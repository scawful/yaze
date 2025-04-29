#ifndef YAZE_APP_GFX_TEXTURE_POOL_H
#define YAZE_APP_GFX_TEXTURE_POOL_H

#include <SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace gfx {

class TexturePool {
 public:
  static TexturePool& GetInstance() {
    static TexturePool instance;
    return instance;
  }

  SDL_Texture* GetTexture(SDL_Renderer* renderer, int width, int height,
                          Uint32 format) {
    std::string key = GenerateKey(width, height, format);

    // Check if we have a suitable texture in the pool
    auto it = available_textures_.find(key);
    if (it != available_textures_.end() && !it->second.empty()) {
      SDL_Texture* texture = it->second.back();
      it->second.pop_back();
      return texture;
    }

    // Create a new texture
    return SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING,
                             width, height);
  }

  void ReturnTexture(SDL_Texture* texture, int width, int height,
                     Uint32 format) {
    if (!texture) return;

    std::string key = GenerateKey(width, height, format);
    available_textures_[key].push_back(texture);
  }

  void Clear() {
    for (auto& pair : available_textures_) {
      for (SDL_Texture* texture : pair.second) {
        SDL_DestroyTexture(texture);
      }
    }
    available_textures_.clear();
  }

 private:
  TexturePool() = default;

  std::string GenerateKey(int width, int height, Uint32 format) {
    return std::to_string(width) + "x" + std::to_string(height) + "_" +
           std::to_string(format);
  }

  std::unordered_map<std::string, std::vector<SDL_Texture*>>
      available_textures_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_TEXTURE_POOL_H
