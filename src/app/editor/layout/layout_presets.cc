#include "app/editor/layout/layout_presets.h"

namespace yaze {
namespace editor {

PanelLayoutPreset LayoutPresets::GetDefaultPreset(EditorType type) {
  PanelLayoutPreset preset;
  preset.editor_type = type;

  switch (type) {
    case EditorType::kOverworld:
      preset.name = "Overworld Default";
      preset.description = "Main canvas with tile16 editor";
      preset.default_visible_panels = {
          Panels::kOverworldCanvas,
          Panels::kOverworldTile16Selector,
      };
      preset.panel_positions = {
          {Panels::kOverworldCanvas, DockPosition::Center},
          {Panels::kOverworldTile16Selector, DockPosition::Right},
          {Panels::kOverworldTile8Selector, DockPosition::Right},
          {Panels::kOverworldAreaGraphics, DockPosition::Left},
          {Panels::kOverworldScratch, DockPosition::Bottom},
      };
      preset.optional_panels = {
          Panels::kOverworldTile8Selector,
          Panels::kOverworldAreaGraphics,
          Panels::kOverworldScratch,
          Panels::kOverworldGfxGroups,
          Panels::kOverworldUsageStats,
          Panels::kOverworldV3Settings,
      };
      break;

    case EditorType::kDungeon:
      preset.name = "Dungeon Default";
      preset.description = "Room editor with object palette and properties";
      preset.default_visible_panels = {
          Panels::kDungeonControlPanel,
          Panels::kDungeonRoomSelector,
      };
      preset.panel_positions = {
          {Panels::kDungeonControlPanel, DockPosition::Center}, // Controls implies canvas usually
          {Panels::kDungeonRoomSelector, DockPosition::Left},
          {Panels::kDungeonRoomMatrix, DockPosition::RightTop},
          {Panels::kDungeonEntrances, DockPosition::RightBottom},
          {Panels::kDungeonObjectEditor, DockPosition::Right},
          {Panels::kDungeonPaletteEditor, DockPosition::Bottom},
      };
      preset.optional_panels = {
          Panels::kDungeonObjectEditor,
          Panels::kDungeonPaletteEditor,
          Panels::kDungeonRoomMatrix,
          Panels::kDungeonEntrances,
          Panels::kDungeonRoomGraphics,
          Panels::kDungeonDebugControls,
      };
      break;

    case EditorType::kGraphics:
      preset.name = "Graphics Default";
      preset.description = "Sheet browser with editor";
      preset.default_visible_panels = {
          Panels::kGraphicsSheetBrowser,
          Panels::kGraphicsSheetEditor,
      };
      preset.panel_positions = {
          {Panels::kGraphicsSheetEditor, DockPosition::Center},
          {Panels::kGraphicsSheetBrowser, DockPosition::Left},
          {Panels::kGraphicsPlayerAnimations, DockPosition::Bottom},
          {Panels::kGraphicsPrototypeViewer, DockPosition::Right},
      };
      preset.optional_panels = {
          Panels::kGraphicsPlayerAnimations,
          Panels::kGraphicsPrototypeViewer,
      };
      break;

    case EditorType::kPalette:
      preset.name = "Palette Default";
      preset.description = "Palette groups with editor and preview";
      preset.default_visible_panels = {
          Panels::kPaletteControlPanel,
          Panels::kPaletteOwMain,
      };
      preset.panel_positions = {
          {Panels::kPaletteOwMain, DockPosition::Center},
          {Panels::kPaletteControlPanel, DockPosition::Left},
          {Panels::kPaletteQuickAccess, DockPosition::Right},
      };
      preset.optional_panels = {
          Panels::kPaletteQuickAccess,
          Panels::kPaletteOwAnimated,
          Panels::kPaletteDungeonMain,
          Panels::kPaletteSprites,
          Panels::kPaletteSpritesAux1,
          Panels::kPaletteSpritesAux2,
          Panels::kPaletteSpritesAux3,
          Panels::kPaletteEquipment,
          Panels::kPaletteCustom,
      };
      break;

    case EditorType::kSprite:
      preset.name = "Sprite Default";
      preset.description = "Sprite browser with editor";
      preset.default_visible_panels = {
          Panels::kSpriteVanillaEditor,
      };
      preset.panel_positions = {
          {Panels::kSpriteVanillaEditor, DockPosition::Left},
          {Panels::kSpriteCustomEditor, DockPosition::Right},
      };
      preset.optional_panels = {
          Panels::kSpriteCustomEditor,
      };
      break;

    case EditorType::kScreen:
      preset.name = "Screen Default";
      preset.description = "Screen browser with tileset editor";
      preset.default_visible_panels = {
          Panels::kScreenDungeonMaps,
      };
      preset.panel_positions = {
          {Panels::kScreenDungeonMaps, DockPosition::Center},
          {Panels::kScreenTitleScreen, DockPosition::RightTop},
          {Panels::kScreenInventoryMenu, DockPosition::RightBottom},
          {Panels::kScreenNamingScreen, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Panels::kScreenTitleScreen,
          Panels::kScreenInventoryMenu,
          Panels::kScreenOverworldMap,
          Panels::kScreenNamingScreen,
      };
      break;

    case EditorType::kMusic:
      preset.name = "Music Default";
      preset.description = "Song browser with playback control and piano roll";
      preset.default_visible_panels = {
          Panels::kMusicSongBrowser,
          Panels::kMusicPlaybackControl,
          Panels::kMusicPianoRoll,
      };
      preset.panel_positions = {
          {Panels::kMusicSongBrowser, DockPosition::Left},
          {Panels::kMusicPlaybackControl, DockPosition::Top},
          {Panels::kMusicPianoRoll, DockPosition::Center},
          {Panels::kMusicInstrumentEditor, DockPosition::Right},
          {Panels::kMusicSampleEditor, DockPosition::RightBottom},
          {Panels::kMusicAssembly, DockPosition::Bottom},
      };
      preset.optional_panels = {
          Panels::kMusicInstrumentEditor,
          Panels::kMusicSampleEditor,
          Panels::kMusicAssembly,
      };
      break;

    case EditorType::kMessage:
      preset.name = "Message Default";
      preset.description = "Message list with editor and preview";
      preset.default_visible_panels = {
          Panels::kMessageList,
          Panels::kMessageEditor,
      };
      preset.panel_positions = {
          {Panels::kMessageEditor, DockPosition::Center},
          {Panels::kMessageList, DockPosition::Left},
          {Panels::kMessageFontAtlas, DockPosition::RightTop},
          {Panels::kMessageDictionary, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Panels::kMessageFontAtlas,
          Panels::kMessageDictionary,
      };
      break;

    case EditorType::kAssembly:
      preset.name = "Assembly Default";
      preset.description = "Assembly editor with file browser";
      preset.default_visible_panels = {
          Panels::kAssemblyEditor,
      };
      preset.panel_positions = {
          {Panels::kAssemblyEditor, DockPosition::Left},
          {Panels::kAssemblyFileBrowser, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Panels::kAssemblyFileBrowser,
      };
      break;

    case EditorType::kEmulator:
      preset.name = "Emulator Default";
      preset.description = "Emulator with debugger tools";
      preset.default_visible_panels = {
          Panels::kEmulatorPpuViewer,
      };
      preset.panel_positions = {
          {Panels::kEmulatorPpuViewer, DockPosition::Center},
          {Panels::kEmulatorCpuDebugger, DockPosition::Right},
          {Panels::kEmulatorMemoryViewer, DockPosition::Bottom},
          {Panels::kEmulatorAiAgent, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Panels::kEmulatorCpuDebugger,
          Panels::kEmulatorMemoryViewer,
          Panels::kEmulatorBreakpoints,
          Panels::kEmulatorPerformance,
          Panels::kEmulatorAiAgent,
          Panels::kEmulatorSaveStates,
          Panels::kEmulatorKeyboardConfig,
          Panels::kEmulatorApuDebugger,
          Panels::kEmulatorAudioMixer,
      };
      break;

    case EditorType::kHex:
      preset.name = "Memory Default";
      preset.description = "Hex editor and memory inspection";
      preset.default_visible_panels = {
          Panels::kMemoryHexEditor,
      };
      preset.panel_positions = {
          {Panels::kMemoryHexEditor, DockPosition::Center},
      };
      break;

    case EditorType::kAgent:
      preset.name = "Agent";
      preset.description = "AI Agent Configuration and Chat";
      preset.default_visible_panels = {
          "agent.configuration",
          "agent.status",
          "agent.chat",
      };
      preset.panel_positions = {
          {"agent.chat", DockPosition::Center},
          {"agent.configuration", DockPosition::Right},
          {"agent.status", DockPosition::Bottom},
      };
      preset.optional_panels = {
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

std::unordered_map<EditorType, PanelLayoutPreset> LayoutPresets::GetAllPresets() {
  std::unordered_map<EditorType, PanelLayoutPreset> presets;

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
  presets[EditorType::kHex] = GetDefaultPreset(EditorType::kHex);
  presets[EditorType::kAgent] = GetDefaultPreset(EditorType::kAgent);

  return presets;
}

std::vector<std::string> LayoutPresets::GetDefaultPanels(EditorType type) {
  return GetDefaultPreset(type).default_visible_panels;
}

std::vector<std::string> LayoutPresets::GetAllPanelsForEditor(EditorType type) {
  auto preset = GetDefaultPreset(type);
  std::vector<std::string> all_panels = preset.default_visible_panels;
  all_panels.insert(all_panels.end(), preset.optional_panels.begin(),
                    preset.optional_panels.end());
  return all_panels;
}

bool LayoutPresets::IsDefaultPanel(EditorType type, const std::string& panel_id) {
  auto default_panels = GetDefaultPanels(type);
  return std::find(default_panels.begin(), default_panels.end(), panel_id) !=
         default_panels.end();
}

// ============================================================================
// Named Workspace Presets
// ============================================================================

PanelLayoutPreset LayoutPresets::GetMinimalPreset() {
  PanelLayoutPreset preset;
  preset.name = "Minimal";
  preset.description = "Essential cards only for focused editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  // Core editing cards across editors
  preset.default_visible_panels = {
      Panels::kOverworldCanvas,
      Panels::kDungeonControlPanel,
      Panels::kGraphicsSheetEditor,
      Panels::kPaletteControlPanel,
      Panels::kSpriteVanillaEditor,
      Panels::kMusicSongBrowser,
      Panels::kMusicPlaybackControl,
      Panels::kMessageEditor,
      Panels::kAssemblyEditor,
      Panels::kEmulatorPpuViewer,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetDeveloperPreset() {
  PanelLayoutPreset preset;
  preset.name = "Developer";
  preset.description = "Debug and development focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_panels = {
      // Emulator/debug cards
      Panels::kEmulatorCpuDebugger,
      Panels::kEmulatorPpuViewer,
      Panels::kEmulatorMemoryViewer,
      Panels::kEmulatorBreakpoints,
      Panels::kEmulatorPerformance,
      Panels::kEmulatorApuDebugger,
      Panels::kMemoryHexEditor,
      // Assembly editing
      Panels::kAssemblyEditor,
      Panels::kAssemblyFileBrowser,
      // Dungeon debug controls
      Panels::kDungeonDebugControls,
      // AI Agent for debugging assistance
      Panels::kEmulatorAiAgent,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetDesignerPreset() {
  PanelLayoutPreset preset;
  preset.name = "Designer";
  preset.description = "Visual and artistic focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_panels = {
      // Graphics cards
      Panels::kGraphicsSheetBrowser,
      Panels::kGraphicsSheetEditor,
      Panels::kGraphicsPlayerAnimations,
      Panels::kGraphicsPrototypeViewer,
      // Palette cards
      Panels::kPaletteControlPanel,
      Panels::kPaletteOwMain,
      Panels::kPaletteOwAnimated,
      Panels::kPaletteDungeonMain,
      Panels::kPaletteSprites,
      Panels::kPaletteQuickAccess,
      // Sprite cards
      Panels::kSpriteVanillaEditor,
      Panels::kSpriteCustomEditor,
      // Screen editing for menus/title
      Panels::kScreenTitleScreen,
      Panels::kScreenInventoryMenu,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetModderPreset() {
  PanelLayoutPreset preset;
  preset.name = "Modder";
  preset.description = "Full-featured layout for comprehensive editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_panels = {
      // Overworld cards
      Panels::kOverworldCanvas,
      Panels::kOverworldTile16Selector,
      Panels::kOverworldTile8Selector,
      Panels::kOverworldAreaGraphics,
      Panels::kOverworldGfxGroups,
      // Dungeon cards
      Panels::kDungeonControlPanel,
      Panels::kDungeonRoomSelector,
      Panels::kDungeonObjectEditor,
      Panels::kDungeonPaletteEditor,
      Panels::kDungeonEntrances,
      // Graphics cards
      Panels::kGraphicsSheetBrowser,
      Panels::kGraphicsSheetEditor,
      // Palette cards
      Panels::kPaletteControlPanel,
      Panels::kPaletteOwMain,
      // Sprite cards
      Panels::kSpriteVanillaEditor,
      // Message editing
      Panels::kMessageList,
      Panels::kMessageEditor,
      // AI Agent for assistance
      Panels::kEmulatorAiAgent,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetOverworldExpertPreset() {
  PanelLayoutPreset preset;
  preset.name = "Overworld Expert";
  preset.description = "Complete overworld editing toolkit";
  preset.editor_type = EditorType::kOverworld;
  preset.default_visible_panels = {
      // All overworld cards
      Panels::kOverworldCanvas,
      Panels::kOverworldTile16Selector,
      Panels::kOverworldTile8Selector,
      Panels::kOverworldAreaGraphics,
      Panels::kOverworldScratch,
      Panels::kOverworldGfxGroups,
      Panels::kOverworldUsageStats,
      Panels::kOverworldV3Settings,
      // Palette support
      Panels::kPaletteControlPanel,
      Panels::kPaletteOwMain,
      Panels::kPaletteOwAnimated,
      // Graphics for tile editing
      Panels::kGraphicsSheetBrowser,
      Panels::kGraphicsSheetEditor,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetDungeonExpertPreset() {
  PanelLayoutPreset preset;
  preset.name = "Dungeon Expert";
  preset.description = "Complete dungeon editing toolkit";
  preset.editor_type = EditorType::kDungeon;
  preset.default_visible_panels = {
      // All dungeon cards
      Panels::kDungeonControlPanel,
      Panels::kDungeonRoomSelector,
      Panels::kDungeonRoomMatrix,
      Panels::kDungeonEntrances,
      Panels::kDungeonRoomGraphics,
      Panels::kDungeonObjectEditor,
      Panels::kDungeonPaletteEditor,
      Panels::kDungeonDebugControls,
      // Palette support
      Panels::kPaletteControlPanel,
      Panels::kPaletteDungeonMain,
      // Graphics for room editing
      Panels::kGraphicsSheetBrowser,
      Panels::kGraphicsSheetEditor,
      // Screen maps for dungeon navigation
      Panels::kScreenDungeonMaps,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetTestingPreset() {
  PanelLayoutPreset preset;
  preset.name = "Testing";
  preset.description = "Quality assurance and ROM testing layout";
  preset.editor_type = EditorType::kEmulator;
  preset.default_visible_panels = {
      // Emulator core
      Panels::kEmulatorPpuViewer,
      Panels::kEmulatorSaveStates,
      Panels::kEmulatorKeyboardConfig,
      Panels::kEmulatorPerformance,
      // Debug tools
      Panels::kEmulatorCpuDebugger,
      Panels::kEmulatorBreakpoints,
      Panels::kEmulatorMemoryViewer,
      // Memory inspection
      Panels::kMemoryHexEditor,
      // AI Agent for test assistance
      Panels::kEmulatorAiAgent,
  };
  return preset;
}

PanelLayoutPreset LayoutPresets::GetAudioPreset() {
  PanelLayoutPreset preset;
  preset.name = "Audio";
  preset.description = "Music and sound editing layout";
  preset.editor_type = EditorType::kMusic;
  preset.default_visible_panels = {
      // Music editing
      Panels::kMusicSongBrowser,
      Panels::kMusicPlaybackControl,
      Panels::kMusicPianoRoll,
      Panels::kMusicInstrumentEditor,
      Panels::kMusicSampleEditor,
      Panels::kMusicAssembly,
      // Audio debugging
      Panels::kEmulatorApuDebugger,
      Panels::kEmulatorAudioMixer,
      // Assembly for custom sound code
      Panels::kAssemblyEditor,
      Panels::kAssemblyFileBrowser,
  };
  preset.panel_positions = {
      {Panels::kMusicSongBrowser, DockPosition::Left},
      {Panels::kMusicPlaybackControl, DockPosition::Top},
      {Panels::kMusicPianoRoll, DockPosition::Center},
      {Panels::kMusicInstrumentEditor, DockPosition::Right},
      {Panels::kMusicSampleEditor, DockPosition::RightBottom},
      {Panels::kMusicAssembly, DockPosition::Bottom},
      {Panels::kEmulatorApuDebugger, DockPosition::LeftBottom},
      {Panels::kEmulatorAudioMixer, DockPosition::RightTop},
  };
  return preset;
}

}  // namespace editor
}  // namespace yaze
