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
      };
      preset.optional_cards = {
          Cards::kDungeonObjectEditor,
          Cards::kDungeonPaletteEditor,
          Cards::kDungeonRoomMatrix,
          Cards::kDungeonEntrances,
          Cards::kDungeonRoomGraphics,
          Cards::kDungeonDebugControls,
      };
      break;

    case EditorType::kGraphics:
      preset.name = "Graphics Default";
      preset.description = "Sheet browser with editor";
      preset.default_visible_cards = {
          Cards::kGraphicsSheetBrowser,
          Cards::kGraphicsSheetEditor,
      };
      preset.optional_cards = {
          Cards::kGraphicsPlayerAnimations,
          Cards::kGraphicsPrototypeViewer,
      };
      break;

    case EditorType::kPalette:
      preset.name = "Palette Default";
      preset.description = "Palette groups with editor and preview";
      preset.default_visible_cards = {
          Cards::kPaletteControlPanel,
          Cards::kPaletteOwMain,
      };
      preset.optional_cards = {
          Cards::kPaletteQuickAccess,
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
      };
      preset.optional_cards = {
          Cards::kSpriteCustomEditor,
      };
      break;

    case EditorType::kScreen:
      preset.name = "Screen Default";
      preset.description = "Screen browser with tileset editor";
      preset.default_visible_cards = {
          Cards::kScreenDungeonMaps,
      };
      preset.optional_cards = {
          Cards::kScreenTitleScreen,
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
      };
      preset.optional_cards = {
          Cards::kMusicInstrumentEditor,
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
      };
      preset.optional_cards = {
          Cards::kAssemblyFileBrowser,
      };
      break;

    case EditorType::kEmulator:
      preset.name = "Emulator Default";
      preset.description = "Emulator with debugger tools";
      preset.default_visible_cards = {
          Cards::kEmulatorPpuViewer,
      };
      preset.optional_cards = {
          Cards::kEmulatorCpuDebugger,
          Cards::kEmulatorMemoryViewer,
          Cards::kEmulatorBreakpoints,
          Cards::kEmulatorPerformance,
          Cards::kEmulatorAiAgent,
          Cards::kEmulatorSaveStates,
          Cards::kEmulatorKeyboardConfig,
          Cards::kEmulatorApuDebugger,
          Cards::kEmulatorAudioMixer,
      };
      break;

    case EditorType::kAgent:
      preset.name = "Agent";
      preset.description = "AI Agent Configuration and Chat";
      preset.default_visible_cards = {
          "agent.configuration",
          "agent.status",
          "agent.chat",
      };
      preset.optional_cards = {
          "agent.prompt_editor",
          "agent.profiles",
          "agent.history",
          "agent.metrics",
          "agent.builder"};
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
  presets[EditorType::kAgent] = GetDefaultPreset(EditorType::kAgent);

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
  preset.description = "Essential cards only for focused editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  // Core editing cards across editors
  preset.default_visible_cards = {
      Cards::kOverworldCanvas,
      Cards::kDungeonControlPanel,
      Cards::kGraphicsSheetEditor,
      Cards::kPaletteControlPanel,
      Cards::kSpriteVanillaEditor,
      Cards::kMusicTracker,
      Cards::kMessageEditor,
      Cards::kAssemblyEditor,
      Cards::kEmulatorPpuViewer,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetDeveloperPreset() {
  CardLayoutPreset preset;
  preset.name = "Developer";
  preset.description = "Debug and development focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_cards = {
      // Emulator/debug cards
      Cards::kEmulatorCpuDebugger,
      Cards::kEmulatorPpuViewer,
      Cards::kEmulatorMemoryViewer,
      Cards::kEmulatorBreakpoints,
      Cards::kEmulatorPerformance,
      Cards::kEmulatorApuDebugger,
      Cards::kMemoryHexEditor,
      // Assembly editing
      Cards::kAssemblyEditor,
      Cards::kAssemblyFileBrowser,
      // Dungeon debug controls
      Cards::kDungeonDebugControls,
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
      // Graphics cards
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
      Cards::kGraphicsPlayerAnimations,
      Cards::kGraphicsPrototypeViewer,
      // Palette cards
      Cards::kPaletteControlPanel,
      Cards::kPaletteOwMain,
      Cards::kPaletteOwAnimated,
      Cards::kPaletteDungeonMain,
      Cards::kPaletteSprites,
      Cards::kPaletteQuickAccess,
      // Sprite cards
      Cards::kSpriteVanillaEditor,
      Cards::kSpriteCustomEditor,
      // Screen editing for menus/title
      Cards::kScreenTitleScreen,
      Cards::kScreenInventoryMenu,
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
      Cards::kOverworldTile8Selector,
      Cards::kOverworldAreaGraphics,
      Cards::kOverworldGfxGroups,
      // Dungeon cards
      Cards::kDungeonControlPanel,
      Cards::kDungeonRoomSelector,
      Cards::kDungeonObjectEditor,
      Cards::kDungeonPaletteEditor,
      Cards::kDungeonEntrances,
      // Graphics cards
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
      // Palette cards
      Cards::kPaletteControlPanel,
      Cards::kPaletteOwMain,
      // Sprite cards
      Cards::kSpriteVanillaEditor,
      // Message editing
      Cards::kMessageList,
      Cards::kMessageEditor,
      // AI Agent for assistance
      Cards::kEmulatorAiAgent,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetOverworldExpertPreset() {
  CardLayoutPreset preset;
  preset.name = "Overworld Expert";
  preset.description = "Complete overworld editing toolkit";
  preset.editor_type = EditorType::kOverworld;
  preset.default_visible_cards = {
      // All overworld cards
      Cards::kOverworldCanvas,
      Cards::kOverworldTile16Selector,
      Cards::kOverworldTile8Selector,
      Cards::kOverworldAreaGraphics,
      Cards::kOverworldScratch,
      Cards::kOverworldGfxGroups,
      Cards::kOverworldUsageStats,
      Cards::kOverworldV3Settings,
      // Palette support
      Cards::kPaletteControlPanel,
      Cards::kPaletteOwMain,
      Cards::kPaletteOwAnimated,
      // Graphics for tile editing
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetDungeonExpertPreset() {
  CardLayoutPreset preset;
  preset.name = "Dungeon Expert";
  preset.description = "Complete dungeon editing toolkit";
  preset.editor_type = EditorType::kDungeon;
  preset.default_visible_cards = {
      // All dungeon cards
      Cards::kDungeonControlPanel,
      Cards::kDungeonRoomSelector,
      Cards::kDungeonRoomMatrix,
      Cards::kDungeonEntrances,
      Cards::kDungeonRoomGraphics,
      Cards::kDungeonObjectEditor,
      Cards::kDungeonPaletteEditor,
      Cards::kDungeonDebugControls,
      // Palette support
      Cards::kPaletteControlPanel,
      Cards::kPaletteDungeonMain,
      // Graphics for room editing
      Cards::kGraphicsSheetBrowser,
      Cards::kGraphicsSheetEditor,
      // Screen maps for dungeon navigation
      Cards::kScreenDungeonMaps,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetTestingPreset() {
  CardLayoutPreset preset;
  preset.name = "Testing";
  preset.description = "Quality assurance and ROM testing layout";
  preset.editor_type = EditorType::kEmulator;
  preset.default_visible_cards = {
      // Emulator core
      Cards::kEmulatorPpuViewer,
      Cards::kEmulatorSaveStates,
      Cards::kEmulatorKeyboardConfig,
      Cards::kEmulatorPerformance,
      // Debug tools
      Cards::kEmulatorCpuDebugger,
      Cards::kEmulatorBreakpoints,
      Cards::kEmulatorMemoryViewer,
      // Memory inspection
      Cards::kMemoryHexEditor,
      // AI Agent for test assistance
      Cards::kEmulatorAiAgent,
  };
  return preset;
}

CardLayoutPreset LayoutPresets::GetAudioPreset() {
  CardLayoutPreset preset;
  preset.name = "Audio";
  preset.description = "Music and sound editing layout";
  preset.editor_type = EditorType::kMusic;
  preset.default_visible_cards = {
      // Music editing
      Cards::kMusicTracker,
      Cards::kMusicInstrumentEditor,
      Cards::kMusicAssembly,
      // Audio debugging
      Cards::kEmulatorApuDebugger,
      Cards::kEmulatorAudioMixer,
      // Assembly for custom sound code
      Cards::kAssemblyEditor,
      Cards::kAssemblyFileBrowser,
  };
  return preset;
}

}  // namespace editor
}  // namespace yaze
