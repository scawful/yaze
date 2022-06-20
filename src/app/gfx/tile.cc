#include "tile.h"

#include <tile.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/palette.h"

namespace yaze {
namespace app {
namespace gfx {

TilesPattern::TilesPattern() {
  tiles_per_row_ = 16;
  number_of_tiles_ = 16;
  // transform_vector_.push_back(std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  // 8,
  //                                              9, 10, 11, 12, 13, 14, 15});
  // transform_vector_.push_back(std::vector<int>{});
  // transform_vector_.push_back(std::vector<int>{});
  // transform_vector_.push_back(std::vector<int>{});

  transform_vector_.push_back(std::vector<int>{0, 1, 2, 3});
  transform_vector_.push_back(std::vector<int>{4, 5, 6, 7});
  transform_vector_.push_back(std::vector<int>{8, 9, 10, 11});
  transform_vector_.push_back(std::vector<int>{12, 13, 14, 15});
}

std::vector<std::vector<tile8> > TilesPattern::transform(
    const std::vector<tile8> &tiles) const {
  uint repeatOffsetY = 0;
  uint repeatOffsetX = 0;
  uint tVectHeight = transform_vector_.size();
  uint tVectWidth = transform_vector_[0].size();
  uint repeat = 0;
  std::vector<std::vector<tile8> > toret;
  uint transPerRow = tiles_per_row_ / tVectWidth;
  uint nbTransform = tiles.size() / number_of_tiles_;
  printf("Tiles size : %d\nnbtransform : %d\npattern number of tiles : %d\n",
         tiles.size(), nbTransform, number_of_tiles_);

  if (transPerRow > nbTransform)
    toret.resize(tVectHeight);
  else
    toret.resize(((uint)(((double)nbTransform / (double)transPerRow) + 0.5)) *
                 tVectHeight);

  for (auto &each : toret) {
    each.resize(tiles_per_row_);
  }

  std::cout << toret[0].size() << " x " << toret.size();
  while (repeat != nbTransform) {
    std::cout << "repeat" << repeat;
    for (uint j = 0; j < tVectHeight; j++) {
      for (uint i = 0; i < tVectWidth; i++) {
        uint posTile = transform_vector_[j][i] + number_of_tiles_ * repeat;
        uint posX = i + repeatOffsetX;
        uint posY = j + repeatOffsetY;
        printf("X: %d - Y: %d - posTile : %d \n", posX, posY, posTile);
        toret.at(posY).at(posX) = tiles[posTile];
      }
    }
    if (repeatOffsetX + tVectWidth == tiles_per_row_) {
      repeatOffsetX = 0;
      repeatOffsetY += tVectHeight;
    } else
      repeatOffsetX += tVectWidth;
    repeat++;
  }
  std::cout << "End of transform" << std::endl;
  return toret;
}

std::vector<tile8> TilesPattern::reverse(
    const std::vector<tile8> &tiles) const {
  uint repeatOffsetY = 0;
  uint repeatOffsetX = 0;
  uint tVectHeight = transform_vector_.size();
  uint tVectWidth = transform_vector_[0].size();
  uint repeat = 0;
  uint nbTransPerRow = tiles_per_row_ / tVectWidth;
  uint nbTiles = tiles.size();
  std::vector<tile8> toretVec(tiles.size());

  for (uint i = 0; i < nbTiles; i++) {
    uint lineNb = i / tiles_per_row_;
    uint lineInTab = lineNb % tVectHeight;
    uint colInTab = i % tVectWidth;
    uint tileNb = transform_vector_[lineInTab][colInTab];

    uint lineBlock = i / (nbTransPerRow * number_of_tiles_);
    uint blockNB =
        (i % (nbTransPerRow * number_of_tiles_) % tiles_per_row_) / tVectWidth;

    std::cout << colInTab << lineInTab << " = " << tileNb;
    uint pos = tileNb + (lineBlock + blockNB) * number_of_tiles_;
    std::cout << i << "Goes to : " << pos;
    toretVec[pos] = tiles[i];
  }
  return toretVec;
}

std::vector<std::vector<tile8> > TilesPattern::transform(
    const TilesPattern &pattern, const std::vector<tile8> &tiles) {
  return pattern.transform(tiles);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
