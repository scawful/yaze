#include "app/editor/overworld/overworld_property_edit.h"

#include "absl/strings/str_format.h"

namespace yaze::editor {

const char* OverworldPropertyFieldName(OverworldPropertyField field) {
  switch (field) {
    case OverworldPropertyField::kAreaSize:
      return "Area size";
    case OverworldPropertyField::kAreaGraphics:
      return "Area GFX";
    case OverworldPropertyField::kAreaPalette:
      return "Area palette";
    case OverworldPropertyField::kMainPalette:
      return "Main palette";
    case OverworldPropertyField::kSpriteGraphics:
      return "Sprite GFX";
    case OverworldPropertyField::kSpritePalette:
      return "Sprite palette";
    case OverworldPropertyField::kAnimatedGraphics:
      return "Animated GFX";
    case OverworldPropertyField::kCustomTileset:
      return "Custom tile GFX";
    case OverworldPropertyField::kMessageId:
      return "Message";
    case OverworldPropertyField::kMusic:
      return "Music";
    case OverworldPropertyField::kMosaic:
      return "Mosaic";
    case OverworldPropertyField::kMosaicExpanded:
      return "Directional mosaic";
    case OverworldPropertyField::kAreaSpecificBgColor:
      return "Background color";
    case OverworldPropertyField::kSubscreenOverlay:
      return "Visual effect";
  }
  return "Overworld property";
}

std::string DescribeOverworldPropertyEdit(const OverworldPropertyEdit& edit) {
  if (!edit.description.empty()) {
    return edit.description;
  }
  switch (edit.field) {
    case OverworldPropertyField::kSpriteGraphics:
    case OverworldPropertyField::kSpritePalette:
    case OverworldPropertyField::kMusic:
    case OverworldPropertyField::kCustomTileset:
    case OverworldPropertyField::kMosaicExpanded:
      return absl::StrFormat("%s %d on map 0x%02X",
                             OverworldPropertyFieldName(edit.field), edit.index,
                             edit.map_id);
    default:
      return absl::StrFormat("%s on map 0x%02X",
                             OverworldPropertyFieldName(edit.field),
                             edit.map_id);
  }
}

}  // namespace yaze::editor
