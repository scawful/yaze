#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_PRESETS_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_PRESETS_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"

namespace yaze {
namespace editor {

/**
 * @enum DockPosition
 * @brief Preferred dock position for a card in a layout
 */
enum class DockPosition {
  Center,
  Left,
  Right,
  Bottom,
  Top,
  LeftBottom,
  RightBottom,
  RightTop,
  LeftTop
};

/**
 * @struct PanelLayoutPreset
 * @brief Defines default panel visibility for an editor type
 */
struct PanelLayoutPreset {
  std::string name;
  std::string description;
  EditorType editor_type;
  std::vector<std::string> default_visible_panels;
  std::vector<std::string> optional_panels;  // Available but hidden by default
  std::unordered_map<std::string, DockPosition> panel_positions;
};

/**
 * @class LayoutPresets
 * @brief Centralized definition of default layouts per editor
 *
 * Provides default panel configurations for each editor type:
 * - Overworld: Main canvas, Tile16 editor docked right
 * - Dungeon: Room selector, Object editor, Properties panel
 * - Graphics: Sheet browser, Palette editor, Preview pane
 * - Debug/Agent: Emulator, Memory editor, Disassembly, Agent chat
 *
 * These presets are applied when switching to an editor for the first time
 * or when user requests "Reset to Default Layout".
 */
class LayoutPresets {
 public:
  /**
   * @brief Get the default layout preset for an editor type
   * @param type The editor type
   * @return PanelLayoutPreset with default panels
   */
  static PanelLayoutPreset GetDefaultPreset(EditorType type);

  /**
   * @brief Get all available presets
   * @return Map of editor type to preset
   */
  static std::unordered_map<EditorType, PanelLayoutPreset> GetAllPresets();

  /**
   * @brief Get default visible panels for an editor
   * @param type The editor type
   * @return Vector of panel IDs that should be visible by default
   */
  static std::vector<std::string> GetDefaultPanels(EditorType type);

  /**
   * @brief Get all available panels for an editor (visible + hidden)
   * @param type The editor type
   * @return Vector of all panel IDs available for this editor
   */
  static std::vector<std::string> GetAllPanelsForEditor(EditorType type);

  /**
   * @brief Check if a panel should be visible by default
   * @param type The editor type
   * @param panel_id The panel ID to check
   * @return True if panel should be visible by default
   */
  static bool IsDefaultPanel(EditorType type, const std::string& panel_id);

  // ============================================================================
  // Named Workspace Presets
  // ============================================================================

  /**
   * @brief Get the "minimal" workspace preset (minimal cards)
   */
  static PanelLayoutPreset GetMinimalPreset();

  /**
   * @brief Get the "developer" workspace preset (debug-focused)
   */
  static PanelLayoutPreset GetDeveloperPreset();

  /**
   * @brief Get the "designer" workspace preset (visual-focused)
   */
  static PanelLayoutPreset GetDesignerPreset();

  /**
   * @brief Get the "modder" workspace preset (full-featured)
   */
  static PanelLayoutPreset GetModderPreset();

  /**
   * @brief Get the "overworld expert" workspace preset
   */
  static PanelLayoutPreset GetOverworldExpertPreset();

  /**
   * @brief Get the "dungeon expert" workspace preset
   */
  static PanelLayoutPreset GetDungeonExpertPreset();

  /**
   * @brief Get the "testing" workspace preset (QA focused)
   */
  static PanelLayoutPreset GetTestingPreset();

  /**
   * @brief Get the "audio" workspace preset (music focused)
   */
  static PanelLayoutPreset GetAudioPreset();

  // ============================================================================
  // Panel ID Constants - synced with actual editor registrations
  // ============================================================================
  struct Panels {
    // Overworld cards (overworld_editor.cc)
    static constexpr const char* kOverworldCanvas = "overworld.canvas";
    static constexpr const char* kOverworldTile16Editor = "overworld.tile16_editor";
    static constexpr const char* kOverworldTile16Selector = "overworld.tile16_selector";
    static constexpr const char* kOverworldTile8Selector = "overworld.tile8_selector";
    static constexpr const char* kOverworldAreaGraphics = "overworld.area_graphics";
    static constexpr const char* kOverworldScratch = "overworld.scratch";
    static constexpr const char* kOverworldGfxGroups = "overworld.gfx_groups";
    static constexpr const char* kOverworldUsageStats = "overworld.usage_stats";
    static constexpr const char* kOverworldV3Settings = "overworld.v3_settings";

