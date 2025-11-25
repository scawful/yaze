#include "app/editor/ui/layout_presets.h"

namespace yaze {
namespace editor {

CardLayoutPreset LayoutPresets::GetDefaultPreset(EditorType type) {
  CardLayoutPreset preset;
  preset.editor_type = type;

  switch (type) {
    case EditorType::kOverworld:
      preset.name = "Overworld Default";
      preset.description = "Main canvas with tile16 editor";
      preset.default_visible_cards = {
          Cards::kOverworldCanvas,
          Cards::kOverworldTile16Selector,
      };
      preset.optional_cards = {
          Cards::kOverworldTile8Selector,
          Cards::kOverworldAreaGraphics,
          Cards::kOverworldScratch,
          Cards::kOverworldGfxGroups,
          Cards::kOverworldUsageStats,
          Cards::kOverworldV3Settings,
      };
      break;

    case EditorType::kDungeon:
      preset.name = "Dungeon Default";
      preset.description = "Room editor with object palette and properties";
      preset.default_visible_cards = {
          Cards::kDungeonControlPanel,
          Cards::kDungeonRoomSelector,
          Cards::kDungeonObjectEditor,
          Cards::kDungeonPaletteEditor,
      };
      preset.optional_cards = {
          Cards::kDungeonRoomMatrix,
          Cards::kDungeonEntrances,
          Cards::kDungeonRoomGraphics,
          Cards::kDungeonDebugControls,
      };
      break;

    case EditorType::kGraphics:
      preset.name = "Graphics Default";
      preset.description = "Sheet browser with editor and animations";
      preset.default_visible_cards = {
          Cards::kGraphicsSheetBrowser,
          Cards::kGraphicsSheetEditor,
          Cards::kGraphicsPlayerAnimations,
      };
      preset.optional_cards = {
          Cards::kGraphicsPrototypeViewer,
      };
      break;

    case EditorType::kPalette:
      preset.name = "Palette Default";
      preset.description = "Palette groups with editor and preview";
      preset.default_visible_cards = {
          Cards::kPaletteControlPanel,
          Cards::kPaletteOwMain,
          Cards::kPaletteQuickAccess,
      };
      preset.optional_cards = {
          Cards::kPaletteOwAnimated,
          Cards::kPaletteDungeonMain,
          Cards::kPaletteSprites,
          Cards::kPaletteSpritesAux1,
          Cards::kPaletteSpritesAux2,
          Cards::kPaletteSpritesAux3,
          Cards::kPaletteEquipment,
          Cards::kPaletteCustom,
      };
      break;

    case EditorType::kSprite:
      preset.name = "Sprite Default";
      preset.description = "Sprite browser with editor";
      preset.default_visible_cards = {
          Cards::kSpriteVanillaEditor,
          Cards::kSpriteCustomEditor,
      };
      preset.optional_cards = {};
      break;

    case EditorType::kScreen:
      preset.name = "Screen Default";
      preset.description = "Screen browser with tileset editor";
      preset.default_visible_cards = {
          Cards::kScreenDungeonMaps,
          Cards::kScreenTitleScreen,
      };
      preset.optional_cards = {
          Cards::kScreenInventoryMenu,
          Cards::kScreenOverworldMap,
          Cards::kScreenNamingScreen,
      };
      break;

    case EditorType::kMusic:
      preset.name = "Music Default";
      preset.description = "Song list with sequencer";
      preset.default_visible_cards = {
          Cards::kMusicTracker,
          Cards::kMusicInstrumentEditor,
      };
      preset.optional_cards = {
          Cards::kMusicAssembly,
      };
      break;

    case EditorType::kMessage:
      preset.name = "Message Default";
      preset.description = "Message list with editor and preview";
      preset.default_visible_cards = {
          Cards::kMessageList,
          Cards::kMessageEditor,
      };
      preset.optional_cards = {
          Cards::kMessageFontAtlas,
          Cards::kMessageDictionary,
      };
      break;

    case EditorType::kAssembly:
      preset.name = "Assembly Default";
      preset.description = "Assembly editor with file browser";
      preset.default_visible_cards = {
          Cards::kAssemblyEditor,
          Cards::kAssemblyFileBrowser,
      };
      preset.optional_cards = {};
      break;

    case EditorType::kEmulator:
      preset.name = "Emulator Default";
      preset.description = "Emulator with debugger tools";
      preset.default_visible_cards = {
          Cards::kEmulatorCpuDebugger,
          Cards::kEmulatorPpuViewer,
          Cards::kEmulatorMemoryViewer,
      };
      preset.optional_cards = {
          Cards::kEmulatorBreakpoints,
          Cards::kEmulatorPerformance,
          Cards::kEmulatorAiAgent,
          Cards::kEmulatorSaveStates,
          Cards::kEmulatorKeyboardConfig,
          Cards::kEmulatorApuDebugger,
          Cards::kEmulatorAudioMixer,
      };
      break;

    case EditorType::kSettings:
      preset.name = "Settings Default";
      preset.description = "Settings panels";
      preset.default_visible_cards = {
          Cards::kSettingsGeneral,
          Cards::kSettingsAppearance,
      };
      preset.optional_cards = {
          Cards::kSettingsEditorBehavior,
          Cards::kSettingsPerformance,
          Cards::kSettingsAiAgent,
          Cards::kSettingsShortcuts,
      };
      break;

    default:
      preset.name = "Default";
      preset.description = "No specific layout";
      break;
  }

  return preset;
}

std::unordered_map<EditorType, CardLayoutPreset> LayoutPresets::GetAllPresets() {
  std::unordered_map<EditorType, CardLayoutPreset> presets;

  presets[EditorType::kOverworld] = GetDefaultPreset(EditorType::kOverworld);
  presets[EditorType::kDungeon] = GetDefaultPreset(EditorType::kDungeon);
  presets[EditorType::kGraphics] = GetDefaultPreset(EditorType::kGraphics);
  presets[EditorType::kPalette] = GetDefaultPreset(EditorType::kPalette);
  presets[EditorType::kSprite] = GetDefaultPreset(EditorType::kSprite);
  presets[EditorType::kScreen] = GetDefaultPreset(EditorType::kScreen);
  presets[EditorType::kMusic] = GetDefaultPreset(EditorType::kMusic);
  presets[EditorType::kMessage] = GetDefaultPreset(EditorType::kMessage);
  presets[EditorType::kAssembly] = GetDefaultPreset(EditorType::kAssembly);
  presets[EditorType::kEmulator] = GetDefaultPreset(EditorType::kEmulator);
  presets[EditorType::kSettings] = GetDefaultPreset(EditorType::kSettings);

  return presets;
}

std::vector<std::string> LayoutPresets::GetDefaultCards(EditorType type) {
  return GetDefaultPreset(type).default_visible_cards;
}

std::vector<std::string> LayoutPresets::GetAllCardsForEditor(EditorType type) {
  auto preset = GetDefaultPreset(type);
  std::vector<std::string> all_cards = preset.default_visible_cards;
  all_cards.insert(all_cards.end(), preset.optional_cards.begin(),
                   preset.optional_cards.end());
  return all_cards;
}

bool LayoutPresets::IsDefaultCard(EditorType type, const std::string& card_id) {
  auto default_cards = GetDefaultCards(type);
  return std::find(default_cards.begin(), default_cards.end(), card_id) !=
         default_cards.end();
}

// ============================================================================
// Named Workspace Presets
// ============================================================================

CardLayoutPreset LayoutPresets::GetMinimalPreset() {
  CardLayoutPreset preset;
  preset.name = "Minimal";
  preset.description = "Minimal cards for focused editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  // Only the main canvas/editor for each type
  preset.default_visible_cards = {};
  return preset;
}

CardLayoutPreset LayoutPresets::GetDeveloperPreset() {
  CardLayoutPreset preset;
  preset.name = "Developer";
  preset.description = "Debug and development focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_cards = {
      // Emulator/debug cards always visible
      Cards::kEmulatorCpuDebugger,
      Cards::kEmulatorPpuViewer,
      Cards::kEmulatorMemoryViewer,
      Cards::kEmulatorBreakpoints,
      Cards::kEmulatorPerformance,
      Cards::kMemoryHexEditor,
      // AI Agent for debugging assistance
      Cards::kEmulatorAiAgent,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetDesignerPreset() {
  CardLayoutPreset preset;
  preset.name = "Designer";
  preset.description = "Visual and artistic focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_cards = {
      // Graphics and palette cards
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
      Cards::kGraphicsPlayerAnimations,
      Cards::kPaletteControlPanel,
      Cards::kPaletteOwMain,
      Cards::kPaletteQuickAccess,
      // Sprite cards
      Cards::kSpriteVanillaEditor,
      Cards::kSpriteCustomEditor,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetModderPreset() {
  CardLayoutPreset preset;
  preset.name = "Modder";
  preset.description = "Full-featured layout for comprehensive editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_cards = {
      // Overworld cards
      Cards::kOverworldCanvas,
      Cards::kOverworldTile16Selector,
      Cards::kOverworldAreaGraphics,
      // Dungeon cards
      Cards::kDungeonControlPanel,
      Cards::kDungeonRoomSelector,
      Cards::kDungeonObjectEditor,
      Cards::kDungeonPaletteEditor,
      // Graphics cards
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
      // AI Agent for assistance
      Cards::kEmulatorAiAgent,
  };
  return preset;
}

}  // namespace editor
}  // namespace yaze
