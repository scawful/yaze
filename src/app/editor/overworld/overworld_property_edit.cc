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

const char* OverworldMapMetadataClipboardScopeName(
    OverworldMapMetadataClipboardScope scope) {
  switch (scope) {
    case OverworldMapMetadataClipboardScope::kAll:
      return "map metadata";
    case OverworldMapMetadataClipboardScope::kGraphics:
      return "graphics metadata";
    case OverworldMapMetadataClipboardScope::kPalettes:
      return "palette metadata";
    case OverworldMapMetadataClipboardScope::kMusicMessages:
      return "music/message metadata";
  }
  return "map metadata";
}

std::string DescribeOverworldMapMetadataClipboard(
    const OverworldMapMetadataClipboard& clipboard) {
  if (!clipboard.valid) {
    return "No map metadata";
  }
  return absl::StrFormat(
      "%s from map 0x%02X",
      OverworldMapMetadataClipboardScopeName(clipboard.scope),
      clipboard.source_map_id);
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

bool CanPasteOverworldMapMetadata(
    const OverworldMapMetadataClipboard& clipboard,
    OverworldMapMetadataClipboardScope requested_scope) {
  return clipboard.valid &&
         (clipboard.scope == OverworldMapMetadataClipboardScope::kAll ||
          clipboard.scope == requested_scope);
}

std::vector<OverworldPropertyEdit> BuildOverworldMetadataPasteEdits(
    int target_map_id, const OverworldMapMetadataClipboard& clipboard,
    OverworldMapMetadataClipboardScope requested_scope) {
  std::vector<OverworldPropertyEdit> edits;
  if (!CanPasteOverworldMapMetadata(clipboard, requested_scope)) {
    return edits;
  }

  if (requested_scope == OverworldMapMetadataClipboardScope::kAll) {
    edits.push_back({target_map_id, OverworldPropertyField::kAreaSize, 0,
                     clipboard.area_size});
  }

  if (requested_scope == OverworldMapMetadataClipboardScope::kAll ||
      requested_scope == OverworldMapMetadataClipboardScope::kGraphics) {
    edits.push_back({target_map_id, OverworldPropertyField::kAreaGraphics, 0,
                     clipboard.area_graphics});
    edits.push_back({target_map_id, OverworldPropertyField::kAnimatedGraphics,
                     0, clipboard.animated_graphics});
    for (int i = 0; i < 3; ++i) {
      edits.push_back({target_map_id, OverworldPropertyField::kSpriteGraphics,
                       i, clipboard.sprite_graphics[i]});
    }
    for (int i = 0; i < 8; ++i) {
      edits.push_back({target_map_id, OverworldPropertyField::kCustomTileset, i,
                       clipboard.custom_tilesets[i]});
    }
  }

  if (requested_scope == OverworldMapMetadataClipboardScope::kAll ||
      requested_scope == OverworldMapMetadataClipboardScope::kPalettes) {
    edits.push_back({target_map_id, OverworldPropertyField::kAreaPalette, 0,
                     clipboard.area_palette});
    edits.push_back({target_map_id, OverworldPropertyField::kMainPalette, 0,
                     clipboard.main_palette});
    edits.push_back({target_map_id,
                     OverworldPropertyField::kAreaSpecificBgColor, 0,
                     clipboard.area_specific_bg_color});
    for (int i = 0; i < 3; ++i) {
      edits.push_back({target_map_id, OverworldPropertyField::kSpritePalette, i,
                       clipboard.sprite_palette[i]});
    }
  }

  if (requested_scope == OverworldMapMetadataClipboardScope::kAll) {
    edits.push_back({target_map_id, OverworldPropertyField::kMosaic, 0,
                     clipboard.mosaic ? 1 : 0});
    for (int i = 0; i < 4; ++i) {
      edits.push_back({target_map_id, OverworldPropertyField::kMosaicExpanded,
                       i, clipboard.mosaic_expanded[i] ? 1 : 0});
    }
    edits.push_back({target_map_id, OverworldPropertyField::kSubscreenOverlay,
                     0, clipboard.subscreen_overlay});
  }

  if (requested_scope == OverworldMapMetadataClipboardScope::kAll ||
      requested_scope == OverworldMapMetadataClipboardScope::kMusicMessages) {
    edits.push_back({target_map_id, OverworldPropertyField::kMessageId, 0,
                     clipboard.message_id});
    for (int i = 0; i < 4; ++i) {
      edits.push_back({target_map_id, OverworldPropertyField::kMusic, i,
                       clipboard.music[i]});
    }
  }

  return edits;
}

}  // namespace yaze::editor
