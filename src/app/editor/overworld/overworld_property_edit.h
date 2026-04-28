#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_PROPERTY_EDIT_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_PROPERTY_EDIT_H

#include <array>
#include <string>
#include <vector>

namespace yaze::editor {

enum class OverworldPropertyField {
  kAreaSize,
  kAreaGraphics,
  kAreaPalette,
  kMainPalette,
  kSpriteGraphics,
  kSpritePalette,
  kAnimatedGraphics,
  kCustomTileset,
  kMessageId,
  kMusic,
  kMosaic,
  kMosaicExpanded,
  kAreaSpecificBgColor,
  kSubscreenOverlay,
};

enum class OverworldMapMetadataClipboardScope {
  kAll,
  kGraphics,
  kPalettes,
  kMusicMessages,
};

struct OverworldPropertyEdit {
  int map_id = 0;
  OverworldPropertyField field = OverworldPropertyField::kAreaGraphics;
  int index = 0;
  int value = 0;
  std::string description;
};

struct OverworldMapMetadataClipboard {
  bool valid = false;
  OverworldMapMetadataClipboardScope scope =
      OverworldMapMetadataClipboardScope::kAll;
  int source_map_id = 0;
  int area_size = 0;
  int area_graphics = 0;
  int area_palette = 0;
  int main_palette = 0;
  int animated_graphics = 0;
  int message_id = 0;
  int subscreen_overlay = 0;
  int area_specific_bg_color = 0;
  bool mosaic = false;
  std::array<bool, 4> mosaic_expanded{};
  std::array<int, 3> sprite_graphics{};
  std::array<int, 3> sprite_palette{};
  std::array<int, 4> music{};
  std::array<int, 8> custom_tilesets{};
};

const char* OverworldPropertyFieldName(OverworldPropertyField field);
const char* OverworldMapMetadataClipboardScopeName(
    OverworldMapMetadataClipboardScope scope);
std::string DescribeOverworldPropertyEdit(const OverworldPropertyEdit& edit);
bool CanPasteOverworldMapMetadata(
    const OverworldMapMetadataClipboard& clipboard,
    OverworldMapMetadataClipboardScope requested_scope);
std::vector<OverworldPropertyEdit> BuildOverworldMetadataPasteEdits(
    int target_map_id, const OverworldMapMetadataClipboard& clipboard,
    OverworldMapMetadataClipboardScope requested_scope);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_PROPERTY_EDIT_H