    // Dungeon cards (dungeon_editor_v2.cc)
    static constexpr const char* kDungeonRoomSelector = "dungeon.room_selector";
    static constexpr const char* kDungeonRoomMatrix = "dungeon.room_matrix";
    static constexpr const char* kDungeonEntrances = "dungeon.entrances";
    static constexpr const char* kDungeonRoomGraphics = "dungeon.room_graphics";
    static constexpr const char* kDungeonObjectEditor = "dungeon.object_editor";
    static constexpr const char* kDungeonPaletteEditor = "dungeon.palette_editor";

    // Graphics cards (graphics_editor.cc)
    static constexpr const char* kGraphicsSheetEditor = "graphics.pixel_editor";
    static constexpr const char* kGraphicsSheetBrowser = "graphics.sheet_browser_v2";
    static constexpr const char* kGraphicsPlayerAnimations = "graphics.link_sprite_editor";
    static constexpr const char* kGraphicsPaletteControls = "graphics.palette_controls";
    static constexpr const char* kGraphicsGfxGroupEditor = "graphics.gfx_group_editor";
    static constexpr const char* kGraphicsPalettesetEditor = "graphics.paletteset_editor";

    // Palette cards (palette_editor.cc)
    static constexpr const char* kPaletteControlPanel = "palette.control_panel";
    static constexpr const char* kPaletteOwMain = "palette.ow_main";
    static constexpr const char* kPaletteOwAnimated = "palette.ow_animated";
    static constexpr const char* kPaletteDungeonMain = "palette.dungeon_main";
    static constexpr const char* kPaletteSprites = "palette.sprites";
    static constexpr const char* kPaletteSpritesAux1 = "palette.sprites_aux1";
    static constexpr const char* kPaletteSpritesAux2 = "palette.sprites_aux2";
    static constexpr const char* kPaletteSpritesAux3 = "palette.sprites_aux3";
    static constexpr const char* kPaletteEquipment = "palette.equipment";
    static constexpr const char* kPaletteQuickAccess = "palette.quick_access";
    static constexpr const char* kPaletteCustom = "palette.custom";

    // Sprite cards (sprite_editor.cc)
    static constexpr const char* kSpriteVanillaEditor = "sprite.vanilla_editor";
    static constexpr const char* kSpriteCustomEditor = "sprite.custom_editor";

    // Screen cards (screen_editor.cc)
    static constexpr const char* kScreenDungeonMaps = "screen.dungeon_maps";
    static constexpr const char* kScreenInventoryMenu = "screen.inventory_menu";
    static constexpr const char* kScreenOverworldMap = "screen.overworld_map";
    static constexpr const char* kScreenTitleScreen = "screen.title_screen";
    static constexpr const char* kScreenNamingScreen = "screen.naming_screen";

    // Music cards (music_editor.cc)
    static constexpr const char* kMusicSongBrowser = "music.song_browser";
    static constexpr const char* kMusicPlaybackControl = "music.tracker";  // Playback control panel
    static constexpr const char* kMusicPianoRoll = "music.piano_roll";
    static constexpr const char* kMusicInstrumentEditor = "music.instrument_editor";
    static constexpr const char* kMusicSampleEditor = "music.sample_editor";
    static constexpr const char* kMusicAssembly = "music.assembly";

    // Message cards (message_editor.cc)
    static constexpr const char* kMessageList = "message.message_list";
    static constexpr const char* kMessageEditor = "message.message_editor";
    static constexpr const char* kMessageFontAtlas = "message.font_atlas";
    static constexpr const char* kMessageDictionary = "message.dictionary";

    // Assembly cards (assembly_editor.cc)
    static constexpr const char* kAssemblyEditor = "assembly.editor";
    static constexpr const char* kAssemblyFileBrowser = "assembly.file_browser";

    // Emulator cards (editor_manager.cc)
    static constexpr const char* kEmulatorCpuDebugger = "emulator.cpu_debugger";
    static constexpr const char* kEmulatorPpuViewer = "emulator.ppu_viewer";
    static constexpr const char* kEmulatorMemoryViewer = "emulator.memory_viewer";
    static constexpr const char* kEmulatorBreakpoints = "emulator.breakpoints";
    static constexpr const char* kEmulatorPerformance = "emulator.performance";
    static constexpr const char* kEmulatorAiAgent = "emulator.ai_agent";
    static constexpr const char* kEmulatorSaveStates = "emulator.save_states";
    static constexpr const char* kEmulatorKeyboardConfig = "emulator.keyboard_config";
    static constexpr const char* kEmulatorApuDebugger = "emulator.apu_debugger";
    static constexpr const char* kEmulatorAudioMixer = "emulator.audio_mixer";

    // Memory cards (editor_manager.cc)
    static constexpr const char* kMemoryHexEditor = "memory.hex_editor";
  };
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_PRESETS_H_
