#include "scene.h"

#include <SDL2/SDL.h>
#include <tile.h>

#include <iostream>
#include <vector>

#include "Graphics/tile.h"

namespace yaze {
namespace Application {
namespace Graphics {

void Scene::buildSurface(const std::vector<tile8>& tiles, SNESPalette& mPalette,
                         const TilesPattern& tp) {
  arrangedTiles = TilesPattern::transform(tp, tiles);
  tilesPattern = tp;
  allTiles = tiles;

  for (unsigned int j = 0; j < arrangedTiles.size(); j++) {
    for (unsigned int i = 0; i < arrangedTiles[0].size(); i++) {
      tile8 tile = arrangedTiles[j][i];
      // SDL_PIXELFORMAT_RGB888 ?
      SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
          0, 8, 8, SDL_BITSPERPIXEL(3), SDL_PIXELFORMAT_RGB444);
      if (surface == nullptr) {
        SDL_Log("SDL_CreateRGBSurfaceWithFormat() failed: %s", SDL_GetError());
        exit(1);
      }
      SDL_PixelFormat* format = surface->format;
      format->palette = mPalette.GetSDL_Palette();
      char* ptr = (char*)surface->pixels;

      for (int k = 0; k < 8; k++) {
        for (int l = 0; l < 8; l++) {
          ptr[k * 8 + l] = tile.data[k * 8 + l];
        }
      }

      // SDL_Texture* texture =
      //     SDL_CreateTextureFromSurface(Core::renderer, surface);
      // if (texture == nullptr) {
      //   std::cout << "Error: " << SDL_GetError() << std::endl;
      // }
      // imagesCache[tile.id] = texture;
    }
  }
}

void Scene::updateScene() {
  std::cout << "Update scene";
  unsigned int itemCpt = 0;
  for (unsigned int j = 0; j < arrangedTiles.size(); j++) {
    for (unsigned int i = 0; i < arrangedTiles[0].size(); i++) {
      tile8 tile = arrangedTiles[j][i];
      // QPixmap m = imagesCache[tile.id];
      // GraphicsTileItem *tileItem = (GraphicsTileItem *)items()[itemCpt];
      // tileItem->image = m;
      // tileItem->rawTile = tile;
      // tileItem->setTileZoom(tilesZoom);
      // tileItem->setPos(i * tileItem->boundingRect().width() + i,
      //                  j * tileItem->boundingRect().width() + j);
      itemCpt++;
    }
  }
}

void Scene::setTilesZoom(unsigned int tileZoom) {
  tilesZoom = tileZoom;
  // if (!items().isEmpty()) updateScene();
}

void Scene::setTilesPattern(TilesPattern tp) { tilesPattern = tp; }

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze
