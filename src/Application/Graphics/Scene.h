#ifndef YAZE_APPLICATION_GRAPHICS_SCENE_H
#define YAZE_APPLICATION_GRAPHICS_SCENE_H

#include <SDL2/SDL.h>
#include <tile.h>

#include <iostream>
#include <vector>

#include "Core/Renderer.h"
#include "Graphics/Tile.h"


namespace yaze {
namespace Application {
namespace Graphics {

class Scene {
 public:
  Scene() = default;
  void buildScene(const std::vector<tile8>& tiles, const SNESPalette mPalette,
                  const TilesPattern& tp);

  SDL_Surface* buildSurface(const std::vector<tile8>& tiles,
                            const SNESPalette mPalette, const TilesPattern& tp);

  void updateScene();
  void setTilesZoom(unsigned int tileZoom);
  void setTilesPattern(TilesPattern tp);

 private:
  std::vector<tile8> allTiles;
  std::vector<std::vector<tile8> > arrangedTiles;
  unsigned int tilesZoom;
  TilesPattern tilesPattern;
  // QMap<unsigned int, QPixmap>  imagesCache;
};

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif