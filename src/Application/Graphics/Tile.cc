#include "Tile.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>

#include "png.h"

namespace yaze {
namespace Application {
namespace Graphics {

std::unordered_map<std::string, TilesPattern> TilesPattern::m_Patterns;

ushort TileInfo::toShort() {
  ushort value = 0;
  // vhopppcc cccccccc
  if (over_ == 1) {
    value |= 0x2000;
  };
  if (horizontal_mirror_ == 1) {
    value |= 0x4000;
  };
  if (vertical_mirror_ == 1) {
    value |= 0x8000;
  };
  value |= (ushort)((palette_ << 10) & 0x1C00);
  value |= (ushort)(id_ & 0x3FF);
  return value;
}

char *hexString(const char *str, const unsigned int size) {
  char *toret = (char *)malloc(size * 3 + 1);

  unsigned int i;
  for (i = 0; i < size; i++) {
    sprintf(toret + i * 3, "%02X ", (unsigned char)str[i]);
  }
  toret[size * 3] = 0;
  return toret;
}

TilesPattern::TilesPattern() {
  tilesPerRow = 16;
  numberOfTiles = 16;
  transformVector.push_back(std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 9,
                                             11, 12, 13, 14, 15, 16});

  // transformVector.push_back(std::vector<int>{0, 1, 2, 3});
  // transformVector.push_back(std::vector<int>{4, 5, 6, 7});
  // transformVector.push_back(std::vector<int>{8, 9, 11, 12});
  // transformVector.push_back(std::vector<int>{13, 14, 15, 16});
}
// [pattern]
// name = "32x32 B (4x4)"
// number_of_tile = 16
// pattern =
void TilesPattern::default_settings() {
  numberOfTiles = 16;
  std::string patternString =
      "[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, A, B], [C, D, E, F]";

  transformVector.clear();

  std::smatch cm;
  std::regex arrayRegExp("(\\[[\\s|0-F|a-f|,]+\\])");

  int pos = 0;
  while (std::regex_search(patternString, cm, arrayRegExp)) {
    // while (arrayRegExp.indexIn(patternString, pos) != -1) {
    std::string arrayString = cm[1];
    std::vector<int> tmpVect;
    // std::cout << arrayString;
    unsigned int stringPos = 1;

    while (arrayString[stringPos] != ']') {
      while (arrayString[stringPos] == ' ') stringPos++;
      std::smatch cm_;
      std::regex hex("([0-F|a-f]+)");
      bool ok;

      std::regex_search(arrayString, cm, hex);
      if (cm[1] == stringPos) {
        std::cout << "here" << std::endl;
        // tmpVect.push_back(stoi(cm[1]));
      }
      while (arrayString[stringPos] == ' ') stringPos++;
      stringPos++;  // should be the comma
    }

    pos += cm.size();
    transformVector.push_back(tmpVect);
  }
  std::cout << transformVector.size() << std::endl;
}

// bool TilesPattern::load(std::string patternFile) {
// QSettings pFile(patternFile, QSettings::IniFormat);
// name = pFile.value("pattern/name").toString();
// description = pFile.value("pattern/description").toString();
// numberOfTiles = pFile.value("pattern/number_of_tile").toInt();
// std::cout << name;
// // unsigned int nbOfTile = pFile.value("_/number_of_tile").toUInt();
// std::string patternString = pFile.value("pattern/pattern").toString();
// std::cout << patternString;

// // Pattern String is a array description

// transformVector.clear();
// QRegExp arrayRegExp("(\\[[\\s|0-F|a-f|,]+\\])");
// int pos = 0;
// while (arrayRegExp.indexIn(patternString, pos) != -1) {
//   std::string arrayString = arrayRegExp.cap(1);
//   std::vector<int> tmpVect;
//   // std::cout << arrayString;
//   unsigned int stringPos = 1;

//   while (arrayString[stringPos] != ']') {
//     while (arrayString[stringPos].isSpace()) stringPos++;
//     QRegExp hex("([0-F|a-f]+)");
//     bool ok;
//     if (hex.indexIn(arrayString, stringPos) == stringPos) {
//       tmpVect.append(hex.cap(1).toInt(&ok, 16));
//     }
//     while (arrayString[stringPos].isSpace()) stringPos++;
//     stringPos++;  // should be the comma
//   }
//   pos += arrayRegExp.matchedLength();
//   transformVector.append(tmpVect);
// }
// std::cout << transformVector.size() << transformVector;
// return true;
// }

bool TilesPattern::loadPatterns() {
  // foreach (std::string fileName, patternDirectory.entryList(QDir::Files)) {
  //   TilesPattern tp;
  //   std::cout << "Loading " << fileName;
  //   if (!tp.load(patternDirectory.absoluteFilePath(fileName))) return false;
  //   m_Patterns[tp.name] = tp;
  // }
  return true;
}

TilesPattern TilesPattern::pattern(std::string name) {
  return m_Patterns[name];
}

std::unordered_map<std::string, TilesPattern> TilesPattern::Patterns() {
  return m_Patterns;
}

std::vector<std::vector<tile8> > TilesPattern::transform(
    const std::vector<tile8> &tiles) const {
  unsigned int repeatOffsetY = 0;
  unsigned int repeatOffsetX = 0;
  unsigned int tVectHeight = transformVector.size();
  unsigned int tVectWidth = transformVector[0].size();
  unsigned int repeat = 0;
  std::vector<std::vector<tile8> > toret;
  unsigned int transPerRow = tilesPerRow / tVectWidth;
  unsigned int nbTransform = tiles.size() / numberOfTiles;
  printf("Tiles size : %d - nbtransform : %d - pattern number of tiles : %d",
         tiles.size(), nbTransform, numberOfTiles);

  if (transPerRow > nbTransform)
    toret.resize(tVectHeight);
  else
    toret.resize(
        ((unsigned int)(((double)nbTransform / (double)transPerRow) + 0.5)) *
        tVectHeight);

  std::vector<std::vector<tile8> > vec(toret);
  auto it = vec.begin();
  for (auto &each : vec) {
    each.resize(tilesPerRow);
  }
  // while (it.hasNext()) {
  //   it.next().resize(tilesPerRow);
  // }
  // std::cout << toret[0].size() << "x" << toret.size();
  while (repeat != nbTransform) {
    std::cout << "repeat" << repeat;
    for (unsigned int j = 0; j < tVectHeight; j++) {
      for (unsigned int i = 0; i < tVectWidth; i++) {
        unsigned int posTile = transformVector[j][i] + numberOfTiles * repeat;
        unsigned int posX = i + repeatOffsetX;
        unsigned int posY = j + repeatOffsetY;
        printf("X: %d - Y: %d - posTile : %d", posX, posY, posTile);
        toret[posY][posX] = tiles[posTile];
      }
    }
    if (repeatOffsetX + tVectWidth == tilesPerRow) {
      repeatOffsetX = 0;
      repeatOffsetY += tVectHeight;
    } else
      repeatOffsetX += tVectWidth;
    repeat++;
  }
  std::cout << "End of transform";
  return toret;
}

std::vector<tile8> TilesPattern::reverse(
    const std::vector<tile8> &tiles) const {
  unsigned int repeatOffsetY = 0;
  unsigned int repeatOffsetX = 0;
  unsigned int tVectHeight = transformVector.size();
  unsigned int tVectWidth = transformVector[0].size();
  unsigned int repeat = 0;
  unsigned int nbTransPerRow = tilesPerRow / tVectWidth;
  unsigned int nbTiles = tiles.size();
  std::vector<tile8> toretVec(tiles.size());

  for (unsigned int i = 0; i < nbTiles; i++) {
    unsigned int lineNb = i / tilesPerRow;
    unsigned int lineInTab = lineNb % tVectHeight;
    unsigned int colInTab = i % tVectWidth;
    unsigned int tileNb = transformVector[lineInTab][colInTab];

    unsigned int lineBlock = i / (nbTransPerRow * numberOfTiles);
    unsigned int blockNB =
        (i % (nbTransPerRow * numberOfTiles) % tilesPerRow) / tVectWidth;

    // std::cout << colInTab << lineInTab << " = " << tileNb;
    // unsigned int pos = tileNb + ((i % tilesPerRow) / nbTransPerRow) *
    // numberOfTiles;
    unsigned int pos = tileNb + (lineBlock + blockNB) * numberOfTiles;
    // std::cout << i << "Goes to : " << pos;
    toretVec[pos] = tiles[i];
  }
  return toretVec;
}

std::vector<std::vector<tile8> > TilesPattern::transform(
    const TilesPattern &pattern, const std::vector<tile8> &tiles) {
  return pattern.transform(tiles);
}

std::vector<std::vector<tile8> > TilesPattern::transform(
    const std::string id, const std::vector<tile8> &tiles) {
  return m_Patterns[id].transform(tiles);
}

std::vector<tile8> TilesPattern::reverse(const TilesPattern &pattern,
                                         const std::vector<tile8> &tiles) {
  return pattern.reverse(tiles);
}

TilePreset::TilePreset() {
  name = "";
  romName = "";
  romType = "";

  pcTilesLocation = -1;
  SNESTilesLocation = 0;
  length = 0;
  bpp = 0;
  compression = "None";
  pcPaletteLocation = 0;
  SNESPaletteLocation = 0;
  paletteNoZeroColor = false;
}

bool TilePreset::save(const std::string &file) {
  // QSettings pFile(file, QSettings::IniFormat);

  // if (pFile.isWritable() == false)
  //     return false;

  // pFile.setValue("_/name", name);
  // pFile.setValue("rom/name", romName);
  // pFile.setValue("rom/type", romType);

  // pFile.setValue("tiles/pc_location", QString::number(pcTilesLocation, 16));
  // pFile.setValue("tiles/snes_location", QString::number(SNESTilesLocation,
  // 16)); pFile.setValue("tiles/length", length); pFile.setValue("tiles/bpp",
  // bpp); pFile.setValue("tiles/compression", compression);
  // pFile.setValue("tiles/pattern", tilesPattern.name);

  // pFile.setValue("palette/pc_location", QString::number(pcPaletteLocation,
  // 16)); pFile.setValue("palette/snes_location",
  // QString::number(SNESPaletteLocation, 16));
  // pFile.setValue("palette/nozerocolor", paletteNoZeroColor);

  return true;
}

bool TilePreset::load(const std::string &file) {
  // QSettings   pFile(file, QSettings::IniFormat);

  // /* Meh solution to know if the file is right*/
  // if (pFile.value("_/name").toString().isEmpty())
  //     return false;
  // name = pFile.value("_/name").toString();
  // romName = pFile.value("rom/name").toString();
  // romType = pFile.value("rom/type").toString();

  // /* Locations are stored in a hex string */
  // bool ok;
  // pcTilesLocation = pFile.value("tiles/pc_location").toString().toUInt(&ok,
  // 16); SNESTilesLocation =
  // pFile.value("tiles/snes_location").toString().toUInt(&ok, 16); length =
  // pFile.value("tiles/length").toInt(); bpp =
  // pFile.value("tiles/bpp").toInt(); compression =
  // pFile.value("tiles/compression").toString(); QString patternName =
  // pFile.value("tiles/pattern").toString(); if (patternName.isEmpty())
  //     patternName = "normal";
  // tilesPattern = TilesPattern::pattern(patternName);

  // pcPaletteLocation =
  // pFile.value("palette/pc_location").toString().toUInt(&ok, 16);
  // SNESPaletteLocation =
  // pFile.value("palette/snes_location").toString().toUInt(&ok, 16);
  // paletteNoZeroColor = pFile.value("palette/nozerocolor").toBool();
  return true;
}

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze
