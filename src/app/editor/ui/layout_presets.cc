#include "app/editor/ui/layout_presets.h"

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
          Cards::kOverworldCanvas,
          Cards::kOverworldTile16Selector,
      };
      preset.panel_positions = {
          {Cards::kOverworldCanvas, DockPosition::Center},
          {Cards::kOverworldTile16Selector, DockPosition::Right},
          {Cards::kOverworldTile8Selector, DockPosition::Right},
          {Cards::kOverworldAreaGraphics, DockPosition::Left},
          {Cards::kOverworldScratch, DockPosition::Bottom},
      };
      preset.optional_panels = {
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
      preset.default_visible_panels = {
          Cards::kDungeonControlPanel,
          Cards::kDungeonRoomSelector,
      };
      preset.panel_positions = {
          {Cards::kDungeonControlPanel, DockPosition::Center}, // Controls implies canvas usually
          {Cards::kDungeonRoomSelector, DockPosition::Left},
          {Cards::kDungeonRoomMatrix, DockPosition::RightTop},
          {Cards::kDungeonEntrances, DockPosition::RightBottom},
          {Cards::kDungeonObjectEditor, DockPosition::Right},
          {Cards::kDungeonPaletteEditor, DockPosition::Bottom},
      };
      preset.optional_panels = {
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
      preset.default_visible_panels = {
          Cards::kGraphicsSheetBrowser,
          Cards::kGraphicsSheetEditor,
      };
      preset.panel_positions = {
          {Cards::kGraphicsSheetEditor, DockPosition::Center},
          {Cards::kGraphicsSheetBrowser, DockPosition::Left},
          {Cards::kGraphicsPlayerAnimations, DockPosition::Bottom},
          {Cards::kGraphicsPrototypeViewer, DockPosition::Right},
      };
      preset.optional_panels = {
          Cards::kGraphicsPlayerAnimations,
          Cards::kGraphicsPrototypeViewer,
      };
      break;

    case EditorType::kPalette:
      preset.name = "Palette Default";
      preset.description = "Palette groups with editor and preview";
      preset.default_visible_panels = {
          Cards::kPaletteControlPanel,
          Cards::kPaletteOwMain,
      };
      preset.panel_positions = {
          {Cards::kPaletteOwMain, DockPosition::Center},
          {Cards::kPaletteControlPanel, DockPosition::Left},
          {Cards::kPaletteQuickAccess, DockPosition::Right},
      };
      preset.optional_panels = {
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
      preset.default_visible_panels = {
          Cards::kSpriteVanillaEditor,
      };
      preset.panel_positions = {
          {Cards::kSpriteVanillaEditor, DockPosition::Left},
          {Cards::kSpriteCustomEditor, DockPosition::Right},
      };
      preset.optional_panels = {
          Cards::kSpriteCustomEditor,
      };
      break;

    case EditorType::kScreen:
      preset.name = "Screen Default";
      preset.description = "Screen browser with tileset editor";
      preset.default_visible_panels = {
          Cards::kScreenDungeonMaps,
      };
      preset.panel_positions = {
          {Cards::kScreenDungeonMaps, DockPosition::Center},
          {Cards::kScreenTitleScreen, DockPosition::RightTop},
          {Cards::kScreenInventoryMenu, DockPosition::RightBottom},
          {Cards::kScreenNamingScreen, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Cards::kScreenTitleScreen,
          Cards::kScreenInventoryMenu,
          Cards::kScreenOverworldMap,
          Cards::kScreenNamingScreen,
      };
      break;

    case EditorType::kMusic:
      preset.name = "Music Default";
      preset.description = "Song browser with playback control and piano roll";
      preset.default_visible_panels = {
          Cards::kMusicSongBrowser,
          Cards::kMusicPlaybackControl,
          Cards::kMusicPianoRoll,
      };
      preset.panel_positions = {
          {Cards::kMusicSongBrowser, DockPosition::Left},
          {Cards::kMusicPlaybackControl, DockPosition::Top},
          {Cards::kMusicPianoRoll, DockPosition::Center},
          {Cards::kMusicInstrumentEditor, DockPosition::Right},
          {Cards::kMusicSampleEditor, DockPosition::RightBottom},
          {Cards::kMusicAssembly, DockPosition::Bottom},
      };
      preset.optional_panels = {
          Cards::kMusicInstrumentEditor,
          Cards::kMusicSampleEditor,
          Cards::kMusicAssembly,
      };
      break;

    case EditorType::kMessage:
      preset.name = "Message Default";
      preset.description = "Message list with editor and preview";
      preset.default_visible_panels = {
          Cards::kMessageList,
          Cards::kMessageEditor,
      };
      preset.panel_positions = {
          {Cards::kMessageEditor, DockPosition::Center},
          {Cards::kMessageList, DockPosition::Left},
          {Cards::kMessageFontAtlas, DockPosition::RightTop},
          {Cards::kMessageDictionary, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Cards::kMessageFontAtlas,
          Cards::kMessageDictionary,
      };
      break;

    case EditorType::kAssembly:
      preset.name = "Assembly Default";
      preset.description = "Assembly editor with file browser";
      preset.default_visible_panels = {
          Cards::kAssemblyEditor,
      };
      preset.panel_positions = {
          {Cards::kAssemblyEditor, DockPosition::Left},
          {Cards::kAssemblyFileBrowser, DockPosition::RightBottom},
      };
      preset.optional_panels = {
          Cards::kAssemblyFileBrowser,
      };
      break;

    case EditorType::kEmulator:
      preset.name = "Emulator Default";
      preset.description = "Emulator with debugger tools";
      preset.default_visible_panels = {
          Cards::kEmulatorPpuViewer,
      };
      preset.panel_positions = {
          {Cards::kEmulatorPpuViewer, DockPosition::Center},
          {Cards::kEmulatorCpuDebugger, DockPosition::Right},
          {Cards::kEmulatorMemoryViewer, DockPosition::Bottom},
          {Cards::kEmulatorAiAgent, DockPosition::RightBottom},
      };
      preset.optional_panels = {
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
      preset.default_visible_panels = {
          "agent.configuration",
          "agent.status",
          "agent.chat",
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
      Cards::kOverworldCanvas,
      Cards::kDungeonControlPanel,
      Cards::kGraphicsSheetEditor,
      Cards::kPaletteControlPanel,
      Cards::kSpriteVanillaEditor,
      Cards::kMusicSongBrowser,
      Cards::kMusicPlaybackControl,
      Cards::kMessageEditor,
      Cards::kAssemblyEditor,
      Cards::kEmulatorPpuViewer,
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

PanelLayoutPreset LayoutPresets::GetDesignerPreset() {
  PanelLayoutPreset preset;
  preset.name = "Designer";
  preset.description = "Visual and artistic focused layout";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_panels = {
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

PanelLayoutPreset LayoutPresets::GetModderPreset() {
  PanelLayoutPreset preset;
  preset.name = "Modder";
  preset.description = "Full-featured layout for comprehensive editing";
  preset.editor_type = EditorType::kUnknown;  // Applies to all
  preset.default_visible_panels = {
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

PanelLayoutPreset LayoutPresets::GetOverworldExpertPreset() {
  PanelLayoutPreset preset;
  preset.name = "Overworld Expert";
  preset.description = "Complete overworld editing toolkit";
  preset.editor_type = EditorType::kOverworld;
  preset.default_visible_panels = {
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

PanelLayoutPreset LayoutPresets::GetDungeonExpertPreset() {
  PanelLayoutPreset preset;
  preset.name = "Dungeon Expert";
  preset.description = "Complete dungeon editing toolkit";
  preset.editor_type = EditorType::kDungeon;
  preset.default_visible_panels = {
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

PanelLayoutPreset LayoutPresets::GetTestingPreset() {
  PanelLayoutPreset preset;
  preset.name = "Testing";
  preset.description = "Quality assurance and ROM testing layout";
  preset.editor_type = EditorType::kEmulator;
  preset.default_visible_panels = {
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

PanelLayoutPreset LayoutPresets::GetAudioPreset() {
  PanelLayoutPreset preset;
  preset.name = "Audio";
  preset.description = "Music and sound editing layout";
  preset.editor_type = EditorType::kMusic;
  preset.default_visible_panels = {
      // Music editing
      Cards::kMusicSongBrowser,
      Cards::kMusicPlaybackControl,
      Cards::kMusicPianoRoll,
      Cards::kMusicInstrumentEditor,
      Cards::kMusicSampleEditor,
      Cards::kMusicAssembly,
      // Audio debugging
      Cards::kEmulatorApuDebugger,
      Cards::kEmulatorAudioMixer,
      // Assembly for custom sound code
      Cards::kAssemblyEditor,
      Cards::kAssemblyFileBrowser,
  };
  preset.panel_positions = {
      {Cards::kMusicSongBrowser, DockPosition::Left},
      {Cards::kMusicPlaybackControl, DockPosition::Top},
      {Cards::kMusicPianoRoll, DockPosition::Center},
      {Cards::kMusicInstrumentEditor, DockPosition::Right},
      {Cards::kMusicSampleEditor, DockPosition::RightBottom},
      {Cards::kMusicAssembly, DockPosition::Bottom},
      {Cards::kEmulatorApuDebugger, DockPosition::LeftBottom},
      {Cards::kEmulatorAudioMixer, DockPosition::RightTop},
  };
  return preset;
}

}  // namespace editor
}  // namespace yaze
